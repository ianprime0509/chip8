/*
 * Copyright 2018 Ian Johnson
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
#include <config.h>

#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "disassembler.h"
#include "log.h"

static const char *HELP =
    "A disassembler for Chip-8/Super-Chip programs.\n"
    "\n"
    "Options:\n"
    "  -o, --output=OUTPUT    set output file name\n"
    "  -q, --shift-quirks     enable shift quirks mode\n"
    "  -v, --verbose          increase verbosity\n"
    "  -h, --help             show this help message and and exit\n"
    "  -V, --version          show version information and exit\n";
static const char *USAGE = "chip8disasm [OPTION...] FILE\n";
static const char *VERSION_STRING = "chip8disasm " PROJECT_VERSION "\n";

/**
 * Options which can be passed to the program.
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
     * Whether to use shift quirks mode (default false).
     */
    bool shift_quirks;
    /**
     * The output file name.
     *
     * For consistency, this should be heap-allocated memory.
     */
    char *output;
    /**
     * The input file name.
     *
     * For consistency, this should be heap-allocated memory.
     */
    char *input;
};

static struct progopts progopts_default(void);
static int run(struct progopts opts);

static struct progopts progopts_default(void)
{
    return (struct progopts){
        .verbosity = 0, .shift_quirks = false, .input = NULL, .output = NULL};
}

int main(int argc, char **argv)
{
    int option;
    struct progopts opts = progopts_default();
    const struct option options[] = {{"output", required_argument, NULL, 'o'},
                                     {"shift-quirks", no_argument, NULL, 'q'},
                                     {"verbose", no_argument, NULL, 'v'},
                                     {"help", no_argument, NULL, 'h'},
                                     {"version", no_argument, NULL, 'V'},
                                     {0, 0, 0, 0}};

    while ((option = getopt_long(argc, argv, "o:qvhV", options, NULL)) != -1) {
        switch (option) {
        case 'o':
            if (opts.output)
                free(opts.output);
            opts.output = strdup(optarg);
            break;
        case 'q':
            opts.shift_quirks = true;
            break;
        case 'v':
            opts.verbosity++;
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

    if (optind == argc - 1) {
        opts.input = strdup(argv[optind]);
    } else {
        fprintf(stderr, "%s", USAGE);
        return 1;
    }

    /* Use stdout as default output */
    if (!opts.output)
        opts.output = strdup("-");

    return run(opts);
}

static int run(struct progopts opts)
{
    struct chip8disasm *disasm;
    int retval = 0;

    if (!(disasm = chip8disasm_from_file(opts.input))) {
        log_error("Could not create disassembler");
        retval = 1;
        goto EXIT_NOTHING_DONE;
    }

    chip8disasm_destroy(disasm);
EXIT_NOTHING_DONE:
    free(opts.input);
    free(opts.output);
    return retval;
}
