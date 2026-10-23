#pragma once
/* Signature GPG détachée de l'installeur généré (via le programme gpg du système). */
#include <QList>
#include <QString>
#include <QStringList>

struct GpgKey {
    QString fingerprint, keyId, uid;
    QString label() const { return QString("%1  (%2)").arg(uid.isEmpty() ? QStringLiteral("?") : uid, keyId); }
};

struct GpgSubKey { QString algo, keyId, created, expires, caps; };
struct GpgKeyInfo {
    QString fingerprint, keyId, algo, created, expires;      // dates "yyyy-MM-dd" ; expires vide = n'expire jamais
    QStringList uids;
    QList<GpgSubKey> subkeys;
};

namespace Gpg {
QString executable();                       // chemin de gpg, vide s'il est absent
QList<GpgKey> secretKeys();                 // clés secrètes utilisables (ni expirées ni révoquées)
/* Crée <file>.asc (signature détachée, ASCII armor). passphraseFile : optionnel (CI) ; sinon gpg-agent/pinentry demande. */
bool signDetached(const QString &file, const QString &key, const QString &passphraseFile, QString *sigPath, QString *error);
bool verify(const QString &sig, const QString &file, QString *details);
/* Lit les informations de la clé publique (identités, empreinte, algorithme, dates, sous-clés). */
bool keyInfo(const QString &key, GpgKeyInfo *info, QString *error);
bool exportPublicKey(const QString &key, const QString &outFile, QString *error);
}
