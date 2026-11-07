/* game.h -- carga de libuaf.so y arranque/bucle del motor UAF. */
#ifndef __GAME_H__
#define __GAME_H__

#include <stddef.h>

/* Region del heap reservada para la imagen del .so (la fija main.c en __libnx_initheap). */
extern void *g_heap_so_base;
extern size_t g_heap_so_limit;

/* Carga y relocaliza libuaf.so, crea el JNI falso y arranca el motor (InitGlobal ... init).
 * Requiere EGL ya iniciado con contexto actual. Llama a fatal_error() si algo falla. */
void game_boot(int screen_w, int screen_h);

/* Un frame: entrada -> UAFJNILib.step() -> swap. Devuelve 0 para salir. */
int game_frame(void);

/* Guarda/pausa y libera lo que se pueda. */
void game_shutdown(void);

#endif
