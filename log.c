#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#include "log.h"
#include "utils.h"

/* The message mode refers to where informational messages go
   0 - stdout, 1 - syslog, 2 - quiet. The default is quiet. */
static enum logger_mode log_mode = LOG_MODE_QUIET;
static enum logger_level log_level = LOG_DBG;

static FILE *logfp = NULL;  /* Only use for LOG_MODE_FILE */
static void (*log_cbprint)(int, const char *) = NULL;

static char level_tags[LOG_LEVEL_MAX + 1] = {
    'F', 'E', 'W', 'I', 'D', 'V',
};

static const char *log_mode_str[LOG_MODE_MAX + 1] = {
    [LOG_MODE_QUIET] = "quiet",
    [LOG_MODE_STDOUT] = "stdout",
    [LOG_MODE_FILE] = "file",
    [LOG_MODE_CLOUD] = "cloud",
    [LOG_MODE_CALLBACK] = "callback",
};

__attribute__((weak)) const char *get_log_path(void)
{
    return LOG_FILE;
}

void default_cbprint(int level, const char *log)
{
    fputc(level_tags[level], stdout);
    fputc(':', stdout);
    fputs(log, stdout);
    fputc('\n', stdout);
    fflush(stdout);
}

void log_set_logpath(const char *path)
{
    if (logfp)
        fclose(logfp);

    logfp = fopen(path, "a+");
    if (!logfp) {
        log_mode = LOG_MODE_STDOUT;
    }
    logv("log file:%s", path);
}

void log_set_callback(void (*cb)(int, const char *))
{
    log_cbprint = cb;
}

void log_print(int level, const char *fmt, ...)
{
    int len;
    va_list ap;
    char buf[LOG_BUF_SIZE];
    time_t now;
    char timestr[32];

    if (log_mode == LOG_MODE_QUIET)
        return;

    if (level > log_level || level < 0)
        return;

    time(&now);
    strftime(timestr, 32, "%Y-%m-%d %H:%M:%S", localtime(&now));

    len = xsnprintf(buf, LOG_BUF_SIZE, "%s(%s)/[%c] ",
                    timestr, LOG_TAG, level_tags[level]);
    va_start(ap, fmt);
    vsnprintf(buf + len, LOG_BUF_SIZE - len, fmt, ap);
    va_end(ap);

    if (log_mode == LOG_MODE_FILE) {
        fputs(buf, logfp);
        fputs("\n", logfp);
        fflush(logfp);
    } else if (log_mode == LOG_MODE_CLOUD) {
        /* Skip Tag */
    } else if (log_mode == LOG_MODE_CALLBACK) {
        if (log_cbprint)
            log_cbprint(level, buf + len);
    } else {
        fputs(buf, stdout);
        fputc('\n', stdout);
        fflush(stdout);
    }
}

void log_init(enum logger_mode mode, enum logger_level level)
{
    log_mode = mode;
    log_level = level;

    if (log_level > LOG_LEVEL_MAX || log_level < 0)
        log_level = DEFAULT_LOG_LEVEL;

    if (log_mode >= LOG_MODE_MAX || log_mode < 0)
        log_mode = DEFAULT_LOG_LEVEL;

    if (log_mode == LOG_MODE_FILE && !logfp) {
        logfp = fopen(get_log_path(), "a+");
        if (!logfp) {
            log_mode = LOG_MODE_STDOUT;
        }
        logv("log file:%s", get_log_path());
    } else if (log_mode == LOG_MODE_CALLBACK && !log_cbprint) {
        log_cbprint = default_cbprint;
    }
    logi("log init. mode:%s, level:%c", log_mode_str[mode], level_tags[log_level]);
}


void log_release(void)
{
    if (log_mode == LOG_MODE_FILE) {
        fclose(logfp);
    }
}

