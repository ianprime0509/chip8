/*
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

#include "audio.h"

#include <stdlib.h>

struct audio_ring_buffer {
    /**
     * The current buffer position.
     */
    size_t pos;
    /**
     * The size of the audio buffer, in bytes.
     */
    size_t size;
    /**
     * The audio buffer itself (flexible array member).
     */
    int16_t buf[];
};

void audio_ring_buffer_free(struct audio_ring_buffer *ring)
{
    free(ring);
}

void audio_ring_buffer_fill(struct audio_ring_buffer *ring, int16_t *buf,
                            size_t len)
{
    while (len--) {
        *buf++ = ring->buf[ring->pos++];
        if (ring->pos >= ring->size)
            ring->pos = 0;
    }
}

struct audio_ring_buffer *audio_square_wave(int sample_rate, int frequency,
                                            int16_t volume)
{
    struct audio_ring_buffer *ring;
    int period = sample_rate / frequency;

    /* We only need our audio buffer to hold a single period */
    if (!(ring = malloc(sizeof *ring + period * sizeof ring->buf[0])))
        return NULL;
    ring->size = period;
    ring->pos = 0;

    /* Populate the buffer with the square wave data */
    for (int i = 0; i < period / 2; i++)
        ring->buf[i] = volume;
    for (int i = period / 2; i < (int)ring->size; i++)
        ring->buf[i] = -volume;

    return ring;
}
