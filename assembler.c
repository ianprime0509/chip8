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
/**
 * The maximum size of the stacks in the `chip8asm_eval` method.
 */
#define STACK_SIZE 100

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
 * Returns whether the given character can be used in the body of an identifier.
 */
#define isidentbody(c) (isalnum((c)) || (c) == '_')
/**
 * Returns whether the given character can be used to start an identifier.
 */
#define isidentstart(c) (isalpha((c)) || (c) == '_')

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
     * Pseudo-operands like the `HF` in `LD HF, Vx` are not to be included in
     * this array; to do so would be redundant, since this information is
     * already contained in `chipop`, and would make several aspects of
     * processing these instructions more complicated.
     */
    char *operands[MAX_OPERANDS];
    /**
     * The number of operands.
     */
    int n_operands;
    /**
     * The line on which this instruction was found.
     */
    int line;
    /**
     * The location in memory for the final instruction after processing.
     */
    uint16_t pc;
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
     * The label that should be associated with the next instruction processed.
     */
    char *line_label;
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
 * Adds the instruction to the internal list, completing its first pass.
 *
 * This will also process any label that might be associated with this
 * instruction.
 */
static int chip8asm_add_instruction(struct chip8asm *chipasm,
                                    struct instruction instr);
/**
 * Attempts to compile the given Chip-8 instruction into an opcode.
 * The given instruction MUST have type `IT_CHIP8_OP`, or the results are
 * undefined.
 *
 * @param[out] opcode The resulting opcode.
 */
static int chip8asm_compile_chip8op(const struct chip8asm *chipasm,
                                    const struct instruction *instr,
                                    uint16_t *opcode);
/**
 * Processes the given operation with the given operands.
 * This will take the instruction, which is given to us in split form (the
 * operation and operands have been extracted for us), convert it to a `struct
 * instruction`, and add it to `chipasm`'s instruction list.
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
 * Clears the given instruction list.
 * This also frees the underlying data, so it should be used for cleanup too.
 */
static void instructions_clear(struct instructions *lst);
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
/**
 * Clears the given table.
 * This also frees the underlying data, so it should be used for cleanup too.
 */
static void ltable_clear(struct ltable *tab);
/**
 * Gets the value corresponding to the given label in the table.
 *
 * @param[out] value The corresponding value.
 * @return Whether the label was present in the table.
 */
static bool ltable_get(const struct ltable *tab, const char *key,
                       uint16_t *value);
/**
 * Applies the given operator to the given stack of numbers.
 */
static int operator_apply(char op, unsigned *numstack, int *numpos, int line);
/**
 * Returns the precedence of the given operator.
 * A higher precedence means the operator is more tightly binding.
 */
static int precedence(char op);
/**
 * Returns the number (0-15) of the given register name (V0-VF).
 * Returns -1 if the register name is invalid.
 */
static int register_num(const char *name);

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
    ltable_clear(&chipasm->labels);
    instructions_clear(&chipasm->instructions);
    free(chipasm->line_label);
    free(chipasm);
}

int chip8asm_emit(struct chip8asm *chipasm, struct chip8asm_program *prog)
{
    for (size_t i = 0; i < chipasm->instructions.len; i++) {
        const struct instruction *instr = &chipasm->instructions.data[i];
        uint16_t opcode;
        size_t mempos = instr->pc - CHIP8_PROG_START;
        int err;

        switch (instr->type) {
        case IT_INVALID:
            FAIL(1, instr->line,
                 "invalid instruction (this should never happen)");
        case IT_DB:
            if ((err = chip8asm_eval(chipasm, instr->operands[0], instr->line,
                                     &opcode)))
                return err;
            prog->mem[mempos] = opcode & 0xFF;
            if (mempos + 1 > prog->len)
                prog->len = mempos + 1;
            break;
        case IT_DW:
            if ((err = chip8asm_eval(chipasm, instr->operands[0], instr->line,
                                     &opcode)))
                return err;
            prog->mem[mempos] = (opcode >> 8) & 0xFF;
            prog->mem[mempos + 1] = opcode & 0xFF;
            if (mempos + 2 > prog->len)
                prog->len = mempos + 2;
            break;
        case IT_CHIP8_OP:
            if ((err = chip8asm_compile_chip8op(chipasm, instr, &opcode)))
                return err;
            prog->mem[mempos] = (opcode >> 8) & 0xFF;
            prog->mem[mempos + 1] = opcode & 0xFF;
            if (mempos + 2 > prog->len)
                prog->len = mempos + 2;
            break;
        }
    }

    /* All instructions emitted, so we can get rid of them */
    instructions_clear(&chipasm->instructions);

    return 0;
}

int chip8asm_eval(const struct chip8asm *chipasm, const char *expr, int line,
                  uint16_t *value)
{
    /*
     * We use the shunting-yard algorithm
     * (https://en.wikipedia.org/wiki/Shunting-yard_algorithm) here, since it's
     * simple and does what we need. We do need to be a bit careful about the
     * unary `-` operator: if the last token read was an operator, then any `-`
     * should be parsed as the unary operator, but otherwise it is the binary
     * operator. Internally, we will use '_' to represent the unary `-`.
     */
    char opstack[STACK_SIZE];
    int oppos = 0;
    unsigned numstack[STACK_SIZE];
    int numpos = 0;
    bool expecting_num = true;
    int err;
    int i = 0;

    while (expr[i] != '\0') {
        if (isspace(expr[i])) {
            /* Skip space */
            i++;
        } else if (expecting_num && expr[i] == '-') {
            int p = precedence('_');
            /* The unary - operator is right-associative */
            while (oppos > 0 && precedence(opstack[oppos - 1]) > p)
                if ((err = operator_apply(opstack[--oppos], numstack, &numpos,
                                          line)))
                    FAIL(1, line, "could not evaluate expression");
            opstack[oppos++] = '_';
            i++;
        } else if (expr[i] == '#') {
            /* Parse hex number */
            int n = 0;

            if (!isxdigit(expr[++i]))
                FAIL(1, line, "expected hexadecimal number");
            while (expr[i] != '\0') {
                if (isdigit(expr[i]))
                    n = 16 * n + (expr[i] - '0');
                else if ('A' <= expr[i] && expr[i] <= 'F')
                    n = 16 * n + (expr[i] - 'A' + 10);
                else if ('a' <= expr[i] && expr[i] <= 'f')
                    n = 16 * n + (expr[i] - 'a' + 10);
                else
                    break;
                i++;
            }
            numstack[numpos++] = n;
            expecting_num = false;
        } else if (expr[i] == '$') {
            /* Parse binary number */
            int n = 0;

            if (expr[++i] != '0' && expr[i] != '1')
                FAIL(1, line, "expected binary number");
            while (expr[i] != '\0') {
                if (expr[i] == '0')
                    n = 2 * n;
                else if (expr[i] == '1')
                    n = 2 * n + 1;
                else
                    break;
                i++;
            }
            numstack[numpos++] = n;
            expecting_num = false;
        } else if (isdigit(expr[i])) {
            /* Parse decimal number */
            int n = 0;

            while (expr[i] != '\0' && isdigit(expr[i])) {
                n = 10 * n + (expr[i] - '0');
                i++;
            }
            numstack[numpos++] = n;
            expecting_num = false;
        } else if (isidentstart(expr[i])) {
            /* Parse identifier */
            char ident[MAXOP];
            int identpos = 0;
            uint16_t val;

            while (isidentbody(expr[i]))
                ident[identpos++] = expr[i++];
            ident[identpos] = '\0';
            if (!ltable_get(&chipasm->labels, ident, &val))
                FAIL(1, line, "unknown identifier `%s`", ident);
            numstack[numpos++] = val;
            expecting_num = false;
        } else if (expr[i] == '(') {
            opstack[oppos++] = expr[i++];
            expecting_num = true;
        } else if (expr[i] == ')') {
            while (oppos > 0 && opstack[oppos - 1] != '(')
                if ((err = operator_apply(opstack[--oppos], numstack, &numpos,
                                          line)))
                    FAIL(1, line, "could not evaluate expression");
            /* Get rid of the '(' pseudo-operator on the stack */
            if (oppos == 0)
                FAIL(1, line, "found ')' with no matching '('");
            oppos--;
            expecting_num = false;
            i++;
        } else if (expr[i] == '~') {
            /*
             * We need to treat any unary operators a little differently, since
             * they should be right-associative instead of left-associative like
             * the binary operators, and we should only process them when
             * `expecting_num == true`, since otherwise an expression like `1 ~`
             * would be parsed the same as `~ 1`.
             */
            int p = precedence(expr[i]);
            if (!expecting_num)
                FAIL(1, line, "did not expect unary operator `~`");
            /*
             * Note the `precedence(...) > p instead of >=; this is what makes
             * the operator right-associative.
             */
            while (oppos > 0 && precedence(opstack[oppos - 1]) > p)
                if ((err = operator_apply(opstack[--oppos], numstack, &numpos,
                                          line)))
                    FAIL(1, line, "could not evaluate expression");
            opstack[oppos++] = expr[i++];
        } else {
            /* Must be a binary operator */
            int p = precedence(expr[i]);

            if (p < 0)
                FAIL(1, line, "unknown operator `%c`", expr[i]);
            while (oppos > 0 && precedence(opstack[oppos - 1]) >= p)
                if ((err = operator_apply(opstack[--oppos], numstack, &numpos,
                                          line)))
                    FAIL(1, line, "could not evaluate expression");
            opstack[oppos++] = expr[i++];
            expecting_num = true;
        }

        if (oppos == STACK_SIZE)
            FAIL(1, line, "operator stack overflowed (too many operators)");
        if (numpos == STACK_SIZE)
            FAIL(1, line,
                 "number stack overflowed (too many numbers/identifiers)");
    }

    /* Now that we're done here, we need to use the remaining operators */
    while (oppos > 0) {
        if (opstack[--oppos] == '(')
            FAIL(1, line, "found '(' with no matching ')'");
        if ((err = operator_apply(opstack[oppos], numstack, &numpos, line)))
            FAIL(1, line, "could not evaluate expression");
    }

    if (numpos != 1)
        FAIL(1, line, "expected operation");
    if (value)
        *value = numstack[0];
    return 0;
}

int chip8asm_process_line(struct chip8asm *chipasm, const char *line)
{
    /* The current position in the line */
    int linepos = 0;
    /* The current position in the operand being processed */
    int bufpos = 0;
    char buf[MAXOP + 1];
    /* The operands to the instruction */
    char operands[MAX_OPERANDS][MAXOP + 1];
    /* The current operand being processed */
    int n_op;
    /* The cursor position in the current operand */
    int oppos;
    /* Whether we should process a constant assignment */
    bool is_assignment = false;

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
            if (chipasm->line_label)
                FAIL(1, chipasm->line, "cannot associate more than one label "
                                       "with a statement; already found label "
                                       "'%s'",
                     chipasm->line_label);
            chipasm->line_label = strdup(buf);
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

    /* Skip whitespace */
    while (isspace(line[linepos]))
        linepos++;
    /*
     * At this point, whatever is in `buf` is the name of an operation, or of a
     * variable to be assigned using `=`; if `buf` is empty, that means there's
     * nothing left on the line to inspect
     */
    if (bufpos == 0)
        return 0;
    /*
     * If we ended with a comment character or a NUL byte, then we have no
     * operands.
     */
    if (line[linepos] == ';' || line[linepos] == '\0')
        return chip8asm_process_instruction(chipasm, buf, operands, 0);

    /* If we come across an `=`, that means we have a variable assignment */
    if (line[linepos] == '=') {
        linepos++;
        is_assignment = true;
        /* Skip whitespace */
        while (isspace(line[linepos]))
            linepos++;
    }

    /*
     * It's not a variable assignment, so it must be something else. That means
     * we have to parse the operands.
     */
    n_op = 0;
    oppos = 0;
    while (line[linepos]) {
        /*
         * Just shove characters into the operand until we hit a comma or the
         * beginning of a comment
         */
        if (line[linepos] == ';') {
            if (oppos == 0)
                FAIL(1, chipasm->line, "found empty operand");
            break;
        } else if (line[linepos] == ',') {
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
    while (oppos > 0 && isspace(operands[n_op][--oppos]))
        ;
    operands[n_op][oppos + 1] = '\0';

    if (is_assignment) {
        uint16_t value;
        int err;

        if (n_op != 0)
            FAIL(1, chipasm->line, "too many operands given to `=`");
        if ((err = chip8asm_eval(chipasm, operands[0], chipasm->line, &value)))
            FAIL(err, chipasm->line, "failed to evaluate expression");
        /* Now, `buf` stores the name of the variable with value `value` */
        if (ltable_add(&chipasm->labels, buf, value))
            FAIL(1, chipasm->line, "duplicate label or variable `%s` found",
                 buf);
        return 0;
    } else {
        return chip8asm_process_instruction(chipasm, buf, operands, n_op + 1);
    }
}

struct chip8asm_program *chip8asm_program_new(void)
{
    return calloc(1, sizeof(struct chip8asm_program));
}

void chip8asm_program_destroy(struct chip8asm_program *prog)
{
    free(prog);
}

uint16_t chip8asm_program_opcode(const struct chip8asm_program *prog,
                                 uint16_t addr)
{
    return (uint16_t)prog->mem[addr] << 8 | prog->mem[addr + 1];
}

static int chip8asm_add_instruction(struct chip8asm *chipasm,
                                    struct instruction instr)
{
    /* Add label to ltable, if any */
    if (chipasm->line_label) {
        if (ltable_add(&chipasm->labels, chipasm->line_label, instr.pc))
            FAIL(1, chipasm->line, "duplicate label or variable `%s` found",
                 chipasm->line_label);
        free(chipasm->line_label);
        chipasm->line_label = NULL;
    }
    instructions_add(&chipasm->instructions, instr);
    return 0;
}

static int chip8asm_compile_chip8op(const struct chip8asm *chipasm,
                                    const struct instruction *instr,
                                    uint16_t *opcode)
{
    struct chip8_instruction ci;
    /* Temporary variable for storing eval result */
    uint16_t value;
    /* Temporary variable for storing register numbers */
    int regno;
    int err;

    ci.op = instr->chipop;
    /*
     * We group the cases here by which arguments they have, and set the
     * corresponding fields of `ci` appropriately. Converting `ci`
     * to an opcode is outsourced to another function in `instruction.h`.
     *
     * This switch statement is much simpler because we made the (smart)
     * decision not to include pseudo-operands in the instruction operands array
     * (`instr->operands`), so in `LD K, Vx`, `Vx` is the first operand in the
     * array, reducing the number of distinct cases to check below.
     */
    switch (instr->chipop) {
    case OP_INVALID:
        FAIL(1, instr->line, "invalid operation (this should never happen)");
    /* No operands */
    case OP_CLS:
    case OP_RET:
    case OP_SCR:
    case OP_SCL:
    case OP_EXIT:
    case OP_LOW:
    case OP_HIGH:
        break;
    /* Nibble as first operand */
    case OP_SCD:
        if ((err = chip8asm_eval(chipasm, instr->operands[0], instr->line,
                                 &value)))
            return err;
        ci.nibble = value;
        break;
    /* Address as first operand */
    case OP_JP:
    case OP_CALL:
    case OP_LD_I:
    case OP_JP_V0:
        if ((err = chip8asm_eval(chipasm, instr->operands[0], instr->line,
                                 &value)))
            return err;
        ci.addr = value;
        break;
    /* Register first, then byte */
    case OP_SE_BYTE:
    case OP_SNE_BYTE:
    case OP_LD_BYTE:
    case OP_ADD_BYTE:
    case OP_RND:
        if ((regno = register_num(instr->operands[0])) == -1)
            FAIL(1, instr->line, "'%s' is not the name of a register",
                 instr->operands[0]);
        if ((err = chip8asm_eval(chipasm, instr->operands[1], instr->line,
                                 &value)))
            return err;
        ci.vx = regno;
        ci.byte = value;
        break;
    /* Two register operands */
    case OP_SE_REG:
    case OP_LD_REG:
    case OP_OR:
    case OP_AND:
    case OP_XOR:
    case OP_ADD_REG:
    case OP_SUB:
    case OP_SUBN:
    case OP_SNE_REG:
        if ((regno = register_num(instr->operands[0])) == -1)
            FAIL(1, instr->line, "'%s' is not the name of a register",
                 instr->operands[0]);
        ci.vx = regno;
        if ((regno = register_num(instr->operands[1])) == -1)
            FAIL(1, instr->line, "'%s' is not the name of a register",
                 instr->operands[1]);
        ci.vy = regno;
        break;
    /* Single register operand */
    case OP_SHR:
    case OP_SHL:
    case OP_SKP:
    case OP_SKNP:
    case OP_LD_REG_DT:
    case OP_LD_KEY:
    case OP_LD_DT_REG:
    case OP_LD_ST:
    case OP_ADD_I:
    case OP_LD_F:
    case OP_LD_HF:
    case OP_LD_B:
    case OP_LD_DEREF_I_REG:
    case OP_LD_REG_DEREF_I:
    case OP_LD_R_REG:
    case OP_LD_REG_R:
        if ((regno = register_num(instr->operands[0])) == -1)
            FAIL(1, instr->line, "'%s' is not the name of a register",
                 instr->operands[0]);
        ci.vx = regno;
        break;
    /* Two registers then a nibble */
    case OP_DRW:
        if ((regno = register_num(instr->operands[0])) == -1)
            FAIL(1, instr->line, "'%s' is not the name of a register",
                 instr->operands[0]);
        ci.vx = regno;
        if ((regno = register_num(instr->operands[1])) == -1)
            FAIL(1, instr->line, "'%s' is not the name of a register",
                 instr->operands[1]);
        ci.vy = regno;
        if ((err = chip8asm_eval(chipasm, instr->operands[2], instr->line,
                                 &value)))
            return err;
        ci.nibble = value;
        break;
    }

    /* Finally, we need to turn this into an opcode */
    if (opcode)
        *opcode = chip8_instruction_to_opcode(ci);

    return 0;
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

#define EXPECT_OPERANDS(line, op, want, got)                                   \
    do {                                                                       \
        if ((want) > (got)) {                                                  \
            FAIL(1, (line), "too few operands to %s", (op));                   \
        } else if ((got) > (want)) {                                           \
            FAIL(1, (line), "too many operands to %s", (op));                  \
        }                                                                      \
    } while (0)

    /* End preprocessor abuse */

    struct instruction instr = {0};
    instr.line = chipasm->line;
    instr.n_operands = n_operands;

    /* Handle special assembler instructions */
    if (!strcasecmp(op, "DEFINE")) {
        EXPECT_OPERANDS(chipasm->line, op, 1, n_operands);
        ltable_add(&chipasm->labels, strdup(operands[0]), 0);
        return 0;
    } else if (!strcasecmp(op, "DB")) {
        EXPECT_OPERANDS(chipasm->line, op, 1, n_operands);
        instr.type = IT_DB;
        instr.operands[0] = strdup(operands[0]);
        /* We don't have to worry about aligning pc here */
        instr.pc = chipasm->pc;
        chip8asm_add_instruction(chipasm, instr);
        chipasm->pc++;
        return 0;
    } else if (!strcasecmp(op, "DW")) {
        EXPECT_OPERANDS(chipasm->line, op, 1, n_operands);
        instr.type = IT_DW;
        instr.operands[0] = strdup(operands[0]);
        /* We don't have to worry about aligning pc here */
        instr.pc = chipasm->pc;
        chip8asm_add_instruction(chipasm, instr);
        chipasm->pc += 2;
        return 0;
    } else if (!strcasecmp(op, "OPTION")) {
        EXPECT_OPERANDS(chipasm->line, op, 1, n_operands);
        WARN(chipasm->line, "ignoring unrecognized option '%s'", operands[0]);
        return 0;
    }

    /*
     * Begin handling Chip-8 instructions here
     */
    /* We need to align the program counter to the nearest word */
    chipasm->pc = (chipasm->pc + 1) & ~1;
    instr.pc = chipasm->pc;

    /*
     * We need the if (0) here because the CHIPOP macro expands to have an else
     * at the beginning
     */
    if (0) {
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
        if (register_num(operands[1]) != -1)
            instr.chipop = OP_SE_REG;
        else
            instr.chipop = OP_SE_BYTE;
    }
    else if (!strcasecmp(op, "SNE"))
    {
        EXPECT_OPERANDS(chipasm->line, op, 2, n_operands);
        instr.type = IT_CHIP8_OP;
        instr.operands[0] = strdup(operands[0]);
        instr.operands[1] = strdup(operands[1]);
        /* Figure out which `SNE` we're using */
        if (register_num(operands[1]) != -1)
            instr.chipop = OP_SNE_REG;
        else
            instr.chipop = OP_SNE_BYTE;
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
        } else if (register_num(operands[1]) != -1) {
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
        } else if (register_num(operands[1]) != -1) {
            instr.chipop = OP_ADD_REG;
            instr.operands[0] = strdup(operands[0]);
            instr.operands[1] = strdup(operands[1]);
        } else {
            instr.chipop = OP_ADD_BYTE;
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

    /* Every Chip-8 instruction is exactly 2 bytes long */
    chipasm->pc += 2;
    chip8asm_add_instruction(chipasm, instr);
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

static void instructions_clear(struct instructions *lst)
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
    if (!(new = realloc(lst->data, cap * sizeof *lst->data)))
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

static void ltable_clear(struct ltable *tab)
{
    for (int i = 0; i < LTABLE_SIZE; i++) {
        struct ltable_bucket *b = tab->buckets[i];
        while (b) {
            struct ltable_bucket *next = b->next;
            free(b->label);
            free(b);
            b = next;
        }
        tab->buckets[i] = NULL;
    }
}

static bool ltable_get(const struct ltable *tab, const char *key,
                       uint16_t *value)
{
    size_t n_bucket = hash_str(key) % LTABLE_SIZE;

    for (struct ltable_bucket *b = tab->buckets[n_bucket]; b != NULL;
         b = b->next) {
        if (!strcmp(key, b->label)) {
            if (value)
                *value = b->addr;
            return true;
        }
    }
    return false;
}

static int operator_apply(char op, unsigned *numstack, int *numpos, int line)
{
    if (op == '~' || op == '_') {
        if (*numpos == 0)
            FAIL(1, line, "expected argument to operator");
    } else {
        if (*numpos <= 1)
            FAIL(1, line, "expected argument to operator");
    }

    switch (op) {
    case '&':
        numstack[*numpos - 2] &= numstack[*numpos - 1];
        (*numpos)--;
        break;
    case '|':
        numstack[*numpos - 2] |= numstack[*numpos - 1];
        (*numpos)--;
        break;
    case '^':
        numstack[*numpos - 2] ^= numstack[*numpos - 1];
        (*numpos)--;
        break;
    case '>':
        numstack[*numpos - 2] >>= numstack[*numpos - 1];
        (*numpos)--;
        break;
    case '<':
        numstack[*numpos - 2] <<= numstack[*numpos - 1];
        (*numpos)--;
        break;
    case '+':
        numstack[*numpos - 2] += numstack[*numpos - 1];
        (*numpos)--;
        break;
    case '-':
        numstack[*numpos - 2] -= numstack[*numpos - 1];
        (*numpos)--;
        break;
    case '*':
        numstack[*numpos - 2] *= numstack[*numpos - 1];
        (*numpos)--;
        break;
    case '/':
        numstack[*numpos - 2] /= numstack[*numpos - 1];
        (*numpos)--;
        break;
    case '%':
        numstack[*numpos - 2] %= numstack[*numpos - 1];
        (*numpos)--;
        break;
    case '~':
        numstack[*numpos - 1] = ~numstack[*numpos - 1];
        break;
    case '_':
        numstack[*numpos - 1] = -numstack[*numpos - 1];
        break;
    default:
        FAIL(1, line, "unknown operator `%c`", op);
    }

    return 0;
}

static int precedence(char op)
{
    switch (op) {
    case '|':
        return 1;
    case '^':
        return 2;
    case '&':
        return 3;
    case '>':
    case '<':
        return 4;
    case '+':
    case '-':
        return 5;
    case '*':
    case '/':
    case '%':
        return 6;
    case '~':
    case '_':
        return 1000;
    default:
        return -1;
    }
}

static int register_num(const char *name)
{
    if ((name[0] == 'V' || name[0] == 'v') && name[1] != '\0' &&
        name[2] == '\0') {
        if (isdigit(name[1]))
            return name[1] - '0';
        else if ('A' <= name[1] && name[1] <= 'F')
            return name[1] - 'A' + 10;
        else if ('a' <= name[1] && name[1] <= 'f')
            return name[1] - 'a' + 10;
        else
            return -1;
    }
    return -1;
}
