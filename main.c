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
    for (int i = 0; i < CHIP8_DISPLAY_WIDTH; i++)
        for (int j = 0; j < CHIP8_DISPLAY_HEIGHT; j++) {
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
    struct chip8 *chip = chip8_new();
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

    chip->display[2][2] = true;
    chip->display[3][3] = true;
    win_surface = SDL_GetWindowSurface(win);
    oncolor = SDL_MapRGB(win_surface->format, 255, 255, 255);
    offcolor = SDL_MapRGB(win_surface->format, 0, 0, 0);
    draw(win_surface, chip, opts.scale, oncolor, offcolor);
    SDL_UpdateWindowSurface(win);

    while (!should_exit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                should_exit = true;
        }
    }

    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
