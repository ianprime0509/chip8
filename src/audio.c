/*
 * Copyright 2017-2018 Ian Johnson
 *
 * This is free software, distributed under the MIT license.  A copy of the
 * license can be found in the LICENSE file in the project root, or at
 * https://opensource.org/licenses/MIT.
 */
#include "audio.h"

#include <stdlib.h>

#include "memory.h"

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

void audio_ring_buffer_fill(
    struct audio_ring_buffer *ring, int16_t *buf, size_t len)
{
    while (len--) {
        *buf++ = ring->buf[ring->pos++];
        if (ring->pos >= ring->size)
            ring->pos = 0;
    }
}

struct audio_ring_buffer *audio_square_wave(
    int sample_rate, int frequency, int16_t volume)
{
    struct audio_ring_buffer *ring;
    int period = sample_rate / frequency;

    /* We only need our audio buffer to hold a single period */
    ring = xmalloc(sizeof *ring + period * sizeof ring->buf[0]);
    ring->size = period;
    ring->pos = 0;

    /* Populate the buffer with the square wave data */
    for (int i = 0; i < period / 2; i++)
        ring->buf[i] = volume;
    for (int i = period / 2; i < (int)ring->size; i++)
        ring->buf[i] = -volume;

    return ring;
}
