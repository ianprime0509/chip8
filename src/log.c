/*
 * Copyright 2017-2018 Ian Johnson
 *
 * This is free software, distributed under the MIT license.  A copy of the
 * license can be found in the LICENSE file in the project root, or at
 * https://opensource.org/licenses/MIT.
 */
#include "log.h"

#include <stdarg.h>

/**
 * The current maximum log level.
 */
static enum log_level max_level;
/**
 * The current message log level.
 *
 * This is undefined if there is no message currently being logged; a message
 * should always be started with the log_message_begin function.
 */
static enum log_level message_level;
/**
 * The current log output file.
 */
static FILE *log_output;

/**
 * Returns a string with the name of the given log level.
 */
static char *log_level_string(enum log_level level);

void log_init(FILE *output, enum log_level max)
{
    log_set_output(output);
    log_set_level(max);
    log_message(LOG_DEBUG, "Logging initialized");
}

void log_set_level(enum log_level max)
{
    max_level = max;
}

void log_set_output(FILE *output)
{
    log_output = output;
}

static char *log_level_string(enum log_level level)
{
    switch (level) {
    case LOG_ERROR:
        return "ERROR";
    case LOG_WARNING:
        return "WARNING";
    case LOG_INFO:
        return "INFO";
    case LOG_DEBUG:
        return "DEBUG";
    default:
        return "UNKNOWN";
    }
}

void log_message(enum log_level level, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    if (log_output != NULL && level <= max_level) {
        fprintf(log_output, "%s: ", log_level_string(level));
        vfprintf(log_output, fmt, args);
        putc('\n', log_output);
    }
    va_end(args);
}

void log_message_begin(enum log_level level)
{
    message_level = level;
    if (log_output != NULL && message_level <= max_level)
        fprintf(log_output, "%s: ", log_level_string(level));
}

void log_message_part(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    if (log_output != NULL && message_level <= max_level)
        vfprintf(log_output, fmt, args);
    va_end(args);
}

void log_message_end(void)
{
    if (log_output != NULL && message_level <= max_level)
        putc('\n', log_output);
}
