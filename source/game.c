/* game.c -- ver game.h
 *
 * Replica lo que hace la capa Java de Rayman Adventures (GameActivity + GL2JNIView.Renderer), en este orden:
 *   GameActivity.initEngine:        InitGlobal(activity), InitMobileSDK, ordenes genericas, setupDeviceInfo
 *   GL2JNIView.init:                callGenericNativeMethod("setAppRootPath", ...)
 *   Renderer.onSurfaceCreated:      initNativeEngine(apk, raiz, obb, assets), init(w, h, locale, dpi),
 *                                   setNativeScreenResolution, setDeviceOrientation, setDeviceTouchOrientation
 *   Renderer.onDrawFrame:           step()           (una vez por frame)
 * La entrada (touch / mando) la gestiona input.c.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <switch.h>

#include "config.h"
#include "error.h"
#include "game.h"
#include "imports.h"
#include "input.h"
#include "jni_fake.h"
#include "log.h"
#include "modmenu.h"
#include "modmenu_compat.h"
#include "opensl_nx.h"
#include "paths.h"
#include "pthr.h"
#include "so_util.h"
#include "ui.h"
#include "util.h"

void *g_heap_so_base = NULL;
size_t g_heap_so_limit = 0;

static so_module game_mod;
static AppletHookCookie s_applet_hook;
static volatile int s_app_focused = 1;

/* ----- funciones nativas de UAFJNILib (todas: JNIEnv*, jclass, args...) ----- */
static struct {
  void (*InitGlobal)(void *env, void *cls, void *activity);
  void (*InitMobileSDK)(void *env, void *cls);
  void (*callGeneric)(void *env, void *cls, void *jname, int i, float f, void *jstr);
  void (*setupDeviceInfo)(void *env, void *cls, void *model, void *code, void *model_l, void *code_l, void *assets);
  void (*initNativeEngine)(void *env, void *cls, void *apk, void *root, void *obb, void *assets);
  void (*init)(void *env, void *cls, int w, int h, void *locale, int dpi);
  void (*setNativeScreenResolution)(void *env, void *cls, int w, int h);
  void (*setDeviceOrientation)(void *env, void *cls, int a, int b);
  void (*setDeviceTouchOrientation)(void *env, void *cls, int a, int b);
  void (*step)(void *env, void *cls);
  void (*pause)(void *env, void *cls);
  void (*resume)(void *env, void *cls);
  void (*resumeAudio)(void *env, void *cls);
  void (*exitsystem)(void *env, void *cls);
  void (*setDeviceBackPressed)(void *env, void *cls);
  void (*touch)(void *env, void *cls, float x, float y, float x2, float y2, int zero, int pointer, int action);
  void (*nativeSetPadAxisValues)(void *env, void *cls, int index, float x, float y);
  void (*setGamePadConnected)(void *env, void *cls, int connected);
} uaf;

static int (*game_JNI_OnLoad)(void *vm, void *reserved);

#define RESOLVE(field, name) uaf.field = (void *)so_find_addr_rx(&game_mod, "Java_com_ubisoft_uaf_UAFJNILib_" name)

static void resolve_exports(void) {
  RESOLVE(InitGlobal, "InitGlobal");
  RESOLVE(InitMobileSDK, "InitMobileSDK");
  RESOLVE(callGeneric, "callGenericNativeMethod");
  RESOLVE(setupDeviceInfo, "setupDeviceInfo");
  RESOLVE(initNativeEngine, "initNativeEngine");
  RESOLVE(init, "init");
  RESOLVE(setNativeScreenResolution, "setNativeScreenResolution");
  RESOLVE(setDeviceOrientation, "setDeviceOrientation");
  RESOLVE(setDeviceTouchOrientation, "setDeviceTouchOrientation");
  RESOLVE(step, "step");
  RESOLVE(pause, "pause");
  RESOLVE(resume, "resume");
  RESOLVE(resumeAudio, "resumeAudio");
  RESOLVE(exitsystem, "exitsystem");
  RESOLVE(setDeviceBackPressed, "setDeviceBackPressed");
  RESOLVE(touch, "touch");
  RESOLVE(nativeSetPadAxisValues, "nativeSetPadAxisValues");
  RESOLVE(setGamePadConnected, "setGamePadConnected");
  game_JNI_OnLoad = (void *)so_try_find_addr_rx(&game_mod, "JNI_OnLoad");
}

static void generic(const char *name, int i, float f, const char *s) {
  uaf.callGeneric(fake_env, NULL, jni_make_string(name), i, f, jni_make_string(s));
}

/* ------------------------------------------------------------- foco (HOME) */

static void applet_focus_hook(AppletHookType type, void *param) {
  (void)param;
  if (type != AppletHookType_OnFocusState)
    return;
  const int focused = (appletGetFocusState() == AppletFocusState_InFocus);
  if (focused && !s_app_focused) {
    s_app_focused = 1;
    uaf.resume(fake_env, NULL);
    uaf.resumeAudio(fake_env, NULL);
    LOG("foco: reanudado");
  } else if (!focused && s_app_focused) {
    s_app_focused = 0;
    uaf.pause(fake_env, NULL);
    LOG("foco: pausado");
  }
}

/* ------------------------------------------------------------------- boot */

void game_boot(int w, int h) {
  LOG("cargando %s en %p (%u MB disponibles)", SO_NAME, g_heap_so_base, (unsigned)(g_heap_so_limit >> 20));
  if (so_load(&game_mod, path_so_game(), g_heap_so_base, g_heap_so_limit) < 0)
    fatal_error("No se pudo cargar\n%s.", SO_NAME);

  update_imports();
  so_relocate(&game_mod);
  so_resolve(&game_mod, dynlib_functions, dynlib_numfunctions, 1);
  resolve_exports();

  /* OpenSSL 1.0.2 va dentro de libuaf: OPENSSL_cpuid_setup (constructor estatico) sondea la CPU con un arnes
   * SIGILL/longjmp que aqui fallaria sin entrega de senales; se neutraliza y OpenSSL usa C portable. */
  {
    uintptr_t cpuid = so_try_find_addr(&game_mod, "OPENSSL_cpuid_setup");
    if (cpuid)
      hook_arm64(cpuid, (uintptr_t)&ret0);
  }

  so_finalize(&game_mod);
  so_flush_caches(&game_mod);

  /* el TLS falso debe existir antes de cualquier codigo del juego (los constructores leen la cookie de la pila) */
  pthr_install_fake_tls();
  so_execute_init_array(&game_mod);
  so_free_temp(&game_mod);
  LOG("init_array ejecutado");

  jni_init();
  opensl_nx_install();                        /* el audio se abre cuando Wwise empiece a reproducir */

  if (game_JNI_OnLoad)
    game_JNI_OnLoad(fake_vm, NULL);
  LOG("JNI_OnLoad ok");

  /* --- GameActivity.initEngine --- */
  void *activity = jni_make_object("com/ubisoft/uaf/GameActivity");
  void *assets = jni_make_object("android/content/res/AssetManager");

  if (!modmenu_compat_attach("libuaf.so",
          "Java_com_ubisoft_uaf_UAFJNILib_InitGlobal", uaf.InitGlobal)) {
    fatal_error("No se pudo preparar el adaptador Mod Menu / InitGlobal.");
  }
  modmenu_compat_call_initglobal(fake_env, NULL, activity);
  LOG("InitGlobal ok");
  uaf.InitMobileSDK(fake_env, NULL);
  LOG("InitMobileSDK ok");

  generic("FPSMANAGER_LOG_TO_FILE", 0, 0.f, "Version Code:2086, OS:21");
  generic("setDeviceModel", 0, 0.f, "Nintendo Switch");
  generic("setDeviceCodeName", 0, 0.f, "nx");
  generic("setOSVersion", 0, 0.f, "21");
  generic("setCodeVersion", 0, 0.f, "2086");
  generic("setMultiAPKVersion", 0, 0.f, "2086");
  generic("setConnectionStatus", 1, 0.f, "");
  generic("setPlayLicensing", 0, 0.f, "");
  generic("setEngineVersion", 277216, 0.f, "277216");
  generic("setDataVersion", 127897, 0.f, "127897");

  uaf.setupDeviceInfo(fake_env, NULL, jni_make_string("Nintendo Switch"), jni_make_string("nx"),
                      jni_make_string("nintendo_switch"), jni_make_string("nx"), assets);
  LOG("setupDeviceInfo ok");

  /* --- GL2JNIView.init / Renderer.onSurfaceCreated --- */
  generic("setAppRootPath", 0, 0.f, path_data());
  uaf.initNativeEngine(fake_env, NULL, jni_make_string(path_apk()), jni_make_string(path_data()),
                       jni_make_string(path_obb_file()), assets);
  LOG("initNativeEngine ok (apk=%s obb=%s)", path_apk(), path_obb_file());

  uaf.init(fake_env, NULL, w, h, jni_make_string(config_locale()), 320);
  LOG("init(%d, %d, %s) ok", w, h, config_locale());
  uaf.setNativeScreenResolution(fake_env, NULL, w, h);
  uaf.setDeviceOrientation(fake_env, NULL, w, h);
  uaf.setDeviceTouchOrientation(fake_env, NULL, w, h);
  generic("setAppInstaller", 0, 0.f, "");
  LOG("arranque del motor completo");
  modmenu_compat_engine_ready();

  InputSink sink = {
    .touch = uaf.touch,
    .pad_axis = uaf.nativeSetPadAxisValues,
    .pad_connected = uaf.setGamePadConnected,
    .back_pressed = uaf.setDeviceBackPressed,
  };
  input_init(w, h, &sink);
  if (!modmenu_init(w, h))
    LOG("ModMenu: no se pudo inicializar; el juego continua sin menu");
  appletHook(&s_applet_hook, applet_focus_hook, NULL);
}

/* ------------------------------------------------------------------ frame */

int game_frame(void) {
  if (!s_app_focused) {                 /* en segundo plano (HOME): no se dibuja */
    svcSleepThread(16000000);
    return appletMainLoop() && !g_quit_requested;
  }
  if (!appletMainLoop() || g_quit_requested)
    return 0;
  input_set_game_input_blocked(modmenu_update());
  if (!input_update())
    return 0;

  uaf.step(fake_env, NULL);
  input_render_cursor();
  modmenu_render();
  eglSwapBuffers(ui_egl_display(), ui_egl_surface());

  static int frames;
  if (++frames == 1 || frames == 60 || frames == 600)
    LOG("frame %d", frames);
  return 1;
}

void game_shutdown(void) {
  modmenu_exit();
  modmenu_compat_shutdown();
  LOG("cerrando");
  if (s_app_focused && uaf.pause)
    uaf.pause(fake_env, NULL);
}
