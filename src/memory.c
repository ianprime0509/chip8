/*
 * Copyright 2018 Ian Johnson
 *
 * This is free software, distributed under the MIT license.  A copy of the
 * license can be found in the LICENSE file in the project root, or at
 * https://opensource.org/licenses/MIT.
 */
#include "memory.h"

#include <stdlib.h>
#include <string.h>

#include "log.h"

void *xcalloc(size_t n, size_t sz)
{
    void *ret;
    if (n == 0 || sz == 0)
        die(2, "xcalloc: zero size allocation");
    ret = calloc(n, sz);
    if (ret)
        return ret;
    else
        die(2,
            "xcalloc: out of memory (allocating %zu objects, each %zu bytes)",
            n, sz);
}

void *xmalloc(size_t sz)
{
    void *ret;
    if (sz == 0)
        die(2, "xmalloc: zero size allocation");
    ret = malloc(sz);
    if (ret)
        return ret;
    else
        die(2, "xmalloc: out of memory (allocating %zu bytes)", sz);
}

void *xrealloc(void *ptr, size_t sz)
{
    void *ret;
    if (sz == 0)
        die(2, "xrealloc: zero size allocation");
    ret = realloc(ptr, sz);
    if (ret)
        return ret;
    else
        die(2, "xrealloc: out of memory (allocating %zu bytes)", sz);
}

char *xstrdup(const char *s)
{
    char *ret = strdup(s);
    if (ret)
        return ret;
    else
        die(2, "xstrdup: out of memory");
}
