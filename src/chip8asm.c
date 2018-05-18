/*
 * Copyright 2017-2018 Ian Johnson
 *
 * This is free software, distributed under the MIT license.  A copy of the
 * license can be found in the LICENSE file in the project root, or at
 * https://opensource.org/licenses/MIT.
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
#include "memory.h"

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
        {"verbose", no_argument, NULL, 'v'}, {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'V'}, {0, 0, 0, 0}};
    int retval = 0;

    log_init(argc >= 1 ? argv[0] : "chip8asm", stderr, LOG_WARNING);

    while ((option = getopt_long(argc, argv, "o:qvhV", options, NULL)) != -1) {
        switch (option) {
        case 'o':
            free(opts.output);
            opts.output = xstrdup(optarg);
            break;
        case 'q':
            opts.shift_quirks = true;
            break;
        case 'v':
            opts.verbosity++;
            break;
        case 'h':
            printf("%s%s", USAGE, HELP);
            goto EXIT;
        case 'V':
            printf("%s", VERSION_STRING);
            goto EXIT;
        case '?':
            fprintf(stderr, "%s", USAGE);
            retval = 1;
            goto EXIT;
        }
    }

    if (optind == argc - 1) {
        opts.input = xstrdup(argv[optind]);
    } else if (optind == argc) {
        opts.input = xstrdup("-");
    } else {
        fprintf(stderr, "%s", USAGE);
        retval = 1;
        goto EXIT;
    }

    /* We try to deduce the output file name if it is not given */
    if (!opts.output) {
        opts.output = !strcmp(opts.input, "-") ? xstrdup("-")
                                               : replace_extension(opts.input);
    }

    retval = run(opts);

EXIT:
    free(opts.output);
    free(opts.input);
    return retval;
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
    size_t extlen = strlen(OUTPUTEXT);
    size_t buflen = extpos + extlen + 1;
    char *buf = xmalloc(buflen);

    memcpy(buf, fname, extpos);
    memcpy(buf + extpos, OUTPUTEXT, extlen);
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
    if (opts.verbosity == 1)
        log_set_level(LOG_INFO);
    else if (opts.verbosity >= 2)
        log_set_level(LOG_DEBUG);

    asmopts.shift_quirks = opts.shift_quirks;
    chipasm = chip8asm_new(asmopts);
    prog = chip8asm_program_new();

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
        log_error(
            "Could not open output file for writing: %s", strerror(errno));
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
    chip8asm_destroy(chipasm);
    return retval;
}
