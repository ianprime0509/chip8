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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "disassembler.h"
#include "log.h"
#include "memory.h"

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

int main(int argc, char **argv)
{
    int option;
    struct progopts opts = progopts_default();
    const struct option options[] = {{"output", required_argument, NULL, 'o'},
        {"shift-quirks", no_argument, NULL, 'q'},
        {"verbose", no_argument, NULL, 'v'}, {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'V'}, {0, 0, 0, 0}};
    int retval = 0;

    log_init(argv[0], stderr, LOG_WARNING);

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
    } else {
        fprintf(stderr, "%s", USAGE);
        retval = 1;
        goto EXIT;
    }

    /* Use stdout as default output */
    if (!opts.output)
        opts.output = xstrdup("-");

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

static int run(struct progopts opts)
{
    struct chip8disasm *disasm;
    struct chip8disasm_options disopts = chip8disasm_options_default();
    FILE *output;
    int retval = 0;

    if (opts.verbosity == 1)
        log_set_level(LOG_INFO);
    else if (opts.verbosity >= 2)
        log_set_level(LOG_DEBUG);

    if (opts.shift_quirks)
        disopts.shift_quirks = true;

    if (!(disasm = chip8disasm_from_file(disopts, opts.input))) {
        log_error("Could not disassemble input file '%s'", opts.input);
        retval = 1;
        goto EXIT_NOTHING_DONE;
    }
    if (!strcmp(opts.output, "-")) {
        output = stdout;
    } else if (!(output = fopen(opts.output, "w"))) {
        log_error("Could not open output file '%s': %s", opts.output,
            strerror(errno));
        retval = 1;
        goto EXIT_DISASM_CREATED;
    }

    if (chip8disasm_dump(disasm, output)) {
        log_error("Disassembly dump failed");
        retval = 1;
        goto EXIT_OUTPUT_OPENED;
    }

EXIT_OUTPUT_OPENED:
    if (output != stdout)
        fclose(output);
EXIT_DISASM_CREATED:
    chip8disasm_destroy(disasm);
EXIT_NOTHING_DONE:
    return retval;
}
