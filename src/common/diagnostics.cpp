#include "diagnostics.h"

#include <QClipboard>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QHBoxLayout>
#include <QLabel>
#include <QLoggingCategory>
#include <QMutex>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QStyle>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include <cerrno>
#include <csignal>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <execinfo.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
const char *volatile g_phase = "démarrage";
bool g_debug = false;
QString g_app, g_logPath, g_reportPath;
FILE *g_log = nullptr;
int g_reportFd = -1, g_logFd = -1;
char g_exe[4096], g_reportC[4096], g_logC[4096], g_appC[128], g_header[4096];
int g_pipeFd = -1;          // extrémité « écriture » du tube vers le processus surveillant
bool g_noWatch = false;
alignas(16) char g_altStack[65536];
QMutex g_mutex;
int g_dialogs = 0;
bool g_inDialog = false;

void wr(int fd, const char *s) { if (fd >= 0 && s) { ssize_t r = write(fd, s, strlen(s)); (void)r; } }

void hex(char *out, unsigned long v)   // formate 0x... sans allocation (async-signal-safe)
{
    static const char d[] = "0123456789abcdef";
    char tmp[24]; int n = 0;
    do { tmp[n++] = d[v & 15]; v >>= 4; } while (v);
    int k = 0; out[k++] = '0'; out[k++] = 'x';
    while (n) out[k++] = tmp[--n];
    out[k] = 0;
}

const char *sigName(int s)
{
    switch (s) {
    case SIGSEGV: return "SIGSEGV (erreur de segmentation)";
    case SIGABRT: return "SIGABRT (abandon : assertion ou erreur fatale)";
    case SIGBUS:  return "SIGBUS (accès mémoire invalide)";
    case SIGFPE:  return "SIGFPE (erreur arithmétique)";
    case SIGILL:  return "SIGILL (instruction illégale)";
    default:      return "signal inconnu";
    }
}

void crashHandler(int sig, siginfo_t *si, void *)
{
    static volatile sig_atomic_t entered = 0;
    if (entered) _exit(128 + sig);
    entered = 1;

    void *frames[64];
    const int n = backtrace(frames, 64);
    char addr[32];
    hex(addr, (unsigned long)(si ? si->si_addr : nullptr));

    for (int fd : {g_reportFd, STDERR_FILENO, g_logFd}) {
        if (fd < 0) continue;
        wr(fd, "\n=== PLANTAGE ===\n");
        wr(fd, g_header);
        wr(fd, "Signal : "); wr(fd, sigName(sig)); wr(fd, "\nAdresse fautive : "); wr(fd, addr);
        wr(fd, "\nDernière étape atteinte : "); wr(fd, g_phase ? g_phase : "?");
        wr(fd, "\n\nPile d'appels :\n");
        backtrace_symbols_fd(frames, n, fd);
        wr(fd, "\n");
    }
    // On remet l'action par défaut et on relance le signal : le processus meurt (core dump éventuel) et
    // le surveillant détecte la fin anormale (tube fermé sans « K ») pour ouvrir la fenêtre de rapport.
    signal(sig, SIG_DFL);
    raise(sig);
}

void termHandler(int sig)       // Ctrl+C, kill, fermeture du terminal : arrêts voulus => « K »
{
    if (g_pipeFd >= 0) { char k = 'K'; ssize_t r = write(g_pipeFd, &k, 1); (void)r; }
    signal(sig, SIG_DFL);
    raise(sig);
}

void spawnWatcher()
{
    int fds[2];
    if (pipe(fds) != 0) return;
    fcntl(fds[1], F_SETFD, FD_CLOEXEC);          // ni l'exec du surveillant ni nos autres enfants ne la gardent
    const pid_t pid = fork();
    if (pid == 0) {
        close(fds[1]);
        setsid();
        char fdarg[16], pidarg[24];
        snprintf(fdarg, sizeof fdarg, "%d", fds[0]);
        snprintf(pidarg, sizeof pidarg, "%ld", (long)getppid());
        execl("/proc/self/exe", g_exe[0] ? g_exe : "kit", "--watch", fdarg, pidarg, g_appC, g_reportC, g_logC, (char *)nullptr);
        _exit(127);
    }
    close(fds[0]);
    if (pid < 0) { close(fds[1]); return; }
    g_pipeFd = fds[1];
}

QString tailLog(int lines)
{
    QFile f(g_logPath);
    if (!f.open(QIODevice::ReadOnly)) return QString();
    const QStringList all = QString::fromUtf8(f.readAll()).split('\n');
    return all.mid(qMax(0, all.size() - lines)).join('\n');
}

void messageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    const char *lvl = type == QtDebugMsg ? "DEBUG" : type == QtInfoMsg ? "INFO " :
                      type == QtWarningMsg ? "WARN " : type == QtCriticalMsg ? "ERREUR" : "FATAL";
    QString line = QStringLiteral("%1 [%2] %3").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"), lvl, msg);
    if (g_debug && ctx.file) line += QStringLiteral("  (%1:%2)").arg(QString::fromUtf8(ctx.file)).arg(ctx.line);
    {
        QMutexLocker lock(&g_mutex);
        std::fprintf(stderr, "%s\n", line.toLocal8Bit().constData());
        std::fflush(stderr);
        if (g_log) { std::fprintf(g_log, "%s\n", line.toUtf8().constData()); std::fflush(g_log); }
    }
    if (type == QtCriticalMsg && qApp && g_dialogs < 3) {
        ++g_dialogs;
        QMetaObject::invokeMethod(qApp, [msg] {
            if (g_inDialog) return;
            g_inDialog = true;
            Diag::showError(nullptr, QObject::tr("Erreur"), msg, tailLog(40));
            g_inDialog = false;
        }, Qt::QueuedConnection);
    }
    // QtFatalMsg : Qt appelle abort() ensuite -> SIGABRT -> rapport de plantage ci-dessus.
}

QString envInfo()
{
    auto e = [](const char *k) { const QByteArray v = qgetenv(k); return v.isEmpty() ? QStringLiteral("-") : QString::fromLocal8Bit(v); };
    return QStringLiteral("Programme : %1\nDémarré : %2\nQt compilé : %3 — Qt exécuté : %4\n"
                          "XDG_SESSION_TYPE=%5  DISPLAY=%6  WAYLAND_DISPLAY=%7\n"
                          "QT_QPA_PLATFORM=%8  QT_QPA_PLATFORMTHEME=%9  QT_STYLE_OVERRIDE=%10\n")
        .arg(g_app, QDateTime::currentDateTime().toString(Qt::ISODate), QStringLiteral(QT_VERSION_STR),
             QString::fromLatin1(qVersion()), e("XDG_SESSION_TYPE"), e("DISPLAY"), e("WAYLAND_DISPLAY"),
             e("QT_QPA_PLATFORM"), e("QT_QPA_PLATFORMTHEME"), e("QT_STYLE_OVERRIDE"));
}

void setHeader(const QString &s)
{
    const QByteArray b = s.toUtf8();
    const size_t n = qMin(size_t(b.size()), sizeof g_header - 1);
    memcpy(g_header, b.constData(), n);
    g_header[n] = 0;
}
}  // namespace

namespace Diag {

bool debug() { return g_debug; }
QString logPath() { return g_logPath; }

void phase(const char *name)
{
    g_phase = name;
    if (g_debug) { std::fprintf(stderr, "[trace] %s\n", name); std::fflush(stderr); }
}

void install(const char *appName, int argc, char **argv)
{
    g_app = QString::fromLatin1(appName);
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--debug")) g_debug = true;
        if (!strcmp(argv[i], "--cli") || !strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) g_noWatch = true;
    }
    if (qEnvironmentVariableIsSet("KIT_NO_WATCHER")) g_noWatch = true;
    if (qEnvironmentVariableIsSet("KIT_DEBUG")) g_debug = true;
    if (g_debug) QLoggingCategory::setFilterRules(QStringLiteral("qt.qpa.*=true\nqt.qpa.input*=false"));

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + "/kit-installer";
    QDir().mkpath(dir);
    g_logPath = dir + "/" + g_app + ".log";
    g_reportPath = dir + "/" + g_app + "-crash.txt";
    if (QFileInfo(g_logPath).size() > 512 * 1024) QFile::remove(g_logPath);
    g_log = std::fopen(g_logPath.toUtf8().constData(), "a");
    if (g_log) g_logFd = fileno(g_log);

    strncpy(g_appC, appName, sizeof g_appC - 1);
    strncpy(g_logC, g_logPath.toLocal8Bit().constData(), sizeof g_logC - 1);
    qInstallMessageHandler(messageHandler);
    setHeader(envInfo());
    std::fprintf(stderr, "[kit] %s — journal : %s%s\n", appName, g_logPath.toLocal8Bit().constData(),
                 g_debug ? "  (mode --debug)" : "  (ajoutez --debug pour plus de détails)");
    if (g_log) { std::fprintf(g_log, "\n---- Démarrage ----\n%s", g_header); std::fflush(g_log); }

    if (qEnvironmentVariableIsSet("KIT_NO_CRASH_HANDLER")) return;

    // Prépare la gestion des plantages
    const QByteArray exe = QFile::symLinkTarget("/proc/self/exe").toLocal8Bit();
    strncpy(g_exe, exe.constData(), sizeof g_exe - 1);
    strncpy(g_reportC, g_reportPath.toLocal8Bit().constData(), sizeof g_reportC - 1);
    g_reportFd = open(g_reportC, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    void *warm[4];
    backtrace(warm, 4);                       // précharge libgcc (backtrace n'est pas sûr au 1er appel)

    stack_t ss;
    ss.ss_sp = g_altStack; ss.ss_size = sizeof g_altStack; ss.ss_flags = 0;
    sigaltstack(&ss, nullptr);
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = crashHandler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
    sigemptyset(&sa.sa_mask);
    for (int s : {SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL}) sigaction(s, &sa, nullptr);
    if (!g_noWatch) {
        spawnWatcher();
        struct sigaction st;
        memset(&st, 0, sizeof st);
        st.sa_handler = termHandler;
        sigemptyset(&st.sa_mask);
        for (int s : {SIGINT, SIGTERM, SIGHUP}) sigaction(s, &st, nullptr);
    }
    phase("installation du diagnostic");
}

void appCreated()
{
    phase("QApplication créée");
    const QString extra = QStringLiteral("Plateforme Qt : %1\nStyle : %2\n")
                              .arg(QGuiApplication::platformName(), QApplication::style()->objectName());
    setHeader(envInfo() + extra);
    if (g_log) { std::fprintf(g_log, "%s", extra.toUtf8().constData()); std::fflush(g_log); }
    if (g_debug) {
        std::fprintf(stderr, "[kit] %s", g_header);
        std::fprintf(stderr, "[kit] Dossiers de plugins Qt : %s\n",
                     QCoreApplication::libraryPaths().join(':').toLocal8Bit().constData());
    }
    // Arrêt normal : on supprime le rapport vide.
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, [] {
        if (g_reportFd >= 0) { close(g_reportFd); g_reportFd = -1; QFile::remove(g_reportPath); }
    });
}

void showError(QWidget *parent, const QString &title, const QString &text, const QString &details)
{
    if (QThread::currentThread() != qApp->thread()) {
        QMetaObject::invokeMethod(qApp, [=] { showError(parent, title, text, details); }, Qt::QueuedConnection);
        return;
    }
    QDialog dlg(parent);
    dlg.setWindowTitle(title);
    dlg.resize(760, details.isEmpty() ? 220 : 520);
    auto *v = new QVBoxLayout(&dlg);
    auto *head = new QLabel(text);
    head->setWordWrap(true);
    head->setTextInteractionFlags(Qt::TextSelectableByMouse);
    v->addWidget(head);
    QString all = text;
    if (!details.isEmpty()) {
        auto *box = new QPlainTextEdit(details);
        box->setReadOnly(true);
        box->setLineWrapMode(QPlainTextEdit::NoWrap);
        v->addWidget(box, 1);
        all += "\n\n" + details;
    }
    if (!g_logPath.isEmpty()) {
        auto *p = new QLabel(QObject::tr("Journal complet : %1").arg(g_logPath));
        p->setTextInteractionFlags(Qt::TextSelectableByMouse);
        v->addWidget(p);
    }
    auto *row = new QHBoxLayout;
    auto *copy = new QPushButton(QObject::tr("Copier le rapport"));
    auto *close = new QPushButton(QObject::tr("Fermer"));
    row->addStretch(1); row->addWidget(copy); row->addWidget(close);
    v->addLayout(row);
    QObject::connect(copy, &QPushButton::clicked, [all] { QGuiApplication::clipboard()->setText(all); });
    QObject::connect(close, &QPushButton::clicked, &dlg, &QDialog::accept);
    dlg.exec();
}

void finish(int rc)
{
    if (rc == 0 && g_pipeFd >= 0) {            // arrêt normal : on prévient le surveillant
        char k = 'K';
        ssize_t r = write(g_pipeFd, &k, 1);
        (void)r;
    }
    if (rc == 0 && g_reportFd >= 0) { close(g_reportFd); g_reportFd = -1; QFile::remove(g_reportPath); }
    if (g_log) std::fflush(g_log);
}

bool watcherRequested(int argc, char **argv) { return argc >= 7 && !strcmp(argv[1], "--watch"); }

/* Processus surveillant : POSIX pur (aucun Qt tant qu'il n'y a rien à signaler). */
int runWatcher(int, char **argv)
{
    const int fd = atoi(argv[2]);
    const long pid = atol(argv[3]);
    const char *app = argv[4], *report = argv[5], *log = argv[6];

    for (;;) {
        char c;
        const ssize_t r = read(fd, &c, 1);
        if (r < 0 && errno == EINTR) continue;
        if (r == 1 && c == 'K') return 0;      // arrêt normal
        if (r <= 0) break;                      // tube fermé sans « K » : arrêt anormal
    }
    usleep(300000);                             // laisse le noyau / le gestionnaire finir d'écrire

    char when[64];
    const time_t now = time(nullptr);
    struct tm tmv;
    localtime_r(&now, &tmv);
    strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S", &tmv);

    const int rf = open(report, O_WRONLY | O_CREAT | O_APPEND, 0644);
    off_t size = rf >= 0 ? lseek(rf, 0, SEEK_END) : 0;
    char head[512];
    snprintf(head, sizeof head, "%s=== ARRÊT ANORMAL de %s (pid %ld) détecté le %s ===\n",
             size > 0 ? "\n" : "", app, pid, when);
    wr(rf, head);
    if (size <= 0)
        wr(rf, "Aucune information de plantage n'a été écrite : le processus a été arrêté de force "
               "(kill -9, manque de mémoire, fermeture forcée…) ou a quitté de façon anormale.\n"
               "Consultez les derniers messages du journal ci-dessous.\n");
    if (rf >= 0) close(rf);
    const int lf = open(log, O_WRONLY | O_CREAT | O_APPEND, 0644);
    wr(lf, head);
    if (lf >= 0) close(lf);
    std::fprintf(stderr, "\n[kit] %s s'est arrêté de façon anormale — rapport : %s\n", app, report);

    // 1) Fenêtre Qt de rapport (processus neuf)
    pid_t p = fork();
    if (p == 0) {
        execl("/proc/self/exe", argv[0], "--crash-report", report, (char *)nullptr);
        _exit(127);
    }
    int status = 0;
    if (p > 0 && waitpid(p, &status, 0) == p && WIFEXITED(status) && WEXITSTATUS(status) == 0) return 0;

    // 2) Secours : outils graphiques du système
    char title[160];
    snprintf(title, sizeof title, "--title=Arrêt anormal de %s", app);
    const char *cmds[][8] = {
        {"zenity", "--text-info", title, report, "--width=820", "--height=520", nullptr, nullptr},
        {"kdialog", "--textbox", report, "820", "520", nullptr, nullptr, nullptr},
        {"xmessage", "-file", report, nullptr, nullptr, nullptr, nullptr, nullptr},
    };
    char fileArg[4200];
    snprintf(fileArg, sizeof fileArg, "--filename=%s", report);
    for (auto &c : cmds) {
        pid_t q = fork();
        if (q == 0) {
            if (!strcmp(c[0], "zenity")) {
                execlp("zenity", "zenity", "--text-info", title, fileArg, "--width=820", "--height=520", (char *)nullptr);
            } else {
                execvp(c[0], const_cast<char *const *>(c));
            }
            _exit(127);
        }
        if (q > 0 && waitpid(q, &status, 0) == q && WIFEXITED(status) && WEXITSTATUS(status) != 127) return 0;
    }
    return 1;
}

bool crashReporterRequested(int argc, char **argv, QString *reportPath)
{
    for (int i = 1; i + 1 < argc; ++i)
        if (!strcmp(argv[i], "--crash-report")) { *reportPath = QString::fromLocal8Bit(argv[i + 1]); return true; }
    return false;
}

int runCrashReporter(int &argc, char **argv, const QString &reportPath)
{
    qputenv("QT_STYLE_OVERRIDE", "Fusion");     // évite un éventuel thème système fautif
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/logo.png"));
    QFile f(reportPath);
    QString report;
    if (f.open(QIODevice::ReadOnly)) report = QString::fromUtf8(f.readAll());
    if (report.trimmed().isEmpty()) report = QObject::tr("(rapport vide)");
    const QString app_ = QFileInfo(reportPath).completeBaseName().remove("-crash");
    g_logPath = QFileInfo(reportPath).absolutePath() + "/" + app_ + ".log";
    showError(nullptr, QObject::tr("Plantage de %1").arg(app_),
              QObject::tr("<b>%1 s’est arrêté de façon inattendue.</b><br>Copiez ce rapport pour le signaler.").arg(app_),
              report + "\n--- Derniers messages du journal ---\n" + tailLog(40));
    return 0;
}

}  // namespace Diag

bool DiagApplication::notify(QObject *receiver, QEvent *event)
{
    try {
        return QApplication::notify(receiver, event);
    } catch (const std::exception &e) {
        qCritical().noquote() << "Exception C++ non gérée :" << e.what();
    } catch (...) {
        qCritical().noquote() << "Exception C++ non gérée (type inconnu)";
    }
    return false;
}
