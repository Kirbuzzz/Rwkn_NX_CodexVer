/* log.h -- log a <base>/data/log.txt (se reescribe en cada arranque; seguro entre hilos). */
#ifndef __LOG_H__
#define __LOG_H__

void log_init(const char *path);
void log_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
const char *log_path(void);

#define LOG(...) log_printf(__VA_ARGS__)

#endif
