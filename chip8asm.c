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

#include "assembler.h"

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

    if (optind != argc - 1) {
        fprintf(stderr, "Usage: chip8asm [OPTION...] FILE\n");
        return 1;
    }
    opts.input = argv[optind];

    /* For now, require a specified output path */
    if (!opts.output) {
        fprintf(stderr, "Error: must specify an output path (use `-o`)\n");
        return 1;
    }

    return run(opts);
}

static struct progopts progopts_default(void)
{
    return (struct progopts){};
}

static int run(struct progopts opts)
{
    printf("This is a placeholder for later code.\n");
    printf("The input file is '%s' and the output file is '%s'.\n", opts.input,
           opts.output);
    return 0;
}
