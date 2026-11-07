#include "rdl.h"
#include "rdl_md5.h"
#include "rdl_fs.h"
#include <curl/curl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MANIFEST_MAX  (256u * 1024u)
#define IO_BUF        (256u * 1024u)
#define SPACE_MARGIN  (16ull * 1024 * 1024)

static char g_err[CURL_ERROR_SIZE];   /* buffer de curl */
static char g_msg[320];               /* mensaje para mostrar */

static struct {
    char base_url[256];
    char ca_file[256];
    char user_agent[64];
    bool insecure;
    bool ready;
    const char *ca_pem;
    size_t ca_pem_len;
} g;

/* -------------------------------------------------------------- curl base */

rdl_result rdl_init(const rdl_config *cfg)
{
    if (!cfg || !cfg->base_url || strncmp(cfg->base_url, "https://", 8) != 0) return RDL_ERR_ARG;
    if (!rdl_fs_copy_str(g.base_url, sizeof g.base_url, cfg->base_url)) return RDL_ERR_ARG;
    size_t n = strlen(g.base_url);
    while (n > 8 && g.base_url[n - 1] == '/') g.base_url[--n] = 0;

    g.ca_file[0] = 0;
    if (cfg->ca_file && !rdl_fs_copy_str(g.ca_file, sizeof g.ca_file, cfg->ca_file)) return RDL_ERR_ARG;
    if (!rdl_fs_copy_str(g.user_agent, sizeof g.user_agent, cfg->user_agent ? cfg->user_agent : "Reawaken"))
        return RDL_ERR_ARG;
    g.insecure = cfg->insecure_tls;
    g.ca_pem = cfg->ca_pem;
    g.ca_pem_len = cfg->ca_pem ? cfg->ca_pem_len : 0;

    /* sin cacert.pem en la SD: dejarlo escrito (mejor esfuerzo) para poder reemplazarlo por uno mas nuevo */
    uint64_t csz;
    if (g.ca_file[0] && g.ca_pem && g.ca_pem_len && !rdl_fs_size(g.ca_file, &csz)) {
        rdl_fs_ensure_parent(g.ca_file);
        FILE *cf = fopen(g.ca_file, "wb");
        if (cf) {
            bool wrote = fwrite(g.ca_pem, 1, g.ca_pem_len, cf) == g.ca_pem_len;
            if (fclose(cf) != 0 || !wrote) remove(g.ca_file);
        }
    }

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) return RDL_ERR_INIT;
    g.ready = true;
    return RDL_OK;
}

void rdl_exit(void)
{
    if (g.ready) curl_global_cleanup();
    g.ready = false;
}

const char *rdl_last_error(void) { return g_msg; }

static void note_error(CURLcode cc, long http)
{
    const char *txt = g_err[0] ? g_err : curl_easy_strerror(cc);
    if (http > 0) snprintf(g_msg, sizeof g_msg, "curl %d (HTTP %ld): %s", (int)cc, http, txt);
    else          snprintf(g_msg, sizeof g_msg, "curl %d: %s", (int)cc, txt);
}

static CURL *new_easy(const char *url)
{
    if (!g.ready || strncmp(url, "https://", 8) != 0) return NULL;
    CURL *h = curl_easy_init();
    if (!h) return NULL;
    g_err[0] = 0;
    g_msg[0] = 0;
    curl_easy_setopt(h, CURLOPT_ERRORBUFFER, g_err);
    curl_easy_setopt(h, CURLOPT_URL, url);
    curl_easy_setopt(h, CURLOPT_USERAGENT, g.user_agent);
    curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(h, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(h, CURLOPT_LOW_SPEED_LIMIT, 1024L);   /* corta si se queda sin datos... */
    curl_easy_setopt(h, CURLOPT_LOW_SPEED_TIME, 30L);      /* ...durante 30 s (se podra reanudar) */
    curl_easy_setopt(h, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(h, CURLOPT_FAILONERROR, 1L);
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(h, CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(h, CURLOPT_REDIR_PROTOCOLS_STR, "https");
#else
    curl_easy_setopt(h, CURLOPT_PROTOCOLS, (long)CURLPROTO_HTTPS);
    curl_easy_setopt(h, CURLOPT_REDIR_PROTOCOLS, (long)CURLPROTO_HTTPS);
#endif
    uint64_t csz;
    if (g.ca_file[0] && rdl_fs_size(g.ca_file, &csz) && csz > 0) {
        curl_easy_setopt(h, CURLOPT_CAINFO, g.ca_file);
    } else if (g.ca_pem && g.ca_pem_len) {
#if LIBCURL_VERSION_NUM >= 0x074D00
        struct curl_blob blob;
        blob.data = (void *)g.ca_pem; blob.len = g.ca_pem_len; blob.flags = CURL_BLOB_COPY;
        curl_easy_setopt(h, CURLOPT_CAINFO_BLOB, &blob);
#endif
    } else if (g.ca_file[0]) {
        curl_easy_setopt(h, CURLOPT_CAINFO, g.ca_file);
    }
    if (g.insecure) {
        curl_easy_setopt(h, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(h, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    return h;
}

/* ---------------------------------------------------------------- manifest */

struct membuf { char *p; size_t len, cap; };

static size_t mem_cb(char *ptr, size_t sz, size_t nm, void *ud)
{
    struct membuf *mb = ud;
    size_t n = sz * nm;
    if (mb->len + n > MANIFEST_MAX) return 0;             /* demasiado grande: abortar */
    if (mb->len + n > mb->cap) {
        size_t nc = mb->cap ? mb->cap * 2 : 4096;
        while (nc < mb->len + n) nc *= 2;
        char *np = realloc(mb->p, nc);
        if (!np) return 0;
        mb->p = np; mb->cap = nc;
    }
    memcpy(mb->p + mb->len, ptr, n);
    mb->len += n;
    return n;
}

static rdl_result classify(CURLcode cc)
{
    if (cc == CURLE_OK) return RDL_OK;
    if (cc == CURLE_HTTP_RETURNED_ERROR) return RDL_ERR_HTTP;
    return RDL_ERR_NET;
}

static rdl_result http_get_mem(const char *url, struct membuf *mb)
{
    CURL *h = new_easy(url);
    if (!h) return RDL_ERR_INIT;
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, mem_cb);
    curl_easy_setopt(h, CURLOPT_WRITEDATA, mb);
    curl_easy_setopt(h, CURLOPT_TIMEOUT, 30L);
    CURLcode cc = curl_easy_perform(h);
    if (cc != CURLE_OK) {
        long code = 0;
        curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &code);
        note_error(cc, code);
    }
    curl_easy_cleanup(h);
    return classify(cc);
}

static bool read_small_file(const char *path, struct membuf *mb)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    char tmp[4096];
    size_t n;
    bool ok = true;
    while ((n = fread(tmp, 1, sizeof tmp, f)) > 0)
        if (mem_cb(tmp, 1, n, mb) != n) { ok = false; break; }
    fclose(f);
    return ok && mb->len > 0;
}

rdl_result rdl_fetch_manifest(rdl_manifest *out, const char *cache_path)
{
    if (!out) return RDL_ERR_ARG;
    if (!g.ready) return RDL_ERR_INIT;
    memset(out, 0, sizeof *out);

    char url[384];
    int n = snprintf(url, sizeof url, "%s/dl/manifest.json?t=%lld", g.base_url, (long long)time(NULL));
    if (n <= 0 || (size_t)n >= sizeof url) return RDL_ERR_ARG;

    struct membuf mb = {0};
    rdl_result r = http_get_mem(url, &mb);
    if (r == RDL_OK) {
        if (rdl_manifest_parse(mb.p ? mb.p : "", mb.len, out) == 0) {
            if (cache_path) {
                rdl_fs_ensure_parent(cache_path);
                FILE *f = fopen(cache_path, "wb");
                if (f) { fwrite(mb.p, 1, mb.len, f); fclose(f); }
            }
            free(mb.p);
            return RDL_OK;
        }
        snprintf(g_msg, sizeof g_msg, "el servidor devolvio algo que no es el manifest (%zu bytes)", mb.len);
        r = RDL_ERR_MANIFEST;
    }
    free(mb.p);

    if (cache_path) {                                      /* sin red: usar la copia local */
        struct membuf cb = {0};
        if (read_small_file(cache_path, &cb) && rdl_manifest_parse(cb.p, cb.len, out) == 0) {
            out->from_cache = true;
            free(cb.p);
            return RDL_OK;
        }
        free(cb.p);
        memset(out, 0, sizeof *out);
    }
    return r;
}

/* ------------------------------------------------------------- estado obb */

rdl_obb_kind rdl_obb_installed(const rdl_manifest *m, const char *obb_dir)
{
    if (!m || !obb_dir) return RDL_OBB_NONE;
    char path[512];
    uint64_t sz;
    bool exists = false;

    const rdl_obb_info *cand[2] = { &m->normal, &m->hd };
    rdl_obb_kind kind[2] = { RDL_OBB_NORMAL, RDL_OBB_HD };
    for (int i = 0; i < 2; i++) {
        if (!cand[i]->valid) continue;
        if (!rdl_fs_join(path, sizeof path, obb_dir, cand[i]->file_name)) continue;
        if (!rdl_fs_size(path, &sz)) continue;
        exists = true;
        if (sz == cand[i]->size) return kind[i];
    }
    return exists ? RDL_OBB_OTHER : RDL_OBB_NONE;
}

static rdl_result md5_file(const char *path, char out_hex[33], rdl_progress_fn cb, void *user, uint64_t total)
{
    FILE *f = fopen(path, "rb");
    if (!f) return RDL_ERR_IO;
    uint8_t *buf = malloc(IO_BUF);
    if (!buf) { fclose(f); return RDL_ERR_IO; }

    rdl_md5_ctx c;
    rdl_md5_init(&c);
    uint64_t done = 0;
    size_t n;
    rdl_result rc = RDL_OK;
    while ((n = fread(buf, 1, IO_BUF, f)) > 0) {
        rdl_md5_update(&c, buf, n);
        done += n;
        if (cb && !cb(RDL_PHASE_VERIFY, done, total, user)) { rc = RDL_ERR_CANCELLED; break; }
    }
    if (rc == RDL_OK && ferror(f)) rc = RDL_ERR_IO;
    free(buf);
    fclose(f);
    if (rc != RDL_OK) return rc;

    uint8_t d[16];
    rdl_md5_final(&c, d);
    rdl_md5_hex(d, out_hex);
    return RDL_OK;
}

rdl_result rdl_obb_verify(const rdl_obb_info *info, const char *obb_dir, rdl_progress_fn cb, void *user)
{
    if (!info || !info->valid || !obb_dir) return RDL_ERR_ARG;
    char path[512];
    if (!rdl_fs_join(path, sizeof path, obb_dir, info->file_name)) return RDL_ERR_ARG;
    uint64_t sz;
    if (!rdl_fs_size(path, &sz)) return RDL_ERR_IO;
    if (sz != info->size) return RDL_ERR_MD5;

    char hex[33];
    rdl_result r = md5_file(path, hex, cb, user, info->size);
    if (r != RDL_OK) return r;
    return strcmp(hex, info->md5) == 0 ? RDL_OK : RDL_ERR_MD5;
}

/* --------------------------------------------------------------- descarga */

typedef struct {
    FILE *f;
    uint64_t resume_from, total;
    rdl_progress_fn cb;
    void *user;
    bool cancelled, io_error;
} dl_ctx;

static size_t dl_write(char *ptr, size_t sz, size_t nm, void *ud)
{
    dl_ctx *c = ud;
    size_t n = sz * nm;
    if (fwrite(ptr, 1, n, c->f) != n) { c->io_error = true; return 0; }
    return n;
}

static int dl_xfer(void *ud, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
    (void)dltotal; (void)ultotal; (void)ulnow;
    dl_ctx *c = ud;
    if (c->cb && !c->cb(RDL_PHASE_DOWNLOAD, c->resume_from + (uint64_t)dlnow, c->total, c->user)) {
        c->cancelled = true;
        return 1;
    }
    return 0;
}

/* Descarga desde `resume` hasta el final, agregando al .part. */
static rdl_result fetch_to_part(const rdl_obb_info *info, const char *part, uint64_t resume,
                                rdl_progress_fn cb, void *user, bool *range_failed)
{
    FILE *f = fopen(part, resume ? "ab" : "wb");
    if (!f) return RDL_ERR_IO;
    setvbuf(f, NULL, _IOFBF, IO_BUF);

    CURL *h = new_easy(info->url);
    if (!h) { fclose(f); return RDL_ERR_INIT; }

    dl_ctx c = { f, resume, info->size, cb, user, false, false };
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, dl_write);
    curl_easy_setopt(h, CURLOPT_WRITEDATA, &c);
    curl_easy_setopt(h, CURLOPT_XFERINFOFUNCTION, dl_xfer);
    curl_easy_setopt(h, CURLOPT_XFERINFODATA, &c);
    curl_easy_setopt(h, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(h, CURLOPT_BUFFERSIZE, 131072L);
    curl_easy_setopt(h, CURLOPT_RESUME_FROM_LARGE, (curl_off_t)resume);

    CURLcode cc = curl_easy_perform(h);
    long code = 0;
    curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &code);
    if (cc != CURLE_OK) note_error(cc, code);
    curl_easy_cleanup(h);
    if (fclose(f) != 0 && cc == CURLE_OK) c.io_error = true;     /* fallo al vaciar el buffer */

    if (cc == CURLE_OK) return c.io_error ? RDL_ERR_IO : RDL_OK;
    if (cc == CURLE_ABORTED_BY_CALLBACK && c.cancelled) return RDL_ERR_CANCELLED;
    if (c.io_error || cc == CURLE_WRITE_ERROR) return RDL_ERR_IO;
    if (cc == CURLE_RANGE_ERROR || (cc == CURLE_HTTP_RETURNED_ERROR && code == 416)) {
        *range_failed = true;                                     /* el servidor no acepta Range */
        return RDL_ERR_HTTP;
    }
    return classify(cc);
}

rdl_result rdl_obb_download(const rdl_obb_info *info, const char *obb_dir, rdl_progress_fn cb, void *user)
{
    if (!info || !info->valid || !obb_dir) return RDL_ERR_ARG;
    if (!g.ready) return RDL_ERR_INIT;

    char final_path[512], part_path[520];
    if (!rdl_fs_join(final_path, sizeof final_path, obb_dir, info->file_name)) return RDL_ERR_ARG;
    if (snprintf(part_path, sizeof part_path, "%s.part", final_path) >= (int)sizeof part_path) return RDL_ERR_ARG;
    if (!rdl_fs_mkdir_p(obb_dir)) return RDL_ERR_IO;

    uint64_t sz;
    /* 1) ya instalado y correcto: nada que hacer */
    if (rdl_fs_size(final_path, &sz) && sz == info->size) {
        rdl_result v = rdl_obb_verify(info, obb_dir, cb, user);
        if (v == RDL_OK || v == RDL_ERR_CANCELLED) return v;
        /* no coincide: se reemplazara al final, cuando el nuevo este validado */
    }

    /* 2) .part previo: reanudar */
    uint64_t have = 0;
    if (rdl_fs_size(part_path, &have) && have > info->size) { remove(part_path); have = 0; }

    if (have < info->size) {
        uint64_t fr;
        if (rdl_fs_free_space(obb_dir, &fr) && fr < (info->size - have) + SPACE_MARGIN) return RDL_ERR_SPACE;

        bool retried = false;
        for (;;) {
            bool range_failed = false;
            rdl_result r = fetch_to_part(info, part_path, have, cb, user, &range_failed);
            if (r == RDL_OK) break;
            if (range_failed && have > 0 && !retried) {          /* reiniciar desde cero una vez */
                remove(part_path);
                have = 0;
                retried = true;
                continue;
            }
            return r;                                           /* el .part se conserva para reanudar */
        }
    }

    /* 3) validar el .part */
    if (!rdl_fs_size(part_path, &sz)) return RDL_ERR_IO;
    if (sz < info->size) return RDL_ERR_NET;                    /* incompleto: reanudable */
    if (sz > info->size) { remove(part_path); return RDL_ERR_MD5; }

    char hex[33];
    rdl_result r = md5_file(part_path, hex, cb, user, info->size);
    if (r != RDL_OK) return r;
    if (strcmp(hex, info->md5) != 0) { remove(part_path); return RDL_ERR_MD5; }

    /* 4) reemplazar (FAT no permite rename sobre un archivo existente) */
    remove(final_path);
    if (rename(part_path, final_path) != 0) return RDL_ERR_IO;
    return RDL_OK;
}

const char *rdl_strerror(rdl_result r)
{
    switch (r) {
        case RDL_OK:            return "ok";
        case RDL_ERR_ARG:       return "argumento invalido";
        case RDL_ERR_INIT:      return "no inicializado";
        case RDL_ERR_NET:       return "error de red (se puede reanudar)";
        case RDL_ERR_HTTP:      return "error del servidor";
        case RDL_ERR_MANIFEST:  return "manifest invalido";
        case RDL_ERR_IO:        return "error de lectura/escritura";
        case RDL_ERR_SPACE:     return "espacio insuficiente en la SD";
        case RDL_ERR_MD5:       return "el archivo no coincide con el manifest";
        case RDL_ERR_CANCELLED: return "cancelado";
        case RDL_ERR_APK:       return "apk invalido o incompleto";
    }
    return "error desconocido";
}
