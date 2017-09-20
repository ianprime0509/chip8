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
/**
 * @file
 * An assembler for the Chip-8 and Super-Chip.
 */

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assembler.h"

/**
 * The size of the temporary input line buffer.
 */
#define MAXLINE 500
/**
 * The output extension to use by default.
 */
#define OUTPUTEXT ".bin"

static const char *HELP =
    "An assembler for Chip-8/Super-Chip programs.\n"
    "The assembler will read from standard input if no "
    "FILE is provided, or if FILE is '-'.\n"
    "\n"
    "Options:\n"
    "  -o, --output=OUTPUT    set output file name\n"
    "  -q, --shift-quirks     enable shift quirks mode\n"
    "  -h, --help             show this help message and exit\n"
    "  -V, --version          show version information and exit\n";
static const char *USAGE = "chip8asm [OPTION...] [FILE]\n";
static const char *VERSION = "chip8asm 0.1.0\n";

/**
 * Options which can be passed to the program.
 */
struct progopts {
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

int main(int argc, char **argv)
{
    int option;
    struct progopts opts = progopts_default();
    const struct option options[] = {{"output", required_argument, NULL, 'o'},
                                     {"shift-quirks", no_argument, NULL, 'q'},
                                     {"help", no_argument, NULL, 'h'},
                                     {"version", no_argument, NULL, 'V'},
                                     {0, 0, 0, 0}};
    char *extension;

    while ((option = getopt_long(argc, argv, "o:hV", options, NULL)) != -1) {
        switch (option) {
        case 'o':
            if (opts.output)
                free(opts.output);
            opts.output = strdup(optarg);
            break;
        case 'q':
            opts.shift_quirks = true;
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

    if (optind == argc - 1) {
        opts.input = strdup(argv[optind]);
    } else if (optind == argc) {
        opts.input = strdup("-");
    } else {
        fprintf(stderr, "%s", USAGE);
        return 1;
    }

    /* We try to deduce the output file name if it is not given */
    if (!opts.output) {
        if (!strcmp(opts.input, "-")) {
            opts.output = strdup("-");
        } else if (!(extension = strrchr(opts.input, '.'))) {
            /* No extension given */
            opts.output = malloc(strlen(opts.input) + strlen(OUTPUTEXT) + 1);
            strcpy(opts.output, opts.input);
            strcat(opts.output, OUTPUTEXT);
        } else {
            /* Replace extension */
            int extpos = extension - opts.input;

            opts.output = malloc(extpos + strlen(OUTPUTEXT) + 1);
            memcpy(opts.output, opts.input, extpos);
            strcpy(opts.output + extpos, OUTPUTEXT);
        }
    }

    return run(opts);
}

static struct progopts progopts_default(void)
{
    return (struct progopts){
        .shift_quirks = false, .input = NULL, .output = NULL};
}

static int run(struct progopts opts)
{
    struct chip8asm *chipasm;
    struct chip8asm_options asmopts;
    struct chip8asm_program *prog;
    FILE *input, *output;
    char str[MAXLINE];
    int err;
    int retval = 0;

    asmopts.shift_quirks = opts.shift_quirks;

    if (!(chipasm = chip8asm_new(asmopts))) {
        perror("Could not create assembler");
        retval = 1;
        goto EXIT_NOTHING_CREATED;
    }
    if (!(prog = chip8asm_program_new())) {
        perror("Could not allocate space for program buffer");
        retval = 1;
        goto EXIT_CHIPASM_CREATED;
    }

    if (!strcmp(opts.input, "-")) {
        input = stdin;
    } else if (!(input = fopen(opts.input, "r"))) {
        perror("Could not open input file for reading");
        retval = 1;
        goto EXIT_PROG_CREATED;
    }

    while (fgets(str, MAXLINE, input)) {
        if ((err = chip8asm_process_line(chipasm, str))) {
            fprintf(stderr, "Could not process input file; aborting\n");
            retval = 1;
            goto EXIT_INPUT_OPENED;
        }
    }
    /* Make sure there wasn't an error in reading */
    if (ferror(input)) {
        perror("Error reading from input file");
        retval = 1;
        goto EXIT_INPUT_OPENED;
    }

    if ((err = chip8asm_emit(chipasm, prog))) {
        fprintf(stderr, "Assembler second pass failed; aborting\n");
        retval = 1;
        goto EXIT_INPUT_OPENED;
    }

    if (!strcmp(opts.output, "-")) {
        output = stdout;
    } else if (!(output = fopen(opts.output, "w"))) {
        perror("Could not open output file for writing");
        retval = 1;
        goto EXIT_INPUT_OPENED;
    }

    fwrite(prog->mem, 1, prog->len, output);
    if (ferror(output)) {
        perror("Error writing to output file");
        retval = 1;
        goto EXIT_OUTPUT_OPENED;
    }

EXIT_OUTPUT_OPENED:
    fclose(output);
EXIT_INPUT_OPENED:
    fclose(input);
EXIT_PROG_CREATED:
    chip8asm_program_destroy(prog);
EXIT_CHIPASM_CREATED:
    chip8asm_destroy(chipasm);
EXIT_NOTHING_CREATED:
    free(opts.input);
    free(opts.output);
    return retval;
}
