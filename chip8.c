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
#include <getopt.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>

#include "audio.h"
#include "interpreter.h"

static const char *HELP =
    "A Chip-8/Super-Chip interpreter.\n"
    "\n"
    "Options:\n"
    "      --frequency=FREQ        set game timer frequency (in Hz)\n"
    "  -q, --shift-quirks          whether to use shift quirks mode\n"
    "  -s, --scale=SCALE           set game display scale\n"
    "  -t, --tone=FREQ             set game buzzer tone (in Hz)\n"
    "      --volume=VOL            set game buzzer volume (0-100)\n"
    "  -h, --help                  show this help message and exit\n"
    "  -V, --version               show version information and exit\n";
static const char *USAGE = "Usage: chip8 [OPTION...] FILE\n";
static const char *VERSION = "chip8 0.1.0\n";

/**
 * Options that can be passed to the program.
 */
struct progopts {
    /**
     * The scale of the display (default 6).
     * One Super-Chip pixel will be displayed as a square of width `scale`.
     */
    int scale;
    /**
     * The frequency (in Hz) of the game timer (default 60).
     */
    long game_freq;
    /**
     * Whether to use shift quirks mode (default false).
     */
    bool shift_quirks;
    /**
     * The frequency (in Hz) of the game beeper (default 440).
     */
    int tone_freq;
    /**
     * The volume (from 0 to 100) of the game beeper (default 10).
     */
    int tone_vol;
    /**
     * The filename of the game to load.
     */
    char *fname;
};

/**
 * The keymap to use in-game.
 * The layout of the original Chip-8 keyboard is as follows:
 * 1 2 3 C
 * 4 5 6 D
 * 7 8 9 E
 * A 0 B F
 * So for now, I just map those to the left side of the keyboard:
 * 1 2 3 4
 * q w e r
 * a s d f
 * z x c v
 */
SDL_Keycode keymap[16] = {
    SDLK_x, SDLK_1, SDLK_2, SDLK_3, SDLK_q, SDLK_w, SDLK_e, SDLK_a,
    SDLK_s, SDLK_d, SDLK_z, SDLK_c, SDLK_4, SDLK_r, SDLK_f, SDLK_v,
};

/**
 * Arguments to be passed to the input thread.
 */
struct input_thrd_args {
    /**
     * The Chip8 key state vector.
     */
    _Atomic uint16_t *key_states;
    atomic_bool *should_exit;
};

/**
 * The SDL audio callback function.
 */
static void audio_callback(void *userdata, uint8_t *stream, int len);
/**
 * Redraws the Chip-8 display onto the given surface.
 */
static void draw(SDL_Surface *surface, struct chip8 *chip, int scale,
                 uint32_t oncolor, uint32_t offcolor);
/**
 * The function to run the input loop in another thread.
 */
static void *input_thrd_func(void *args);
/**
 * Returns the default set of program options.
 */
static struct progopts progopts_default(void);
static int run(struct progopts opts);

int main(int argc, char **argv)
{
    struct progopts opts = progopts_default();
    int option;
    int got_frequency = 0;
    int got_volume = 0;
    const struct option options[] = {
        {"frequency", required_argument, &got_frequency, 1},
        {"shift-quirks", no_argument, NULL, 'q'},
        {"scale", required_argument, NULL, 's'},
        {"tone", required_argument, NULL, 't'},
        {"volume", required_argument, &got_volume, 1},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'V'},
        {0, 0, 0, 0},
    };

    while ((option = getopt_long(argc, argv, "qs:t:hV", options, NULL)) != -1) {
        switch (option) {
        case 'q':
            opts.shift_quirks = true;
            break;
        case 's':
            /* TODO: change atoi to something a little better */
            opts.scale = atoi(optarg);
            break;
        case 't':
            opts.tone_freq = atoi(optarg);
            break;
        case 0:
            /* Long option found */
            if (got_frequency)
                opts.game_freq = atol(optarg);
            else if (got_volume)
                opts.tone_vol = atoi(optarg);
            break;
        case 'h':
            printf("%s%s", USAGE, HELP);
            return 0;
        case 'V':
            printf("%s", VERSION);
            return 0;
        case '?':
            fprintf(stderr, "%s", USAGE);
            return 1;
        }
    }

    if (optind != argc - 1) {
        fprintf(stderr, "%s", USAGE);
        return 1;
    }
    opts.fname = argv[optind];

    return run(opts);
}

static void audio_callback(void *userdata, uint8_t *stream, int len)
{
    struct audio_ring_buffer *ring = (struct audio_ring_buffer *)userdata;

    audio_ring_buffer_fill(ring, (int16_t *)stream, len / 2);
}

static void draw(SDL_Surface *surface, struct chip8 *chip, int scale,
                 uint32_t oncolor, uint32_t offcolor)
{
    int width, height;

    if (chip->highres) {
        width = CHIP8_DISPLAY_WIDTH;
        height = CHIP8_DISPLAY_HEIGHT;
    } else {
        width = CHIP8_DISPLAY_WIDTH / 2;
        height = CHIP8_DISPLAY_HEIGHT / 2;
        scale *= 2;
    }

    for (int i = 0; i < width; i++)
        for (int j = 0; j < height; j++) {
            SDL_Rect rect = {i * scale, j * scale, scale, scale};
            SDL_FillRect(surface, &rect,
                         chip->display[i][j] ? oncolor : offcolor);
        }
}

static void *input_thrd_func(void *args)
{
    SDL_Event e;
    struct input_thrd_args targs = *(struct input_thrd_args *)args;

    while (!*targs.should_exit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                *targs.should_exit = true;
            } else if (e.type == SDL_KEYDOWN) {
                SDL_Keycode key = e.key.keysym.sym;

                for (int i = 0; i < 16; i++)
                    if (key == keymap[i])
                        *targs.key_states |= 1 << i;
            } else if (e.type == SDL_KEYUP) {
                SDL_Keycode key = e.key.keysym.sym;

                for (int i = 0; i < 16; i++)
                    if (key == keymap[i])
                        *targs.key_states &= ~(1 << i);
            }
        }
    }

    return NULL;
}

static struct progopts progopts_default(void)
{
    return (struct progopts){
        .scale = 6,
        .game_freq = 60,
        .shift_quirks = false,
        .tone_freq = 440,
        .tone_vol = 10,
        .fname = NULL,
    };
}

static int run(struct progopts opts)
{
    const int win_width = CHIP8_DISPLAY_WIDTH * opts.scale;
    const int win_height = CHIP8_DISPLAY_HEIGHT * opts.scale;
    SDL_Window *win;
    SDL_Surface *win_surface;
    SDL_AudioDeviceID audio_device;
    SDL_AudioSpec as_want, as_got;
    struct audio_ring_buffer *audio_ring;
    uint32_t oncolor, offcolor;
    pthread_t input_thread;
    struct chip8_options chipopts = chip8_options_default();
    struct chip8 *chip;
    FILE *input;
    atomic_bool should_exit = false;

    /* Set options for the interpreter */
    chipopts.shift_quirks = opts.shift_quirks;
    chipopts.timer_freq = opts.game_freq;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        fprintf(stderr, "Could not initialize SDL: %s\n", SDL_GetError());
        return 1;
    }
    if (!(win = SDL_CreateWindow("Chip-8", SDL_WINDOWPOS_UNDEFINED,
                                 SDL_WINDOWPOS_UNDEFINED, win_width, win_height,
                                 SDL_WINDOW_SHOWN))) {
        fprintf(stderr, "Could not create SDL window: %s\n", SDL_GetError());
        goto error_sdl_initialized;
    }

    /* Set up audio */
    if (!(audio_ring = audio_square_wave(48000, opts.tone_freq,
                                         opts.tone_vol * INT16_MAX / 100))) {
        fprintf(stderr, "Could not create audio ring buffer\n");
        goto error_window_created;
    }
    SDL_zero(as_want);
    as_want.freq = 48000;
    as_want.format = AUDIO_S16SYS;
    as_want.channels = 1;
    as_want.samples = 4096;
    as_want.callback = audio_callback;
    as_want.userdata = audio_ring;
    if (!(audio_device = SDL_OpenAudioDevice(NULL, 0, &as_want, &as_got, 0))) {
        fprintf(stderr, "Could not initialize SDL audio: %s\n", SDL_GetError());
        goto error_audio_ring_created;
    }

    chip = chip8_new(chipopts);
    win_surface = SDL_GetWindowSurface(win);
    oncolor = SDL_MapRGB(win_surface->format, 255, 255, 255);
    offcolor = SDL_MapRGB(win_surface->format, 0, 0, 0);
    draw(win_surface, chip, opts.scale, oncolor, offcolor);
    SDL_UpdateWindowSurface(win);

    if (!(input = fopen(opts.fname, "r"))) {
        fprintf(stderr, "Failed to open game file; aborting\n");
        goto error_chip8_created;
    }
    if (chip8_load_from_file(chip, input)) {
        fprintf(stderr, "Could not load game; aborting\n");
        fclose(input);
        goto error_chip8_created;
    }
    fclose(input);

    /*
     * We run the input loop on a separate thread so that the interpreter
     * doesn't block waiting for input
     */
    if (pthread_create(
            &input_thread, NULL, input_thrd_func,
            &(struct input_thrd_args){
                .key_states = &chip->key_states, .should_exit = &should_exit,
            })) {
        fprintf(stderr, "Failed to spawn timer thread; aborting\n");
        goto error_chip8_created;
    }

    while (!should_exit) {
        chip8_step(chip);
        /* Pause/unpause the audio track as needed */
        SDL_PauseAudioDevice(audio_device, chip->reg_st == 0);
        /* Refresh display as needed */
        if (chip->needs_refresh) {
            draw(win_surface, chip, opts.scale, oncolor, offcolor);
            SDL_UpdateWindowSurface(win);
            chip->needs_refresh = false;
        }
        if (chip->halted) {
            printf("Interpreter was halted\n");
            should_exit = true;
        }
    }

    /* Wait for the input thread to clean up first */
    pthread_join(input_thread, NULL);
    chip8_destroy(chip);
    SDL_CloseAudioDevice(audio_device);
    audio_ring_buffer_free(audio_ring);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;

error_chip8_created:
    chip8_destroy(chip);
    SDL_CloseAudioDevice(audio_device);
error_audio_ring_created:
    audio_ring_buffer_free(audio_ring);
error_window_created:
    SDL_DestroyWindow(win);
error_sdl_initialized:
    SDL_Quit();
    return 1;
}
