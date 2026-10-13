#include "project.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>

namespace ProjectFile {

static const int kVersion = 1;

static QString toStored(const QString &baseDir, const QString &path)
{
    if (path.isEmpty() || !QDir::isAbsolutePath(path)) return path;
    const QString rel = QDir(baseDir).relativeFilePath(path);
    int ups = 0;
    for (const QString &part : rel.split('/'))
        if (part == "..") ++ups;
    return ups > 3 ? QDir::cleanPath(path) : rel;           // trop de « ../ » : le chemin absolu est plus lisible
}

static QString fromStored(const QString &baseDir, const QString &stored)
{
    if (stored.isEmpty()) return stored;
    if (QDir::isAbsolutePath(stored)) return QDir::cleanPath(stored);
    return QDir::cleanPath(QDir(baseDir).absoluteFilePath(stored));
}

bool save(const QString &path, const Project &p, QString *error)
{
    const QString base = QFileInfo(path).absolutePath();
    const PackRequest &r = p.req;

    QJsonObject prog;
    prog["name"] = r.name;
    prog["version"] = r.version;
    prog["description"] = r.description;
    prog["developer"] = r.developer;
    prog["email"] = r.email;
    prog["website"] = r.website;
    prog["license"] = r.license;
    prog["license_file"] = toStored(base, r.licenseFile);
    prog["category"] = r.category;
    prog["executable"] = toStored(base, r.exe);
    prog["icon"] = toStored(base, r.icon);
    prog["languages"] = QJsonArray::fromStringList(r.programLanguages);

    QJsonObject root;
    root["format"] = "kit-gen";
    root["version"] = kVersion;
    root["program"] = prog;
    root["output"] = QJsonObject{{"directory", toStored(base, r.outDir)}};
    root["signing"] = QJsonObject{{"enabled", p.sign}, {"gpg_key", p.gpgKey}};

    QSaveFile f(path);                                       // écriture atomique : pas de fichier à moitié écrit
    if (!f.open(QIODevice::WriteOnly)) { if (error) *error = f.errorString(); return false; }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!f.commit()) { if (error) *error = f.errorString(); return false; }
    return true;
}

bool load(const QString &path, Project *out, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { if (error) *error = QObject::tr("Impossible de lire %1 : %2").arg(path, f.errorString()); return false; }
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (doc.isNull() || !doc.isObject()) {
        if (error) *error = QObject::tr("Fichier .kit-gen invalide (%1).").arg(pe.errorString());
        return false;
    }
    const QJsonObject root = doc.object();
    if (root["format"].toString() != "kit-gen") {
        if (error) *error = QObject::tr("Ce fichier n’est pas un projet Kit (.kit-gen).");
        return false;
    }
    if (root["version"].toInt(1) > kVersion) {
        if (error) *error = QObject::tr("Ce projet a été créé par une version plus récente de Kit Builder (format %1).").arg(root["version"].toInt());
        return false;
    }

    const QString base = QFileInfo(path).absolutePath();
    const QJsonObject prog = root["program"].toObject();
    Project p;
    PackRequest &r = p.req;
    r.name = prog["name"].toString();
    r.version = prog["version"].toString("1.0.0");
    r.description = prog["description"].toString();
    r.developer = prog["developer"].toString();
    r.email = prog["email"].toString();
    r.website = prog["website"].toString();
    r.license = prog["license"].toString("MIT");
    r.licenseFile = fromStored(base, prog["license_file"].toString());
    r.category = prog["category"].toString("Utility");
    r.exe = fromStored(base, prog["executable"].toString());
    r.icon = fromStored(base, prog["icon"].toString());
    const QJsonValue langs = prog["languages"];
    if (langs.isArray()) for (const QJsonValue &v : langs.toArray()) r.programLanguages << v.toString();
    else if (langs.isString()) r.programLanguages = langs.toString().split(QRegularExpression("[,;\\s]+"), Qt::SkipEmptyParts);
    r.outDir = fromStored(base, root["output"].toObject()["directory"].toString());
    p.sign = root["signing"].toObject()["enabled"].toBool(false);
    p.gpgKey = root["signing"].toObject()["gpg_key"].toString();
    *out = p;
    return true;
}

}  // namespace ProjectFile
