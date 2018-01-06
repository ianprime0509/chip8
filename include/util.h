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
/**
 * @file
 * Various simple utility functions.
 */
#ifndef CHIP8_UTIL_H
#define CHIP8_UTIL_H

/**
 * Returns the index of the lowest set bit of the given number.
 *
 * The returned index will be 0-indexed, so that given the input 0xf8 the
 * result will be 3.
 */
unsigned lowest_set_bit(unsigned n);

/**
 * Returns a clock time, in seconds.
 *
 * The start of the clock is unspecified, so the return value is only
 * meaningful when viewed relative to another return value of this same
 * function.
 */
double clock_seconds(void);

#endif
