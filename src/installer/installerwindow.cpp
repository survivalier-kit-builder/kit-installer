#include "installerwindow.h"
#include "chrome.h"
#include "diagnostics.h"
#include "gpgkeywindow.h"
#include "sidepanel.h"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QIcon>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressBar>
#include <QRegularExpression>
#include <QTextBrowser>
#include <QUrl>
#include <QSettings>
#include <QPushButton>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStyle>
#include <QVBoxLayout>

static QString S(const char *s) { return QString::fromUtf8(s); }

/* Texte brut -> HTML échappé dont les adresses http(s):// sont cliquables. */
static QString linkify(const QString &plain)
{
    QString html = plain.toHtmlEscaped();
    static const QRegularExpression url(R"((https?://[^\s<>"']*[^\s<>"'.,;:!?)\]]))");
    html.replace(url, QStringLiteral("<a href=\"\\1\">\\1</a>"));
    return html;
}
/* Site web saisi par le développeur : lien http(s) uniquement. */
static QString webLink(const QString &value)
{
    const QString v = value.trimmed();
    if (v.isEmpty()) return QString();
    const QUrl u(v.contains("://") ? v : "https://" + v);
    if ((u.scheme() != "http" && u.scheme() != "https") || u.host().isEmpty()) return v.toHtmlEscaped();
    return QString("<a href=\"%1\">%2</a>").arg(QString::fromUtf8(u.toEncoded()).toHtmlEscaped(), v.toHtmlEscaped());
}
static QString mailLink(const QString &value)
{
    const QString v = value.trimmed();
    static const QRegularExpression ok(R"(^[^\s@<>"']+@[^\s@<>"']+\.[^\s@<>"']+$)");
    if (!ok.match(v).hasMatch()) return v.toHtmlEscaped();
    return QString("<a href=\"mailto:%1\">%1</a>").arg(v.toHtmlEscaped());
}
static QString licenseHtml(const QString &text)
{
    return "<div style=\"white-space: pre-wrap;\">" + linkify(text) + "</div>";
}
static const char *kStepKeys[] = {"step.language", "step.welcome", "step.license", "step.destination",
                                  "step.options", "step.install", "step.finish"};
static const char *kPagePrefix[] = {"lang", "welcome", "license", "dest", "options", "install", "finish"};

InstallerWindow::InstallerWindow(const QString &payloadDir, const KitManifest &manifest)
    : m_payload(payloadDir), m_m(manifest)
{
    char id[128];
    kit_make_id(m_m.name, id, sizeof id);
    m_id = QString::fromUtf8(id);

    setFixedSize(920, 600);
    loadLanguages();
    buildUi();

    // Langue initiale : celle du système si elle est proposée, sinon la langue par défaut du développeur
    int row = 0;
    const QString sys = QLocale::system().name();                  // ex. fr_FR
    const QString sysShort = sys.section('_', 0, 0);
    int found = -1;
    for (int i = 0; i < m_langs.size(); ++i)
        if (m_langs[i].code.compare(sys, Qt::CaseInsensitive) == 0) { found = i; break; }
    for (int i = 0; found < 0 && i < m_langs.size(); ++i)
        if (m_langs[i].code.compare(sysShort, Qt::CaseInsensitive) == 0) found = i;
    for (int i = 0; found < 0 && i < m_langs.size(); ++i)
        if (m_langs[i].code == S(m_m.default_language)) found = i;
    if (found >= 0) row = found;
    m_langList->setCurrentRow(row);                                // déclenche setLanguage()

    goTo(m_first);
}

/* ---------- Langues ---------- */

void InstallerWindow::loadLanguages()
{
    Lang::load(":/lang/en.lang", nullptr, &m_fallback);            // anglais intégré = filet de sécurité

    // Les langues appartiennent à l'installeur : toutes celles intégrées, plus celles ajoutées dans le payload
    QMap<QString, LangInfo> found;
    auto scan = [&](const QString &dir) {
        for (const QFileInfo &fi : QDir(dir).entryInfoList({"*.lang"}, QDir::Files)) {
            LangInfo li;
            if (!fi.fileName().startsWith('_') && Lang::load(fi.absoluteFilePath(), &li, nullptr)) found.insert(li.code, li);
        }
    };
    scan(":/lang");
    scan(m_payload + "/lang");

    m_langs = found.values();
    std::sort(m_langs.begin(), m_langs.end(), [](const LangInfo &a, const LangInfo &b) {
        if ((a.code == "en") != (b.code == "en")) return a.code == "en";       // English en premier
        return a.name.localeAwareCompare(b.name) < 0;
    });
    if (m_langs.isEmpty()) {
        LangInfo en;
        Lang::load(":/lang/en.lang", &en, nullptr);
        m_langs << en;
    }
    m_first = m_langs.size() > 1 ? Language : Welcome;
}

void InstallerWindow::setLanguage(const LangInfo &lang)
{
    m_lang = lang;
    QMap<QString, QString> cur;
    Lang::load(lang.path, nullptr, &cur);
    T.set(cur, m_fallback);
    retranslate();
}

void InstallerWindow::onLanguageRow(int row)
{
    if (row >= 0 && row < m_langs.size()) setLanguage(m_langs[row]);
}

void InstallerWindow::retranslate()
{
    const QString name = S(m_m.name);
    setWindowTitle(T("window.title", name));
    m_side->setTitle(T("side.title"));

    for (int i = 0; i < PageCount; ++i) {
        const QByteArray tk = QByteArray(kPagePrefix[i]) + ".title", sk = QByteArray(kPagePrefix[i]) + ".subtitle";
        m_titles[i]->setText(T(tk.constData(), name));
        m_subs[i]->setText(T(sk.constData(), name));
    }

    // Bienvenue (les adresses web et e-mail sont cliquables)
    QString html = "<p style='line-height:140%'>" + linkify(S(m_m.description)).replace("\n", "<br>") + "</p><br><table>";
    auto row = [&](const char *key, const QString &valueHtml) {
        if (!valueHtml.isEmpty())
            html += QString("<tr><td style='color:#6b7794;padding-right:16px;padding-bottom:6px'>%1</td>"
                            "<td><b>%2</b></td></tr>").arg(T(key), valueHtml);
    };
    row("info.version", S(m_m.version).toHtmlEscaped());
    row("info.developer", S(m_m.developer).toHtmlEscaped());
    row("info.license", S(m_m.license).toHtmlEscaped());
    row("info.website", webLink(S(m_m.website)));
    row("info.contact", mailLink(S(m_m.email)));
    QStringList progNames;                                         // langues du programme (saisies par le développeur)
    for (const QString &code : S(m_m.program_languages).split(',', Qt::SkipEmptyParts)) {
        QString n;
        for (const LangInfo &li : m_langs)                       // nom exact si l'installeur a cette langue (English, Français...)
            if (li.code.compare(code, Qt::CaseInsensitive) == 0) n = li.name;
        if (n.isEmpty()) {
            const QLocale loc(code);
            n = loc.language() == QLocale::C ? code.toUpper() : loc.nativeLanguageName();
        }
        if (!n.isEmpty()) n[0] = n[0].toUpper();
        progNames << n;
    }
    row("info.languages", progNames.join(", ").toHtmlEscaped());
    m_welcomeBody->setText(html + "</table>");
    m_keyBtn->setText(T("welcome.view_key"));

    if (!m_hasLicenseFile) m_licenseText->setHtml(licenseHtml(T("license.default_text", S(m_m.license))));
    m_accept->setText(T("license.accept"));
    m_destHint->setText(T("dest.hint"));
    m_browseBtn->setText(T("btn.browse"));
    m_menuCheck->setText(T("options.menu"));
    m_deskCheck->setText(T("options.desktop"));
    m_pathCheck->setText(T("options.path", m_id));
    m_launchCheck->setText(T("finish.launch", name));
    m_cancelBtn->setText(T("btn.cancel"));
    m_backBtn->setText(T("btn.back"));
    m_titleBar->setInfoToolTip(T("about.tooltip"));
    updateUi();
}

/* ---------- Interface ---------- */

void InstallerWindow::buildUi()
{
    auto *central = new QWidget;
    auto *root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_side = new SidePanel;
    m_side->setProgramName(S(m_m.name));
    m_side->setVersion(m_m.version[0] ? "v" + S(m_m.version) : QString());
    m_side->setFooter(m_m.copyright[0] ? S(m_m.copyright)
                                       : QString("© %1 | %2").arg(QDate::currentDate().year())
                                             .arg(m_m.developer[0] ? S(m_m.developer) : S(m_m.name)));
    m_side->setLogo(QPixmap(m_payload + "/icon.png"));
    root->addWidget(m_side);

    auto *right = new QWidget;
    right->setObjectName("rightPane");
    auto *rv = new QVBoxLayout(right);
    rv->setContentsMargins(32, 26, 32, 22);
    rv->setSpacing(16);

    auto *stepper = new QHBoxLayout;
    stepper->setSpacing(6);
    for (int i = m_first; i < PageCount; ++i) {
        auto *l = new QLabel;
        m_stepLabels << l;
        stepper->addWidget(l);
    }
    stepper->addStretch();
    rv->addLayout(stepper);

    m_pages = new QStackedWidget;
    m_pages->addWidget(makeLanguage());
    m_pages->addWidget(makeWelcome());
    m_pages->addWidget(makeLicense());
    m_pages->addWidget(makeDestination());
    m_pages->addWidget(makeOptions());
    m_pages->addWidget(makeInstall());
    m_pages->addWidget(makeFinish());
    rv->addWidget(m_pages, 1);

    auto *buttons = new QHBoxLayout;
    m_cancelBtn = new QPushButton;
    m_backBtn = new QPushButton;
    m_nextBtn = new QPushButton;
    m_nextBtn->setObjectName("primary");
    m_nextBtn->setDefault(true);
    buttons->addWidget(m_cancelBtn);
    buttons->addStretch();
    buttons->addWidget(m_backBtn);
    buttons->addWidget(m_nextBtn);
    rv->addLayout(buttons);

    // Barre de titre personnalisée au-dessus des étapes ; le panneau 300x600 garde toute la hauteur
    setWindowFlag(Qt::FramelessWindowHint, true);
    auto *rightWrap = new QWidget;
    auto *rw = new QVBoxLayout(rightWrap);
    rw->setContentsMargins(0, 0, 0, 0);
    rw->setSpacing(0);
    m_titleBar = new TitleBar(this, TitleBar::Info | TitleBar::Minimize | TitleBar::Close);
    rw->addWidget(m_titleBar);
    rw->addWidget(right, 1);
    Chrome::enableDrag(m_side, this);
    connect(m_titleBar, &TitleBar::infoClicked, this, [this] { Chrome::showAbout(this, &T); });
    root->addWidget(rightWrap, 1);
    setCentralWidget(central);

    connect(m_nextBtn, &QPushButton::clicked, this, &InstallerWindow::onNext);
    connect(m_backBtn, &QPushButton::clicked, this, &InstallerWindow::onBack);
    connect(m_cancelBtn, &QPushButton::clicked, this, &InstallerWindow::onCancel);
}

QWidget *InstallerWindow::makePage(int page, QWidget *body)
{
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(6);
    m_titles[page] = new QLabel;
    m_titles[page]->setObjectName("pageTitle");
    m_titles[page]->setWordWrap(true);
    m_subs[page] = new QLabel;
    m_subs[page]->setObjectName("pageSub");
    m_subs[page]->setWordWrap(true);
    v->addWidget(m_titles[page]);
    v->addWidget(m_subs[page]);
    v->addSpacing(10);
    if (body) v->addWidget(body, 1);
    else v->addStretch(1);
    return w;
}

QWidget *InstallerWindow::makeLanguage()
{
    m_langList = new QListWidget;
    for (const LangInfo &li : m_langs) m_langList->addItem(li.name);
    connect(m_langList, &QListWidget::currentRowChanged, this, &InstallerWindow::onLanguageRow);
    return makePage(Language, m_langList);
}

QWidget *InstallerWindow::makeWelcome()
{
    m_welcomeBody = new QLabel;
    m_welcomeBody->setWordWrap(true);
    m_welcomeBody->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_welcomeBody->setTextFormat(Qt::RichText);
    m_welcomeBody->setOpenExternalLinks(true);                      // liens cliquables
    m_welcomeBody->setTextInteractionFlags(Qt::TextBrowserInteraction);
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(12);
    v->addWidget(m_welcomeBody);
    m_keyBtn = new QPushButton;                                     // visible seulement si une clé GPG est embarquée
    m_keyBtn->setVisible(GpgKeyWindow::available(m_payload));
    connect(m_keyBtn, &QPushButton::clicked, this, [this] { GpgKeyWindow::show(this, m_payload, T); });
    v->addWidget(m_keyBtn, 0, Qt::AlignLeft);
    v->addStretch(1);
    return makePage(Welcome, w);
}

QWidget *InstallerWindow::makeLicense()
{
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    m_licenseText = new QTextBrowser;
    m_licenseText->setOpenExternalLinks(true);                      // URLs du texte de licence cliquables
    QFile f(m_payload + "/license.txt");
    if (f.open(QIODevice::ReadOnly)) {
        m_licenseText->setHtml(licenseHtml(QString::fromUtf8(f.readAll())));   // texte légal : fourni par le développeur
        m_hasLicenseFile = true;
    }
    m_accept = new QCheckBox;
    v->addWidget(m_licenseText, 1);
    v->addWidget(m_accept);
    connect(m_accept, &QCheckBox::toggled, this, &InstallerWindow::updateUi);
    return makePage(License, w);
}

QWidget *InstallerWindow::makeDestination()
{
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(10);
    auto *row = new QHBoxLayout;
    m_destEdit = new QLineEdit(QDir::homePath() + "/.local/opt/" + m_id);
    m_browseBtn = new QPushButton;
    row->addWidget(m_destEdit, 1);
    row->addWidget(m_browseBtn);
    m_destHint = new QLabel;
    m_destHint->setObjectName("hint");
    m_destHint->setWordWrap(true);
    v->addLayout(row);
    v->addWidget(m_destHint);
    v->addStretch(1);
    connect(m_browseBtn, &QPushButton::clicked, this, &InstallerWindow::onBrowse);
    return makePage(Destination, w);
}

QWidget *InstallerWindow::makeOptions()
{
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(14);
    m_menuCheck = new QCheckBox;
    m_menuCheck->setChecked(true);
    m_deskCheck = new QCheckBox;
    m_pathCheck = new QCheckBox;
    v->addWidget(m_menuCheck);
    v->addWidget(m_deskCheck);
    v->addWidget(m_pathCheck);
    v->addStretch(1);
    return makePage(Options, w);
}

QWidget *InstallerWindow::makeInstall()
{
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(12);
    m_bar = new QProgressBar;
    m_bar->setRange(0, 100);
    m_bar->setTextVisible(false);
    m_status = new QLabel;
    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    v->addWidget(m_bar);
    v->addWidget(m_status);
    v->addWidget(m_log, 1);
    return makePage(Install, w);
}

QWidget *InstallerWindow::makeFinish()
{
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(14);
    m_finishLabel = new QLabel;
    m_finishLabel->setWordWrap(true);
    m_finishLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_finishLabel->setOpenExternalLinks(true);
    m_launchCheck = new QCheckBox;
    m_launchCheck->setChecked(true);
    v->addWidget(m_finishLabel);
    v->addWidget(m_launchCheck);
    v->addStretch(1);
    return makePage(Finish, w);
}

/* ---------- Navigation ---------- */

void InstallerWindow::goTo(int page)
{
    m_cur = page;
    m_pages->setCurrentIndex(page);
    updateUi();
}

void InstallerWindow::updateUi()
{
    if (!m_nextBtn) return;
    for (int i = m_first; i < PageCount; ++i) {
        QLabel *l = m_stepLabels[i - m_first];
        const int n = i - m_first + 1;
        l->setText(i == m_cur ? QString("%1  %2").arg(n).arg(T(kStepKeys[i])) : QString::number(n));
        l->setProperty("step", i == m_cur ? "active" : (i < m_cur ? "done" : "todo"));
        l->style()->unpolish(l);
        l->style()->polish(l);
    }
    m_backBtn->setEnabled(m_cur > m_first && m_cur != Install && m_cur != Finish);
    m_backBtn->setVisible(m_cur != Finish);
    m_cancelBtn->setVisible(m_cur != Finish);
    m_nextBtn->setEnabled(m_cur != Install && (m_cur != License || m_accept->isChecked()));
    m_nextBtn->setText(T(m_cur == Options ? "btn.install" : m_cur == Finish ? "btn.finish" : "btn.next"));
}

void InstallerWindow::onBack() { if (m_cur > m_first) goTo(m_cur - 1); }

void InstallerWindow::onNext()
{
    if (m_cur == Destination) {
        const QString d = m_destEdit->text().trimmed();
        if (d.isEmpty() || !d.startsWith('/')) {
            QMessageBox::warning(this, T("dest.invalid_title"), T("dest.invalid_text"));
            return;
        }
    }
    if (m_cur == Options) { goTo(Install); runInstall(); return; }
    if (m_cur == Finish) {
        if (m_launchCheck->isChecked()) QProcess::startDetached(m_exePath, QStringList());
        close();
        return;
    }
    goTo(m_cur + 1);
}

void InstallerWindow::onCancel() { close(); }          // la confirmation est demandée dans closeEvent()

/* Toute fermeture (bouton Annuler, croix de la barre de titre, Alt+F4, gestionnaire de fenêtres) demande confirmation,
   sauf sur la page finale. Pendant l'installation, « Oui » annule l'installation en cours. */
void InstallerWindow::closeEvent(QCloseEvent *e)
{
    if (m_cur == Finish) { e->accept(); return; }
    if (m_confirming) { e->ignore(); return; }
    m_confirming = true;
    const bool yes = Chrome::confirm(this, T("quit.title"), T("quit.confirm"), T("quit.yes"), T("quit.no"));
    m_confirming = false;
    if (!yes) { e->ignore(); return; }
    if (m_busy) { m_cancelled = true; e->ignore(); return; }   // l'installation s'interrompt et revient aux options
    e->accept();
}

void InstallerWindow::onBrowse()
{
    const QString d = QFileDialog::getExistingDirectory(this, T("dest.choose"), QDir::homePath());
    if (!d.isEmpty()) m_destEdit->setText(QDir::cleanPath(d + "/" + m_id));
}

/* ---------- Installation ---------- */

void InstallerWindow::log(const QString &line)
{
    m_log->appendPlainText(line);
    qInfo().noquote() << "[install]" << line;
}

void InstallerWindow::step(const QString &text, int percent)
{
    m_status->setText(text);
    m_bar->setValue(percent);
    log(text);
    QApplication::processEvents();
}

int InstallerWindow::onProgress(double f, void *user)
{
    auto *w = static_cast<InstallerWindow *>(user);
    w->m_bar->setValue(int(w->m_progBase + f * w->m_progWeight));
    QApplication::processEvents();
    return w->m_cancelled ? 1 : 0;
}

void InstallerWindow::runInstall()
{
    m_busy = true;
    m_cancelled = false;
    m_pathLinked = false;
    m_log->clear();
    m_bar->setValue(0);
    m_cancelBtn->setEnabled(true);
    updateUi();

    QString err;
    const bool ok = doInstall(&err);
    m_busy = false;
    m_cancelBtn->setEnabled(true);

    if (ok) {
        m_finishLabel->setText(T("finish.text", m_destEdit->text().toHtmlEscaped()) +
                               (m_pathLinked ? T("finish.path", m_id.toHtmlEscaped()) : QString()));
        goTo(Finish);
    } else if (m_cancelled) {
        goTo(Options);
    } else {
        Diag::showError(this, T("install.failed_title"), err);
        goTo(Destination);
    }
}

bool InstallerWindow::doInstall(QString *err)
{
    QStringList created;
    auto fail = [&](const QString &msg) {
        *err = msg;
        log("[ERROR] " + msg);
        for (const QString &f : created) QFile::remove(f);
        return false;
    };
    auto path8 = [](const QString &s) { return s.toUtf8(); };

    const QString dest = QDir::cleanPath(m_destEdit->text().trimmed());
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QString desktopDir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    m_exePath = dest + "/" + m_id;
    const QString iconDst = dest + "/" + m_id + ".png";
    const QString licDst = dest + "/license.txt";
    const QString manDst = dest + "/manifest.ini";
    const QString uninst = dest + "/uninstall.sh";
    const QString unBin = dest + "/uninstall.AppImage";          // copie de l'installeur : sert d'assistant de désinstallation
    const QString unIni = dest + "/install.ini";
    const QString unList = dest + "/uninstall.list";
    const QString langDst = dest + "/lang/" + m_lang.code + ".lang";
    const QString unEntry = dataDir + "/applications/" + m_id + "-uninstall.desktop";
    const QString themeIcon = dataDir + "/icons/hicolor/256x256/apps/" + m_id + ".png";
    const QString menuFile = dataDir + "/applications/" + m_id + ".desktop";
    const QString shortcut = desktopDir + "/" + m_id + ".desktop";
    const QByteArray uninstLabel = T("desktop.uninstall").toUtf8();
    const QByteArray unCmd = ("\"" + unBin + "\" --uninstall").toUtf8();

    step(T("install.creating_dir"), 2);
    if (kit_mkdir_p(path8(dest).constData(), 0755) != 0) return fail(T("install.err_mkdir", dest));

    step(T("install.copying"), 3);
    m_progBase = 3; m_progWeight = 57;
    int r = kit_copy_file(path8(m_payload + "/app.bin").constData(), path8(m_exePath).constData(),
                          0755, &InstallerWindow::onProgress, this);
    if (r == KIT_ABORTED) {
        *err = T("install.cancelled");
        log(*err);
        QFile::remove(m_exePath);
        return false;
    }
    if (r != 0) return fail(T("install.err_copy", m_exePath));
    created << m_exePath;

    step(T("install.copying_uninstaller"), 61);
    QString self = QString::fromLocal8Bit(qgetenv("APPIMAGE"));      // l'AppImage elle-même (sinon l'exécutable courant)
    if (self.isEmpty()) self = QCoreApplication::applicationFilePath();
    m_progBase = 61; m_progWeight = 19;
    r = kit_copy_file(path8(self).constData(), path8(unBin).constData(), 0755, &InstallerWindow::onProgress, this);
    if (r == KIT_ABORTED) {
        *err = T("install.cancelled");
        log(*err);
        QFile::remove(m_exePath); QFile::remove(unBin);
        return false;
    }
    if (r != 0) return fail(T("install.err_uninstaller_copy"));
    created << unBin;

    step(T("install.copying_icon"), 82);
    m_progBase = 82; m_progWeight = 0;
    QFile::remove(iconDst); QFile::remove(licDst); QFile::remove(manDst);
    if (QFile::copy(m_payload + "/icon.png", iconDst)) created << iconDst;
    if (QFile::exists(m_payload + "/license.txt") && QFile::copy(m_payload + "/license.txt", licDst)) created << licDst;
    if (QFile::copy(m_payload + "/manifest.ini", manDst)) created << manDst;

    QStringList toRemove = {m_exePath, iconDst, licDst, manDst};
    QDir().mkpath(dest + "/lang");
    QFile::remove(langDst);
    if (QFile::copy(m_lang.path, langDst)) { created << langDst; toRemove << langDst; }

    if (m_menuCheck->isChecked()) {
        step(T("install.menu"), 88);
        kit_mkdir_p(path8(dataDir + "/applications").constData(), 0755);
        kit_mkdir_p(path8(dataDir + "/icons/hicolor/256x256/apps").constData(), 0755);
        QFile::remove(themeIcon);
        if (QFile::copy(m_payload + "/icon.png", themeIcon)) { created << themeIcon; toRemove << themeIcon; }
        if (kit_write_desktop_entry(&m_m, path8(m_exePath).constData(), path8(iconDst).constData(),
                                    unCmd.constData(), uninstLabel.constData(), path8(menuFile).constData()) != 0)
            return fail(T("install.err_desktop", menuFile));
        created << menuFile; toRemove << menuFile;
        const QByteArray unLabel = T("un.entry", S(m_m.name)).toUtf8();
        if (kit_write_uninstall_entry(unLabel.constData(), unCmd.constData(), path8(iconDst).constData(), path8(unEntry).constData()) == 0) {
            created << unEntry; toRemove << unEntry;               // entrée « Désinstaller <programme> » dans le menu
        }
        QProcess::startDetached("update-desktop-database", QStringList{dataDir + "/applications"});
    }

    if (m_deskCheck->isChecked()) {
        step(T("install.shortcut"), 94);
        if (QDir().mkpath(desktopDir) &&
            kit_write_desktop_entry(&m_m, path8(m_exePath).constData(), path8(iconDst).constData(),
                                    unCmd.constData(), uninstLabel.constData(), path8(shortcut).constData()) == 0) {
            QFile(shortcut).setPermissions(QFile::permissions(shortcut) | QFile::ExeOwner);
            QProcess::startDetached("gio", QStringList{"set", shortcut, "metadata::trusted", "true"});
            created << shortcut; toRemove << shortcut;
        } else {
            log(T("install.shortcut_skipped"));
        }
    }

    if (m_pathCheck->isChecked()) {
        step(T("install.path"), 96);
        const QString binDir = QDir::homePath() + "/.local/bin";
        const QString linkPath = binDir + "/" + m_id;
        kit_mkdir_p(path8(binDir).constData(), 0755);
        const int lr = kit_link_command(path8(m_exePath).constData(), path8(linkPath).constData());
        if (lr == 0) {
            created << linkPath; toRemove << linkPath;
            m_pathLinked = true;
            if (!kit_path_contains(path8(binDir).constData())) {       // ~/.local/bin pas encore dans le PATH
                const QString home = QDir::homePath();
                for (const QString &name : {QString(".profile"), QString(".bashrc"), QString(".zshrc")}) {
                    const QString file = home + "/" + name;
                    if (name != ".profile" && !QFile::exists(file)) continue;
                    if (kit_ensure_path_profile(path8(file).constData(), "$HOME/.local/bin") == 1)
                        log(T("install.path_profile", file));
                }
            }
        } else if (lr == 1) {
            log(T("install.path_skipped", linkPath));
        } else {
            return fail(T("install.err_path", linkPath));
        }
    }

    step(T("install.uninstaller"), 97);
    toRemove << uninst;
    // Données de l'assistant de désinstallation (supprimées en dernier : liste, config, puis l'AppImage elle-même)
    {
        QSettings ini(unIni, QSettings::IniFormat);
        ini.setValue("kit/language", m_lang.code);
        ini.setValue("kit/id", m_id);
        ini.setValue("kit/dir", dest);
        ini.sync();
        created << unIni;
        QStringList all = toRemove;
        all << unIni << unList << unBin;
        QFile lf(unList);
        if (!lf.open(QIODevice::WriteOnly) || lf.write(all.join('\n').toUtf8() + "\n") < 0)
            return fail(T("install.err_uninstaller"));
        created << unList;
        toRemove << unIni << unList;                                  // uninstall.sh les supprime aussi
    }
    toRemove << unBin;
    std::vector<QByteArray> store;
    for (const QString &f : toRemove) store.push_back(path8(f));
    std::vector<const char *> ptrs;
    for (const QByteArray &b : store) ptrs.push_back(b.constData());
    ptrs.push_back(nullptr);
    const QByteArray doneMsg = T("uninstall.done").toUtf8();
    if (kit_write_uninstaller(path8(uninst).constData(), path8(dest).constData(), ptrs.data(), doneMsg.constData()) != 0)
        return fail(T("install.err_uninstaller"));

    step(T("install.done"), 100);
    return true;
}
