#include "gpgsign.h"
#include "languages.h"
#include "packager.h"

#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QLibraryInfo>
#include <QPainter>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QSysInfo>
#include <QTemporaryDir>

extern "C" {
#include "kit_appimage.h"
}

static const QFileDevice::Permissions kExec =
    QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner | QFileDevice::ReadGroup |
    QFileDevice::ExeGroup | QFileDevice::ReadOther | QFileDevice::ExeOther;

static void setField(char *dst, size_t size, QString s)
{
    s = s.trimmed();
    while (s.toUtf8().size() >= int(size)) s.chop(1);
    const QByteArray b = s.toUtf8();
    memcpy(dst, b.constData(), size_t(b.size()) + 1);
}

/* Bibliothèques fournies par le système hôte : on ne les embarque pas. */
static bool isExcluded(const QString &name)
{
    static const char *prefixes[] = {"linux-vdso", "ld-linux", "libc.so", "libm.so", "libdl.so", "libpthread.so",
                                     "librt.so", "libutil.so", "libresolv.so", "libnsl.so", "libGL.so", "libGLX.so",
                                     "libGLdispatch.so", "libOpenGL.so", "libEGL.so", "libdrm.so", "libgbm.so",
                                     "libfontconfig.so", "libnss_", "libanl.so"};
    for (const char *p : prefixes)
        if (name.startsWith(QLatin1String(p))) return true;
    return false;
}

static QString pluginsPath()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QLibraryInfo::path(QLibraryInfo::PluginsPath);
#else
    return QLibraryInfo::location(QLibraryInfo::PluginsPath);
#endif
}

int Packager::onSquashProgress(double f, void *user)
{
    auto *self = static_cast<Packager *>(user);
    emit self->progress(35 + int(f * 64));
    return self->m_cancel ? 1 : 0;
}

bool Packager::extractRuntime(const QString &dst, QString *error)
{
    QFile res(":/runtime");
    QByteArray data;
    if (res.open(QIODevice::ReadOnly)) data = res.readAll();
    if (data.isEmpty()) {
        const QString local = QCoreApplication::applicationDirPath() + "/runtime";
        QFile f(local);
        if (f.open(QIODevice::ReadOnly)) data = f.readAll();
    }
    if (data.isEmpty()) { *error = tr("Runtime AppImage introuvable (reconstruisez kit-builder avec accès à GitHub)."); return false; }
    QFile out(dst);
    if (!out.open(QIODevice::WriteOnly) || out.write(data) != data.size()) { *error = tr("Impossible d’écrire le runtime temporaire."); return false; }
    return true;
}

/* Embarque les bibliothèques Qt et les plugins nécessaires (équivalent de linuxdeploy-plugin-qt). */
bool Packager::bundleQt(const QString &appdir, QString *error)
{
    const QString libDir = appdir + "/usr/lib", plugDir = appdir + "/usr/plugins";
    QDir().mkpath(libDir);
    QDir().mkpath(plugDir);

    QStringList targets = {appdir + "/usr/bin/kit-installer"};
    const QString src = pluginsPath();
    for (const char *sub : {"platforms", "imageformats", "platforminputcontexts", "xcbglintegrations",
                            "wayland-decoration-client", "wayland-graphics-integration-client",
                            "wayland-shell-integration", "iconengines"}) {
        const QDir d(src + "/" + sub);
        if (!d.exists()) continue;
        QDir().mkpath(plugDir + "/" + sub);
        for (const QFileInfo &fi : d.entryInfoList({"*.so"}, QDir::Files)) {
            const QString dst = plugDir + "/" + sub + "/" + fi.fileName();
            if (QFile::copy(fi.absoluteFilePath(), dst)) { QFile::setPermissions(dst, kExec); targets << dst; }
        }
    }
    if (!QFileInfo::exists(plugDir + "/platforms") || QDir(plugDir + "/platforms").isEmpty())
        emit message(tr("[Attention] Aucun plugin de plateforme Qt trouvé dans %1 : l’installeur dépendra du Qt de la machine cible.").arg(src));

    const QRegularExpression re(R"(^\s*(\S+)\s+=>\s+(\S+)\s+\()");
    QSet<QString> done;
    int idx = 0;
    for (const QString &target : targets) {
        emit progress(10 + 25 * idx++ / qMax(1, int(targets.size())));
        QProcess p;
        p.start("ldd", {target});
        if (!p.waitForStarted(5000)) { *error = tr("La commande « ldd » est introuvable."); return false; }
        p.waitForFinished(30000);
        for (const QString &line : QString::fromLocal8Bit(p.readAllStandardOutput()).split('\n')) {
            if (line.contains("not found")) { emit message(tr("[Attention] Dépendance introuvable : %1").arg(line.trimmed())); continue; }
            const auto m = re.match(line);
            if (!m.hasMatch()) continue;
            const QString name = m.captured(1);
            if (isExcluded(name) || done.contains(name)) continue;
            done.insert(name);
            const QString dst = libDir + "/" + name;
            if (QFile::copy(m.captured(2), dst)) QFile::setPermissions(dst, kExec);
        }
        if (m_cancel) { *error = tr("Opération annulée."); return false; }
    }
    emit message(tr("%1 bibliothèques embarquées.").arg(done.size()));
    return true;
}

bool Packager::run(const PackRequest &req, QString *outFile, QString *error)
{
    m_cancel = false;
    emit progress(0);

    // --- Validation
    QImage icon(req.icon);
    QString runtime = QCoreApplication::applicationDirPath() + "/kit-installer";
    if (!QFileInfo::exists(runtime)) runtime = QCoreApplication::applicationDirPath() + "/../bin/kit-installer";
    if (!QFileInfo(req.exe).isFile())            { *error = tr("Sélectionnez l’exécutable du programme à installer."); return false; }
    if (icon.isNull())                           { *error = tr("Sélectionnez une icône valide."); return false; }
    if (req.name.trimmed().isEmpty())            { *error = tr("Le nom du programme est obligatoire."); return false; }
    if (!QFileInfo(req.outDir).isDir() && !QDir().mkpath(req.outDir)) { *error = tr("Impossible de créer le dossier de sortie : %1").arg(req.outDir); return false; }   // créé s'il n'existe pas (projets .kit-gen)
    // Langues du programme : codes saisis par le développeur (en, fr, de, pt_BR...)
    QStringList progLangs;
    static const QRegularExpression codeRe("^([A-Za-z]{2,3})(?:[_-]([A-Za-z0-9]{2,8}))?$");
    for (const QString &raw : req.programLanguages) {
        const QString code = raw.trimmed();
        if (code.isEmpty()) continue;
        const auto m = codeRe.match(code);
        if (!m.hasMatch()) {
            *error = tr("Code de langue invalide : « %1 » (attendu : en, fr, de, pt_BR...).").arg(code);
            return false;
        }
        const QString norm = m.captured(1).toLower() + (m.captured(2).isEmpty() ? QString() : "_" + m.captured(2));
        if (!progLangs.contains(norm)) progLangs << norm;
    }
    if (!QFileInfo::exists(runtime))             { *error = tr("Le moteur « kit-installer » est introuvable (il doit être à côté de kit-builder)."); return false; }

    // --- Manifeste
    KitManifest m;
    memset(&m, 0, sizeof m);
    const QString copyright = QString("© %1 | %2").arg(QDate::currentDate().year())
                                  .arg(req.developer.isEmpty() ? req.name : req.developer);
    setField(m.name, sizeof m.name, req.name);
    setField(m.version, sizeof m.version, req.version);
    setField(m.description, sizeof m.description, req.description);
    setField(m.developer, sizeof m.developer, req.developer);
    setField(m.website, sizeof m.website, req.website);
    setField(m.email, sizeof m.email, req.email);
    setField(m.license, sizeof m.license, req.license);
    setField(m.category, sizeof m.category, req.category);
    setField(m.copyright, sizeof m.copyright, copyright);
    setField(m.program_languages, sizeof m.program_languages, progLangs.join(','));
    setField(m.default_language, sizeof m.default_language, "en");
    char idc[128];
    kit_make_id(m.name, idc, sizeof idc);
    const QString id = QString::fromUtf8(idc);
    setField(m.executable, sizeof m.executable, id);

    // --- AppDir temporaire
    QTemporaryDir tmp;
    if (!tmp.isValid()) { *error = tr("Impossible de créer un dossier temporaire."); return false; }
    const QString appdir = tmp.path() + "/" + id + ".AppDir";
    const QString payload = appdir + "/usr/share/kit/payload";
    QDir().mkpath(appdir + "/usr/bin");
    QDir().mkpath(payload);

    emit message(tr("Préparation des fichiers…"));
    auto copyExec = [&](const QString &s, const QString &d) { return QFile::copy(s, d) && QFile::setPermissions(d, kExec); };
    if (!copyExec(runtime, appdir + "/usr/bin/kit-installer") || !copyExec(req.exe, payload + "/app.bin")) {
        *error = tr("Impossible de copier les fichiers dans l’AppDir."); return false;
    }
    if (!req.licenseFile.isEmpty()) QFile::copy(req.licenseFile, payload + "/license.txt");

    // Langues de l'installeur : gérées par Kit Installer, toutes embarquées (aucun choix du développeur)
    QDir().mkpath(payload + "/lang");
    int nLang = 0;
    for (const LangInfo &li : Lang::discover()) {
        const QString dst = payload + "/lang/" + li.code + ".lang";
        if (QFile::copy(li.path, dst)) { QFile::setPermissions(dst, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup | QFileDevice::ReadOther); ++nLang; }
    }
    emit message(tr("Langues de l’installeur : %1 (gérées automatiquement). Langues du programme : %2.")
                     .arg(nLang).arg(progLangs.isEmpty() ? tr("non précisées") : progLangs.join(", ")));

    QImage square(256, 256, QImage::Format_ARGB32);
    square.fill(Qt::transparent);
    const QImage scaled = icon.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    { QPainter p(&square); p.drawImage((256 - scaled.width()) / 2, (256 - scaled.height()) / 2, scaled); }
    const QString iconName = "kit-installer-" + id;
    square.save(payload + "/icon.png");
    square.save(appdir + "/" + iconName + ".png");
    QFile::link(iconName + ".png", appdir + "/.DirIcon");

    if (kit_manifest_save(&m, (payload + "/manifest.ini").toUtf8().constData()) != 0) { *error = tr("Impossible d’écrire le manifeste."); return false; }

    QFile df(appdir + "/" + iconName + ".desktop");
    if (df.open(QIODevice::WriteOnly)) {
        df.write(QString("[Desktop Entry]\nType=Application\nName=%1 — Installation\nExec=kit-installer\n"
                         "Icon=%2\nCategories=Utility;\nTerminal=false\n").arg(req.name, iconName).toUtf8());
        df.close();
    }
    QFile run(appdir + "/AppRun");
    if (run.open(QIODevice::WriteOnly)) {
        run.write("#!/bin/sh\nHERE=\"$(dirname \"$(readlink -f \"$0\")\")\"\nexport APPDIR=\"${APPDIR:-$HERE}\"\n"
                  "export LD_LIBRARY_PATH=\"$HERE/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}\"\n"
                  "export QT_PLUGIN_PATH=\"$HERE/usr/plugins\"\n"
                  "export QT_QPA_PLATFORM_PLUGIN_PATH=\"$HERE/usr/plugins/platforms\"\n"
                  "exec \"$HERE/usr/bin/kit-installer\" \"$@\"\n");
        run.close();
        run.setPermissions(kExec);
    }
    // --- Clé GPG : sa clé publique et ses informations sont embarquées dans l'AppImage (aucun fichier exporté)
    if (!req.gpgKey.isEmpty()) {
        emit message(tr("Intégration de la clé publique GPG dans l’installeur..."));
        GpgKeyInfo ki;
        QString gerr;
        if (!Gpg::keyInfo(req.gpgKey, &ki, &gerr)) { *error = tr("Clé GPG introuvable ou illisible : %1").arg(gerr); return false; }
        if (!Gpg::exportPublicKey(req.gpgKey, payload + "/signing-key.asc", &gerr)) { *error = tr("Export de la clé publique impossible : %1").arg(gerr); return false; }
        QSettings ini(payload + "/signing-key.ini", QSettings::IniFormat);
        ini.beginGroup("key");
        ini.setValue("fingerprint", ki.fingerprint);
        ini.setValue("keyid", ki.keyId);
        ini.setValue("algorithm", ki.algo);
        ini.setValue("created", ki.created);
        ini.setValue("expires", ki.expires);
        ini.setValue("uid_count", ki.uids.size());
        for (int i = 0; i < ki.uids.size(); ++i) ini.setValue(QString("uid_%1").arg(i), ki.uids[i]);
        ini.setValue("sub_count", ki.subkeys.size());
        for (int i = 0; i < ki.subkeys.size(); ++i) {
            const GpgSubKey &k = ki.subkeys[i];
            ini.setValue(QString("sub_%1").arg(i), QStringList{k.algo, k.keyId, k.created, k.expires, k.caps}.join('|'));
        }
        ini.endGroup();
        ini.sync();
    }
    emit progress(10);

    // --- Qt embarqué
    emit message(tr("Embarquement des bibliothèques Qt…"));
    if (!bundleQt(appdir, error)) return false;

    // --- Assemblage de l'AppImage (SquashFS + runtime), tout en C, sans outil externe
    const QString rtFile = tmp.path() + "/runtime";
    if (!extractRuntime(rtFile, error)) return false;
    QString arch = QSysInfo::currentCpuArchitecture();
    if (arch == "arm64") arch = "aarch64";
    const QString out = QString("%1/%2-Installer-%3-%4.AppImage").arg(req.outDir, id, req.version.trimmed(), arch);

    emit message(kit_appimage_has_zlib() ? tr("Assemblage de l’AppImage (compression zlib)…")
                                         : tr("Assemblage de l’AppImage (sans compression)…"));
    char err[512];
    if (kit_appimage_create(appdir.toUtf8().constData(), rtFile.toUtf8().constData(), out.toUtf8().constData(),
                            1, &Packager::onSquashProgress, this, err, sizeof err) != 0) {
        *error = QString::fromUtf8(err);
        return false;
    }
    *outFile = out;

    // --- Signature GPG détachée (optionnelle)
    if (!req.gpgKey.isEmpty()) {
        emit progress(99);
        emit message(tr("Signature GPG avec la clé %1 (une fenêtre de phrase secrète peut s’ouvrir)...").arg(req.gpgKey));
        QString sig, gerr;
        if (!Gpg::signDetached(out, req.gpgKey, req.gpgPassphraseFile, &sig, &gerr)) {
            *error = tr("L’installeur a été généré mais la signature GPG a échoué : %1").arg(gerr.isEmpty() ? tr("erreur inconnue") : gerr);
            return false;
        }
        QString details;
        if (Gpg::verify(sig, out, &details)) emit message(tr("[OK] Signature GPG créée et vérifiée : %1").arg(sig));
        else emit message(tr("[Attention] Signature créée mais la vérification a échoué : %1").arg(details));
        emit message(tr("Vérification par vos utilisateurs : gpg --verify %1 %2")
                         .arg(QFileInfo(sig).fileName(), QFileInfo(out).fileName()));
    }
    emit progress(100);
    emit message(tr("[OK] Installeur généré : %1 (%2 Mo)").arg(out).arg(QFileInfo(out).size() / 1048576.0, 0, 'f', 1));
    return true;
}
