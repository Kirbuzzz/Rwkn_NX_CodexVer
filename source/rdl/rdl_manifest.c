#include "rdl_manifest.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static const char *skip_ws(const char *p, const char *end)
{
    while (p < end && isspace((unsigned char)*p)) p++;
    return p;
}

/* p apunta a la comilla de apertura; devuelve la comilla de cierre o NULL */
static const char *str_end(const char *p, const char *end)
{
    p++;
    while (p < end) {
        if (*p == '\\') { p += 2; continue; }
        if (*p == '"') return p;
        p++;
    }
    return NULL;
}

static const char *skip_value(const char *p, const char *end)
{
    if (p >= end) return NULL;
    if (*p == '"') { const char *e = str_end(p, end); return e ? e + 1 : NULL; }
    if (*p == '{' || *p == '[') {
        int depth = 0;
        while (p < end) {
            char ch = *p;
            if (ch == '"') { p = str_end(p, end); if (!p) return NULL; p++; continue; }
            if (ch == '{' || ch == '[') depth++;
            else if (ch == '}' || ch == ']') { if (--depth == 0) return p + 1; }
            p++;
        }
        return NULL;
    }
    while (p < end && *p != ',' && *p != '}' && *p != ']' && !isspace((unsigned char)*p)) p++;
    return p;
}

/* p apunta a '{'. Devuelve el inicio del valor del miembro `key` (solo hijos directos). */
static const char *obj_find(const char *p, const char *end, const char *key)
{
    size_t klen = strlen(key);
    if (!p || p >= end || *p != '{') return NULL;
    p++;
    while (p < end) {
        p = skip_ws(p, end);
        if (p >= end || *p == '}') return NULL;
        if (*p == ',') { p++; continue; }
        if (*p != '"') return NULL;
        const char *ks = p + 1;
        const char *ke = str_end(p, end);
        if (!ke) return NULL;
        p = skip_ws(ke + 1, end);
        if (p >= end || *p != ':') return NULL;
        p = skip_ws(p + 1, end);
        if ((size_t)(ke - ks) == klen && memcmp(ks, key, klen) == 0) return p;
        p = skip_value(p, end);
        if (!p) return NULL;
    }
    return NULL;
}

static int get_string(const char *v, const char *end, char *out, size_t cap)
{
    if (!v || v >= end || *v != '"') return -1;
    const char *e = str_end(v, end);
    if (!e) return -1;
    size_t o = 0;
    for (const char *p = v + 1; p < e; p++) {
        char ch = *p;
        if (ch == '\\') {
            p++;
            if (p >= e) return -1;
            switch (*p) {
                case '"': ch = '"'; break;
                case '\\': ch = '\\'; break;
                case '/': ch = '/'; break;
                case 'n': ch = '\n'; break;
                case 't': ch = '\t'; break;
                default: return -1;         /* \uXXXX etc.: no esperado en este manifest */
            }
        }
        if (o + 1 >= cap) return -1;
        out[o++] = ch;
    }
    out[o] = 0;
    return 0;
}

static int get_u64(const char *v, const char *end, uint64_t *out)
{
    if (!v || v >= end || !isdigit((unsigned char)*v)) return -1;
    uint64_t n = 0;
    while (v < end && isdigit((unsigned char)*v)) {
        unsigned d = (unsigned)(*v - '0');
        if (n > (UINT64_MAX - d) / 10) return -1;
        n = n * 10 + d;
        v++;
    }
    *out = n;
    return 0;
}

static bool name_ok(const char *s)
{
    if (!*s || s[0] == '.') return false;
    for (; *s; s++)
        if (!(isalnum((unsigned char)*s) || *s == '.' || *s == '_' || *s == '-')) return false;
    return true;
}

static bool md5_ok(char *s)
{
    if (strlen(s) != 32) return false;
    for (int i = 0; i < 32; i++) {
        if (!isxdigit((unsigned char)s[i])) return false;
        s[i] = (char)tolower((unsigned char)s[i]);
    }
    return true;
}

static void parse_obb(const char *v, const char *end, rdl_obb_info *o, const char *default_name)
{
    memset(o, 0, sizeof *o);
    const char *f;
    f = obj_find(v, end, "fileName");
    if (f) { if (get_string(f, end, o->file_name, sizeof o->file_name)) return; }
    else if (default_name) snprintf(o->file_name, sizeof o->file_name, "%s", default_name);
    else return;
    if (!(f = obj_find(v, end, "url"))      || get_string(f, end, o->url, sizeof o->url)) return;
    if (!(f = obj_find(v, end, "md5"))      || get_string(f, end, o->md5, sizeof o->md5)) return;
    if (!(f = obj_find(v, end, "size"))     || get_u64(f, end, &o->size)) return;

    if (!name_ok(o->file_name)) return;                       /* evita rutas ("../x") */
    if (strncmp(o->url, "https://", 8) != 0) return;          /* solo HTTPS */
    if (!md5_ok(o->md5)) return;
    if (o->size == 0 || o->size > 0xFFFFFFFFull) return;      /* limite de FAT32 */
    o->valid = true;
}

int rdl_manifest_parse(const char *json, size_t len, rdl_manifest *out)
{
    memset(out, 0, sizeof *out);
    const char *end = json + len;
    const char *root = skip_ws(json, end);
    const char *rw = obj_find(root, end, "reawaken");
    if (!rw || *rw != '{') return -1;

    const char *o = obj_find(rw, end, "obb");
    if (o) parse_obb(o, end, &out->normal, NULL);
    if (!out->normal.valid) return -2;

    const char *h = obj_find(rw, end, "obbHd");
    if (h) parse_obb(h, end, &out->hd, NULL);

    const char *a = obj_find(rw, end, "apk");
    if (a) parse_obb(a, end, &out->apk, "Reawaken.apk");
    return 0;
}
