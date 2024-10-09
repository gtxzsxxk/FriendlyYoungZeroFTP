//
// Created by hanyuan on 2024/10/9.
//

#ifndef SERVER_LOGGER_H
#define SERVER_LOGGER_H

#include <stdarg.h>

void logger_info(const char *fmt, ...);

void logger_err(const char *fmt, ...);

#endif //SERVER_LOGGER_H
