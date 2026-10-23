#include "builderwindow.h"
#include "diagnostics.h"
#include "packager.h"
#include "project.h"

#include <QApplication>
#include <QIcon>
#include <QFile>
#include <QFileInfo>
#include <cstdio>

static int runCli(const QStringList &a)
{
    auto has = [&](const char *name) { return a.contains(QString("--") + name); };
    auto opt = [&](const char *name) {
        const int i = a.indexOf(QString("--") + name);
        return (i >= 0 && i + 1 < a.size()) ? a.at(i + 1) : QString();
    };
    if (has("help") || a.contains("-h")) {
        std::puts("Usage : kit-builder --cli [PROJET.kit-gen] [options]\n"
                  "  --project FICHIER.kit-gen   charge toutes les informations depuis un projet (les options ci-dessous le surchargent)\n"
                  "  --exe FICHIER --icon IMAGE --name NOM [--version V] [--description TEXTE] [--developer NOM]\n"
                  "  --email MAIL --website URL --license NOM --license-file FICHIER --category Utility|Game|Development|...\n"
                  "  --program-languages en,fr,de   (langues prises en charge par votre programme)\n"
                  "  --sign-key EMPREINTE|ID|EMAIL   signe l'installeur avec cette clé GPG (crée <fichier>.AppImage.asc ; la clé publique est embarquée)\n"
                  "  --gpg-passphrase-file FICHIER   (signature sans pinentry, pour CI)\n"
                  "  --out DOSSIER   (défaut : dossier du projet, sinon dossier courant)");
        return 0;
    }

    PackRequest r;
    QString projPath = opt("project");
    if (projPath.isEmpty())
        for (const QString &arg : a.mid(1))
            if (arg.endsWith(".kit-gen", Qt::CaseInsensitive)) { projPath = arg; break; }
    if (!projPath.isEmpty()) {
        Project p;
        QString err;
        if (!ProjectFile::load(projPath, &p, &err)) { std::fprintf(stderr, "Erreur : %s\n", err.toLocal8Bit().constData()); return 1; }
        r = p.req;
        r.gpgKey = p.sign ? p.gpgKey : QString();
        if (r.outDir.isEmpty()) r.outDir = QFileInfo(projPath).absolutePath();
        std::printf("Projet : %s\n", projPath.toLocal8Bit().constData());
    }
    auto setIf = [&](const char *name, QString &dst) { if (has(name)) dst = opt(name); };
    setIf("exe", r.exe); setIf("icon", r.icon); setIf("name", r.name); setIf("version", r.version);
    setIf("description", r.description); setIf("developer", r.developer); setIf("email", r.email);
    setIf("website", r.website); setIf("license", r.license); setIf("license-file", r.licenseFile);
    setIf("category", r.category); setIf("out", r.outDir); setIf("gpg-passphrase-file", r.gpgPassphraseFile);
    if (has("sign-key")) r.gpgKey = opt("sign-key");
    if (has("program-languages")) r.programLanguages = opt("program-languages").split(',', Qt::SkipEmptyParts);
    if (r.outDir.isEmpty()) r.outDir = ".";

    Packager pk;
    QObject::connect(&pk, &Packager::message, [](const QString &m) { std::puts(m.toLocal8Bit().constData()); });
    QString file, err;
    if (!pk.run(r, &file, &err)) { std::fprintf(stderr, "Erreur : %s\n", err.toLocal8Bit().constData()); return 1; }
    return 0;
}

int main(int argc, char *argv[])
{
    if (Diag::watcherRequested(argc, argv)) return Diag::runWatcher(argc, argv);
    QString report;
    if (Diag::crashReporterRequested(argc, argv, &report)) return Diag::runCrashReporter(argc, argv, report);

    Diag::install("kit-builder", argc, argv);
    bool cli = false;
    for (int i = 1; i < argc; ++i) if (QByteArray(argv[i]) == "--cli") cli = true;
    if (cli && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) qputenv("QT_QPA_PLATFORM", "offscreen");

    Diag::phase("création de QApplication");
    DiagApplication app(argc, argv);
    app.setApplicationName("kit-builder");
    Diag::appCreated();
    { QPalette pal = app.palette(); pal.setColor(QPalette::Link, QColor("#3d5bd9")); pal.setColor(QPalette::LinkVisited, QColor("#6c4bd9")); app.setPalette(pal); }
    app.setWindowIcon(QIcon(":/logo.png"));
    QGuiApplication::setDesktopFileName("kit-builder");   // icône du lanceur sous Wayland
    if (cli) { const int rc = runCli(app.arguments()); Diag::finish(rc); return rc; }

    Diag::phase("chargement de la feuille de style");
    QFile qss(":/theme.qss");
    if (qss.open(QIODevice::ReadOnly)) app.setStyleSheet(QString::fromUtf8(qss.readAll()));
    else qWarning("Feuille de style :/theme.qss introuvable");

    Diag::phase("construction de BuilderWindow");
    BuilderWindow w;
    Diag::phase("affichage de la fenêtre");
    w.show();
    for (const QString &arg : app.arguments().mid(1))                 // « kit-builder projet.kit-gen » ou double-clic sur un .kit-gen
        if (!arg.startsWith("--") && arg.endsWith(".kit-gen", Qt::CaseInsensitive)) { w.openProject(arg); break; }
    Diag::phase("boucle d'événements");
    const int rc = app.exec();
    Diag::finish(rc);
    return rc;
}
