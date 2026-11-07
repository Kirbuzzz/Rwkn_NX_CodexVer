/* error.c -- error handler
 *
 * Copyright (C) 2021 fgsfds, Andy Nguyen
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <switch.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "util.h"
#include "log.h"
#include "ui.h"
#include "error.h"

void fatal_error(const char *fmt, ...) {
  char msg[512];
  va_list list;
  va_start(list, fmt);
  vsnprintf(msg, sizeof msg, fmt, list);
  va_end(list);

  LOG("FATAL: %s", msg);

  /* con EGL ya iniciado, la consola de libnx no se puede usar: pantalla grafica */
  if (!ui_fatal(msg)) {
    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    consoleInit(NULL);
    printf("%s\n\nPulsa A para salir.", msg);
    consoleUpdate(NULL);

    while (appletMainLoop()) {
      padUpdate(&pad);
      const u64 keys = padGetButtonsDown(&pad);
      if (keys & HidNpadButton_A) break;
    }
    consoleExit(NULL);
  }
  exit(1);
}
