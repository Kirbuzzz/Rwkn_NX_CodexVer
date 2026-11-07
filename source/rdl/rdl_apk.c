#include "rdl_apk.h"
#include "rdl_fs.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define ZNAME_MAX    300
#define IOBUF        (256u * 1024u)
#define READ_MAX     (64u * 1024u * 1024u)
#define SPACE_MARGIN (16ull * 1024 * 1024)

typedef struct {
    char     name[ZNAME_MAX];
    uint16_t flags, method;
    uint32_t crc, csize, usize, local_off;
} zentry;

typedef struct { FILE *f; zentry *e; size_t n; } zip_t;

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void zip_close(zip_t *z)
{
    if (z->f) fclose(z->f);
    free(z->e);
    memset(z, 0, sizeof *z);
}

static rdl_result zip_open(zip_t *z, const char *path)
{
    memset(z, 0, sizeof *z);
    z->f = fopen(path, "rb");
    if (!z->f) return RDL_ERR_IO;

    uint8_t *tail = NULL, *cd = NULL;
    rdl_result rc = RDL_ERR_APK;

    if (fseek(z->f, 0, SEEK_END) != 0) goto done;
    long fsz = ftell(z->f);
    if (fsz < 22) goto done;

    long tlen = fsz < 65557 ? fsz : 65557;                 /* EOCD (22) + comentario maximo (65535) */
    tail = malloc((size_t)tlen);
    if (!tail) { rc = RDL_ERR_IO; goto done; }
    if (fseek(z->f, fsz - tlen, SEEK_SET) != 0 || fread(tail, 1, (size_t)tlen, z->f) != (size_t)tlen) {
        rc = RDL_ERR_IO; goto done;
    }

    long pos = -1;
    for (long i = tlen - 22; i >= 0; i--)
        if (rd32(tail + i) == 0x06054b50u) { pos = i; break; }
    if (pos < 0) goto done;

    uint16_t total = rd16(tail + pos + 10);
    uint32_t cdsize = rd32(tail + pos + 12), cdoff = rd32(tail + pos + 16);
    if (total == 0xFFFF || cdsize == 0xFFFFFFFFu || cdoff == 0xFFFFFFFFu) goto done;   /* zip64: no soportado */
    if ((uint64_t)cdoff + cdsize > (uint64_t)fsz) goto done;

    cd = malloc(cdsize ? cdsize : 1);
    z->e = calloc(total ? total : 1, sizeof(zentry));
    if (!cd || !z->e) { rc = RDL_ERR_IO; goto done; }
    if (fseek(z->f, (long)cdoff, SEEK_SET) != 0 || fread(cd, 1, cdsize, z->f) != cdsize) { rc = RDL_ERR_IO; goto done; }

    size_t off = 0;
    for (uint16_t i = 0; i < total; i++) {
        if (off + 46 > cdsize || rd32(cd + off) != 0x02014b50u) goto done;
        uint16_t nl = rd16(cd + off + 28), el = rd16(cd + off + 30), cl = rd16(cd + off + 32);
        if (off + 46 + nl + el + cl > cdsize) goto done;
        if (nl < ZNAME_MAX) {
            zentry *e = &z->e[z->n++];
            memcpy(e->name, cd + off + 46, nl);
            e->name[nl] = 0;
            e->flags = rd16(cd + off + 8);
            e->method = rd16(cd + off + 10);
            e->crc = rd32(cd + off + 16);
            e->csize = rd32(cd + off + 20);
            e->usize = rd32(cd + off + 24);
            e->local_off = rd32(cd + off + 42);
        }
        off += 46u + nl + el + cl;
    }
    rc = RDL_OK;

done:
    free(tail);
    free(cd);
    if (rc != RDL_OK) zip_close(z);
    return rc;
}

static const zentry *zip_find(const zip_t *z, const char *name)
{
    for (size_t i = 0; i < z->n; i++)
        if (strcmp(z->e[i].name, name) == 0) return &z->e[i];
    return NULL;
}

typedef rdl_result (*sink_fn)(const uint8_t *p, size_t n, void *ud);

/* Descomprime una entrada enviando los bytes al sink y comprobando tamano y CRC32. */
static rdl_result zip_stream(zip_t *z, const zentry *e, sink_fn sink, void *ud)
{
    if (e->flags & 1u) return RDL_ERR_APK;                         /* cifrada */
    uint8_t h[30];
    if (fseek(z->f, (long)e->local_off, SEEK_SET) != 0 || fread(h, 1, 30, z->f) != 30) return RDL_ERR_APK;
    if (rd32(h) != 0x04034b50u) return RDL_ERR_APK;
    long data_off = (long)e->local_off + 30 + rd16(h + 26) + rd16(h + 28);
    if (fseek(z->f, data_off, SEEK_SET) != 0) return RDL_ERR_APK;

    uint8_t *in = malloc(IOBUF), *out = malloc(IOBUF);
    if (!in || !out) { free(in); free(out); return RDL_ERR_IO; }

    rdl_result rc = RDL_OK;
    uLong crc = crc32(0L, Z_NULL, 0);
    uint64_t produced = 0, remaining = e->csize;

    if (e->method == 0) {
        if (e->csize != e->usize) rc = RDL_ERR_APK;
        while (rc == RDL_OK && remaining) {
            size_t want = remaining < IOBUF ? (size_t)remaining : IOBUF;
            size_t n = fread(in, 1, want, z->f);
            if (n == 0) { rc = RDL_ERR_APK; break; }
            crc = crc32(crc, in, (uInt)n);
            produced += n; remaining -= n;
            rc = sink(in, n, ud);
        }
    } else if (e->method == 8) {
        z_stream s;
        memset(&s, 0, sizeof s);
        if (inflateInit2(&s, -15) != Z_OK) rc = RDL_ERR_APK;
        int ret = Z_OK;
        while (rc == RDL_OK && ret != Z_STREAM_END) {
            if (s.avail_in == 0) {
                if (!remaining) { rc = RDL_ERR_APK; break; }
                size_t want = remaining < IOBUF ? (size_t)remaining : IOBUF;
                size_t n = fread(in, 1, want, z->f);
                if (n == 0) { rc = RDL_ERR_APK; break; }
                remaining -= n;
                s.next_in = in; s.avail_in = (uInt)n;
            }
            s.next_out = out; s.avail_out = IOBUF;
            ret = inflate(&s, Z_NO_FLUSH);
            if (ret != Z_OK && ret != Z_STREAM_END) { rc = RDL_ERR_APK; break; }
            size_t have = IOBUF - s.avail_out;
            if (have) {
                crc = crc32(crc, out, (uInt)have);
                produced += have;
                rc = sink(out, have, ud);
            }
        }
        inflateEnd(&s);
    } else {
        rc = RDL_ERR_APK;                                           /* metodo no soportado */
    }

    free(in); free(out);
    if (rc != RDL_OK) return rc;
    if (produced != e->usize || (uint32_t)crc != e->crc) return RDL_ERR_APK;
    return RDL_OK;
}

/* ---- sinks ---- */

typedef struct {
    FILE *f;
    rdl_progress_fn cb; void *user;
    uint64_t *done; uint64_t total;
} file_sink;

static rdl_result file_sink_write(const uint8_t *p, size_t n, void *ud)
{
    file_sink *s = ud;
    if (fwrite(p, 1, n, s->f) != n) return RDL_ERR_IO;
    *s->done += n;
    if (s->cb && !s->cb(RDL_PHASE_EXTRACT, *s->done, s->total, s->user)) return RDL_ERR_CANCELLED;
    return RDL_OK;
}

typedef struct { uint8_t *p; size_t len; } mem_sink;
static rdl_result mem_sink_write(const uint8_t *p, size_t n, void *ud)
{
    mem_sink *s = ud;
    memcpy(s->p + s->len, p, n);
    s->len += n;
    return RDL_OK;
}

static rdl_result extract_entry(zip_t *z, const zentry *e, const char *out_path,
                                rdl_progress_fn cb, void *user, uint64_t *done, uint64_t total)
{
    char tmp[600];
    if (snprintf(tmp, sizeof tmp, "%s.tmp", out_path) >= (int)sizeof tmp) return RDL_ERR_IO;
    rdl_fs_ensure_parent(out_path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return RDL_ERR_IO;
    setvbuf(f, NULL, _IOFBF, IOBUF);

    file_sink s = { f, cb, user, done, total };
    rdl_result rc = zip_stream(z, e, file_sink_write, &s);
    if (fclose(f) != 0 && rc == RDL_OK) rc = RDL_ERR_IO;
    if (rc != RDL_OK) { remove(tmp); return rc; }

    remove(out_path);                                               /* FAT: rename no pisa */
    if (rename(tmp, out_path) != 0) { remove(tmp); return RDL_ERR_IO; }
    return RDL_OK;
}

/* "assets/x/y.bin" -> "x/y.bin"; NULL si no es un archivo de assets o la ruta es peligrosa. */
static const char *safe_asset_rel(const char *name)
{
    if (strncmp(name, "assets/", 7) != 0) return NULL;
    const char *rel = name + 7;
    size_t n = strlen(rel);
    if (n == 0 || rel[n - 1] == '/') return NULL;                   /* directorio */
    if (rel[0] == '/' || strchr(rel, '\\') || strchr(rel, ':')) return NULL;
    for (const char *p = rel; *p; ) {                               /* ningun componente "." ni ".." */
        const char *q = strchr(p, '/');
        size_t len = q ? (size_t)(q - p) : strlen(p);
        if (len == 0 || (len == 1 && p[0] == '.') || (len == 2 && p[0] == '.' && p[1] == '.')) return NULL;
        if (!q) break;
        p = q + 1;
    }
    return rel;
}

rdl_result rdl_apk_extract(const char *apk_path, const char *app_dir,
                           rdl_progress_fn cb, void *user, bool *did_work)
{
    if (did_work) *did_work = false;
    if (!apk_path || !app_dir) return RDL_ERR_ARG;

    zip_t z;
    rdl_result rc = zip_open(&z, apk_path);
    if (rc != RDL_OK) return rc;

    const zentry *lib = zip_find(&z, RDL_APK_LIB_ENTRY);
    if (!lib) { zip_close(&z); return RDL_ERR_APK; }

    /* sello: suma de CRC + cantidad + tamano total de lo que se extrae (sin leer datos) */
    uint32_t sum = lib->crc;
    uint64_t total = lib->usize, count = 1;
    for (size_t i = 0; i < z.n; i++) {
        if (!safe_asset_rel(z.e[i].name)) continue;
        sum += z.e[i].crc; total += z.e[i].usize; count++;
    }
    char stamp[96], stamp_path[600], lib_path[600], old[96] = {0};
    snprintf(stamp, sizeof stamp, "%08x-%llu-%llu\n", sum, (unsigned long long)count, (unsigned long long)total);
    char libdir[560];
    if (!rdl_fs_join(libdir, sizeof libdir, app_dir, "lib") ||
        !rdl_fs_join(stamp_path, sizeof stamp_path, libdir, ".apk_stamp") ||
        !rdl_fs_join(lib_path, sizeof lib_path, libdir, "libuaf.so")) { zip_close(&z); return RDL_ERR_ARG; }

    FILE *sf = fopen(stamp_path, "rb");
    if (sf) { size_t n = fread(old, 1, sizeof old - 1, sf); old[n] = 0; fclose(sf); }
    uint64_t lsz;
    if (strcmp(old, stamp) == 0 && rdl_fs_size(lib_path, &lsz) && lsz == lib->usize) {
        zip_close(&z);
        return RDL_OK;                                              /* ya extraido */
    }

    uint64_t fr;
    if (rdl_fs_mkdir_p(app_dir) && rdl_fs_free_space(app_dir, &fr) && fr < total + SPACE_MARGIN) {
        zip_close(&z);
        return RDL_ERR_SPACE;
    }
    remove(stamp_path);                                             /* se reescribe al final */

    uint64_t done = 0;
    rc = extract_entry(&z, lib, lib_path, cb, user, &done, total);

    char assets_dir[560];
    if (rc == RDL_OK && !rdl_fs_join(assets_dir, sizeof assets_dir, app_dir, "assets")) rc = RDL_ERR_ARG;
    for (size_t i = 0; rc == RDL_OK && i < z.n; i++) {
        const char *rel = safe_asset_rel(z.e[i].name);
        if (!rel) continue;
        char out[700];
        if (!rdl_fs_join(out, sizeof out, assets_dir, rel)) { rc = RDL_ERR_ARG; break; }
        rc = extract_entry(&z, &z.e[i], out, cb, user, &done, total);
    }
    zip_close(&z);
    if (rc != RDL_OK) return rc;

    FILE *w = fopen(stamp_path, "wb");                              /* ultimo: marca extraccion completa */
    if (!w) return RDL_ERR_IO;
    bool ok = fwrite(stamp, 1, strlen(stamp), w) == strlen(stamp);
    if (fclose(w) != 0) ok = false;
    if (!ok) return RDL_ERR_IO;
    if (did_work) *did_work = true;
    return RDL_OK;
}

rdl_result rdl_apk_read(const char *apk_path, const char *entry_name, void **data, size_t *len)
{
    if (!apk_path || !entry_name || !data || !len) return RDL_ERR_ARG;
    *data = NULL; *len = 0;

    zip_t z;
    rdl_result rc = zip_open(&z, apk_path);
    if (rc != RDL_OK) return rc;
    const zentry *e = zip_find(&z, entry_name);
    if (!e || e->usize > READ_MAX) { zip_close(&z); return RDL_ERR_APK; }

    mem_sink s = { malloc(e->usize ? e->usize : 1), 0 };
    if (!s.p) { zip_close(&z); return RDL_ERR_IO; }
    rc = zip_stream(&z, e, mem_sink_write, &s);
    zip_close(&z);
    if (rc != RDL_OK) { free(s.p); return rc; }
    *data = s.p; *len = s.len;
    return RDL_OK;
}
