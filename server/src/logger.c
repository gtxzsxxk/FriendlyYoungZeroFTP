//
// Created by hanyuan on 2024/10/9.
//

#include "logger.h"
#include <stdio.h>
#include <time.h>
#include <pthread.h>

typedef enum {
    LOGGER_INFO,
    LOGGER_ERR
} log_level_t;

/* 多个线程都会进行log，所以需要上锁 */
/* 但是一般也不会log，所以不用考虑性能 */
static pthread_mutex_t lock;

/* 获取当前时间的字符串 */
static const char *get_current_time() {
    static char buffer[20];
    time_t t;
    struct tm *tm_info;

    time(&t);
    tm_info = localtime(&t);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);

    return buffer;
}

/* 打印日志的核心函数 */
static void logger_log(log_level_t level, const char *format, va_list args) {
    pthread_mutex_lock(&lock);
    const char *level_str = (level == LOGGER_INFO) ? "INFO" : "ERROR";
    printf("[%s] [%s] ", get_current_time(), level_str);
    vprintf(format, args);
    printf("\n");
    pthread_mutex_unlock(&lock);
}

void logger_info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    logger_log(LOGGER_INFO, format, args);
    va_end(args);
}

void logger_err(const char *format, ...) {
    va_list args;
    va_start(args, format);
    logger_log(LOGGER_ERR, format, args);
    va_end(args);
}

void logger_init(void) {
    pthread_mutex_init(&lock, NULL);
    setbuf(stdout, NULL);
}

void logger_exit(void) {
    pthread_mutex_destroy(&lock);
}
