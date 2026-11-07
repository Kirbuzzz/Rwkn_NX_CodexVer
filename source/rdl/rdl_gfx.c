#include "rdl_gfx.h"
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

/* ------------------------------------------------------------ imagenes */

bool gfx_image_cover(const gfx_image *src, int w, int h, gfx_image *dst)
{
    dst->w = dst->h = 0; dst->rgba = NULL;
    if (!src || !src->rgba || src->w <= 0 || src->h <= 0 || w <= 0 || h <= 0) return false;
    uint8_t *o = malloc((size_t)w * h * 4);
    if (!o) return false;

    /* "cover": escala uniforme minima que llena w x h, centrada */
    double sx = (double)w / src->w, sy = (double)h / src->h;
    double s = sx > sy ? sx : sy;
    double ox = (src->w - w / s) / 2.0, oy = (src->h - h / s) / 2.0;

    for (int y = 0; y < h; y++) {
        int syi = (int)(oy + (y + 0.5) / s);
        if (syi < 0) syi = 0;
        if (syi >= src->h) syi = src->h - 1;
        for (int x = 0; x < w; x++) {
            int sxi = (int)(ox + (x + 0.5) / s);
            if (sxi < 0) sxi = 0;
            if (sxi >= src->w) sxi = src->w - 1;
            const uint8_t *p = src->rgba + ((size_t)syi * src->w + sxi) * 4;
            uint8_t *q = o + ((size_t)y * w + x) * 4;
            unsigned a = p[3];
            q[0] = (uint8_t)(p[0] * a / 255); q[1] = (uint8_t)(p[1] * a / 255); q[2] = (uint8_t)(p[2] * a / 255);
            q[3] = 255;
        }
    }
    dst->w = w; dst->h = h; dst->rgba = o;
    return true;
}

void gfx_canvas_blit(gfx_canvas *c, const gfx_image *img)
{
    if (!c || !img || !img->rgba) return;
    int w = img->w < c->w ? img->w : c->w, h = img->h < c->h ? img->h : c->h;
    for (int y = 0; y < h; y++)
        memcpy(c->px + (size_t)y * c->stride, img->rgba + (size_t)y * img->w * 4, (size_t)w * 4);
}

/* -------------------------------------------------------------- formas */

static inline uint32_t blend_px(uint32_t d, uint32_t s, unsigned cov /* 0..255 */)
{
    unsigned sa = ((s >> 24) & 255u) * cov / 255u;
    if (sa == 0) return d;
    unsigned dr = d & 255u, dg = (d >> 8) & 255u, db = (d >> 16) & 255u;
    unsigned sr = s & 255u, sg = (s >> 8) & 255u, sb = (s >> 16) & 255u;
    dr += ((int)sr - (int)dr) * (int)sa / 255;
    dg += ((int)sg - (int)dg) * (int)sa / 255;
    db += ((int)sb - (int)db) * (int)sa / 255;
    return dr | (dg << 8) | (db << 16) | (255u << 24);
}

void gfx_fill_rect(gfx_canvas *c, int x, int y, int w, int h, uint32_t color)
{
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > c->w ? c->w : x + w, y1 = y + h > c->h ? c->h : y + h;
    unsigned a = (color >> 24) & 255u;
    for (int yy = y0; yy < y1; yy++) {
        uint32_t *row = c->px + (size_t)yy * c->stride;
        if (a == 255) { for (int xx = x0; xx < x1; xx++) row[xx] = color; }
        else for (int xx = x0; xx < x1; xx++) row[xx] = blend_px(row[xx], color, 255);
    }
}

void gfx_round_rect(gfx_canvas *c, int x, int y, int w, int h, int r, uint32_t color)
{
    if (w <= 0 || h <= 0) return;
    if (r * 2 > h) r = h / 2;
    if (r * 2 > w) r = w / 2;
    for (int yy = 0; yy < h; yy++) {
        int py = y + yy;
        if (py < 0 || py >= c->h) continue;
        for (int xx = 0; xx < w; xx++) {
            int px = x + xx;
            if (px < 0 || px >= c->w) continue;
            double cx = -1, cy = -1;
            if (xx < r) cx = r - 0.5; else if (xx >= w - r) cx = w - r + 0.5 - 1.0;
            if (yy < r) cy = r - 0.5; else if (yy >= h - r) cy = h - r + 0.5 - 1.0;
            unsigned cov = 255;
            if (cx >= 0 && cy >= 0) {
                double dx = xx + 0.5 - (cx + 0.5), dy = yy + 0.5 - (cy + 0.5);
                double dist = __builtin_sqrt(dx * dx + dy * dy);
                double k = r - dist + 0.5;
                cov = k <= 0 ? 0 : (k >= 1 ? 255 : (unsigned)(k * 255));
            }
            if (cov) { uint32_t *p = &c->px[(size_t)py * c->stride + px]; *p = blend_px(*p, color, cov); }
        }
    }
}

void gfx_clear_gradient(gfx_canvas *c, uint32_t top, uint32_t bottom)
{
    for (int y = 0; y < c->h; y++) {
        unsigned t = c->h > 1 ? (unsigned)(y * 255 / (c->h - 1)) : 0;
        unsigned r = (top & 255u) + (((int)(bottom & 255u) - (int)(top & 255u)) * (int)t) / 255;
        unsigned g = ((top >> 8) & 255u) + (((int)((bottom >> 8) & 255u) - (int)((top >> 8) & 255u)) * (int)t) / 255;
        unsigned b = ((top >> 16) & 255u) + (((int)((bottom >> 16) & 255u) - (int)((top >> 16) & 255u)) * (int)t) / 255;
        uint32_t px = GFX_RGBA(r, g, b, 255);
        uint32_t *row = c->px + (size_t)y * c->stride;
        for (int x = 0; x < c->w; x++) row[x] = px;
    }
}

/* --------------------------------------------------------------- texto */

#define FIRST_CP 32
#define NGLYPH   224            /* 32..255 */

typedef struct { uint8_t adv; int8_t bx, by; uint8_t w, h; uint32_t off; } glyph_t;
typedef struct { int px, ascent, height; glyph_t g[NGLYPH]; } fsize_t;

static struct { bool ready; int n; fsize_t s[3]; uint8_t *raw; const uint8_t *pix; } F;

bool gfx_font_init(void)
{
    if (F.ready) return true;
    uint8_t *raw = malloc(rdl_font_raw_size);
    if (!raw) return false;
    uLongf dl = rdl_font_raw_size;
    if (uncompress(raw, &dl, rdl_font_blob, rdl_font_blob_size) != Z_OK || dl != rdl_font_raw_size ||
        memcmp(raw, "RFNT", 4) != 0) { free(raw); return false; }

    int n = raw[4];
    if (n < 1 || n > 3) { free(raw); return false; }
    size_t p = 5;
    for (int i = 0; i < n; i++, p += 4) {
        F.s[i].px = raw[p]; F.s[i].ascent = (int8_t)raw[p + 1]; F.s[i].height = raw[p + 2];
    }
    for (int i = 0; i < n; i++) {
        for (int g = 0; g < NGLYPH; g++, p += 9) {
            glyph_t *gl = &F.s[i].g[g];
            gl->adv = raw[p]; gl->bx = (int8_t)raw[p + 1]; gl->by = (int8_t)raw[p + 2];
            gl->w = raw[p + 3]; gl->h = raw[p + 4];
            gl->off = (uint32_t)raw[p + 5] | ((uint32_t)raw[p + 6] << 8) | ((uint32_t)raw[p + 7] << 16) | ((uint32_t)raw[p + 8] << 24);
        }
    }
    F.raw = raw; F.pix = raw + p; F.n = n; F.ready = true;
    return true;
}

void gfx_font_exit(void)
{
    free(F.raw);
    memset(&F, 0, sizeof F);
}

/* siguiente codepoint UTF-8; avanza *s. Fuera de 32..255 -> '?' */
static unsigned next_cp(const unsigned char **s)
{
    const unsigned char *p = *s;
    unsigned cp = *p++;
    if (cp >= 0xF0 && (p[0] & 0xC0) == 0x80 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) { cp = '?'; p += 3; }
    else if (cp >= 0xE0 && (p[0] & 0xC0) == 0x80 && (p[1] & 0xC0) == 0x80) { cp = '?'; p += 2; }
    else if (cp >= 0xC0 && (p[0] & 0xC0) == 0x80) { cp = ((cp & 0x1Fu) << 6) | (p[0] & 0x3Fu); p += 1; }
    else if (cp >= 0x80) cp = '?';
    *s = p;
    if (cp < FIRST_CP || cp > 255) cp = '?';
    return cp;
}

static const fsize_t *fs(int font)
{
    if (!F.ready || font < 0 || font >= F.n) return NULL;
    return &F.s[font];
}

int gfx_font_ascent(int font) { const fsize_t *f = fs(font); return f ? f->ascent : 0; }

int gfx_text_width(int font, const char *utf8)
{
    const fsize_t *f = fs(font);
    if (!f || !utf8) return 0;
    int w = 0;
    const unsigned char *s = (const unsigned char *)utf8;
    while (*s) w += f->g[next_cp(&s) - FIRST_CP].adv;
    return w;
}

void gfx_text(gfx_canvas *c, int font, int x, int baseline, uint32_t color, const char *utf8)
{
    const fsize_t *f = fs(font);
    if (!f || !utf8) return;
    const unsigned char *s = (const unsigned char *)utf8;
    while (*s) {
        const glyph_t *g = &f->g[next_cp(&s) - FIRST_CP];
        if (g->w && g->h) {
            int gx = x + g->bx, gy = baseline - g->by;
            const uint8_t *src = F.pix + g->off;
            for (int yy = 0; yy < g->h; yy++) {
                int py = gy + yy;
                if (py < 0 || py >= c->h) continue;
                uint32_t *row = c->px + (size_t)py * c->stride;
                for (int xx = 0; xx < g->w; xx++) {
                    int px = gx + xx;
                    unsigned cov = src[yy * g->w + xx];
                    if (cov && px >= 0 && px < c->w) row[px] = blend_px(row[px], color, cov);
                }
            }
        }
        x += g->adv;
    }
}

void gfx_text_center(gfx_canvas *c, int font, int cx, int baseline, uint32_t color, const char *utf8)
{
    gfx_text(c, font, cx - gfx_text_width(font, utf8) / 2, baseline, color, utf8);
}

void gfx_text_right(gfx_canvas *c, int font, int right_x, int baseline, uint32_t color, const char *utf8)
{
    gfx_text(c, font, right_x - gfx_text_width(font, utf8), baseline, color, utf8);
}

void gfx_text_fit(int font, const char *in, int max_w, char *out, size_t cap)
{
    if (!cap) return;
    out[0] = 0;
    if (!in) return;
    if (gfx_text_width(font, in) <= max_w) { strncpy(out, in, cap - 1); out[cap - 1] = 0; return; }

    int dots = gfx_text_width(font, "...");
    const unsigned char *s = (const unsigned char *)in, *keep = s;
    int w = 0;
    while (*s) {
        int cw = fs(font) ? fs(font)->g[next_cp(&s) - FIRST_CP].adv : 0;
        if (w + cw + dots > max_w) break;
        w += cw;
        keep = s;
    }
    size_t n = (size_t)(keep - (const unsigned char *)in);
    if (n + 4 > cap) n = cap > 4 ? cap - 4 : 0;
    memcpy(out, in, n);
    memcpy(out + n, "...", 4);
}
