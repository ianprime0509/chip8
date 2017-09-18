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
/**
 * @file
 * An assembler for the Chip-8 and Super-Chip.
 */

#include <getopt.h>
#include <stdio.h>
#include <string.h>

#include "assembler.h"

/**
 * The size of the temporary input line buffer.
 */
#define MAXLINE 500

/**
 * Options which can be passed to the program.
 */
struct progopts {
    /**
     * The output file name.
     */
    char *output;
    /**
     * The input file name.
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
                                     {0, 0, 0, 0}};

    while ((option = getopt_long(argc, argv, "o:", options, NULL)) != -1) {
        switch (option) {
        case 'o':
            opts.output = optarg;
            break;
        case '?':
            return 1;
        }
    }

    if (optind == argc - 1) {
        opts.input = argv[optind];
    } else if (optind == argc) {
        opts.input = "-";
    } else {
        fprintf(stderr, "Usage: chip8asm [OPTION...] [FILE]\n");
        return 1;
    }

    return run(opts);
}

static struct progopts progopts_default(void)
{
    return (struct progopts){.input = "-", .output = "-"};
}

static int run(struct progopts opts)
{
    struct chip8asm *chipasm = chip8asm_new();
    struct chip8asm_program prog;
    FILE *input, *output;
    char str[MAXLINE];
    int err;

    if (!strcmp(opts.input, "-")) {
        input = stdin;
    } else if (!(input = fopen(opts.input, "r"))) {
        perror("Could not open input file for reading");
        return 1;
    }

    if (!strcmp(opts.output, "-")) {
        output = stdout;
    } else if (!(output = fopen(opts.output, "w"))) {
        perror("Could not open output file for writing");
        return 1;
    }

    while (fgets(str, MAXLINE, input)) {
        if ((err = chip8asm_process_line(chipasm, str))) {
            fprintf(stderr, "Could not process input file; aborting\n");
            return 1;
        }
    }
    /* Make sure there wasn't an error in reading */
    if (ferror(input)) {
        perror("Error reading from input file");
        return 1;
    }

    memset(&prog, 0, sizeof prog);
    if ((err = chip8asm_emit(chipasm, &prog))) {
        fprintf(stderr, "Assembler second pass failed; aborting\n");
        return 1;
    }

    fwrite(prog.mem, 1, prog.len, output);
    if (ferror(output)) {
        perror("Error writing to output file");
        return 1;
    }

    return 0;
}
