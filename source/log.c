/* log.c -- ver log.h */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <switch.h>

#include "log.h"

static FILE *s_f;
static Mutex s_mutex;
static char s_path[320];
static u64 s_t0;

void log_init(const char *path) {
  snprintf(s_path, sizeof s_path, "%s", path);
  s_t0 = armTicksToNs(armGetSystemTick());
  s_f = fopen(path, "w");
}

const char *log_path(void) { return s_path; }

void log_printf(const char *fmt, ...) {
  if (!s_f)
    return;
  char buf[900];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);

  const u64 ms = (armTicksToNs(armGetSystemTick()) - s_t0) / 1000000ULL;
  mutexLock(&s_mutex);
  fprintf(s_f, "[%6llu.%03llu] %s", (unsigned long long)(ms / 1000), (unsigned long long)(ms % 1000), buf);
  if (buf[0] && buf[strlen(buf) - 1] != '\n')
    fputc('\n', s_f);
  fflush(s_f);            /* si el juego se cuelga, el log llega hasta la ultima linea */
  mutexUnlock(&s_mutex);
}
