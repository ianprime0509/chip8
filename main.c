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
 */
SDL_Keycode keymap[16] = {
    SDLK_0, SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5, SDLK_6, SDLK_7,
    SDLK_8, SDLK_9, SDLK_a, SDLK_b, SDLK_c, SDLK_d, SDLK_e, SDLK_f,
};

/**
 * Redraws the Chip-8 display onto the given surface.
 */
static void draw(SDL_Surface *surface, struct chip8 *chip, int scale,
                 uint32_t oncolor, uint32_t offcolor);
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

static int run(struct progopts opts)
{
    const int win_width = CHIP8_DISPLAY_WIDTH * opts.scale;
    const int win_height = CHIP8_DISPLAY_HEIGHT * opts.scale;
    SDL_Window *win;
    SDL_Event e;
    SDL_Surface *win_surface;
    uint32_t oncolor, offcolor;
    struct chip8 *chip;
    FILE *input;
    bool should_exit;

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

    chip = chip8_new();
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

    while (!should_exit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                should_exit = true;
            } else if (e.type == SDL_KEYDOWN) {
                SDL_Keycode key = e.key.keysym.sym;

                for (int i = 0; i < 16; i++)
                    if (key == keymap[i])
                        chip->key_states |= 1 << i;
            } else if (e.type == SDL_KEYUP) {
                SDL_Keycode key = e.key.keysym.sym;

                for (int i = 0; i < 16; i++)
                    if (key == keymap[i])
                        chip->key_states &= ~(1 << i);
            }
        }
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
