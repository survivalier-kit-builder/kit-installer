#define _GNU_SOURCE
#include "kit_core.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct { const char *key; size_t off; size_t size; } Field;
#define F(k, m) { k, offsetof(KitManifest, m), sizeof(((KitManifest *)0)->m) }
static const Field FIELDS[] = {
    F("name", name), F("version", version), F("description", description),
    F("developer", developer), F("website", website), F("email", email),
    F("license", license), F("category", category), F("executable", executable),
    F("copyright", copyright), F("program_languages", program_languages), F("default_language", default_language),
};
#define NFIELDS (sizeof FIELDS / sizeof FIELDS[0])

static void write_escaped(FILE *f, const char *s)
{
    for (; *s; s++) {
        if (*s == '\\')      fputs("\\\\", f);
        else if (*s == '\n') fputs("\\n", f);
        else if (*s != '\r') fputc(*s, f);
    }
}

int kit_manifest_save(const KitManifest *m, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fputs("[kit]\n", f);
    for (size_t i = 0; i < NFIELDS; i++) {
        fprintf(f, "%s=", FIELDS[i].key);
        write_escaped(f, (const char *)m + FIELDS[i].off);
        fputc('\n', f);
    }
    return fclose(f) == 0 ? 0 : -1;
}

int kit_manifest_load(KitManifest *m, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    memset(m, 0, sizeof *m);

    size_t cap = KIT_TEXT * 2 + 64;
    char *line = malloc(cap);
    if (!line) { fclose(f); return -1; }

    while (fgets(line, (int)cap, f)) {
        size_t len = strlen(line);
        while (len && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = 0;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        for (size_t i = 0; i < NFIELDS; i++) {
            if (strcmp(line, FIELDS[i].key) != 0) continue;
            char *dst = (char *)m + FIELDS[i].off;
            size_t n = 0;
            for (const char *p = eq + 1; *p && n + 1 < FIELDS[i].size; p++) {
                if (*p == '\\' && p[1]) { p++; dst[n++] = (*p == 'n') ? '\n' : *p; }
                else dst[n++] = *p;
            }
            dst[n] = 0;
            break;
        }
    }
    free(line);
    fclose(f);
    return m->name[0] ? 0 : -1;
}

void kit_make_id(const char *name, char *out, size_t out_size)
{
    /* U+00C0..U+00FF -> équivalent ASCII ('-' si aucun) */
    static const char latin1[] = "AAAAAAACEEEEIIIIDNOOOOO-OUUUUYTsaaaaaaaceeeeiiiidnooooo-ouuuuyty";
    size_t n = 0;
    int last_dash = 1;
    for (const unsigned char *p = (const unsigned char *)name; *p && n + 1 < out_size; p++) {
        int ch = -1;
        if (isascii(*p) && isalnum(*p)) ch = tolower(*p);
        else if (*p == 0xC3 && p[1] >= 0x80 && p[1] <= 0xBF) {
            char l = latin1[p[1] - 0x80];
            p++;
            if (l != '-') ch = tolower((unsigned char)l);
        }
        if (ch >= 0) { out[n++] = (char)ch; last_dash = 0; }
        else if (!last_dash) { out[n++] = '-'; last_dash = 1; }
    }
    while (n && out[n - 1] == '-') n--;
    if (n == 0) { strncpy(out, "app", out_size - 1); out[out_size - 1] = 0; return; }
    out[n] = 0;
}

int kit_mkdir_p(const char *path, unsigned mode)
{
    char *tmp = strdup(path);
    if (!tmp) return -1;
    for (char *p = tmp + 1; *p; p++) {
        if (*p != '/') continue;
        *p = 0;
        if (mkdir(tmp, mode) != 0 && errno != EEXIST) { free(tmp); return -1; }
        *p = '/';
    }
    int r = (mkdir(tmp, mode) != 0 && errno != EEXIST) ? -1 : 0;
    free(tmp);
    return r;
}

int kit_copy_file(const char *src, const char *dst, unsigned mode,
                  KitProgressCb cb, void *user)
{
    int in = open(src, O_RDONLY);
    if (in < 0) return -1;
    struct stat st;
    if (fstat(in, &st) != 0) { close(in); return -1; }

    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (out < 0) { close(in); return -1; }

    enum { CHUNK = 256 * 1024 };
    char *buf = malloc(CHUNK);
    if (!buf) { close(in); close(out); return -1; }

    off_t done = 0;
    int rc = 0;
    ssize_t n;
    while ((n = read(in, buf, CHUNK)) > 0) {
        char *p = buf;
        ssize_t left = n;
        while (left > 0) {
            ssize_t w = write(out, p, (size_t)left);
            if (w < 0) { if (errno == EINTR) continue; rc = -1; goto end; }
            p += w; left -= w;
        }
        done += n;
        if (cb && cb(st.st_size ? (double)done / (double)st.st_size : 1.0, user)) {
            rc = KIT_ABORTED; goto end;
        }
    }
    if (n < 0) rc = -1;
end:
    free(buf);
    close(in);
    if (rc == 0) fchmod(out, (mode_t)mode);
    if (close(out) != 0 && rc == 0) rc = -1;
    if (rc != 0) unlink(dst);
    if (rc == 0 && cb) cb(1.0, user);
    return rc;
}

int kit_write_desktop_entry(const KitManifest *m, const char *exec_path,
                            const char *icon_path, const char *uninstall_cmd,
                            const char *uninstall_label, const char *out_path)
{
    FILE *f = fopen(out_path, "w");
    if (!f) return -1;

    char comment[KIT_FIELD];
    size_t i = 0;
    for (; m->description[i] && m->description[i] != '\n' && i + 1 < sizeof comment; i++)
        comment[i] = m->description[i];
    comment[i] = 0;

    fprintf(f,
        "[Desktop Entry]\nType=Application\nVersion=1.0\nName=%s\nComment=%s\n"
        "Exec=\"%s\"\nIcon=%s\nTerminal=false\nCategories=%s;\n",
        m->name, comment, exec_path, icon_path, m->category[0] ? m->category : "Utility");

    if (uninstall_cmd && *uninstall_cmd)
        fprintf(f, "Actions=Uninstall;\n\n[Desktop Action Uninstall]\nName=%s\nExec=%s\n",
                (uninstall_label && *uninstall_label) ? uninstall_label : "Uninstall", uninstall_cmd);
    return fclose(f) == 0 ? 0 : -1;
}

int kit_write_uninstall_entry(const char *label, const char *exec_cmd,
                              const char *icon_path, const char *out_path)
{
    FILE *f = fopen(out_path, "w");
    if (!f) return -1;
    fprintf(f, "[Desktop Entry]\nType=Application\nVersion=1.0\nName=%s\nExec=%s\nIcon=%s\n"
               "Terminal=false\nCategories=Utility;\n", label, exec_cmd, icon_path);
    return fclose(f) == 0 ? 0 : -1;
}

int kit_link_command(const char *target, const char *link_path)
{
    struct stat st;
    if (lstat(link_path, &st) == 0) {
        if (!S_ISLNK(st.st_mode)) return 1;                  /* fichier réel : on n'y touche pas */
        char old[4096];
        ssize_t l = readlink(link_path, old, sizeof old - 1);
        if (l < 0) return -1;
        old[l] = 0;
        const char *a = strrchr(old, '/'), *b = strrchr(link_path, '/');
        if (strcmp(a ? a + 1 : old, b ? b + 1 : link_path) != 0) return 1;   /* lien d'un autre programme */
        if (unlink(link_path) != 0) return -1;               /* réinstallation : on remplace notre lien */
    }
    return symlink(target, link_path) == 0 ? 0 : -1;
}

int kit_path_contains(const char *dir)
{
    const char *path = getenv("PATH");
    if (!path || !dir) return 0;
    size_t dl = strlen(dir);
    while (dl > 1 && dir[dl - 1] == '/') dl--;
    const char *p = path;
    while (*p) {
        const char *e = strchr(p, ':');
        size_t n = e ? (size_t)(e - p) : strlen(p);
        while (n > 1 && p[n - 1] == '/') n--;
        if (n == dl && strncmp(p, dir, n) == 0) return 1;
        p += e ? (size_t)(e - p) + 1 : strlen(p);
    }
    return 0;
}

#define KIT_PATH_MARK "# Added by Kit Installer (PATH)"

int kit_ensure_path_profile(const char *profile_file, const char *dir_expr)
{
    FILE *f = fopen(profile_file, "r");
    if (f) {
        char line[1024];
        while (fgets(line, sizeof line, f))
            if (strstr(line, KIT_PATH_MARK)) { fclose(f); return 0; }
        fclose(f);
    }
    f = fopen(profile_file, "a");
    if (!f) return -1;
    fprintf(f, "\n%s\ncase \":$PATH:\" in *\":%s:\"*) ;; *) export PATH=\"%s:$PATH\" ;; esac\n",
            KIT_PATH_MARK, dir_expr, dir_expr);
    return fclose(f) == 0 ? 1 : -1;
}

static void sh_quote(FILE *f, const char *s)
{
    fputc('\'', f);
    for (; *s; s++) {
        if (*s == '\'') fputs("'\\''", f);
        else fputc(*s, f);
    }
    fputc('\'', f);
}

int kit_write_uninstaller(const char *path, const char *install_dir,
                          const char *const *files, const char *done_message)
{
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fputs("#!/bin/sh\n# Generated by Kit Installer\n", f);
    for (size_t i = 0; files && files[i]; i++) {
        fputs("rm -f ", f); sh_quote(f, files[i]); fputc('\n', f);
    }
    fputs("rmdir ", f); sh_quote(f, install_dir); fputs(" 2>/dev/null\n", f);
    fputs("update-desktop-database \"$HOME/.local/share/applications\" >/dev/null 2>&1\n", f);
    fputs("echo ", f);
    sh_quote(f, (done_message && *done_message) ? done_message : "Uninstall complete.");
    fputc('\n', f);
    if (fclose(f) != 0) return -1;
    return chmod(path, 0755);
}
