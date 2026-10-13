#pragma once
/* Fichiers de langue (.lang) : lecture, découverte (builder) et traducteur (installeur). */
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

struct LangInfo {
    QString code;   // fr, en, pt_BR...
    QString name;   // nom affiché, dans la langue elle-même
    QString path;   // chemin du fichier (ou :/lang/xx.lang)
};

namespace Lang {
/* info et/ou strings peuvent être nullptr. Retourne false si le fichier est illisible. */
bool load(const QString &path, LangInfo *info, QMap<QString, QString> *strings);
/* Langues disponibles pour le builder : intégrées + dossiers externes (ceux-ci priment). */
QList<LangInfo> discover();
QStringList searchDirs();
QString userDir();                 // ~/.local/share/kit-installer/languages
void ensureUserTemplate();         // copie _template.lang dans userDir si absent
}

class Translator {
public:
    void set(const QMap<QString, QString> &current, const QMap<QString, QString> &fallback)
    { m_cur = current; m_fb = fallback; }
    QString operator()(const char *key) const
    {
        const QString k = QString::fromLatin1(key);
        if (m_cur.contains(k)) return m_cur.value(k);
        if (m_fb.contains(k)) return m_fb.value(k);
        return k;
    }
    QString operator()(const char *key, const QString &a1) const { return (*this)(key).replace("%1", a1); }
private:
    QMap<QString, QString> m_cur, m_fb;
};
