/*
 * Copyright 2018 Ian Johnson
 *
 * This is free software, distributed under the MIT license.  A copy of the
 * license can be found in the LICENSE file in the project root, or at
 * https://opensource.org/licenses/MIT.
 */
/**
 * @file
 * Additional memory management functions.
 */
#ifndef CHIP8_MEMORY_H
#define CHIP8_MEMORY_H

#include <stddef.h>

void *xcalloc(size_t n, size_t sz);
void *xmalloc(size_t sz);
void *xrealloc(void *ptr, size_t sz);
char *xstrdup(const char *s);

#endif
