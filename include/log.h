/*
 * Copyright 2018 Ian Johnson
 *
 * This is free software, distributed under the MIT license.  A copy of the
 * license can be found in the LICENSE file in the project root, or at
 * https://opensource.org/licenses/MIT.
 */
/**
 * @file
 * General logging utilities.
 */
#ifndef CHIP8_LOG_H
#define CHIP8_LOG_H

#include <stdio.h>

/**
 * The level of a log message.
 *
 * The lower the level, the more urgent the message is.  There is always a
 * maximum log level in effect; any log messages with a level higher than this
 * maximum will be suppressed.
 */
enum log_level {
    LOG_ERROR,
    LOG_WARNING,
    LOG_INFO,
    LOG_DEBUG,
    LOG_TRACE,
};

/**
 * Initializes the logging system.
 *
 * Until this function is called, you won't see any log messages, but nothing
 * (else) bad will happen.
 *
 * @param progname The name of the program to display in messages.
 * @param output The output file to use for log messages.
 * @param max The initial maximum log level to display.
 */
void log_init(const char *progname, FILE *output, enum log_level max);
/**
 * Returns the maximum log level.
 */
enum log_level log_get_level(void);
/**
 * Sets the maximum log level.
 *
 * Any log messages which are generated with a level exceeding the maximum
 * (that is, which are less urgent than the given level) will be suppressed.
 */
void log_set_level(enum log_level max);
/**
 * Sets the output file to which log messages are written.
 *
 * If the specified output file is NULL, the log messages will not be output
 * anywhere.
 */
void log_set_output(FILE *output);

/**
 * Logs a formatted message using the logging system.
 *
 * The format string should use the same format as those used for the printf
 * family of functions, but the final newline and other niceties will be
 * provided for you (so in particular you shouldn't be putting any newlines in
 * yourself).
 */
void log_message(enum log_level level, const char *fmt, ...);
/**
 * Begins a multi-part log message.
 */
void log_message_begin(enum log_level level);
/**
 * Logs part of a multi-part log message.
 */
void log_message_part(const char *fmt, ...);
/**
 * Ends a multi-part log message.
 */
void log_message_end(void);
/**
 * Logs an error message.
 */
#define log_error(...) log_message(LOG_ERROR, __VA_ARGS__)
/**
 * Logs a warning message.
 */
#define log_warning(...) log_message(LOG_WARNING, __VA_ARGS__)
/**
 * Logs an info message.
 */
#define log_info(...) log_message(LOG_INFO, __VA_ARGS__)
/**
 * Logs a debug message.
 */
#define log_debug(...) log_message(LOG_DEBUG, __VA_ARGS__)
/**
 * Logs a trace message.
 */
#define log_trace(...) log_message(LOG_TRACE, __VA_ARGS__)

/**
 * Exits the program immediately with the given exit status and message.
 */
#define die(status, ...)                                                       \
    do {                                                                       \
        log_error(__VA_ARGS__);                                                \
        exit((status));                                                        \
    } while (0)

#endif
