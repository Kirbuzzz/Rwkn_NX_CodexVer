#ifndef REAWAKEN_NX_MODMENU_COMPAT_H
#define REAWAKEN_NX_MODMENU_COMPAT_H

/* Adaptador del punto de enganche que usa el mod Android original.
 * No carga libModMenu.so ni simula APIs de Android: envuelve la llamada
 * InitGlobal desde el propio port para mantener el mismo orden de arranque.
 */
typedef void (*modmenu_initglobal_fn)(void *env, void *cls, void *activity);

void modmenu_compat_prepare(modmenu_initglobal_fn original);
void modmenu_compat_call_initglobal(void *env, void *cls, void *activity);
void modmenu_compat_engine_ready(void);
void modmenu_compat_shutdown(void);

#endif
