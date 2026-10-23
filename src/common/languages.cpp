#include "languages.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

namespace Lang {

bool load(const QString &path, LangInfo *info, QMap<QString, QString> *strings)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QString text = QString::fromUtf8(f.readAll());
    if (text.startsWith(QChar(0xFEFF))) text.remove(0, 1);

    LangInfo li;
    li.path = path;
    QMap<QString, QString> map;
    QString section;
    for (QString line : text.split('\n')) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith('#') || line.startsWith(';')) continue;
        if (line.startsWith('[') && line.endsWith(']')) { section = line.mid(1, line.size() - 2).toLower(); continue; }
        const int eq = line.indexOf('=');
        if (eq <= 0) continue;
        const QString key = line.left(eq).trimmed();
        QString val = line.mid(eq + 1).trimmed();
        val.replace("\\n", "\n");
        if (section == "meta") {
            if (key == "code") li.code = val;
            else if (key == "name") li.name = val;
        } else {
            map.insert(key, val);
        }
    }
    if (li.code.isEmpty()) li.code = QFileInfo(path).completeBaseName();
    if (li.name.isEmpty()) li.name = li.code;
    static const QRegularExpression ok("^[A-Za-z]{2,3}([_-][A-Za-z0-9]{2,8})?$");
    if (!ok.match(li.code).hasMatch() || li.code == "xx") return false;   // « xx » = modèle non rempli
    if (info) *info = li;
    if (strings) *strings = map;
    return true;
}

QString userDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/kit-installer/languages";
}

QStringList searchDirs()
{
    const QString app = QCoreApplication::applicationDirPath();
    return {app + "/languages", app + "/../share/kit-installer/languages", userDir()};
}

QList<LangInfo> discover()
{
    QMap<QString, LangInfo> found;
    auto scan = [&](const QString &dir) {
        const QDir d(dir);
        for (const QFileInfo &fi : d.entryInfoList({"*.lang"}, QDir::Files)) {
            if (fi.fileName().startsWith('_')) continue;          // _template.lang = modèle, pas une langue
            LangInfo li;
            if (load(fi.absoluteFilePath(), &li, nullptr)) found.insert(li.code, li);
        }
    };
    scan(":/lang");
    for (const QString &d : searchDirs()) scan(QDir::cleanPath(d));

    QList<LangInfo> out = found.values();
    std::sort(out.begin(), out.end(), [](const LangInfo &a, const LangInfo &b) { return a.name.localeAwareCompare(b.name) < 0; });
    return out;
}

void ensureUserTemplate()
{
    QDir().mkpath(userDir());
    const QString dst = userDir() + "/_template.lang";
    if (QFile::exists(dst)) return;
    for (const QString &d : searchDirs()) {
        const QString src = QDir::cleanPath(d + "/_template.lang");
        if (src != dst && QFile::exists(src)) { QFile::copy(src, dst); return; }
    }
}

}  // namespace Lang
