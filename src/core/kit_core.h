/* kit_core : cœur en C de Kit Installer (manifeste, copie, .desktop, désinstalleur). */
#ifndef KIT_CORE_H
#define KIT_CORE_H

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KIT_FIELD   256
#define KIT_TEXT    4096
#define KIT_ABORTED (-2)

typedef struct {
    char name[KIT_FIELD];
    char version[64];
    char description[KIT_TEXT];
    char developer[KIT_FIELD];
    char website[KIT_FIELD];
    char email[KIT_FIELD];
    char license[KIT_FIELD];
    char category[64];      /* catégorie freedesktop : Utility, Game, ... */
    char executable[KIT_FIELD];
    char copyright[KIT_FIELD];
    char program_languages[KIT_FIELD]; /* langues prises en charge par le programme : "en,fr,de" */
    char default_language[32];         /* langue par défaut de l'installeur ("en") */
} KitManifest;

/* Retourne 0 si OK. */
int  kit_manifest_save(const KitManifest *m, const char *path);
int  kit_manifest_load(KitManifest *m, const char *path);

/* "Mon Super Prog!" -> "mon-super-prog" */
void kit_make_id(const char *name, char *out, size_t out_size);

int  kit_mkdir_p(const char *path, unsigned mode);

/* Callback : fraction 0..1 ; retourner non-zéro pour annuler. */
typedef int (*KitProgressCb)(double fraction, void *user);
int  kit_copy_file(const char *src, const char *dst, unsigned mode,
                   KitProgressCb cb, void *user);

int  kit_write_desktop_entry(const KitManifest *m, const char *exec_path,
                             const char *icon_path, const char *uninstall_cmd,
                             const char *uninstall_label, const char *out_path);
/* Entrée de menu autonome « Désinstaller <programme> » (exec_cmd = ligne Exec complète). */
int  kit_write_uninstall_entry(const char *label, const char *exec_cmd,
                               const char *icon_path, const char *out_path);

/* Commande dans le PATH : crée le lien symbolique link_path -> target (ex. ~/.local/bin/monprog).
   Retourne 0 si OK, 1 si un autre fichier/lien du même nom existe (non touché), -1 si erreur. */
int  kit_link_command(const char *target, const char *link_path);
/* 1 si le dossier est dans $PATH de ce processus. */
int  kit_path_contains(const char *dir);
/* Ajoute (une seule fois) un bloc « export PATH » au fichier de profil. dir_expr ex. "$HOME/.local/bin".
   Retourne 1 si le fichier a été modifié, 0 s'il contenait déjà le bloc, -1 si erreur. */
int  kit_ensure_path_profile(const char *profile_file, const char *dir_expr);

/* files : tableau de chemins terminé par NULL (le script se supprime lui-même s'il y figure). */
int  kit_write_uninstaller(const char *path, const char *install_dir,
                           const char *const *files, const char *done_message);

#ifdef __cplusplus
}
#endif
#endif
