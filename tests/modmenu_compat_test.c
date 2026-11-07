#include <assert.h>
#include <stdio.h>
#include "../source/modmenu_compat.h"

static int calls;
static void fake_initglobal(void *env, void *cls, void *activity) {
  assert(env == (void *)0x11);
  assert(cls == (void *)0x22);
  assert(activity == (void *)0x33);
  calls++;
}
void log_printf(const char *fmt, ...) { (void)fmt; }

int main(void) {
  /* La API vigente prepara el wrapper con el punto de entrada real. */
  modmenu_compat_prepare(NULL);
  modmenu_compat_call_initglobal((void *)0x11, (void *)0x22, (void *)0x33);
  assert(calls == 0);

  modmenu_compat_prepare(fake_initglobal);
  modmenu_compat_call_initglobal((void *)0x11, (void *)0x22, (void *)0x33);
  assert(calls == 1);
  modmenu_compat_engine_ready();
  modmenu_compat_shutdown();

  /* El cierre debe dejar el adaptador desactivado. */
  modmenu_compat_call_initglobal((void *)0x11, (void *)0x22, (void *)0x33);
  assert(calls == 1);
  puts("[ModMenuCompat] prepare/call/ready/shutdown verificado");
  return 0;
}
