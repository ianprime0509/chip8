/*
 * Copyright 2018 Ian Johnson
 *
 * This file is part of Chip-8.
 *
 * Chip-8 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Chip-8 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Chip-8.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "log.h"

#include <stdarg.h>

/**
 * The current maximum log level.
 */
static enum log_level max_level;
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
