#define _GNU_SOURCE
#include "kit_appimage.h"

#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifdef KIT_HAVE_ZLIB
#include <zlib.h>
#endif

#define BS  131072u   /* taille de bloc de données */
#define MD  8192u     /* taille d'un bloc de métadonnées */
#define SQFS_MAGIC 0x73717368u

typedef struct Node Node;
struct Node {
    char *name, *path, *target;
    int type;                 /* 1 dossier, 2 fichier, 3 lien symbolique */
    unsigned mode;
    uint32_t mtime, ino;
    uint64_t size, data_start, ipos, dpos;
    uint32_t nblocks, dsize, nsub;
    uint32_t *bsizes;
    Node *parent, **kids;
    size_t nkids, capkids;
};

typedef struct { uint8_t *d; size_t len, cap; } Buf;

typedef struct {
    Node **all; size_t n, cap;
    uint64_t total, done, pos;
    FILE *out;
    int compress;
    KitProgressCb cb; void *user;
    char *err; size_t errsz;
} Ctx;

#define FAIL(c, ...) do { snprintf((c)->err, (c)->errsz, __VA_ARGS__); return -1; } while (0)

int kit_appimage_has_zlib(void)
{
#ifdef KIT_HAVE_ZLIB
    return 1;
#else
    return 0;
#endif
}

/* ---------- Tampons ---------- */
static int put(Buf *b, const void *p, size_t n)
{
    if (b->len + n > b->cap) {
        size_t nc = b->cap ? b->cap : 4096;
        while (nc < b->len + n) nc *= 2;
        uint8_t *q = realloc(b->d, nc);
        if (!q) return -1;
        b->d = q; b->cap = nc;
    }
    memcpy(b->d + b->len, p, n);
    b->len += n;
    return 0;
}
static int put16(Buf *b, uint16_t v) { uint8_t t[2] = { v & 0xff, v >> 8 }; return put(b, t, 2); }
static int put32(Buf *b, uint32_t v)
{
    uint8_t t[4] = { v & 0xff, (v >> 8) & 0xff, (v >> 16) & 0xff, v >> 24 };
    return put(b, t, 4);
}
static int put64(Buf *b, uint64_t v) { return put32(b, (uint32_t)v) | put32(b, (uint32_t)(v >> 32)); }

static int out_write(Ctx *c, const void *p, size_t n)
{
    if (n && fwrite(p, 1, n, c->out) != n) FAIL(c, "Erreur d'écriture : %s", strerror(errno));
    c->pos += n;
    return 0;
}

/* ---------- Analyse de l'arborescence ---------- */
static Node *new_node(Ctx *c, Node *parent, const char *name, const char *path, int type)
{
    Node *n = calloc(1, sizeof *n);
    if (!n) return NULL;
    if (c->n == c->cap) {
        size_t nc = c->cap ? c->cap * 2 : 256;
        Node **q = realloc(c->all, nc * sizeof *q);
        if (!q) { free(n); return NULL; }
        c->all = q; c->cap = nc;
    }
    n->name = strdup(name); n->path = strdup(path);
    n->type = type; n->parent = parent;
    c->all[c->n++] = n;
    n->ino = (uint32_t)c->n;
    if (parent) {
        if (parent->nkids == parent->capkids) {
            size_t nc = parent->capkids ? parent->capkids * 2 : 8;
            Node **q = realloc(parent->kids, nc * sizeof *q);
            if (!q) return NULL;
            parent->kids = q; parent->capkids = nc;
        }
        parent->kids[parent->nkids++] = n;
        if (type == 1) parent->nsub++;
    }
    return n;
}

static int cmp_names(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

static int scan_dir(Ctx *c, Node *dir)
{
    DIR *d = opendir(dir->path);
    if (!d) FAIL(c, "Impossible d'ouvrir %s : %s", dir->path, strerror(errno));
    char **names = NULL; size_t nn = 0, cap = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        if (nn == cap) { cap = cap ? cap * 2 : 16; names = realloc(names, cap * sizeof *names); }
        names[nn++] = strdup(e->d_name);
    }
    closedir(d);
    qsort(names, nn, sizeof *names, cmp_names);

    int rc = 0;
    for (size_t i = 0; i < nn && rc == 0; i++) {
        char *path = NULL;
        if (asprintf(&path, "%s/%s", dir->path, names[i]) < 0) { rc = -1; break; }
        struct stat st;
        if (lstat(path, &st) != 0) { snprintf(c->err, c->errsz, "lstat %s : %s", path, strerror(errno)); rc = -1; free(path); break; }
        int type = S_ISDIR(st.st_mode) ? 1 : S_ISREG(st.st_mode) ? 2 : S_ISLNK(st.st_mode) ? 3 : 0;
        if (!type) { free(path); continue; }
        if (strlen(names[i]) > 255) { snprintf(c->err, c->errsz, "Nom trop long : %s", names[i]); rc = -1; free(path); break; }
        Node *n = new_node(c, dir, names[i], path, type);
        if (!n) { snprintf(c->err, c->errsz, "Mémoire insuffisante"); rc = -1; free(path); break; }
        n->mtime = st.st_mtime > 0 ? (uint32_t)st.st_mtime : 0;
        if (type == 1) { n->mode = 0755; rc = scan_dir(c, n); }
        else if (type == 2) {
            n->mode = (st.st_mode & 0111) ? 0755 : 0644;
            n->size = (uint64_t)st.st_size;
            if (n->size > 0xFFFFFFFFull) { snprintf(c->err, c->errsz, "Fichier trop volumineux (>4 Go) : %s", path); rc = -1; }
            c->total += n->size;
        } else {
            char buf[4096];
            ssize_t l = readlink(path, buf, sizeof buf - 1);
            if (l < 0) { snprintf(c->err, c->errsz, "readlink %s", path); rc = -1; }
            else { buf[l] = 0; n->target = strdup(buf); n->mode = 0777; }
        }
        free(path);
    }
    for (size_t i = 0; i < nn; i++) free(names[i]);
    free(names);
    return rc;
}

/* ---------- Données des fichiers ---------- */
static int write_data(Ctx *c, Node *n)
{
    FILE *f = fopen(n->path, "rb");
    if (!f) FAIL(c, "Impossible de lire %s : %s", n->path, strerror(errno));
    n->nblocks = (uint32_t)((n->size + BS - 1) / BS);
    n->bsizes = calloc(n->nblocks ? n->nblocks : 1, sizeof(uint32_t));
    n->data_start = c->pos;

    size_t outcap = BS + 1024;
#ifdef KIT_HAVE_ZLIB
    outcap = compressBound(BS) + 64;
#endif
    uint8_t *in = malloc(BS), *zb = malloc(outcap);
    int rc = 0;
    for (uint32_t i = 0; i < n->nblocks && rc == 0; i++) {
        size_t want = (i == n->nblocks - 1) ? (size_t)(n->size - (uint64_t)i * BS) : BS;
        if (fread(in, 1, want, f) != want) { snprintf(c->err, c->errsz, "Lecture incomplète : %s", n->path); rc = -1; break; }
        const uint8_t *w = in; size_t wl = want; uint32_t flag = 1u << 24; /* bloc non compressé */
#ifdef KIT_HAVE_ZLIB
        if (c->compress) {
            uLongf dl = (uLongf)outcap;
            if (compress2(zb, &dl, in, (uLong)want, 6) == Z_OK && dl < want) { w = zb; wl = dl; flag = 0; }
        }
#endif
        n->bsizes[i] = (uint32_t)wl | flag;
        if (out_write(c, w, wl) != 0) { rc = -1; break; }
        c->done += want;
        if (c->cb && c->cb(c->total ? (double)c->done / (double)c->total : 1.0, c->user)) {
            snprintf(c->err, c->errsz, "Opération annulée."); rc = -1;
        }
    }
    free(in); free(zb); fclose(f);
    return rc;
}

/* ---------- Métadonnées ---------- */
static int write_meta(Ctx *c, const Buf *b)
{
    for (size_t off = 0; off < b->len; off += MD) {
        size_t chunk = b->len - off < MD ? b->len - off : MD;
        uint8_t h[2] = { chunk & 0xff, 0x80 | (chunk >> 8) };   /* bit 15 : non compressé */
        if (out_write(c, h, 2) || out_write(c, b->d + off, chunk)) return -1;
    }
    return 0;
}

static int build_listing(Ctx *c, Node *d, Buf *ds)
{
    size_t i = 0;
    while (i < d->nkids) {
        Node *first = d->kids[i];
        uint64_t blk = first->ipos / MD;
        size_t j = i;
        while (j < d->nkids && j - i < 256 && d->kids[j]->ipos / MD == blk &&
               (int64_t)d->kids[j]->ino - (int64_t)first->ino <= 32767) j++;
        if (put32(ds, (uint32_t)(j - i - 1)) || put32(ds, (uint32_t)(blk * (MD + 2))) || put32(ds, first->ino))
            FAIL(c, "Mémoire insuffisante");
        for (size_t k = i; k < j; k++) {
            Node *n = d->kids[k];
            size_t nl = strlen(n->name);
            if (put16(ds, (uint16_t)(n->ipos % MD)) || put16(ds, (uint16_t)(int16_t)((int64_t)n->ino - (int64_t)first->ino)) ||
                put16(ds, (uint16_t)n->type) || put16(ds, (uint16_t)(nl - 1)) || put(ds, n->name, nl))
                FAIL(c, "Mémoire insuffisante");
        }
        i = j;
    }
    return 0;
}

static void free_all(Ctx *c)
{
    for (size_t i = 0; i < c->n; i++) {
        Node *n = c->all[i];
        free(n->name); free(n->path); free(n->target); free(n->bsizes); free(n->kids); free(n);
    }
    free(c->all);
}

int kit_squashfs_write(const char *src_dir, FILE *out, int compress,
                       KitProgressCb cb, void *user, char *err, size_t errsz)
{
    Ctx c;
    memset(&c, 0, sizeof c);
    c.out = out; c.compress = compress; c.cb = cb; c.user = user; c.err = err; c.errsz = errsz;
    err[0] = 0;
    Buf is = {0}, ds = {0}, sb = {0};
    int rc = -1;
    off_t base = ftello(out);

    Node *root = new_node(&c, NULL, "", src_dir, 1);
    if (!root) { snprintf(err, errsz, "Mémoire insuffisante"); goto done; }
    root->mode = 0755; root->mtime = (uint32_t)time(NULL);
    if (scan_dir(&c, root)) goto done;

    uint8_t zero[96] = {0};
    if (out_write(&c, zero, sizeof zero)) goto done;

    for (size_t i = 0; i < c.n; i++)
        if (c.all[i]->type == 2 && write_data(&c, c.all[i])) goto done;
    if (c.pos >= 0xFFFFFFFFull) { snprintf(err, errsz, "Image trop volumineuse (>4 Go)."); goto done; }

    /* Position de chaque inode dans la table des inodes */
    uint64_t ip = 0;
    for (size_t i = 0; i < c.n; i++) {
        Node *n = c.all[i];
        n->ipos = ip;
        ip += n->type == 1 ? 32 : n->type == 2 ? 32 + 4ull * n->nblocks : 24 + strlen(n->target);
    }
    /* Table des répertoires */
    for (size_t i = 0; i < c.n; i++) {
        Node *n = c.all[i];
        if (n->type != 1) continue;
        n->dpos = ds.len;
        if (build_listing(&c, n, &ds)) goto done;
        n->dsize = (uint32_t)(ds.len - n->dpos);
        if (n->dsize + 3 > 0xFFFF) { snprintf(err, errsz, "Dossier trop volumineux : %s", n->path); goto done; }
    }
    /* Inodes */
    for (size_t i = 0; i < c.n; i++) {
        Node *n = c.all[i];
        int bad = put16(&is, (uint16_t)n->type) | put16(&is, (uint16_t)n->mode) | put16(&is, 0) | put16(&is, 0) |
                  put32(&is, n->mtime) | put32(&is, n->ino);
        if (n->type == 1) {
            bad |= put32(&is, (uint32_t)((n->dpos / MD) * (MD + 2))) | put32(&is, 2 + n->nsub) |
                   put16(&is, (uint16_t)(n->dsize + 3)) | put16(&is, (uint16_t)(n->dpos % MD)) |
                   put32(&is, n->parent ? n->parent->ino : (uint32_t)c.n + 1);
        } else if (n->type == 2) {
            bad |= put32(&is, (uint32_t)n->data_start) | put32(&is, 0xFFFFFFFFu) | put32(&is, 0) | put32(&is, (uint32_t)n->size);
            for (uint32_t b = 0; b < n->nblocks; b++) bad |= put32(&is, n->bsizes[b]);
        } else {
            bad |= put32(&is, 1) | put32(&is, (uint32_t)strlen(n->target)) | put(&is, n->target, strlen(n->target));
        }
        if (bad) { snprintf(err, errsz, "Mémoire insuffisante"); goto done; }
    }

    uint64_t inode_start = c.pos;
    if (write_meta(&c, &is)) goto done;
    uint64_t dir_start = c.pos;
    if (write_meta(&c, &ds)) goto done;

    Buf idb = {0};
    put32(&idb, 0);                       /* un seul identifiant : 0 (root) */
    uint64_t id_block = c.pos;
    int w = write_meta(&c, &idb);
    free(idb.d);
    if (w) goto done;
    uint64_t id_table = c.pos;
    uint8_t p8[8];
    for (int k = 0; k < 8; k++) p8[k] = (uint8_t)(id_block >> (8 * k));
    if (out_write(&c, p8, 8)) goto done;

    uint64_t used = c.pos;
    while (c.pos % 4096) { uint8_t z = 0; if (out_write(&c, &z, 1)) goto done; }

    /* Superbloc */
    put32(&sb, SQFS_MAGIC); put32(&sb, (uint32_t)c.n); put32(&sb, (uint32_t)time(NULL));
    put32(&sb, BS); put32(&sb, 0);                       /* pas de fragments */
    put16(&sb, 1); put16(&sb, 17);                       /* gzip, log2(131072) */
    put16(&sb, 0x0001 | 0x0008 | 0x0010 | 0x0200 | 0x0800);
    put16(&sb, 1); put16(&sb, 4); put16(&sb, 0);
    put64(&sb, 0);                                       /* inode racine : bloc 0, offset 0 */
    put64(&sb, used); put64(&sb, id_table); put64(&sb, ~0ull);
    put64(&sb, inode_start); put64(&sb, dir_start); put64(&sb, ~0ull); put64(&sb, ~0ull);

    if (fseeko(out, base, SEEK_SET) != 0 || fwrite(sb.d, 1, sb.len, out) != sb.len ||
        fseeko(out, 0, SEEK_END) != 0) { snprintf(err, errsz, "Écriture du superbloc impossible"); goto done; }
    if (c.cb) c.cb(1.0, c.user);
    rc = 0;
done:
    free(is.d); free(ds.d); free(sb.d);
    free_all(&c);
    return rc;
}

int kit_appimage_create(const char *appdir, const char *runtime_path, const char *out_path,
                        int compress, KitProgressCb cb, void *user, char *err, size_t errsz)
{
    FILE *rt = fopen(runtime_path, "rb");
    if (!rt) { snprintf(err, errsz, "Runtime AppImage introuvable : %s", runtime_path); return -1; }
    FILE *out = fopen(out_path, "wb");
    if (!out) { fclose(rt); snprintf(err, errsz, "Impossible de créer %s : %s", out_path, strerror(errno)); return -1; }

    char buf[65536];
    size_t n, total = 0;
    int ok = 1;
    while ((n = fread(buf, 1, sizeof buf, rt)) > 0) {
        if (total == 0 && (n < 4 || memcmp(buf, "\177ELF", 4) != 0)) { ok = 0; snprintf(err, errsz, "Le runtime n'est pas un exécutable ELF."); break; }
        if (fwrite(buf, 1, n, out) != n) { ok = 0; snprintf(err, errsz, "Erreur d'écriture"); break; }
        total += n;
    }
    fclose(rt);
    if (ok && total == 0) { ok = 0; snprintf(err, errsz, "Runtime vide."); }
    if (ok && kit_squashfs_write(appdir, out, compress, cb, user, err, errsz) != 0) ok = 0;
    if (fclose(out) != 0 && ok) { ok = 0; snprintf(err, errsz, "Erreur de fermeture"); }
    if (!ok) { unlink(out_path); return -1; }
    chmod(out_path, 0755);
    return 0;
}
