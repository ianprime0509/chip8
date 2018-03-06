/*
 * Copyright 2017-2018 Ian Johnson
 *
 * This is free software, distributed under the MIT license.  A copy of the
 * license can be found in the LICENSE file in the project root, or at
 * https://opensource.org/licenses/MIT.
 */
#include "util.h"

#include <time.h>

unsigned lowest_set_bit(unsigned n)
{
    unsigned b = 0;
    while ((n & 1) == 0) {
        b++;
        n >>= 1;
    }
    return b;
}

double clock_seconds(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec + (double)ts.tv_nsec / 1e9;
}
