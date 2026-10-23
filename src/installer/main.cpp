#include "installerwindow.h"
#include "uninstallerwindow.h"
#include "languages.h"
#include <QSettings>
#include "diagnostics.h"

#include <QApplication>
#include <QIcon>
#include <QDir>
#include <QFile>

static QString findPayload(const QStringList &args)
{
    const int i = args.indexOf("--payload");
    if (i >= 0 && i + 1 < args.size()) return args.at(i + 1);

    QStringList candidates;
    const QByteArray appdir = qgetenv("APPDIR");
    if (!appdir.isEmpty()) candidates << QString::fromLocal8Bit(appdir) + "/usr/share/kit/payload";
    const QString bin = QCoreApplication::applicationDirPath();
    candidates << bin + "/../share/kit/payload" << bin + "/payload";
    for (const QString &c : candidates)
        if (QFile::exists(c + "/manifest.ini")) return QDir::cleanPath(c);
    return QString();
}

int main(int argc, char *argv[])
{
    if (Diag::watcherRequested(argc, argv)) return Diag::runWatcher(argc, argv);
    QString report;
    if (Diag::crashReporterRequested(argc, argv, &report)) return Diag::runCrashReporter(argc, argv, report);

    Diag::install("kit-installer", argc, argv);
    Diag::phase("création de QApplication");
    DiagApplication app(argc, argv);
    app.setApplicationName("kit-installer");
    Diag::appCreated();
    { QPalette pal = app.palette(); pal.setColor(QPalette::Link, QColor("#3d5bd9")); pal.setColor(QPalette::LinkVisited, QColor("#6c4bd9")); app.setPalette(pal); }
    app.setWindowIcon(QIcon(":/logo.png"));      // icône Kit par défaut ; les fenêtres prennent ensuite celle du programme
    QGuiApplication::setDesktopFileName("kit-installer");

    Diag::phase("chargement de la feuille de style");
    QFile qss(":/theme.qss");
    if (qss.open(QIODevice::ReadOnly)) app.setStyleSheet(QString::fromUtf8(qss.readAll()));

    if (app.arguments().contains("--uninstall")) {
        Diag::phase("mode désinstallation");
        const QStringList a = app.arguments();
        const int i = a.indexOf("--uninstall");
        QString dir = (i + 1 < a.size() && !a.at(i + 1).startsWith("--")) ? a.at(i + 1) : QString();
        if (dir.isEmpty()) {                                  // lancé depuis la copie « uninstall.AppImage » du dossier d'installation
            const QString ai = QString::fromLocal8Bit(qgetenv("APPIMAGE"));
            dir = QFileInfo(ai.isEmpty() ? QCoreApplication::applicationFilePath() : ai).absolutePath();
        }
        QMap<QString, QString> fallback, cur;
        Lang::load(":/lang/en.lang", nullptr, &fallback);
        const QString code = QSettings(dir + "/install.ini", QSettings::IniFormat).value("kit/language", "en").toString();
        Lang::load(dir + "/lang/" + code + ".lang", nullptr, &cur);
        Translator tr;
        tr.set(cur, fallback);
        KitManifest um;
        if (kit_manifest_load(&um, (dir + "/manifest.ini").toUtf8().constData()) != 0) {
            Diag::showError(nullptr, QObject::tr("Uninstall"), tr("un.missing", dir));
            Diag::finish(0);
            return 1;
        }
        app.setQuitOnLastWindowClosed(true);
        UninstallerWindow uw(dir, um, tr);
        uw.show();
        const int rc = app.exec();
        Diag::finish(rc);
        return rc;
    }

    Diag::phase("recherche du payload");
    const QString payload = findPayload(app.arguments());
    qInfo().noquote() << "Payload :" << (payload.isEmpty() ? QString("(introuvable)") : payload);
    KitManifest m;
    if (payload.isEmpty() || kit_manifest_load(&m, (payload + "/manifest.ini").toUtf8().constData()) != 0) {
        Diag::showError(nullptr, QObject::tr("Kit Installer"),
            QObject::tr("Aucun programme à installer.\n\nCe moteur doit être empaqueté par Kit Builder "
                        "(ou lancé avec --payload <dossier>)."),
            QObject::tr("Payload cherché : %1").arg(payload.isEmpty() ? QString("(aucun dossier trouvé)") : payload));
        Diag::finish(0);   // erreur déjà affichée à l'utilisateur
        return 1;
    }

    Diag::phase("construction de InstallerWindow");
    InstallerWindow w(payload, m);
    Diag::phase("affichage de la fenêtre");
    w.show();
    Diag::phase("boucle d'événements");
    const int rc = app.exec();
    Diag::finish(rc);
    return rc;
}
