/*
 * Copyright 2017 Ian Johnson
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
#include "util.h"

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
    return (double)clock() / CLOCKS_PER_SEC;
}
