#ifndef _FILES_WATCHD_LOG_H
#define _FILES_WATCHD_LOG_H

#define LOG_FILE 		"lre.log"

#define LOG_TAG 		"LRE"
#define LOG_BUF_SIZE 	(4096)

#define DEFAULT_LOG_LEVEL 		LOG_INFO
#define DEFAULT_LOG_MODE 		LOG_MODE_STDERR

enum logger_mode {
    LOG_MODE_QUIET,
    LOG_MODE_STDOUT,
    LOG_MODE_FILE,
    LOG_MODE_CLOUD,
    LOG_MODE_CALLBACK,
    LOG_MODE_MAX,
};

enum logger_level {
    LOG_FATAL,
    LOG_ERR,
    LOG_WARN,
    LOG_INFO,
    LOG_DBG,
    LOG_VERBOSE,
    LOG_LEVEL_MAX,
};


#ifdef __cplusplus
extern "C" {
#endif

/* only use for LOG_MODE_FILE mode */
void log_set_logpath(const char *path);
/* only use for LOG_MODE_CALLBACK mode */
void log_set_callback(void (*cb)(int, const char *));

void log_print(int priority, const char *fmt, ...);
void log_init(enum logger_mode mode, enum logger_level level);
void log_release(void);

#define logv(...) log_print(LOG_VERBOSE, __VA_ARGS__)
#define logd(...) log_print(LOG_DBG, __VA_ARGS__)
#define logi(...) log_print(LOG_INFO, __VA_ARGS__)
#define logw(...) log_print(LOG_WARN, __VA_ARGS__)
#define loge(...) log_print(LOG_ERR, __VA_ARGS__)

#define fatal(...) do { log_print(LOG_FATAL, __VA_ARGS__); exit(0); } while(0)



#define __ddd__ 	printf("---%s:%d---\n", __func__, __LINE__);

#ifdef __cplusplus
}
#endif


#endif
