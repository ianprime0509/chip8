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
    bool should_exit;

    if (SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "Could not initialize SDL: %s\n", SDL_GetError());
        return 1;
    }
    if (!(win = SDL_CreateWindow("Chip-8", SDL_WINDOWPOS_UNDEFINED,
                                 SDL_WINDOWPOS_UNDEFINED, win_width, win_height,
                                 SDL_WINDOW_SHOWN))) {
        fprintf(stderr, "Could not create SDL window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    chip = chip8_new();
    win_surface = SDL_GetWindowSurface(win);
    oncolor = SDL_MapRGB(win_surface->format, 255, 255, 255);
    offcolor = SDL_MapRGB(win_surface->format, 0, 0, 0);
    draw(win_surface, chip, opts.scale, oncolor, offcolor);
    SDL_UpdateWindowSurface(win);

    chip->regs[REG_V0] = 0x000F;
    chip->mem[0x200] = 0xF0;
    chip->mem[0x201] = 0x29;
    chip->mem[0x202] = 0xD1;
    chip->mem[0x203] = 0x25;
    chip->mem[0x204] = 0x12;
    chip->mem[0x205] = 0x04;

    while (!should_exit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                should_exit = true;
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
}
