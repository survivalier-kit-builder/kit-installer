/* kit_appimage : générateur d'AppImage autonome (écrivain SquashFS 4.0 intégré, sans mksquashfs). */
#ifndef KIT_APPIMAGE_H
#define KIT_APPIMAGE_H

#include "kit_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 1 si la compression zlib est disponible (sinon image non compressée, mais valide). */
int kit_appimage_has_zlib(void);

/* Écrit une image SquashFS du dossier src_dir à la position courante de out. */
int kit_squashfs_write(const char *src_dir, FILE *out, int compress,
                       KitProgressCb cb, void *user, char *err, size_t errsz);

/* AppImage = runtime ELF (type 2) + image SquashFS du AppDir. Retourne 0 si OK. */
int kit_appimage_create(const char *appdir, const char *runtime_path, const char *out_path,
                        int compress, KitProgressCb cb, void *user, char *err, size_t errsz);

#ifdef __cplusplus
}
#endif
#endif
