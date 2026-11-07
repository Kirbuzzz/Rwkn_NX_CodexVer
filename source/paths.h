/* paths.h -- runtime file layout, resolved from the .nro's own location so the
 * game folder can be named anything (not tied to a fixed /switch/<name>/).
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef PATHS_H
#define PATHS_H

// Resolve the base directory from argv[0] (else the cwd). Call once at startup.
void paths_init(const char *argv0);

const char *path_base(void);          // directorio del .nro
const char *path_apk(void);           // "<base>/Reawaken.apk"
const char *path_assets(void);        // "<base>/assets"      (extraido del apk; lee AAssetManager)
const char *path_data(void);          // "<base>/data"        (equivale a getExternalFilesDir: partidas, logs, cache)
const char *path_save(void);          // = path_data()        (nombre historico, usado por libc_shim)
const char *path_obb_dir(void);       // "<base>/obb"
const char *path_obb_file(void);      // "<base>/obb/main.2086.com.ubisoft.raymanadventures.obb"
const char *path_prefs(void);         // "<base>/data/prefs.txt"
const char *path_log(void);           // "<base>/data/log.txt"
const char *path_config(void);        // "<base>/config.txt"
const char *path_ca_bundle(void);     // "<base>/data/ca-firmware.pem" (exportado del firmware para el HTTPS del motor)
const char *path_ca_user(void);       // "<base>/cacert.pem"           (el que usa el descargador; opcional)
const char *path_so_game(void);       // "<base>/lib/libuaf.so"

// Return the part after an Android app-private package directory, or NULL
// when path is not under a recognized private-data root. The returned pointer
// aliases path and may point at its trailing NUL for the package directory.
const char *path_android_private_suffix(const char *path);

#endif
