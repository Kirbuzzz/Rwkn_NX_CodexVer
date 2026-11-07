/* Graficos por software, portables (sin libpng/FreeType): lienzo RGBA, PNG, formas y texto.
 * Formato de pixel: uint32 little-endian = R | G<<8 | B<<16 | A<<24  (= PIXEL_FORMAT_RGBA_8888 de libnx). */
#ifndef RDL_GFX_H
#define RDL_GFX_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GFX_RGBA(r, g, b, a) ((uint32_t)(r) | ((uint32_t)(g) << 8) | ((uint32_t)(b) << 16) | ((uint32_t)(a) << 24))

typedef struct { uint32_t *px; int w, h, stride; } gfx_canvas;   /* stride en pixeles */
typedef struct { int w, h; uint8_t *rgba; } gfx_image;           /* bytes R,G,B,A (malloc) */

/* ---- imagenes ---- */
/* Decodifica un PNG (sin entrelazado; gris/RGB/paleta/gris+alfa/RGBA, 1-16 bits). */
bool gfx_png_decode(const void *data, size_t len, gfx_image *out);
void gfx_image_free(gfx_image *img);
/* Escala `src` para cubrir w x h (recorta al centro), aplana el alfa sobre negro. Resultado opaco. */
bool gfx_image_cover(const gfx_image *src, int w, int h, gfx_image *dst);
/* Copia una imagen opaca del mismo tamano que el lienzo. */
void gfx_canvas_blit(gfx_canvas *c, const gfx_image *img);

/* ---- formas (con mezcla alfa, recortadas al lienzo) ---- */
void gfx_fill_rect(gfx_canvas *c, int x, int y, int w, int h, uint32_t color);
void gfx_round_rect(gfx_canvas *c, int x, int y, int w, int h, int radius, uint32_t color);
void gfx_clear_gradient(gfx_canvas *c, uint32_t top, uint32_t bottom);

/* ---- texto (DejaVu Sans Bold, Latin-1, UTF-8) ---- */
enum { GFX_FONT_SMALL = 0, GFX_FONT_MEDIUM = 1, GFX_FONT_LARGE = 2 };
bool gfx_font_init(void);                       /* descomprime el atlas embebido (idempotente) */
void gfx_font_exit(void);
int  gfx_text_width(int font, const char *utf8);
int  gfx_font_ascent(int font);
void gfx_text(gfx_canvas *c, int font, int x, int baseline, uint32_t color, const char *utf8);
void gfx_text_center(gfx_canvas *c, int font, int cx, int baseline, uint32_t color, const char *utf8);
void gfx_text_right(gfx_canvas *c, int font, int right_x, int baseline, uint32_t color, const char *utf8);
/* Copia `in` a `out` recortando con "..." para que quepa en max_w pixeles. */
void gfx_text_fit(int font, const char *in, int max_w, char *out, size_t cap);

/* datos embebidos (generados por tools/gen_font.py) */
extern const unsigned char rdl_font_blob[];
extern const unsigned int  rdl_font_blob_size, rdl_font_raw_size;

#ifdef __cplusplus
}
#endif
#endif
