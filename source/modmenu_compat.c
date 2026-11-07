#include <stdbool.h>
#include "log.h"
#include "modmenu_compat.h"

static modmenu_initglobal_fn s_original_initglobal;
static bool s_prepared;
static bool s_initglobal_called;
static bool s_engine_ready;

void modmenu_compat_prepare(modmenu_initglobal_fn original) {
  s_original_initglobal = original;
  s_prepared = (original != 0);
  s_initglobal_called = false;
  s_engine_ready = false;
  if (s_prepared) {
    LOG("ModMenuCompat: punto InitGlobal localizado; usando wrapper nativo Switch");
    LOG("ModMenuCompat: libModMenu.so original no se carga (ABI Android incompatible)");
  } else {
    LOG("ModMenuCompat: InitGlobal no localizado; adaptador desactivado");
  }
}

void modmenu_compat_call_initglobal(void *env, void *cls, void *activity) {
  if (!s_prepared || !s_original_initglobal) {
    LOG("ModMenuCompat: no se puede llamar InitGlobal: falta funcion original");
    return;
  }
  LOG("ModMenuCompat: antes de InitGlobal (hook de ciclo de vida emulado)");
  s_original_initglobal(env, cls, activity);
  s_initglobal_called = true;
  LOG("ModMenuCompat: InitGlobal completado; motor nativo continua");
}

void modmenu_compat_engine_ready(void) {
  if (s_initglobal_called) {
    s_engine_ready = true;
    LOG("ModMenuCompat: ciclo de inicializacion del motor completado");
  }
}

void modmenu_compat_shutdown(void) {
  if (s_prepared)
    LOG("ModMenuCompat: cierre (InitGlobal=%d engine_ready=%d)",
        s_initglobal_called ? 1 : 0, s_engine_ready ? 1 : 0);
  s_original_initglobal = 0;
  s_prepared = false;
  s_initglobal_called = false;
  s_engine_ready = false;
}
