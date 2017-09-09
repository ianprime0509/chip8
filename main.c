#include <stdio.h>

#include <SDL2/SDL.h>

int main(int argc, char **argv)
{
    SDL_Window *win;

    if (SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "Could not initialize SDL: %s\n", SDL_GetError());
        return 1;
    }
    if (!(win = SDL_CreateWindow("Test", SDL_WINDOWPOS_UNDEFINED,
                                 SDL_WINDOWPOS_UNDEFINED, 128, 64,
                                 SDL_WINDOW_SHOWN))) {
        fprintf(stderr, "Could not show SDL window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Delay(5000);

    SDL_Quit();
    return 0;
}
