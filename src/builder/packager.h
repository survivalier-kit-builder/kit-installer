#pragma once
#include <QObject>
#include <QString>
#include <atomic>

struct PackRequest {
    QString exe, icon, name, version = "1.0.0", description, developer, email, website;
    QString license = "MIT", licenseFile, category = "Utility", outDir;
    QString gpgKey;                         // clé GPG de signature (empreinte/ID/e-mail) ; sa clé publique est embarquée dans l'AppImage
    QString gpgPassphraseFile;              // optionnel (CI) ; sinon gpg-agent demande la phrase secrète
    QStringList programLanguages;           // langues prises en charge par le PROGRAMME (codes : en, fr, de...)
};

/* Génère l'AppImage d'installation, sans aucun outil externe (hors `ldd`, fourni par libc). */
class Packager : public QObject {
    Q_OBJECT
public:
    explicit Packager(QObject *parent = nullptr) : QObject(parent) {}
    bool run(const PackRequest &req, QString *outFile, QString *error);   // bloquant
    void cancel() { m_cancel = true; }

signals:
    void message(const QString &line);
    void progress(int percent);

private:
    bool bundleQt(const QString &appdir, QString *error);
    bool extractRuntime(const QString &dst, QString *error);
    static int onSquashProgress(double f, void *user);

    std::atomic<bool> m_cancel{false};
};
