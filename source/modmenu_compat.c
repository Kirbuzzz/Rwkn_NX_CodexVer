#include <stdbool.h>
#include <string.h>
#include "log.h"
#include "modmenu_compat.h"

#define TARGET_LIBRARY "libuaf.so"
#define TARGET_SYMBOL  "Java_com_ubisoft_uaf_UAFJNILib_InitGlobal"

static modmenu_initglobal_fn s_original_initglobal;
static bool s_module_open;
static bool s_symbol_resolved;
static bool s_hook_installed;
static bool s_initglobal_called;
static bool s_engine_ready;

bool modmenu_compat_attach(const char *target_library,
                           const char *target_symbol,
                           modmenu_initglobal_fn resolved_symbol) {
  s_original_initglobal = 0;
  s_module_open = false;
  s_symbol_resolved = false;
  s_hook_installed = false;
  s_initglobal_called = false;
  s_engine_ready = false;

  if (!target_library || strcmp(target_library, TARGET_LIBRARY) != 0) {
    LOG("ModMenuCompat: dlopen virtual rechazado: biblioteca inesperada (%s)",
        target_library ? target_library : "null");
    return false;
  }
  s_module_open = true;
  LOG("ModMenuCompat: dlopen virtual(%s) -> modulo UAF ya cargado", target_library);

  if (!target_symbol || strcmp(target_symbol, TARGET_SYMBOL) != 0) {
    LOG("ModMenuCompat: dlsym virtual rechazado: simbolo inesperado (%s)",
        target_symbol ? target_symbol : "null");
    return false;
  }
  s_symbol_resolved = (resolved_symbol != 0);
  LOG("ModMenuCompat: dlsym virtual(%s) -> %p", target_symbol,
      (void *)resolved_symbol);
  if (!s_symbol_resolved) {
    LOG("ModMenuCompat: no se encontro InitGlobal; no se instala wrapper");
    return false;
  }

  /* Equivalente funcional al hook de InitGlobal del mod Android: el port
   * conserva el puntero real y enruta la llamada por nuestro wrapper. */
  s_original_initglobal = resolved_symbol;
  s_hook_installed = true;
  LOG("ModMenuCompat: hook nativo de InitGlobal instalado (sin MSHookFunction Android)");
  return true;
}

void modmenu_compat_call_initglobal(void *env, void *cls, void *activity) {
  if (!s_hook_installed || !s_original_initglobal) {
    LOG("ModMenuCompat: InitGlobal no disponible; se omite llamada");
    return;
  }
  LOG("ModMenuCompat: entrando al wrapper InitGlobal");
  s_original_initglobal(env, cls, activity);
  s_initglobal_called = true;
  LOG("ModMenuCompat: InitGlobal original completado");
}

void modmenu_compat_engine_ready(void) {
  if (s_initglobal_called) {
    s_engine_ready = true;
    LOG("ModMenuCompat: motor listo tras InitGlobal");
  } else {
    LOG("ModMenuCompat: motor marco ready sin InitGlobal; revisar resolucion JNI");
  }
}

void modmenu_compat_shutdown(void) {
  LOG("ModMenuCompat: cierre (open=%d symbol=%d hook=%d init=%d ready=%d)",
      s_module_open, s_symbol_resolved, s_hook_installed,
      s_initglobal_called, s_engine_ready);
  s_original_initglobal = 0;
  s_module_open = false;
  s_symbol_resolved = false;
  s_hook_installed = false;
  s_initglobal_called = false;
  s_engine_ready = false;
}
