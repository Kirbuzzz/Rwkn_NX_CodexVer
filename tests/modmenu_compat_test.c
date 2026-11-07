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
  assert(!modmenu_compat_attach("libwrong.so",
      "Java_com_ubisoft_uaf_UAFJNILib_InitGlobal", fake_initglobal));
  assert(!modmenu_compat_attach("libuaf.so", "wrong_symbol", fake_initglobal));
  assert(!modmenu_compat_attach("libuaf.so",
      "Java_com_ubisoft_uaf_UAFJNILib_InitGlobal", 0));
  assert(modmenu_compat_attach("libuaf.so",
      "Java_com_ubisoft_uaf_UAFJNILib_InitGlobal", fake_initglobal));
  modmenu_compat_call_initglobal((void *)0x11, (void *)0x22, (void *)0x33);
  assert(calls == 1);
  modmenu_compat_engine_ready();
  modmenu_compat_shutdown();
  puts("[ModMenuCompat] flujo virtual dlopen/dlsym/hook verificado");
  return 0;
}
