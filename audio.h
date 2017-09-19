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
 * Helpful stuff for generating basic audio.
 */
#ifndef CHIP8_AUDIO_H
#define CHIP8_AUDIO_H

#include <stddef.h>
#include <stdint.h>

/**
 * A simple audio ring buffer.
 * This struct just holds some audio data (like a square wave) and can be used
 * repeatedly to fill an audio callback buffer with data. Right now, we use
 * signed 16-bit samples with native endianness and mono output.
 */
struct audio_ring_buffer;

void audio_ring_buffer_free(struct audio_ring_buffer *ring);
/**
 * Fills the given buffer with data from the ring buffer.
 *
 * @param len The length of the buffer to fill, in samples.
 */
void audio_ring_buffer_fill(struct audio_ring_buffer *ring, int16_t *buf,
                            size_t len);

/**
 * Returns a ring buffer containing a square wave.
 */
struct audio_ring_buffer *audio_square_wave(int sample_rate, int frequency,
                                            int16_t volume);

#endif
