#ifndef REAWAKEN_NX_MODMENU_COMPAT_H
#define REAWAKEN_NX_MODMENU_COMPAT_H

#include <stdbool.h>

/* Reproduce el flujo dlopen -> dlsym -> hook que usa el Mod Menu Android,
 * pero sobre el módulo UAF ya cargado por el port. No carga el ELF Android. */
typedef void (*modmenu_initglobal_fn)(void *env, void *cls, void *activity);

bool modmenu_compat_attach(const char *target_library,
                           const char *target_symbol,
                           modmenu_initglobal_fn resolved_symbol);
void modmenu_compat_call_initglobal(void *env, void *cls, void *activity);
void modmenu_compat_engine_ready(void);
void modmenu_compat_shutdown(void);

#endif
