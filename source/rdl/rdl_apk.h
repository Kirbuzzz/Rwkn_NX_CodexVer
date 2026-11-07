/* Lectura de un .apk (zip) puesto en la SD: extrae libuaf.so y assets/, y lee archivos sueltos. */
#ifndef RDL_APK_H
#define RDL_APK_H
#include <stdbool.h>
#include <stddef.h>
#include "rdl.h"

/* Entrada del apk que contiene el motor. */
#define RDL_APK_LIB_ENTRY "lib/arm64-v8a/libuaf.so"

/* Extrae a:
 *   <app_dir>/lib/libuaf.so
 *   <app_dir>/assets/...        (todo lo que esta bajo assets/ en el apk)
 * Si ya se extrajo este mismo apk (mismo sello), no hace nada y *did_work = false.
 * Comprueba el CRC32 de cada archivo contra el del zip; ignora rutas peligrosas ("..", absolutas).
 * Errores: RDL_ERR_IO (no se pudo abrir el apk / escribir), RDL_ERR_APK (zip invalido, sin libuaf,
 * entrada danada o cifrada), RDL_ERR_SPACE, RDL_ERR_CANCELLED. Progreso: RDL_PHASE_EXTRACT en bytes. */
rdl_result rdl_apk_extract(const char *apk_path, const char *app_dir,
                           rdl_progress_fn cb, void *user, bool *did_work);

/* Lee una entrada en memoria (p. ej. "res/drawable/logorc.png"). Liberar *data con free().
 * Limite: 64 MiB. */
rdl_result rdl_apk_read(const char *apk_path, const char *entry_name, void **data, size_t *len);

#endif
