/* Pantalla de descarga: dibuja el estado (modelo) sobre un lienzo 1280x720 con el fondo ya puesto. */
#ifndef RDL_SCREEN_H
#define RDL_SCREEN_H
#include <stdbool.h>
#include "rdl_gfx.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { SCR_BUSY, SCR_CHOOSE, SCR_PROGRESS, SCR_DONE, SCR_ERROR } scr_mode;

typedef struct {
    scr_mode mode;
    char     status[160];                 /* linea principal */
    char     detail[240];                 /* linea secundaria (errores, versiones...) */
    char     hint[200];                   /* controles */
    bool     show_bar;
    double   frac;                        /* 0..1 */
    char     left[64], center[32], right[64];   /* textos bajo la barra */
} scr_model;

/* Fondo de respaldo cuando no hay arte (sin apk): degradado + titulo en texto. */
void scr_draw_fallback_background(gfx_canvas *c);
/* Panel inferior con estado, barra y controles. */
void scr_render(gfx_canvas *c, const scr_model *m);

#ifdef __cplusplus
}
#endif
#endif
