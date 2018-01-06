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
#include <config.h>

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assembler.h"
#include "log.h"

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
    "  -v, --verbose          increase verbosity\n"
    "  -h, --help             show this help message and exit\n"
    "  -V, --version          show version information and exit\n";
static const char *USAGE = "chip8asm [OPTION...] [FILE]\n";
static const char *VERSION_STRING = "chip8asm " PROJECT_VERSION "\n";

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
/**
 * Replaces the file extension of the given filename with that of the output
 * format. This will replace the extension if there is one, or append it if
 * there isn't. The returned string will be allocated on the heap, and should
 * be freed by the caller when done.
 */
static char *replace_extension(const char *fname);
static int run(struct progopts opts);

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
    } else if (optind == argc) {
        opts.input = strdup("-");
    } else {
        fprintf(stderr, "%s", USAGE);
        return 1;
    }

    /* We try to deduce the output file name if it is not given */
    if (!opts.output) {
        opts.output = !strcmp(opts.input, "-") ? strdup("-")
                                               : replace_extension(opts.input);
    }

    return run(opts);
}

static struct progopts progopts_default(void)
{
    return (struct progopts){
        .verbosity = 0, .shift_quirks = false, .input = NULL, .output = NULL};
}

static char *replace_extension(const char *fname)
{
    const char *ext = strrchr(fname, '.');
    size_t extpos = ext ? ext - fname : strlen(fname);
    size_t buflen = extpos + strlen(OUTPUTEXT) + 1;
    char *buf = malloc(buflen);

    memcpy(buf, fname, extpos);
    strcpy(buf + extpos, OUTPUTEXT);
    buf[buflen - 1] = '\0';

    return buf;
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

    /* Set up logging */
    if (opts.verbosity == 0)
        log_init(stderr, LOG_WARNING);
    else if (opts.verbosity == 1)
        log_init(stderr, LOG_INFO);
    else
        log_init(stderr, LOG_DEBUG);

    asmopts.shift_quirks = opts.shift_quirks;

    if (!(chipasm = chip8asm_new(asmopts))) {
        log_error("Could not create assembler: %s", strerror(errno));
        retval = 1;
        goto EXIT_NOTHING_CREATED;
    }
    if (!(prog = chip8asm_program_new())) {
        log_error("Could not allocate space for program buffer: %s",
                  strerror(errno));
        retval = 1;
        goto EXIT_CHIPASM_CREATED;
    }

    if (!strcmp(opts.input, "-")) {
        input = stdin;
    } else if (!(input = fopen(opts.input, "r"))) {
        log_error("Could not open input file for reading: %s", strerror(errno));
        retval = 1;
        goto EXIT_PROG_CREATED;
    }

    while (fgets(str, MAXLINE, input)) {
        if ((err = chip8asm_process_line(chipasm, str))) {
            log_error("Could not process input file; aborting");
            retval = 1;
            goto EXIT_INPUT_OPENED;
        }
    }
    /* Make sure there wasn't an error in reading */
    if (ferror(input)) {
        log_error("Error reading from input file: %s", strerror(errno));
        retval = 1;
        goto EXIT_INPUT_OPENED;
    }

    if ((err = chip8asm_emit(chipasm, prog))) {
        log_error("Assembler second pass failed; aborting");
        retval = 1;
        goto EXIT_INPUT_OPENED;
    }

    if (!strcmp(opts.output, "-")) {
        output = stdout;
    } else if (!(output = fopen(opts.output, "w"))) {
        log_error("Could not open output file for writing: %s",
                  strerror(errno));
        retval = 1;
        goto EXIT_INPUT_OPENED;
    }

    fwrite(prog->mem, 1, prog->len, output);
    if (ferror(output)) {
        log_error("Error writing to output file: %s", strerror(errno));
        retval = 1;
        goto EXIT_OUTPUT_OPENED;
    }

EXIT_OUTPUT_OPENED:
    if (output != stdout)
        fclose(output);
EXIT_INPUT_OPENED:
    if (input != stdin)
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
