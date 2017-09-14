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

#include "assembler.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LTABLE_SIZE 256

/**
 * The maximum size of an operand (or any other component of an instruction).
 */
#define MAXOP 200
/**
 * The maximum number of operands for any instruction.
 */
#define MAX_OPERANDS 3

#define FAIL(errcode, line, ...)                                               \
    do {                                                                       \
        fprintf(stderr, "ERROR on line %d: ", (line));                         \
        fprintf(stderr, __VA_ARGS__);                                          \
        fprintf(stderr, "\n");                                                 \
        return (errcode);                                                      \
    } while (0)

#define WARN(line, ...)                                                        \
    do {                                                                       \
        fprintf(stderr, "WARNING on line %d: ", (line));                       \
        fprintf(stderr, __VA_ARGS__);                                          \
        fprintf(stderr, "\n");                                                 \
    } while (0)

/**
 * A label table, for associating labels with addresses.
 * This is just a hash table that uses a linked list for each bucket. It's not
 * the most efficient, but it works.
 */
struct ltable {
    struct ltable_bucket *buckets[LTABLE_SIZE];
};

/**
 * A bucket in a label table.
 */
struct ltable_bucket {
    /**
     * The label name.
     */
    char *label;
    /**
     * The associated address in Chip-8 memory.
     */
    uint16_t addr;
    /**
     * The next entry in this bucket.
     */
    struct ltable_bucket *next;
};

struct chip8asm {
    /**
     * The labels that have been found by the assembler.
     */
    struct ltable labels;
    /**
     * The current line being processed.
     */
    int line;
    /**
     * The current program counter (address in Chip-8 memory).
     */
    uint16_t pc;
};

/**
 * Attempts to evaluate the given expression.
 * The current state of the assembler will be used to access label values and
 * such.
 *
 * @param[out] value The result of the evaluation.
 * @return 0 if parsed successfully, and a nonzero value if not.
 */
static int chip8asm_eval(const struct chip8asm *chipasm, const char *expr,
                         uint16_t *value);
/**
 * Processes the given line of assembly code.
 *
 * @return 0 if the processing was successful, and a nonzero value if not.
 */
/* static int chip8asm_process(struct chip8asm *chipasm, const char *line); */
/**
 * Processes the given operation with the given operands.
 */
static int chip8asm_process_instruction(struct chip8asm *chipasm,
                                        const char *op,
                                        const char (*operands)[MAXOP + 1],
                                        int n_operands);
/**
 * Returns the hash of the given string.
 * This is the "djb2" algorithm given on http://www.cse.yorku.ca/~oz/hash.html
 */
static unsigned long hash_str(const char *str);
/**
 * Adds the given label/address pair to the table.
 * The label name will be copied inside this function, so you don't need to do
 * it yourself. If the label is already associated, it will be overwritten.
 *
 * @return Whether the label was already present in the table.
 */
static bool ltable_add(struct ltable *tab, const char *label, uint16_t addr);

struct chip8asm *chip8asm_new(void)
{
    struct chip8asm *chipasm = calloc(1, sizeof *chipasm);

    chipasm->pc = CHIP8_PROG_START;

    return chipasm;
}

void chip8asm_destroy(struct chip8asm *chipasm)
{
    if (!chipasm)
        return;
    /* Free the label table */
    for (int i = 0; i < LTABLE_SIZE; i++) {
        struct ltable_bucket *b = chipasm->labels.buckets[i];
        while (b) {
            struct ltable_bucket *next = b->next;
            free(b->label);
            free(b);
            b = next;
        }
    }
}

static int chip8asm_eval(const struct chip8asm *chipasm, const char *expr,
                         uint16_t *value)
{
    return 0;
}

int chip8asm_process(struct chip8asm *chipasm, const char *line)
{
    /* The current position in the line */
    int linepos = 0;
    /* The current position in the operand being processed */
    int bufpos = 0;
    static char buf[MAXOP + 1];
    /* The operands to the instruction */
    static char operands[MAX_OPERANDS][MAXOP + 1];
    /* The current operand being processed */
    int n_op;
    /* The cursor position in the current operand */
    int oppos;

    chipasm->line++;

    /* Skip leading whitespace */
    while (isspace(line[linepos]))
        linepos++;
    /* Process any labels that might be present */
    while (line[linepos]) {
        if (line[linepos] == ';') {
            /* Comment */
            break;
        } else if (line[linepos] == ':') {
            /* Found a label */
            buf[bufpos] = '\0';
            if (bufpos == 0)
                FAIL(1, chipasm->line, "found empty label");
            if (ltable_add(&chipasm->labels, buf, chipasm->pc))
                FAIL(1, chipasm->line, "duplicate label or variable `%s` found",
                     buf);
            /* Now skip whitespace before going on to find the next label */
            while (isspace(line[++linepos]))
                ;
            /* Reset bufpos to begin processing of next label */
            bufpos = 0;
        } else if (isspace(line[linepos])) {
            /* This isn't a label */
            break;
        } else {
            if (bufpos >= MAXOP)
                FAIL(1, chipasm->line,
                     "label, operation, or operand is too long");
            buf[bufpos++] = line[linepos++];
        }
    }
    buf[bufpos] = '\0';

    /* If we ended with a comment character, then we have no operands */
    if (line[linepos] == ';')
        return chip8asm_process_instruction(chipasm, buf, operands, 0);

    /*
     * At this point, whatever is in `buf` is the name of an operation, or of a
     * variable to be assigned using `=`; if `buf` is empty, that means there's
     * nothing left on the line to inspect
     */
    if (bufpos == 0)
        return 0;
    /* Skip whitespace */
    while (isspace(line[linepos]))
        linepos++;
    /* If we come across an `=`, that means we have a variable assignment */
    if (line[linepos] == '=') {
        uint16_t value;
        int err;

        linepos++;
        if ((err = chip8asm_eval(chipasm, line + linepos, &value)))
            FAIL(err, chipasm->line, "failed to evaluate expression");
        /* Now, `buf` stores the name of the variable with value `value` */
        if (ltable_add(&chipasm->labels, buf, value))
            FAIL(1, chipasm->line, "duplicate label or variable `%s` found",
                 buf);
        return 0;
    }

    /*
     * It's not a variable assignment, so it must be something else. That means
     * we have to parse the operands.
     */
    n_op = 0;
    oppos = 0;
    while (line[linepos]) {
        /* Just shove characters into the operand until we hit a comma */
        if (line[linepos] == ',') {
            if (oppos == 0)
                FAIL(1, chipasm->line, "found empty operand");
            /*
             * Trim whitespace off the end by moving back to the last
             * non-whitespace character
             */
            while (isspace(operands[n_op][--oppos]))
                ;
            /*
             * Now we need to move one space forward again, because the
             * character we just landed on is not whitespace (so it's part of
             * our operand); we don't want to overwrite it with the null byte
             */
            operands[n_op++][oppos + 1] = '\0';
            if (n_op >= MAX_OPERANDS)
                FAIL(1, chipasm->line, "too many operands");
            /* Advance input until next non-whitespace character */
            while (isspace(line[++linepos]))
                ;
            if (line[linepos] == '\0')
                FAIL(1, chipasm->line, "expected operand after `,`");
            oppos = 0;
        } else {
            if (oppos >= MAXOP)
                FAIL(1, chipasm->line, "operand is too long");
            operands[n_op][oppos++] = line[linepos++];
        }
    }

    /* Trim whitespace off the end of the last operand */
    while (isspace(operands[n_op][--oppos]))
        ;
    operands[n_op][oppos + 1] = '\0';

    return chip8asm_process_instruction(chipasm, buf, operands, n_op + 1);
}

static int chip8asm_process_instruction(struct chip8asm *chipasm,
                                        const char *op,
                                        const char (*operands)[MAXOP + 1],
                                        int n_operands)
{
    printf("Processed operation: '%s'\n", op);
    for (int i = 0; i < n_operands; i++)
        printf("Operand: '%s'\n", operands[i]);
    return 0;
}

static unsigned long hash_str(const char *str)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

static bool ltable_add(struct ltable *tab, const char *label, uint16_t addr)
{
    size_t n_bucket = hash_str(label) % LTABLE_SIZE;
    struct ltable_bucket *b;

    /* Create a new bucket list if necessary */
    if (!tab->buckets[n_bucket]) {
        b = malloc(sizeof *b);
        b->label = strdup(label);
        b->addr = addr;
        b->next = NULL;
        tab->buckets[n_bucket] = b;
        return false;
    }

    b = tab->buckets[n_bucket];
    while (b->next) {
        /* Check for entry already present */
        if (!strcmp(label, b->label)) {
            b->addr = addr;
            return true;
        }
    }
    b->next = malloc(sizeof *b->next);
    b->next->label = strdup(label);
    b->next->addr = addr;
    b->next->next = NULL;
    return false;
}
