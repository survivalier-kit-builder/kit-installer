#pragma once
/* Fichier de projet .kit-gen : toutes les informations pour générer un installeur .AppImage sans saisie manuelle. */
#include "packager.h"

#include <QString>

struct Project {
    PackRequest req;          // exe, icône, nom, version, description, développeur, licence, catégorie, dossier de sortie, langues
    bool sign = false;        // signature GPG activée
    QString gpgKey;           // empreinte de la clé choisie (jamais de secret dans le fichier)
};

namespace ProjectFile {
constexpr const char *kSuffix = "kit-gen";
/* Les chemins relatifs sont résolus par rapport au dossier du fichier .kit-gen ; req contient ensuite des chemins absolus. */
bool load(const QString &path, Project *project, QString *error);
/* Les chemins sont enregistrés en relatif au fichier quand c'est raisonnable (projets déplaçables, dépôts GitHub). */
bool save(const QString &path, const Project &project, QString *error);
}
