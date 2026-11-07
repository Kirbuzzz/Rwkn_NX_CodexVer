/* Decodificador PNG minimo (solo zlib). Sin entrelazado Adam7; ignora tRNS de gris/RGB. */
#include "rdl_gfx.h"
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static int paeth(int a, int b, int c)
{
    int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    return (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
}

void gfx_image_free(gfx_image *img)
{
    if (img) { free(img->rgba); img->rgba = NULL; img->w = img->h = 0; }
}

bool gfx_png_decode(const void *data, size_t len, gfx_image *out)
{
    static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    const uint8_t *d = data;
    uint8_t *idat = NULL, *raw = NULL;
    size_t idat_len = 0, idat_cap = 0;
    bool ok = false;

    uint32_t w = 0, h = 0;
    int depth = 0, ctype = -1, have_ihdr = 0;
    uint8_t pal[256 * 3], pal_a[256];
    int pal_n = 0;
    memset(pal_a, 255, sizeof pal_a);
    out->w = out->h = 0; out->rgba = NULL;

    if (!d || len < 8 + 12 || memcmp(d, sig, 8) != 0) return false;
    size_t p = 8;
    bool iend = false;
    while (p + 12 <= len && !iend) {
        uint32_t clen = be32(d + p);
        if (clen > len || p + 12 + (size_t)clen > len) goto done;
        const uint8_t *type = d + p + 4, *cd = d + p + 8;
        uLong crc = crc32(0L, type, 4 + clen);
        if ((uint32_t)crc != be32(cd + clen)) goto done;

        if (!memcmp(type, "IHDR", 4)) {
            if (clen != 13) goto done;
            w = be32(cd); h = be32(cd + 4); depth = cd[8]; ctype = cd[9];
            if (cd[10] != 0 || cd[11] != 0 || cd[12] != 0) goto done;     /* sin entrelazado */
            if (w == 0 || h == 0 || w > 8192 || h > 8192) goto done;
            have_ihdr = 1;
        } else if (!memcmp(type, "PLTE", 4)) {
            if (clen % 3 || clen > 768) goto done;
            pal_n = (int)(clen / 3);
            memcpy(pal, cd, clen);
        } else if (!memcmp(type, "tRNS", 4)) {
            if (ctype == 3) memcpy(pal_a, cd, clen > 256 ? 256 : clen);
        } else if (!memcmp(type, "IDAT", 4)) {
            if (idat_len + clen > idat_cap) {
                size_t nc = (idat_len + clen) * 2 + 4096;
                uint8_t *np = realloc(idat, nc);
                if (!np) goto done;
                idat = np; idat_cap = nc;
            }
            memcpy(idat + idat_len, cd, clen);
            idat_len += clen;
        } else if (!memcmp(type, "IEND", 4)) {
            iend = true;
        }
        p += 12 + (size_t)clen;
    }
    if (!have_ihdr || !idat_len) goto done;

    int channels;
    switch (ctype) {
        case 0: channels = 1; if (!(depth == 1 || depth == 2 || depth == 4 || depth == 8 || depth == 16)) goto done; break;
        case 2: channels = 3; if (!(depth == 8 || depth == 16)) goto done; break;
        case 3: channels = 1; if (!(depth == 1 || depth == 2 || depth == 4 || depth == 8) || pal_n == 0) goto done; break;
        case 4: channels = 2; if (!(depth == 8 || depth == 16)) goto done; break;
        case 6: channels = 4; if (!(depth == 8 || depth == 16)) goto done; break;
        default: goto done;
    }
    size_t bits_px = (size_t)channels * (size_t)depth;
    size_t rowbytes = ((size_t)w * bits_px + 7) / 8;
    size_t bpp = (bits_px + 7) / 8;
    size_t expected = (size_t)h * (rowbytes + 1);

    raw = malloc(expected);
    out->rgba = malloc((size_t)w * h * 4);
    if (!raw || !out->rgba) goto done;
    uLongf dlen = (uLongf)expected;
    if (uncompress(raw, &dlen, idat, (uLong)idat_len) != Z_OK || dlen != expected) goto done;

    /* des-filtrado en sitio */
    for (uint32_t y = 0; y < h; y++) {
        uint8_t *row = raw + (size_t)y * (rowbytes + 1);
        uint8_t ft = row[0];
        uint8_t *cur = row + 1;
        const uint8_t *prev = y ? raw + (size_t)(y - 1) * (rowbytes + 1) + 1 : NULL;
        if (ft > 4) goto done;
        for (size_t i = 0; i < rowbytes; i++) {
            int a = i >= bpp ? cur[i - bpp] : 0;
            int b = prev ? prev[i] : 0;
            int c = (prev && i >= bpp) ? prev[i - bpp] : 0;
            int x = cur[i];
            switch (ft) {
                case 1: x += a; break;
                case 2: x += b; break;
                case 3: x += (a + b) / 2; break;
                case 4: x += paeth(a, b, c); break;
            }
            cur[i] = (uint8_t)x;
        }
    }

    /* a RGBA */
    for (uint32_t y = 0; y < h; y++) {
        const uint8_t *cur = raw + (size_t)y * (rowbytes + 1) + 1;
        uint8_t *o = out->rgba + (size_t)y * w * 4;
        for (uint32_t x = 0; x < w; x++, o += 4) {
            if (ctype == 0 || ctype == 3) {
                unsigned v;
                if (depth == 16) v = cur[x * 2];
                else if (depth == 8) v = cur[x];
                else {
                    size_t bit = (size_t)x * depth;
                    v = (cur[bit / 8] >> (8 - depth - (bit % 8))) & ((1u << depth) - 1u);
                }
                if (ctype == 3) {
                    if ((int)v >= pal_n) goto done;
                    o[0] = pal[v * 3]; o[1] = pal[v * 3 + 1]; o[2] = pal[v * 3 + 2]; o[3] = pal_a[v];
                } else {
                    if (depth < 8) v = v * 255u / ((1u << depth) - 1u);
                    o[0] = o[1] = o[2] = (uint8_t)v; o[3] = 255;
                }
            } else {
                size_t step = depth == 16 ? 2 : 1;
                const uint8_t *s = cur + (size_t)x * channels * step;
                if (ctype == 2)      { o[0] = s[0]; o[1] = s[step]; o[2] = s[2 * step]; o[3] = 255; }
                else if (ctype == 4) { o[0] = o[1] = o[2] = s[0]; o[3] = s[step]; }
                else                 { o[0] = s[0]; o[1] = s[step]; o[2] = s[2 * step]; o[3] = s[3 * step]; }
            }
        }
    }
    out->w = (int)w; out->h = (int)h;
    ok = true;

done:
    free(idat);
    free(raw);
    if (!ok) { free(out->rgba); out->rgba = NULL; out->w = out->h = 0; }
    return ok;
}
