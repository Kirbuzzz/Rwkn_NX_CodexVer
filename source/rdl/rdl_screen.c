#include "rdl_screen.h"
#include <string.h>

#define WHITE   GFX_RGBA(255, 255, 255, 255)
#define SHADOW  GFX_RGBA(0, 0, 0, 170)

static void text_c(gfx_canvas *c, int font, int cx, int y, uint32_t col, const char *s)
{
    gfx_text_center(c, font, cx + 2, y + 2, SHADOW, s);
    gfx_text_center(c, font, cx, y, col, s);
}

void scr_draw_fallback_background(gfx_canvas *c)
{
    gfx_clear_gradient(c, GFX_RGBA(36, 92, 52, 255), GFX_RGBA(10, 28, 22, 255));
    text_c(c, GFX_FONT_LARGE, c->w / 2, c->h / 3, WHITE, "Rayman Adventures");
    text_c(c, GFX_FONT_LARGE, c->w / 2, c->h / 3 + 58, GFX_RGBA(190, 120, 255, 255), "REAWAKEN");
}

void scr_render(gfx_canvas *c, const scr_model *m)
{
    const int panel_top = c->h - 220;
    const int cx = c->w / 2;

    /* panel oscuro con degradado hacia arriba, para que el texto se lea sobre el arte */
    for (int i = 0; i < 70; i++)
        gfx_fill_rect(c, 0, panel_top - 70 + i, c->w, 1, GFX_RGBA(0, 0, 0, (unsigned)(i * 175 / 70)));
    gfx_fill_rect(c, 0, panel_top, c->w, c->h - panel_top, GFX_RGBA(0, 0, 0, 175));

    uint32_t status_col = WHITE, bar_col = GFX_RGBA(141, 214, 45, 255);
    if (m->mode == SCR_ERROR) status_col = GFX_RGBA(255, 120, 110, 255);
    if (m->mode == SCR_DONE)  status_col = GFX_RGBA(170, 235, 90, 255);

    char buf[256];
    gfx_text_fit(GFX_FONT_MEDIUM, m->status, c->w - 120, buf, sizeof buf);
    text_c(c, GFX_FONT_MEDIUM, cx, panel_top + 52, status_col, buf);

    const int maxw = c->w - 160;
    const uint32_t detail_col = GFX_RGBA(215, 225, 235, 255);
    if (m->mode == SCR_ERROR && gfx_text_width(GFX_FONT_SMALL, m->detail) > maxw) {
        /* dos lineas: corta en el ultimo espacio que quepa en la primera */
        char l1[240];
        size_t n = strlen(m->detail), cut = 0;
        for (size_t i = 1; i < n && i < sizeof l1; i++) {
            if (m->detail[i] != ' ') continue;
            memcpy(l1, m->detail, i); l1[i] = 0;
            if (gfx_text_width(GFX_FONT_SMALL, l1) > maxw) break;
            cut = i;
        }
        if (cut == 0) cut = n > 60 ? 60 : n;
        memcpy(l1, m->detail, cut); l1[cut] = 0;
        text_c(c, GFX_FONT_SMALL, cx, panel_top + 88, detail_col, l1);
        gfx_text_fit(GFX_FONT_SMALL, m->detail + cut + (m->detail[cut] == ' ' ? 1 : 0), maxw, buf, sizeof buf);
        text_c(c, GFX_FONT_SMALL, cx, panel_top + 116, detail_col, buf);
    } else {
        gfx_text_fit(GFX_FONT_SMALL, m->detail, maxw, buf, sizeof buf);
        text_c(c, GFX_FONT_SMALL, cx, panel_top + 90, detail_col, buf);
    }

    if (m->show_bar) {
        const int bw = 900, bh = 24, bx = cx - bw / 2, by = panel_top + 112;
        double f = m->frac < 0 ? 0 : (m->frac > 1 ? 1 : m->frac);
        gfx_round_rect(c, bx - 3, by - 3, bw + 6, bh + 6, (bh + 6) / 2, GFX_RGBA(0, 0, 0, 150));
        gfx_round_rect(c, bx, by, bw, bh, bh / 2, GFX_RGBA(255, 255, 255, 55));
        if (f > 0) {
            int fw = (int)(bw * f);
            if (fw < bh) fw = bh;
            gfx_round_rect(c, bx, by, fw, bh, bh / 2, bar_col);
            gfx_fill_rect(c, bx + bh / 2, by + 3, fw > bh ? fw - bh : 0, 4, GFX_RGBA(255, 255, 255, 60));  /* brillo */
        }
        const int ty = by + bh + 30;
        if (m->left[0])   { gfx_text(c, GFX_FONT_SMALL, bx + 2, ty + 2, SHADOW, m->left);  gfx_text(c, GFX_FONT_SMALL, bx, ty, WHITE, m->left); }
        if (m->center[0]) text_c(c, GFX_FONT_SMALL, cx, ty, WHITE, m->center);
        if (m->right[0])  { gfx_text_right(c, GFX_FONT_SMALL, bx + bw + 2, ty + 2, SHADOW, m->right); gfx_text_right(c, GFX_FONT_SMALL, bx + bw, ty, WHITE, m->right); }
    }

    gfx_text_fit(GFX_FONT_SMALL, m->hint, c->w - 60, buf, sizeof buf);
    text_c(c, GFX_FONT_SMALL, cx, c->h - 22, GFX_RGBA(255, 230, 150, 255), buf);
}
