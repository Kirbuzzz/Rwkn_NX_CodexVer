/* launcher.h -- pantalla previa: extrae el apk, baja el manifest y descarga/verifica el obb. */
#ifndef __LAUNCHER_H__
#define __LAUNCHER_H__

#include <stdbool.h>

typedef enum { LAUNCH_QUIT, LAUNCH_PLAY } launch_result;

/* Requiere ui_init() y net_init() hechos. `can_play` = hay memoria para el motor; si es false,
 * `why_not` (texto corto) se muestra en vez de ofrecer "jugar". */
launch_result launcher_run(bool can_play, const char *why_not);

#endif
