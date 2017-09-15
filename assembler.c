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

#define EXPECT_OPERANDS(line, op, want, got)                                   \
    do {                                                                       \
        if ((want) > (got)) {                                                  \
            FAIL(1, (line), "too few operands to %s", (op));                   \
        } else if ((got) > (want)) {                                           \
            FAIL(1, (line), "too many operands to %s", (op));                  \
        }                                                                      \
    } while (0)

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
 * The type of an assembler instruction.
 * This could be one of the Chip-8 operations, such as `OP_CLS`, or an assembler
 * instruction like `DW`.
 */
enum instruction_type {
    /**
     * An invalid instruction.
     */
    IT_INVALID,
    /**
     * A Chip-8 operation.
     */
    IT_CHIP8_OP,
    /**
     * Declare a byte (`DB`).
     */
    IT_DB,
    /**
     * Declare a word (`DW`).
     */
    IT_DW,
};

/**
 * An assembler instruction.
 * This is used to delay evaluation of operands until all the labels have been
 * processed.
 */
struct instruction {
    /**
     * The type of the instruction.
     */
    enum instruction_type type;
    /**
     * If the instruction type is `IT_CHIP8_OP`, the corresponding operation.
     */
    enum chip8_operation chipop;
    /**
     * The operands.
     */
    char *operands[MAX_OPERANDS];
    /**
     * The number of operands.
     */
    int n_operands;
};

/**
 * A growable list of instructions.
 */
struct instructions {
    /**
     * The underlying data.
     */
    struct instruction *data;
    /**
     * The length.
     */
    size_t len;
    /**
     * The capacity.
     */
    size_t cap;
};

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
     * The instructions that have been processed.
     * This list is populated during the first pass, and during the second pass
     * is used to emit the actual opcodes.
     */
    struct instructions instructions;
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
static int chip8asm_process(struct chip8asm *chipasm, const char *line);
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
 * Adds an instruction to the instruction list.
 */
static int instructions_add(struct instructions *lst, struct instruction instr);
/**
 * Frees the underlying data of the instruction list.
 */
static void instructions_free(struct instructions *lst);
/**
 * Reserves space for the instructions list.
 * Calling this function will ensure that the capacity of the list is at least
 * the given capacity.
 */
static int instructions_reserve(struct instructions *lst, size_t cap);
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
    /* There's a good chance we'll need space for at least 128 instructions */
    if (instructions_reserve(&chipasm->instructions, 128))
        return NULL;

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
    instructions_free(&chipasm->instructions);
}

static int chip8asm_eval(const struct chip8asm *chipasm, const char *expr,
                         uint16_t *value)
{
    if (value)
        *value = 0;
    return 0;
}

static int chip8asm_process(struct chip8asm *chipasm, const char *line)
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
/* This is probably bordering on preprocessor abuse... */
#define CHIPOP(name, it, nops)                                                 \
    else if (!strcasecmp(op, (name)))                                          \
    {                                                                          \
        EXPECT_OPERANDS(chipasm->line, op, (nops), n_operands);                \
        instr.type = IT_CHIP8_OP;                                              \
        instr.chipop = (it);                                                   \
        for (int i = 0; i < (nops); i++)                                       \
            instr.operands[i] = strdup(operands[i]);                           \
    }
    /* End preprocessor abuse */

    struct instruction instr = {0};

    if (!strcasecmp(op, "DEFINE")) {
        EXPECT_OPERANDS(chipasm->line, op, 1, n_operands);
        ltable_add(&chipasm->labels, strdup(operands[0]), 0);
        return 0;
    } else if (!strcasecmp(op, "DB")) {
        EXPECT_OPERANDS(chipasm->line, op, 1, n_operands);
        instr.type = IT_DB;
        instr.operands[0] = strdup(operands[0]);
    } else if (!strcasecmp(op, "DW")) {
        EXPECT_OPERANDS(chipasm->line, op, 1, n_operands);
        instr.type = IT_DW;
        instr.operands[0] = strdup(operands[0]);
    }
    CHIPOP("SCD", OP_SCD, 1)
    CHIPOP("CLS", OP_CLS, 0)
    CHIPOP("RET", OP_RET, 0)
    CHIPOP("SCR", OP_SCR, 0)
    CHIPOP("SCL", OP_SCL, 0)
    CHIPOP("EXIT", OP_EXIT, 0)
    CHIPOP("LOW", OP_LOW, 0)
    CHIPOP("HIGH", OP_HIGH, 0)
    else if (!strcasecmp(op, "JP"))
    {
        instr.type = IT_CHIP8_OP;
        /* Figure out which `JP` we're using */
        if (n_operands == 1) {
            instr.chipop = OP_JP;
            instr.operands[0] = strdup(operands[0]);
        } else if (n_operands == 2 && !strcasecmp(operands[0], "V0")) {
            instr.chipop = OP_JP_V0;
            instr.operands[0] = strdup(operands[1]);
        }
    }
    CHIPOP("CALL", OP_CALL, 1)
    else if (!strcasecmp(op, "SE"))
    {
        EXPECT_OPERANDS(chipasm->line, op, 2, n_operands);
        instr.type = IT_CHIP8_OP;
        instr.operands[0] = strdup(operands[0]);
        instr.operands[1] = strdup(operands[1]);
        /* Figure out which `SE` we're using */
        if ((operands[1][0] == 'V' || operands[1][0] == 'v') &&
            isxdigit(operands[1][1]) && operands[1][2] == '\0')
            instr.chipop = OP_SE_BYTE;
        else
            instr.chipop = OP_SE_REG;
    }
    else if (!strcasecmp(op, "SNE"))
    {
        EXPECT_OPERANDS(chipasm->line, op, 2, n_operands);
        instr.type = IT_CHIP8_OP;
        instr.operands[0] = strdup(operands[0]);
        instr.operands[1] = strdup(operands[1]);
        /* Figure out which `SNE` we're using */
        if ((operands[1][0] == 'V' || operands[1][0] == 'v') &&
            isxdigit(operands[1][1]) && operands[1][2] == '\0')
            instr.chipop = OP_SNE_BYTE;
        else
            instr.chipop = OP_SNE_REG;
    }
    else if (!strcasecmp(op, "LD"))
    {
        EXPECT_OPERANDS(chipasm->line, op, 2, n_operands);
        instr.type = IT_CHIP8_OP;
        /*
         * Figure out which `LD` we're using.
         * This is very ugly, owing to the large number of overloads for this
         * operation name. First, we check for special values of the first
         * operand; then, special values of the second operand, including a
         * register name. If none of those cases match, it's an ordinary "load
         * byte" instruction.
         */
        if (!strcasecmp(operands[0], "I")) {
            instr.chipop = OP_LD_I;
            instr.operands[0] = strdup(operands[1]);
        } else if (!strcasecmp(operands[0], "DT")) {
            instr.chipop = OP_LD_DT_REG;
            instr.operands[0] = strdup(operands[1]);
        } else if (!strcasecmp(operands[0], "ST")) {
            instr.chipop = OP_LD_ST;
            instr.operands[0] = strdup(operands[1]);
        } else if (!strcasecmp(operands[0], "F")) {
            instr.chipop = OP_LD_F;
            instr.operands[0] = strdup(operands[1]);
        } else if (!strcasecmp(operands[0], "HF")) {
            instr.chipop = OP_LD_HF;
            instr.operands[0] = strdup(operands[1]);
        } else if (!strcasecmp(operands[0], "B")) {
            instr.chipop = OP_LD_B;
            instr.operands[0] = strdup(operands[1]);
        } else if (!strcasecmp(operands[0], "[I]")) {
            instr.chipop = OP_LD_DEREF_I_REG;
            instr.operands[0] = strdup(operands[1]);
        } else if (!strcasecmp(operands[0], "R")) {
            instr.chipop = OP_LD_R_REG;
            instr.operands[0] = strdup(operands[1]);
        } else if ((operands[1][0] == 'V' || operands[1][0] == 'v') &&
                   isxdigit(operands[1][1]) && operands[1][2] == '\0') {
            instr.chipop = OP_LD_REG;
            instr.operands[0] = strdup(operands[0]);
            instr.operands[1] = strdup(operands[1]);
        } else if (!strcasecmp(operands[1], "DT")) {
            instr.chipop = OP_LD_REG_DT;
            instr.operands[0] = strdup(operands[0]);
        } else if (!strcasecmp(operands[1], "K")) {
            instr.chipop = OP_LD_KEY;
            instr.operands[0] = strdup(operands[0]);
        } else if (!strcasecmp(operands[1], "[I]")) {
            instr.chipop = OP_LD_REG_DEREF_I;
            instr.operands[0] = strdup(operands[0]);
        } else if (!strcasecmp(operands[1], "R")) {
            instr.chipop = OP_LD_REG_R;
            instr.operands[0] = strdup(operands[0]);
        } else {
            instr.chipop = OP_LD_BYTE;
            instr.operands[0] = strdup(operands[0]);
            instr.operands[1] = strdup(operands[1]);
        }
    }
    else if (!strcasecmp(op, "ADD"))
    {
        EXPECT_OPERANDS(chipasm->line, op, 2, n_operands);
        instr.type = IT_CHIP8_OP;
        /* Figure out which `ADD` we're using */
        if (!strcasecmp(operands[0], "I")) {
            instr.chipop = OP_ADD_I;
            instr.operands[0] = strdup(operands[1]);
        } else if ((operands[1][0] == 'V' || operands[1][0] == 'v') &&
                   isxdigit(operands[1][1]) && operands[1][2] == '\0') {
            instr.chipop = OP_ADD_BYTE;
            instr.operands[0] = strdup(operands[0]);
            instr.operands[1] = strdup(operands[1]);
        } else {
            instr.chipop = OP_ADD_REG;
            instr.operands[0] = strdup(operands[0]);
            instr.operands[1] = strdup(operands[1]);
        }
    }
    CHIPOP("OR", OP_OR, 2)
    CHIPOP("AND", OP_AND, 2)
    CHIPOP("XOR", OP_XOR, 2)
    CHIPOP("SUB", OP_SUB, 2)
    CHIPOP("SHR", OP_SHR, 1)
    CHIPOP("SUBN", OP_SUBN, 2)
    CHIPOP("SHL", OP_SHL, 1)
    CHIPOP("RND", OP_RND, 2)
    CHIPOP("DRW", OP_DRW, 3)
    CHIPOP("SKP", OP_SKP, 1)
    CHIPOP("SKNP", OP_SKNP, 1)

    if (instr.type == IT_INVALID)
        FAIL(1, chipasm->line, "invalid instruction (operation `%s`)", op);

    instructions_add(&chipasm->instructions, instr);
    return 0;

/* Nobody has to know about this */
#undef CHIPOP
}

static unsigned long hash_str(const char *str)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

static int instructions_add(struct instructions *lst, struct instruction instr)
{
    if (lst->len >= lst->cap) {
        int err;
        /* Reserve more space */
        if (lst->cap == 0) {
            if ((err = instructions_reserve(lst, 1)))
                return err;
        } else if ((err = instructions_reserve(lst, lst->cap * 2))) {
            return err;
        }
    }

    lst->data[lst->len++] = instr;
    return 0;
}

static void instructions_free(struct instructions *lst)
{
    for (size_t i = 0; i < lst->len; i++)
        for (int j = 0; j < lst->data[i].n_operands; j++)
            free(lst->data[i].operands[j]);

    free(lst->data);
    lst->data = NULL;
    lst->len = lst->cap = 0;
}

static int instructions_reserve(struct instructions *lst, size_t cap)
{
    struct instruction *new;

    if (cap <= lst->cap)
        return 0;
    if (!(new = realloc(lst->data, cap)))
        return 1;

    lst->data = new;
    lst->cap = cap;
    return 0;
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
