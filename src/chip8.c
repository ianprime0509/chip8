/*
 * Copyright 2017-2018 Ian Johnson
 *
 * This is free software, distributed under the MIT license.  A copy of the
 * license can be found in the LICENSE file in the project root, or at
 * https://opensource.org/licenses/MIT.
 */
#include <config.h>

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>

#include "audio.h"
#include "interpreter.h"
#include "log.h"

static const char *HELP =
    "A Chip-8/Super-Chip interpreter.\n"
    "\n"
    "Options:\n"
    "      --frequency=FREQ        set game timer frequency (in Hz)\n"
    "  -l, --load-quirks           enable load quirks mode\n"
    "  -q, --shift-quirks          enable shift quirks mode\n"
    "  -s, --scale=SCALE           set game display scale\n"
    "  -t, --tone=FREQ             set game buzzer tone (in Hz)\n"
    "  -v, --verbose               increase verbosity\n"
    "      --volume=VOL            set game buzzer volume (0-100)\n"
    "  -h, --help                  show this help message and exit\n"
    "  -V, --version               show version information and exit\n";
static const char *USAGE = "Usage: chip8 [OPTION...] FILE\n";
static const char *VERSION_STRING = "chip8 " PROJECT_VERSION "\n";

/**
 * Options that can be passed to the program.
 */
struct progopts {
    /**
     * The output verbosity (default 0).
     *
     * The higher this is, the more log messages will be output (currently,
     * anything over 2 won't give any extra messages).
     */
    int verbosity;
    /**
     * The scale of the display (default 6).
     *
     * One Super-Chip pixel will be displayed as a square of width `scale`.
     */
    int scale;
    /**
     * The frequency (in Hz) of the game timer (default 60).
     */
    long game_freq;
    /**
     * Whether to use load quirks mode (default false).
     */
    bool load_quirks;
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
 *
 * The layout of the original Chip-8 keyboard is as follows:
 * 1 2 3 C
 * 4 5 6 D
 * 7 8 9 E
 * A 0 B F
 *
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
 * The SDL audio callback function.
 */
static void audio_callback(void *userdata, uint8_t *stream, int len);
/**
 * Redraws the Chip-8 display onto the given surface.
 */
static void draw(SDL_Surface *surface, struct chip8 *chip, uint32_t oncolor,
                 uint32_t offcolor);
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
        {"load-quirks", no_argument, NULL, 'l'},
        {"shift-quirks", no_argument, NULL, 'q'},
        {"scale", required_argument, NULL, 's'},
        {"tone", required_argument, NULL, 't'},
        {"verbose", no_argument, NULL, 'v'},
        {"volume", required_argument, &got_volume, 1},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'V'},
        {0, 0, 0, 0},
    };

    log_init(stderr, LOG_WARNING);

    while ((option = getopt_long(argc, argv, "lqs:t:vhV", options, NULL)) !=
           -1) {
        char *numend;

        switch (option) {
        case 'l':
            opts.load_quirks = true;
            break;
        case 'q':
            opts.shift_quirks = true;
            break;
        case 's':
            errno = 0;
            opts.scale = strtoul(optarg, &numend, 10);
            if (errno != 0) {
                log_error("Error processing scale: %s", strerror(errno));
                return 1;
            } else if (*numend != '\0') {
                log_error("Scale arguemnt '%s' is invalid", optarg);
                return 1;
            }
            break;
        case 't':
            errno = 0;
            opts.tone_freq = strtoul(optarg, &numend, 10);
            if (errno != 0) {
                log_error("Error processing frequency: %s", strerror(errno));
                return 1;
            } else if (*numend != '\0') {
                log_error("Frequency arguemnt '%s' is invalid", optarg);
                return 1;
            }
            break;
        case 'v':
            opts.verbosity++;
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
            printf("%s", VERSION_STRING);
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

static void draw(SDL_Surface *surface, struct chip8 *chip, uint32_t oncolor,
                 uint32_t offcolor)
{
    int xscale = surface->w / CHIP8_DISPLAY_WIDTH;
    int yscale = surface->h / CHIP8_DISPLAY_HEIGHT;

    if (!chip->highres) {
        xscale *= 2;
        yscale *= 2;
    }

    for (int i = 0; i < CHIP8_DISPLAY_WIDTH; i++)
        for (int j = 0; j < CHIP8_DISPLAY_HEIGHT; j++) {
            SDL_Rect rect = {i * xscale, j * yscale, xscale, yscale};
            SDL_FillRect(surface, &rect,
                         chip->display[i][j] ? oncolor : offcolor);
        }
}

static struct progopts progopts_default(void)
{
    return (struct progopts){
        .verbosity = 0,
        .scale = 6,
        .game_freq = 60,
        .load_quirks = false,
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
    struct chip8_options chipopts = chip8_options_default();
    struct chip8 *chip;
    FILE *input;
    SDL_Event e;
    bool should_exit = false;
    int retval = 0;

    /* Set correct log level */
    if (opts.verbosity == 1)
        log_set_level(LOG_INFO);
    else if (opts.verbosity >= 2)
        log_set_level(LOG_DEBUG);

    /* Set options for the interpreter */
    chipopts.load_quirks = opts.load_quirks;
    chipopts.shift_quirks = opts.shift_quirks;
    chipopts.timer_freq = opts.game_freq;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        log_error("Could not initialize SDL: %s", SDL_GetError());
        retval = 1;
        goto ERROR_NOTHING_INITIALIZED;
    }
    if (!(win = SDL_CreateWindow("Chip-8", SDL_WINDOWPOS_UNDEFINED,
                                 SDL_WINDOWPOS_UNDEFINED, win_width, win_height,
                                 SDL_WINDOW_SHOWN))) {
        log_error("Could not create SDL window: %s", SDL_GetError());
        retval = 1;
        goto ERROR_SDL_INITIALIZED;
    }

    /* Set up audio */
    if (!(audio_ring = audio_square_wave(48000, opts.tone_freq,
                                         opts.tone_vol * INT16_MAX / 100))) {
        log_error("Could not create audio ring buffer");
        retval = 1;
        goto ERROR_WINDOW_CREATED;
    }
    SDL_zero(as_want);
    as_want.freq = 48000;
    as_want.format = AUDIO_S16SYS;
    as_want.channels = 1;
    as_want.samples = 4096;
    as_want.callback = audio_callback;
    as_want.userdata = audio_ring;
    if (!(audio_device = SDL_OpenAudioDevice(NULL, 0, &as_want, &as_got, 0))) {
        log_error("Could not initialize SDL audio: %s", SDL_GetError());
        retval = 1;
        goto ERROR_AUDIO_RING_CREATED;
    }

    chip = chip8_new(chipopts);
    win_surface = SDL_GetWindowSurface(win);
    oncolor = SDL_MapRGB(win_surface->format, 255, 255, 255);
    offcolor = SDL_MapRGB(win_surface->format, 0, 0, 0);
    draw(win_surface, chip, oncolor, offcolor);
    SDL_UpdateWindowSurface(win);

    if (!(input = fopen(opts.fname, "r"))) {
        log_error("Failed to open game file; aborting");
        retval = 1;
        goto ERROR_CHIP8_CREATED;
    }
    if (chip8_load_from_file(chip, input)) {
        log_error("Could not load game; aborting");
        fclose(input);
        retval = 1;
        goto ERROR_CHIP8_CREATED;
    }
    fclose(input);

    while (!should_exit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                should_exit = true;
            } else if (e.type == SDL_WINDOWEVENT) {
                log_debug("Window changed; getting new surface and refreshing");
                /*
                 * We need to force the window to refresh when something
                 * happens to it (e.g. it gets moved or resized) even if the
                 * interpreter hasn't gotten any new display information.
                 */
                chip->needs_refresh = true;
                /*
                 * We also need to get a new window surface, since the old one
                 * is now invalid.
                 */
                win_surface = SDL_GetWindowSurface(win);
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
        if (chip8_step(chip)) {
            log_error("Shutting down interpreter");
            retval = 1;
            goto ERROR_CHIP8_CREATED;
        }
        /* Pause/unpause the audio track as needed */
        SDL_PauseAudioDevice(audio_device, chip->reg_st == 0);
        /* Refresh display as needed */
        if (chip->needs_refresh) {
            draw(win_surface, chip, oncolor, offcolor);
            if (SDL_UpdateWindowSurface(win))
                log_error("Could not update window surface: %s",
                          SDL_GetError());
            chip->needs_refresh = false;
        }
        if (chip->halted) {
            log_info("Interpreter was halted");
            should_exit = true;
        }
    }

ERROR_CHIP8_CREATED:
    chip8_destroy(chip);
    SDL_CloseAudioDevice(audio_device);
ERROR_AUDIO_RING_CREATED:
    audio_ring_buffer_free(audio_ring);
ERROR_WINDOW_CREATED:
    SDL_DestroyWindow(win);
ERROR_SDL_INITIALIZED:
    SDL_Quit();
ERROR_NOTHING_INITIALIZED:
    return retval;
}
