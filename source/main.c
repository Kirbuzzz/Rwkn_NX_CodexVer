/* main.c
 *
 * Rayman Adventures (Android, arm64) en la Switch, via el cargador de .so de gdash_nx.
 *
 * El motor es UAF (Ubisoft) dentro de libuaf.so; el audio es Wwise sobre OpenSL ES (emulado en opensl.c);
 * la capa Java de Android se sustituye por jni_fake.c. Este hilo principal hace de hilo GL del
 * GLSurfaceView: EGL -> pantalla previa (apk, manifest, obb) -> arranque del motor -> bucle entrada/step/swap.
 *
 * Proyecto Reawaken (servidores del obb) y gd_nx por sus respectivos autores. Licencia MIT, ver LICENSE.
 */

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <EGL/egl.h>
#include <switch.h>

#include "config.h"
#include "error.h"
#include "game.h"
#include "launcher.h"
#include "log.h"
#include "net_shim.h"
#include "paths.h"
#include "prefs.h"
#include "pthr.h"
#include "so_util.h"
#include "ui.h"
#include "util.h"


// ---------------------------------------------------------------------------
// heap: se parte entre el heap de newlib y la region donde se carga el .so
// (tal cual en gdash_nx / port de lbbg)
// ---------------------------------------------------------------------------

void __libnx_initheap(void) {
  void *addr;
  size_t size = 0, fake_heap_size = 0;
  size_t mem_available = 0, mem_used = 0;

  if (envHasHeapOverride()) {
    addr = envGetHeapOverrideAddr();
    size = envGetHeapOverrideSize();
  } else {
    svcGetInfo(&mem_available, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
    svcGetInfo(&mem_used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
    if (mem_available > mem_used + 0x200000)
      size = (mem_available - mem_used - 0x200000) & ~0x1FFFFF;
    if (size == 0)
      size = 0x2000000 * 16;
    Result rc = svcSetHeapSize(&addr, size);
    if (R_FAILED(rc))
      diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_HeapAllocFailed));
  }

  const size_t so_reserve = (size_t)SO_REGION_MB * 1024 * 1024;
  fake_heap_size = (size > so_reserve + so_reserve / 2) ? (size - so_reserve) : (size / 2);

  extern char *fake_heap_start;
  extern char *fake_heap_end;
  fake_heap_start = (char *)addr;
  fake_heap_end   = (char *)addr + fake_heap_size;

  g_heap_so_base = (char *)addr + fake_heap_size;
  g_heap_so_base = (void *)ALIGN_MEM((uintptr_t)g_heap_so_base, 0x1000);
  g_heap_so_limit = (char *)addr + size - (char *)g_heap_so_base;
}

// ---------------------------------------------------------------------------

static void set_screen_size(int w, int h) {
  if (w <= 0 || h <= 0 || w > 1920 || h > 1080) {
    if (appletGetOperationMode() == AppletOperationMode_Console) {
      screen_width = 1920; screen_height = 1080;
    } else {
      screen_width = 1280; screen_height = 720;
    }
  } else {
    screen_width = w; screen_height = h;
  }
}

// El cargador necesita mapear memoria como codigo: solo disponible al abrir el homebrew encima de un juego
// (mantener R al iniciar un titulo). En modo applet (album) no hay estas llamadas al sistema.
static const char *engine_blocker(unsigned heap_mb) {
  if (!envIsSyscallHinted(0x77) || !envIsSyscallHinted(0x78) || !envIsSyscallHinted(0x73) ||
      envGetOwnProcessHandle() == INVALID_HANDLE)
    return "Para jugar abre con R pulsado sobre un juego";
  if (heap_mb < 1500)
    return "Memoria insuficiente: abre con R pulsado sobre un juego";
  return NULL;
}

int main(int argc, char **argv) {
  pthr_pin_render_core();
  cpu_boost(1);

  paths_init(argc > 0 ? argv[0] : NULL);
  mkdir(path_data(), 0777);
  log_init(path_log());
  LOG("Reaweaken NX (argv0=%s base=%s)", argc > 0 ? argv[0] : "?", path_base());

  if (read_config(path_config()) < 0)
    write_config(path_config());
  set_screen_size(config.screen_width, config.screen_height);

  extern char *fake_heap_start;
  const unsigned heap_mb = (unsigned)(((char *)g_heap_so_base - fake_heap_start) / (1024 * 1024));
  const char *blocker = engine_blocker(heap_mb);
  LOG("pantalla %dx%d, heap newlib %u MB, region .so %u MB, motor: %s", screen_width, screen_height, heap_mb,
      (unsigned)(g_heap_so_limit >> 20), blocker ? blocker : "disponible");

  prefs_load(path_prefs());
  ui_init(screen_width, screen_height);
  net_init();

  const launch_result res = launcher_run(blocker == NULL, blocker);
  LOG("lanzador: %s", res == LAUNCH_PLAY ? "jugar" : "salir");

  if (res == LAUNCH_PLAY) {
    /* la carga de libuaf.so tarda unos segundos: la ultima imagen presentada queda en pantalla */
    scr_model loading;
    memset(&loading, 0, sizeof loading);
    loading.mode = SCR_BUSY;
    snprintf(loading.status, sizeof loading.status, "Cargando el juego...");
    ui_present(&loading);
    ui_release_stage();                 /* libera la pantalla previa; el contexto EGL queda para el motor */
    cpu_boost(1);
    game_boot(screen_width, screen_height);
    cpu_boost(0);
    while (game_frame()) {}
    game_shutdown();
  }

  net_exit();
  LOG("fin");

  extern void NX_NORETURN __libnx_exit(int rc);
  __libnx_exit(0);
  return 0;
}
