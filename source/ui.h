/* ui.h -- EGL + pantalla de arranque (descarga/estado) dibujada con GLES2.
 *
 * Un solo contexto EGL sirve para la pantalla previa y para el motor: no se mezcla el framebuffer
 * de libnx con EGL. La pantalla es un lienzo por software (rdl_gfx) subido como textura. */
#ifndef __UI_H__
#define __UI_H__

#include <stdbool.h>
#include <stdint.h>
#include <EGL/egl.h>

#include "rdl/rdl_gfx.h"
#include "rdl/rdl_screen.h"

/* EGL (w x h) + objetos GL de la pantalla + mando. Deja el contexto actual. */
void ui_init(int w, int h);
bool ui_active(void);

/* Fondo (arte del juego) desde el apk; si no hay, se usa el de respaldo. */
void ui_load_background(const char *apk_path);

/* Dibuja el modelo y hace swap (espera vsync). */
void ui_present(const scr_model *m);

/* Pulsaciones nuevas (flanco) de cualquier mando; actualiza el estado del pad. */
uint64_t ui_poll_buttons(void);
/* Espera un boton de `mask` (mas Plus); redibuja cada frame. Devuelve Plus si el sistema pide cerrar. */
uint64_t ui_wait_input(const scr_model *m, uint64_t mask);

/* Error fatal en pantalla; true si pudo mostrarlo (EGL activo), false si hay que usar la consola. */
bool ui_fatal(const char *msg);

/* Libera textura, programa y lienzo; deja el estado GL limpio para el motor. EGL sigue vivo. */
void ui_release_stage(void);

EGLDisplay ui_egl_display(void);
EGLSurface ui_egl_surface(void);

#endif
