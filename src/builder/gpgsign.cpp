#include "gpgsign.h"

#include <QDateTime>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>

namespace Gpg {

QString executable()
{
    for (const char *n : {"gpg", "gpg2"}) {
        const QString p = QStandardPaths::findExecutable(n);
        if (!p.isEmpty()) return p;
    }
    return QString();
}

static bool run(const QStringList &args, QByteArray *out, QByteArray *err, int timeoutMs)
{
    const QString gpg = executable();
    if (gpg.isEmpty()) { if (err) *err = "gpg introuvable"; return false; }
    QProcess p;
    p.start(gpg, args);
    if (!p.waitForStarted(5000)) { if (err) *err = "gpg n'a pas pu démarrer"; return false; }
    if (!p.waitForFinished(timeoutMs)) { p.kill(); if (err) *err = "délai dépassé (gpg n'a pas répondu)"; return false; }
    if (out) *out = p.readAllStandardOutput();
    if (err) *err = p.readAllStandardError();
    return p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
}

QList<GpgKey> secretKeys()
{
    QList<GpgKey> keys;
    QByteArray out;
    if (!run({"--list-secret-keys", "--with-colons", "--fingerprint"}, &out, nullptr, 15000)) return keys;

    GpgKey cur;
    bool inSec = false, haveFpr = false, haveUid = false, skip = false;
    auto flush = [&] {
        if (inSec && !skip && !cur.fingerprint.isEmpty()) keys << cur;
        cur = GpgKey();
        inSec = haveFpr = haveUid = skip = false;
    };
    for (const QString &line : QString::fromUtf8(out).split('\n')) {
        const QStringList f = line.split(':');
        if (f.size() < 2) continue;
        if (f[0] == "sec") {
            flush();
            inSec = true;
            skip = (f[1] == "e" || f[1] == "r" || f[1] == "d" || f[1] == "i");     // expirée, révoquée, désactivée, invalide
            if (f.size() > 4) cur.keyId = f[4].right(16);
        } else if (f[0] == "fpr" && inSec && !haveFpr && f.size() > 9) {
            cur.fingerprint = f[9]; haveFpr = true;                                   // 1re empreinte = clé primaire
        } else if (f[0] == "uid" && inSec && !haveUid && f.size() > 9) {
            cur.uid = QString(f[9]).replace("\\x3a", ":"); haveUid = true;
        }
    }
    flush();
    return keys;
}

bool signDetached(const QString &file, const QString &key, const QString &passphraseFile, QString *sigPath, QString *error)
{
    const QString sig = file + ".asc";
    QFile::remove(sig);
    QStringList args = {"--batch", "--yes", "--armor", "--detach-sign", "--local-user", key, "--output", sig};
    if (!passphraseFile.isEmpty()) args << "--pinentry-mode" << "loopback" << "--passphrase-file" << passphraseFile;
    args << file;
    QByteArray err;
    if (!run(args, nullptr, &err, 300000)) {           // 5 min : le temps de saisir la phrase secrète
        if (error) *error = QString::fromUtf8(err).trimmed();
        QFile::remove(sig);
        return false;
    }
    if (sigPath) *sigPath = sig;
    return true;
}

bool verify(const QString &sig, const QString &file, QString *details)
{
    QByteArray err;
    const bool ok = run({"--batch", "--verify", sig, file}, nullptr, &err, 30000);
    if (details) *details = QString::fromUtf8(err).trimmed();
    return ok;
}

static QString algoName(const QStringList &f)          // champs : f[2] = taille, f[3] = algo, f[16] = courbe
{
    const QString bits = f.value(2), curve = f.value(16);
    switch (f.value(3).toInt()) {
    case 1: case 2: case 3: return "RSA " + bits;
    case 16: case 20:       return "ElGamal " + bits;
    case 17:                return "DSA " + bits;
    case 18:                return "ECDH (" + curve + ")";
    case 19:                return "ECDSA (" + curve + ")";
    case 22:                return "EdDSA (" + curve + ")";
    default:                return "algo " + f.value(3);
    }
}

static QString dateOf(const QString &epoch)
{
    return epoch.isEmpty() ? QString() : QDateTime::fromSecsSinceEpoch(epoch.toLongLong()).toString("yyyy-MM-dd");
}

bool keyInfo(const QString &key, GpgKeyInfo *info, QString *error)
{
    QByteArray out, err;
    if (!run({"--list-keys", "--with-colons", "--fingerprint", "--fixed-list-mode", key}, &out, &err, 15000) || out.isEmpty()) {
        if (error) *error = QString::fromUtf8(err).trimmed();
        return false;
    }
    GpgKeyInfo ki;
    bool inPub = false, havePub = false, haveFpr = false;
    for (const QString &line : QString::fromUtf8(out).split('\n')) {
        const QStringList f = line.split(':');
        if (f.size() < 2) continue;
        if (f[0] == "pub") {
            if (havePub) break;                                  // une seule clé : la première correspondance
            havePub = inPub = true;
            ki.keyId = f.value(4).right(16);
            ki.algo = algoName(f);
            ki.created = dateOf(f.value(5));
            ki.expires = dateOf(f.value(6));
        } else if (f[0] == "fpr" && inPub && !haveFpr) {
            ki.fingerprint = f.value(9); haveFpr = true;
        } else if (f[0] == "uid" && inPub && f[1] != "r") {
            ki.uids << QString(f.value(9)).replace("\\x3a", ":");
        } else if (f[0] == "sub" && inPub && f[1] != "r") {
            ki.subkeys << GpgSubKey{algoName(f), f.value(4).right(16), dateOf(f.value(5)), dateOf(f.value(6)), f.value(11)};
        }
    }
    if (!havePub || ki.fingerprint.isEmpty()) { if (error) *error = "clé introuvable"; return false; }
    if (info) *info = ki;
    return true;
}

bool exportPublicKey(const QString &key, const QString &outFile, QString *error)
{
    QByteArray out, err;
    if (!run({"--armor", "--export", key}, &out, &err, 30000) || out.isEmpty()) {
        if (error) *error = QString::fromUtf8(err).trimmed();
        return false;
    }
    QFile f(outFile);
    if (!f.open(QIODevice::WriteOnly) || f.write(out) != out.size()) { if (error) *error = "écriture impossible"; return false; }
    return true;
}

}  // namespace Gpg
