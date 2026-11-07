/* uaf_shims.c -- ver uaf_shims.h */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "error.h"
#include "libc_shim.h"
#include "log.h"
#include "paths.h"
#include "uaf_shims.h"

/* el motor registra muchisimo: solo se vuelca al log con log_imports >= 2 */
int __android_log_vprint_fake(int prio, const char *tag, const char *fmt, va_list ap) {
  (void)prio;
  if (config.log_imports >= 2) {
    char buf[512];
    vsnprintf(buf, sizeof buf, fmt ? fmt : "", ap);
    LOG("[android:%s] %s", tag ? tag : "?", buf);
  }
  return 0;
}

void __assert_fake(const char *file, int line, const char *msg) {
  fatal_error("El motor fallo una asercion:\n%s:%d\n%s", file ? file : "?", line, msg ? msg : "?");
}

int __isfinitef_fake(float x) { return isfinite(x) ? 1 : 0; }

/* bionic _exit: termina sin pasar por atexit; aqui basta exit() (cierra el log) */
void _exit_fake(int code) { exit(code); }

/* ---- archivos: misma traduccion de rutas de Android que el resto de wrappers ---- */

int unlink_fake(const char *path) { return unlink(fix_path(path)); }
int rmdir_fake(const char *path)  { return rmdir(fix_path(path)); }

int utimes_fake(const char *path, const void *times) { (void)path; (void)times; return 0; }

int mkstemp_fake(char *tmpl) {
  if (!tmpl) return -1;
  char buf[512];
  snprintf(buf, sizeof buf, "%s", fix_path(tmpl));
  const int fd = mkstemp(buf);
  if (fd >= 0) {
    const size_t a = strlen(tmpl), b = strlen(buf);
    if (a >= 6 && b >= 6)
      memcpy(tmpl + a - 6, buf + b - 6, 6);   /* devuelve el nombre elegido, como hace libc */
  }
  return fd;
}

char *getcwd_fake(char *buf, size_t size) {
  if (!buf || !size) return NULL;
  snprintf(buf, size, "%s", path_data());
  return buf;
}

int fchmod_fake(int fd, unsigned mode) { (void)fd; (void)mode; return 0; }
int fchown_fake(int fd, unsigned uid, unsigned gid) { (void)fd; (void)uid; (void)gid; return 0; }

/* ---- hilos: Horizon no da control de prioridad POSIX; se acepta y se ignora ---- */

int pthread_attr_setschedparam_fake(void *attr, const void *param) { (void)attr; (void)param; return 0; }
int pthread_setschedparam_fake(unsigned long thread, int policy, const void *param) { (void)thread; (void)policy; (void)param; return 0; }
int pthread_condattr_init_fake(void *attr) { if (attr) memset(attr, 0, sizeof(unsigned)); return 0; }
int pthread_condattr_destroy_fake(void *attr) { (void)attr; return 0; }
int sched_get_priority_max_fake(int policy) { (void)policy; return 99; }
int sched_get_priority_min_fake(int policy) { (void)policy; return 0; }
