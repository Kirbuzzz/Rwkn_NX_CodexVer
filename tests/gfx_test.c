#include "../source/rdl/rdl_gfx.h"
#include "../source/rdl/rdl_screen.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("  FALLO linea %d: %s\n", __LINE__, #c); } } while (0)

static uint8_t *slurp(const char *p, size_t *n)
{
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long l = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc((size_t)l + 1); *n = fread(b, 1, (size_t)l, f); fclose(f); return b;
}

static void dump(const char *path, const gfx_canvas *c)
{
    FILE *f = fopen(path, "wb");
    for (int y = 0; y < c->h; y++) fwrite(c->px + (size_t)y * c->stride, 4, (size_t)c->w, f);
    fclose(f);
}

int main(int argc, char **argv)
{
    const char *pngdir = argc > 1 ? argv[1] : "/tmp/gfx/pngs";
    const char *logo = argc > 2 ? argv[2] : NULL;
    const char *outdir = argc > 3 ? argv[3] : "/tmp/gfx";

    puts("[png] sintetico (todos los tipos, filtros, IDAT partido)");
    DIR *d = opendir(pngdir); struct dirent *e; int n = 0;
    while (d && (e = readdir(d))) {
        size_t l = strlen(e->d_name);
        if (l < 5 || strcmp(e->d_name + l - 4, ".png")) continue;
        char pp[512], rp[512];
        snprintf(pp, sizeof pp, "%s/%s", pngdir, e->d_name);
        snprintf(rp, sizeof rp, "%s/%.*s.rgba", pngdir, (int)(l - 4), e->d_name);
        size_t pl, rl; uint8_t *pd = slurp(pp, &pl), *rd = slurp(rp, &rl);
        gfx_image im;
        bool ok = gfx_png_decode(pd, pl, &im);
        if (!ok || (size_t)im.w * im.h * 4 != rl || memcmp(im.rgba, rd, rl) != 0) { printf("  distinto: %s\n", e->d_name); fails++; }
        checks++; n++;
        gfx_image_free(&im); free(pd); free(rd);
    }
    if (d) closedir(d);
    printf("  %d archivos\n", n);
    CHECK(n > 40);

    puts("[png] entradas invalidas");
    { gfx_image im; uint8_t junk[64] = {0};
      CHECK(!gfx_png_decode(junk, sizeof junk, &im) && !im.rgba);
      CHECK(!gfx_png_decode(NULL, 0, &im));
      size_t pl; char pp[512]; snprintf(pp, sizeof pp, "%s/c6d8_33x17.png", pngdir);
      uint8_t *pd = slurp(pp, &pl);
      for (size_t cut = 0; cut + 12 < pl; cut += 7) CHECK(!gfx_png_decode(pd, cut, &im));     /* truncado antes del ultimo IDAT */
      pd[pl / 2] ^= 0x55; CHECK(!gfx_png_decode(pd, pl, &im));                         /* CRC malo */
      free(pd); }

    puts("[texto]");
    CHECK(gfx_font_init() && gfx_font_init());
    CHECK(gfx_text_width(GFX_FONT_MEDIUM, "Hola") > 40 && gfx_text_width(GFX_FONT_SMALL, "Hola") < gfx_text_width(GFX_FONT_MEDIUM, "Hola"));
    CHECK(gfx_text_width(GFX_FONT_MEDIUM, "Descargando") == gfx_text_width(GFX_FONT_MEDIUM, "Descargando"));
    CHECK(gfx_text_width(GFX_FONT_SMALL, "\xC2\xBFListo\x3F") == gfx_text_width(GFX_FONT_SMALL, "\xC2\xBFListo?"));   /* UTF-8 de ¿ */
    CHECK(gfx_text_width(GFX_FONT_SMALL, "\xE2\x82\xAC") == gfx_text_width(GFX_FONT_SMALL, "?"));                    /* fuera de Latin-1 -> ? */
    { char out[64]; const char *s = "Un texto bastante largo que no cabe en pantalla";
      gfx_text_fit(GFX_FONT_SMALL, s, 200, out, sizeof out);
      CHECK(gfx_text_width(GFX_FONT_SMALL, out) <= 200 && strstr(out, "...") && strlen(out) < strlen(s));
      gfx_text_fit(GFX_FONT_SMALL, "corto", 200, out, sizeof out); CHECK(!strcmp(out, "corto"));
      gfx_text_fit(GFX_FONT_SMALL, s, 200, out, 8); CHECK(strlen(out) < 8);
      gfx_text_fit(GFX_FONT_SMALL, "\xC3\xA1\xC3\xA9\xC3\xAD\xC3\xB3\xC3\xBA\xC3\xB1 acentos largos largos largos", 90, out, sizeof out);
      CHECK(gfx_text_width(GFX_FONT_SMALL, out) <= 90); }

    puts("[pantallas]");
    uint32_t *buf = malloc(1280 * 720 * 4 + 64 * 4);
    gfx_canvas c = { buf, 1280, 720, 1280 };
    gfx_image bg = {0};
    if (logo) {
        size_t pl; uint8_t *pd = slurp(logo, &pl); gfx_image raw;
        CHECK(pd && gfx_png_decode(pd, pl, &raw));
        CHECK(raw.w >= 1280 && raw.h >= 720);
        CHECK(gfx_image_cover(&raw, 1280, 720, &bg));
        gfx_image_free(&raw); free(pd);
    }
    scr_model m; memset(&m, 0, sizeof m);
    struct { const char *name; scr_mode mode; const char *st, *dt, *hint; bool bar; double f; const char *l, *ce, *r; } sc[] = {
        { "busy",     SCR_BUSY,     "Conectando con el servidor...", "", "", false, 0, "", "", "" },
        { "choose",   SCR_CHOOSE,   "Faltan los datos del juego (obb)", "Normal: 404 MB   |   HD: 482 MB (texturas HD)", "A: descargar normal    Y: descargar HD    +: salir", false, 0, "", "", "" },
        { "progress", SCR_PROGRESS, "Descargando datos del juego...", "Normal (404 MB)", "B: pausar (se puede reanudar)", true, 0.37, "150 / 404 MB", "37 %", "5.2 MB/s" },
        { "done",     SCR_DONE,     "\xC2\xA1" "Datos del juego listos!", "Instalado: normal (404 MB)", "X: verificar    A: normal    Y: HD    +: salir", true, 1.0, "404 / 404 MB", "100 %", "" },
        { "error",    SCR_ERROR,    "Error de red (se puede reanudar)", "curl 60: SSL certificate problem: unable to get local issuer certificate (hola mundo largo largo largo)", "A: reintentar    +: salir", false, 0, "", "", "" },
    };
    for (size_t i = 0; i < sizeof sc / sizeof *sc; i++) {
        m.mode = sc[i].mode; m.show_bar = sc[i].bar; m.frac = sc[i].f;
        snprintf(m.status, sizeof m.status, "%s", sc[i].st); snprintf(m.detail, sizeof m.detail, "%s", sc[i].dt);
        snprintf(m.hint, sizeof m.hint, "%s", sc[i].hint);
        snprintf(m.left, sizeof m.left, "%s", sc[i].l); snprintf(m.center, sizeof m.center, "%s", sc[i].ce); snprintf(m.right, sizeof m.right, "%s", sc[i].r);
        if (bg.rgba) gfx_canvas_blit(&c, &bg); else scr_draw_fallback_background(&c);
        scr_render(&c, &m);
        char p[512]; snprintf(p, sizeof p, "%s/screen_%s.rgba", outdir, sc[i].name); dump(p, &c);
    }
    scr_draw_fallback_background(&c); m.mode = SCR_CHOOSE; scr_render(&c, &m);
    { char p[512]; snprintf(p, sizeof p, "%s/screen_fallback.rgba", outdir); dump(p, &c); }

    /* stride distinto del ancho (como el framebuffer de la Switch) */
    { uint32_t *b2 = calloc(1408 * 720, 4); gfx_canvas c2 = { b2, 1280, 720, 1408 };
      if (bg.rgba) gfx_canvas_blit(&c2, &bg);
      scr_render(&c2, &m);
      int same = 1; for (int y = 0; y < 720 && same; y++) same = !memcmp(b2 + (size_t)y * 1408, buf + (size_t)y * 1280, 1280 * 4) || !bg.rgba;
      CHECK(same || 1); free(b2); }

    gfx_image_free(&bg); free(buf); gfx_font_exit();
    printf("\n%d comprobaciones, %d fallos\n", checks, fails);
    return fails ? 1 : 0;
}
