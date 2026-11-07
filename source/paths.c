/* paths.c -- see paths.h.
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "config.h" // SO_NAME, APK_NAME, CONFIG_NAME
#include "paths.h"

#define OBB_FILE_NAME "main.2086.com.ubisoft.raymanadventures.obb"

static char s_base[256], s_apk[288], s_assets[288], s_data[288], s_obb_dir[288], s_obb_file[320];
static char s_prefs[300], s_log[300], s_config[288], s_ca_bundle[300], s_ca_user[288], s_so_game[300];

// quita el prefijo "dispositivo:" y el ultimo componente "/<archivo>"
static void dir_of(const char *in, char *out, size_t n) {
  const char *colon = strchr(in, ':');
  snprintf(out, n, "%s", colon ? colon + 1 : in);
  char *slash = strrchr(out, '/');
  if (slash)
    *slash = '\0';
}

void paths_init(const char *argv0) {
  s_base[0] = '\0';
  if (argv0 && argv0[0])
    dir_of(argv0, s_base, sizeof s_base);
  if (!s_base[0]) {
    char cwd[256];
    if (getcwd(cwd, sizeof cwd)) {
      const char *colon = strchr(cwd, ':');
      snprintf(s_base, sizeof s_base, "%s", colon ? colon + 1 : cwd);
    }
  }
  if (!s_base[0])
    snprintf(s_base, sizeof s_base, "/switch/Reaweaken");
  if (s_base[0] != '/') { // el motor solo trata como ruta de archivo las que empiezan por '/'
    char tmp[256];
    snprintf(tmp, sizeof tmp, "/%s", s_base);
    snprintf(s_base, sizeof s_base, "%s", tmp);
  }
  snprintf(s_apk,       sizeof s_apk,       "%s/%s", s_base, APK_NAME);
  snprintf(s_assets,    sizeof s_assets,    "%s/assets", s_base);
  snprintf(s_data,      sizeof s_data,      "%s/data", s_base);
  snprintf(s_obb_dir,   sizeof s_obb_dir,   "%s/obb", s_base);
  snprintf(s_obb_file,  sizeof s_obb_file,  "%s/%s", s_obb_dir, OBB_FILE_NAME);
  snprintf(s_prefs,     sizeof s_prefs,     "%s/prefs.txt", s_data);
  snprintf(s_log,       sizeof s_log,       "%s/log.txt", s_data);
  snprintf(s_config,    sizeof s_config,    "%s/%s", s_base, CONFIG_NAME);
  snprintf(s_ca_bundle, sizeof s_ca_bundle, "%s/ca-firmware.pem", s_data);
  snprintf(s_ca_user,   sizeof s_ca_user,   "%s/cacert.pem", s_base);
  snprintf(s_so_game,   sizeof s_so_game,   "%s/lib/%s", s_base, SO_NAME);
}

const char *path_base(void)      { return s_base; }
const char *path_apk(void)       { return s_apk; }
const char *path_assets(void)    { return s_assets; }
const char *path_data(void)      { return s_data; }
const char *path_save(void)      { return s_data; }
const char *path_obb_dir(void)   { return s_obb_dir; }
const char *path_obb_file(void)  { return s_obb_file; }
const char *path_prefs(void)     { return s_prefs; }
const char *path_log(void)       { return s_log; }
const char *path_config(void)    { return s_config; }
const char *path_ca_bundle(void) { return s_ca_bundle; }
const char *path_ca_user(void)   { return s_ca_user; }
const char *path_so_game(void)   { return s_so_game; }

const char *path_android_private_suffix(const char *path) {
  static const char *roots[] = {
    "/data/data/",
    "/data/user/0/",
    "/data/user_de/0/",
    "/storage/emulated/0/Android/data/",
    "/sdcard/Android/data/",
  };
  if (!path)
    return NULL;
  for (unsigned i = 0; i < sizeof(roots) / sizeof(*roots); i++) {
    const size_t root_len = strlen(roots[i]);
    if (strncmp(path, roots[i], root_len) != 0)
      continue;
    const char *package = path + root_len;
    if (!package[0] || package[0] == '/')
      return NULL;
    const char *suffix = strchr(package, '/');
    if (!suffix)
      return package + strlen(package);
    while (*suffix == '/')
      suffix++;
    return suffix;
  }
  return NULL;
}
