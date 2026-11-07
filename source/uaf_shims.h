/* uaf_shims.h -- shims libc/bionic extra que necesita libuaf.so y que gdash_nx no tenia. */
#ifndef __UAF_SHIMS_H__
#define __UAF_SHIMS_H__

#include <stdarg.h>
#include <stddef.h>

int  __android_log_vprint_fake(int prio, const char *tag, const char *fmt, va_list ap);
void __assert_fake(const char *file, int line, const char *msg);
int  __isfinitef_fake(float x);
void _exit_fake(int code);

int   unlink_fake(const char *path);
int   rmdir_fake(const char *path);
int   utimes_fake(const char *path, const void *times);
int   mkstemp_fake(char *tmpl);
char *getcwd_fake(char *buf, size_t size);
int   fchmod_fake(int fd, unsigned mode);
int   fchown_fake(int fd, unsigned uid, unsigned gid);

int pthread_attr_setschedparam_fake(void *attr, const void *param);
int pthread_setschedparam_fake(unsigned long thread, int policy, const void *param);
int pthread_condattr_init_fake(void *attr);
int pthread_condattr_destroy_fake(void *attr);
int sched_get_priority_max_fake(int policy);
int sched_get_priority_min_fake(int policy);

#endif
