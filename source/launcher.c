/* launcher.c -- ver launcher.h. Logica de pantallas heredada de la app de prueba del descargador. */
#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "config.h"
#include "launcher.h"
#include "log.h"
#include "paths.h"
#include "ui.h"
#include "rdl/rdl.h"
#include "rdl/rdl_apk.h"
#include "rdl/rdl_fs.h"

#define BASE_URL "https://reawaken-lb.throwaway09728.workers.dev"
#define MB(x) ((unsigned long long)((x) / 1000000ULL))

static scr_model g_m;               /* modelo que se esta mostrando */
static char g_warn[120];            /* aviso persistente (p. ej. falta el apk) */
static char g_notice[100];          /* aviso de una sola vez */

/* ----------------------------------------------------------- utilidades */

static u64 now_ms(void) { return armTicksToNs(armGetSystemTick()) / 1000000ULL; }

static bool exists(const char *path) {
  struct stat st;
  return stat(path, &st) == 0;
}

static void set_model(scr_mode mode, const char *status, const char *detail, const char *hint) {
  memset(&g_m, 0, sizeof g_m);
  g_m.mode = mode;
  snprintf(g_m.status, sizeof g_m.status, "%s", status ? status : "");
  snprintf(g_m.detail, sizeof g_m.detail, "%s", detail ? detail : "");
  snprintf(g_m.hint, sizeof g_m.hint, "%s", hint ? hint : "");
}

/* detalle + aviso persistente + aviso de una vez */
static void add_notes(void) {
  size_t n = strlen(g_m.detail);
  const char *extra[2] = { g_warn, g_notice };
  for (int i = 0; i < 2; i++) {
    if (!extra[i][0]) continue;
    snprintf(g_m.detail + n, sizeof g_m.detail - n, "%s%s", n ? "   |   " : "", extra[i]);
    n = strlen(g_m.detail);
  }
  g_notice[0] = 0;
}

static void busy(const char *status, const char *detail) {
  set_model(SCR_BUSY, status, detail, "");
  ui_present(&g_m);
}

/* ------------------------------------------------------------- progreso */

static struct { rdl_phase ph; bool have; u64 t0; uint64_t b0; double speed; u64 last_draw; } g_pg;

static bool progress(rdl_phase ph, uint64_t done, uint64_t total, void *user) {
  (void)user;
  if (ui_poll_buttons() & HidNpadButton_B) return false;

  u64 now = now_ms();
  if (!g_pg.have || g_pg.ph != ph || done < g_pg.b0) {
    g_pg.have = true; g_pg.ph = ph; g_pg.t0 = now; g_pg.b0 = done; g_pg.speed = 0; g_pg.last_draw = 0;
  }
  if (now - g_pg.t0 >= 1000) {                                    /* velocidad suavizada */
    double s = (double)(done - g_pg.b0) / 1000000.0 / ((double)(now - g_pg.t0) / 1000.0);
    g_pg.speed = g_pg.speed > 0 ? g_pg.speed * 0.6 + s * 0.4 : s;
    g_pg.t0 = now; g_pg.b0 = done;
  }
  if (now - g_pg.last_draw < 80 && done != total) return true;    /* ~12 fps */
  g_pg.last_draw = now;

  g_m.mode = SCR_PROGRESS;
  g_m.show_bar = true;
  g_m.frac = total ? (double)done / (double)total : 0.0;
  snprintf(g_m.status, sizeof g_m.status, "%s",
           ph == RDL_PHASE_DOWNLOAD ? "Descargando datos del juego..." :
           ph == RDL_PHASE_VERIFY   ? "Verificando archivos..." : "Preparando archivos del juego...");
  snprintf(g_m.left, sizeof g_m.left, "%llu / %llu MB", MB(done), MB(total));
  snprintf(g_m.center, sizeof g_m.center, "%d %%", (int)(g_m.frac * 100.0));
  g_m.right[0] = 0;
  if (ph == RDL_PHASE_DOWNLOAD && g_pg.speed > 0) snprintf(g_m.right, sizeof g_m.right, "%.1f MB/s", g_pg.speed);
  ui_present(&g_m);
  return true;
}

static void begin_progress(const char *status, const char *detail) {
  g_pg.have = false;
  set_model(SCR_PROGRESS, status, detail, "B: pausar (se puede reanudar)");
  g_m.show_bar = true;
  snprintf(g_m.center, sizeof g_m.center, "0 %%");
  ui_present(&g_m);
}

/* ---------------------------------------------------------------- estado */

/* libuaf.so extraida, assets/ presente */
static bool engine_files_ready(void) {
  uint64_t sz;
  return rdl_fs_size(path_so_game(), &sz) && sz > 1000000 && exists(path_assets());
}

/* obb presente (por nombre fijo), para poder jugar sin manifest */
static bool obb_present_offline(void) {
  uint64_t sz;
  return rdl_fs_size(path_obb_file(), &sz) && sz > 100000000;
}

static const char *kind_name(rdl_obb_kind k) { return k == RDL_OBB_HD ? "HD" : "normal"; }

/* ------------------------------------------------------------------ main */

launch_result launcher_run(bool can_play, const char *why_not) {
  rdl_manifest m;
  rdl_result r = RDL_OK;
  launch_result result = LAUNCH_QUIT;
  bool rdl_ok = false, first_menu = true;
  char tmp[240];
  rdl_config cfg = { BASE_URL, path_ca_user(), "Reawaken", false, rdl_cacert_pem, rdl_cacert_pem_len };

  memset(&m, 0, sizeof m);
  g_warn[0] = g_notice[0] = 0;
  rdl_fs_mkdir_p(path_data());

  busy("Iniciando...", "");
  if (exists(path_apk())) {
    ui_load_background(path_apk());          /* asi todas las pantallas llevan el titulo del juego */
    busy("Iniciando...", "");
  }

  r = rdl_init(&cfg);
  if (r != RDL_OK) {
    LOG("rdl_init: %s", rdl_strerror(r));
    set_model(SCR_ERROR, "No se pudo iniciar el descargador", rdl_strerror(r), "+: salir");
    ui_wait_input(&g_m, 0);
    return LAUNCH_QUIT;
  }
  rdl_ok = true;

  /* 1) apk -> lib/libuaf.so + assets/ */
  if (!exists(path_apk())) {
    snprintf(g_warn, sizeof g_warn, "Falta Reawaken.apk");
    LOG("no hay %s", path_apk());
  } else {
    for (;;) {
      bool did = false;
      begin_progress("Preparando archivos del juego...", "");
      r = rdl_apk_extract(path_apk(), path_base(), progress, NULL, &did);
      LOG("apk: %s did=%d", rdl_strerror(r), did);
      if (r == RDL_OK) break;
      if (r == RDL_ERR_CANCELLED) { snprintf(g_warn, sizeof g_warn, "Apk sin extraer (cancelado)"); break; }
      set_model(SCR_ERROR, "No se pudo leer Reawaken.apk", rdl_strerror(r),
                "A: reintentar    X: continuar sin extraer    +: salir");
      u64 b = ui_wait_input(&g_m, HidNpadButton_A | HidNpadButton_X);
      if (b & HidNpadButton_Plus) goto done;
      if (b & HidNpadButton_X) { snprintf(g_warn, sizeof g_warn, "Apk sin extraer"); break; }
    }
  }

  /* 2) manifest (red): reintenta hasta conseguirlo, o juega sin conexion si ya esta todo */
  {
    char cache[320];
    snprintf(cache, sizeof cache, "%s/manifest.cache.json", path_data());
    for (;;) {
      busy("Conectando con el servidor...", g_warn);
      r = rdl_fetch_manifest(&m, cache);
      if (r == RDL_OK) break;
      LOG("manifest: %s | %s", rdl_strerror(r), rdl_last_error());
      snprintf(tmp, sizeof tmp, "%s", rdl_last_error()[0] ? rdl_last_error() : rdl_strerror(r));
      const bool offline_ok = can_play && engine_files_ready() && obb_present_offline();
      set_model(SCR_ERROR, r == RDL_ERR_MANIFEST ? "El servidor respondio algo inesperado" : "No se pudo conectar con el servidor",
                tmp, offline_ok ? "A: reintentar    X: jugar sin conexion    +: salir" : "A: reintentar    +: salir");
      u64 b = ui_wait_input(&g_m, HidNpadButton_A | (offline_ok ? HidNpadButton_X : 0));
      if (b & HidNpadButton_Plus) goto done;
      if (b & HidNpadButton_X) { result = LAUNCH_PLAY; goto done; }
    }
  }
  LOG("manifest ok (cache=%d) normal=%llu hd=%llu", m.from_cache, MB(m.normal.size), MB(m.hd.size));

  /* 3) menu de datos del juego */
  for (;;) {
    rdl_obb_kind kind = rdl_obb_installed(&m, path_obb_dir());
    bool have = (kind == RDL_OBB_NORMAL || kind == RDL_OBB_HD);
    bool ready = have && engine_files_ready();
    bool play = ready && can_play;

    if (play && first_menu && config.autoplay) { result = LAUNCH_PLAY; goto done; }
    first_menu = false;

    if (have) {
      const rdl_obb_info *cur = kind == RDL_OBB_HD ? &m.hd : &m.normal;
      snprintf(tmp, sizeof tmp, "Instalado: %s (%llu MB)", kind_name(kind), MB(cur->size));
      set_model(SCR_DONE, ready ? "\xC2\xA1" "Todo listo para jugar!" : "\xC2\xA1" "Datos del juego listos!", tmp, "");
      const char *other = kind == RDL_OBB_HD ? "normal" : "HD";
      const bool can_switch = kind == RDL_OBB_HD ? true : m.hd.valid;
      if (play)
        snprintf(g_m.hint, sizeof g_m.hint, can_switch ? "A: jugar    X: verificar    Y: cambiar a %s    +: salir"
                                                       : "A: jugar    X: verificar    +: salir", other);
      else
        snprintf(g_m.hint, sizeof g_m.hint, can_switch ? "X: verificar    Y: cambiar a %s    +: salir" : "X: verificar    +: salir", other);
      if (ready && !can_play && why_not)
        snprintf(g_notice, sizeof g_notice, "%s", why_not);
      g_m.show_bar = true;
      g_m.frac = 1.0;
    } else {
      if (m.hd.valid)
        snprintf(tmp, sizeof tmp, "Normal: %llu MB   |   HD: %llu MB", MB(m.normal.size), MB(m.hd.size));
      else
        snprintf(tmp, sizeof tmp, "Normal: %llu MB", MB(m.normal.size));
      set_model(SCR_CHOOSE,
                kind == RDL_OBB_OTHER ? "Los datos del juego estan incompletos o son de otra version"
                                      : "Faltan los datos del juego (obb)",
                tmp, m.hd.valid ? "A: descargar normal    Y: descargar HD    +: salir" : "A: descargar    +: salir");
    }
    if (m.from_cache) {
      size_t n = strlen(g_m.detail);
      snprintf(g_m.detail + n, sizeof g_m.detail - n, "   |   Sin conexion (manifest guardado)");
    }
    add_notes();

    u64 btn = ui_wait_input(&g_m, HidNpadButton_A | HidNpadButton_Y | HidNpadButton_X);
    if (btn & HidNpadButton_Plus) break;

    const rdl_obb_info *pick = NULL;
    bool verify_only = false;
    if (have) {
      if ((btn & HidNpadButton_A) && play) { result = LAUNCH_PLAY; goto done; }
      if (btn & HidNpadButton_Y) {
        if (kind == RDL_OBB_HD) pick = &m.normal;
        else if (m.hd.valid)    pick = &m.hd;
      }
      if (btn & HidNpadButton_X) { pick = kind == RDL_OBB_HD ? &m.hd : &m.normal; verify_only = true; }
    } else {
      if (btn & HidNpadButton_A) pick = &m.normal;
      if ((btn & HidNpadButton_Y) && m.hd.valid) pick = &m.hd;
    }
    if (!pick) continue;

    snprintf(tmp, sizeof tmp, "%s (%llu MB)", pick == &m.hd ? "Version HD" : "Version normal", MB(pick->size));
    begin_progress(verify_only ? "Verificando archivos..." : "Descargando datos del juego...", tmp);
    r = verify_only ? rdl_obb_verify(pick, path_obb_dir(), progress, NULL)
                    : rdl_obb_download(pick, path_obb_dir(), progress, NULL);
    LOG("%s: %s | %s", verify_only ? "verificar" : "descargar", rdl_strerror(r), rdl_last_error());

    if (r == RDL_OK) {
      snprintf(g_notice, sizeof g_notice, verify_only ? "Verificacion correcta" : "Descarga completa");
    } else if (r == RDL_ERR_CANCELLED) {
      snprintf(g_notice, sizeof g_notice, "Pausado: pulsa de nuevo para reanudar");
    } else {
      snprintf(tmp, sizeof tmp, "%s", rdl_last_error()[0] ? rdl_last_error() : "");
      set_model(SCR_ERROR, rdl_strerror(r), tmp, "A: volver    +: salir");
      if (ui_wait_input(&g_m, HidNpadButton_A) & HidNpadButton_Plus) break;
    }
  }

done:
  if (rdl_ok) rdl_exit();
  return result;
}
