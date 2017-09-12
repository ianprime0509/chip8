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
#include <getopt.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL.h>

#include "interpreter.h"

/**
 * Options that can be passed to the program.
 */
struct progopts {
    int scale;
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
 * Arguments to be passed to the interpreter thread.
 */
struct input_thrd_args {
    /**
     * The Chip8 key state vector
     */
    _Atomic uint16_t *key_states;
    atomic_bool *should_exit;
};

/**
 * Redraws the Chip-8 display onto the given surface.
 */
static void draw(SDL_Surface *surface, struct chip8 *chip, int scale,
                 uint32_t oncolor, uint32_t offcolor);
/**
 * The function to run the input loop in another thread.
 */
static void *input_thrd_func(void *args);
static int run(struct progopts opts);

int main(int argc, char **argv)
{
    struct progopts opts;
    int option;
    const struct option options[] = {
        {"scale", optional_argument, NULL, 's'}, {0, 0, 0, 0},
    };

    /* Set default values for optional arguments */
    opts.scale = 6;

    while ((option = getopt_long(argc, argv, "s", options, NULL)) != -1) {
        switch (option) {
        case 's':
            /* TODO: change atoi to something a little better */
            opts.scale = atoi(optarg);
            break;
        case '?':
            return 1;
        }
    }

    if (optind != argc - 1) {
        fprintf(stderr, "Usage: chip8 [OPTIONS...] FILE\n");
        return 1;
    }
    opts.fname = argv[optind];

    return run(opts);
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

static int run(struct progopts opts)
{
    const int win_width = CHIP8_DISPLAY_WIDTH * opts.scale;
    const int win_height = CHIP8_DISPLAY_HEIGHT * opts.scale;
    SDL_Window *win;
    SDL_Surface *win_surface;
    uint32_t oncolor, offcolor;
    pthread_t input_thread;
    struct chip8 *chip;
    FILE *input;
    atomic_bool should_exit;

    if (SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "Could not initialize SDL: %s\n", SDL_GetError());
        return 1;
    }
    if (!(win = SDL_CreateWindow("Chip-8", SDL_WINDOWPOS_UNDEFINED,
                                 SDL_WINDOWPOS_UNDEFINED, win_width, win_height,
                                 SDL_WINDOW_SHOWN))) {
        fprintf(stderr, "Could not create SDL window: %s\n", SDL_GetError());
        goto error_sdl_initialized;
    }

    chip = chip8_new(chip8_options_default());
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
        goto error_file_opened;
    }

    /*
     * We run the input loop on a separate thread so that the interpreter
     * doesn't block waiting for input
     */
    pthread_create(
        &input_thread, NULL, input_thrd_func,
        &(struct input_thrd_args){
            .key_states = &chip->key_states, .should_exit = &should_exit,
        });

    while (!should_exit) {
        chip8_step(chip);
        if (chip->needs_refresh) {
            draw(win_surface, chip, opts.scale, oncolor, offcolor);
            SDL_UpdateWindowSurface(win);
            chip->needs_refresh = false;
        }
        if (chip->halted) {
            printf("Interpreter was halted\n");
            break;
        }
    }

    /* Wait for the input thread to clean up first */
    pthread_join(input_thread, NULL);
    chip8_destroy(chip);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;

error_file_opened:
    fclose(input);
error_chip8_created:
    chip8_destroy(chip);
    SDL_DestroyWindow(win);
error_sdl_initialized:
    SDL_Quit();
    return 1;
}
