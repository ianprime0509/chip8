/*
 * Copyright 2017-2018 Ian Johnson
 *
 * This is free software, distributed under the MIT license.  A copy of the
 * license can be found in the LICENSE file in the project root, or at
 * https://opensource.org/licenses/MIT.
 */
#include "assembler.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "log.h"
#include "memory.h"

#define LTABLE_SIZE 256

/**
 * The maximum number of operands for any instruction.
 */
#define MAX_OPERANDS 3
/**
 * The maximum size of the stacks in the 'chip8asm_eval' method.
 */
#define STACK_SIZE 100

#define FAIL_MSG(line, ...)                                                    \
    do {                                                                       \
        log_message_begin(LOG_ERROR);                                          \
        log_message_part("On line %d: ", (line));                              \
        log_message_part(__VA_ARGS__);                                         \
        log_message_end();                                                     \
    } while (0)

#define WARN(line, ...)                                                        \
    do {                                                                       \
        log_message_begin(LOG_WARNING);                                        \
        log_message_part("On line %d: ", (line));                              \
        log_message_part(__VA_ARGS__);                                         \
        log_message_end();                                                     \
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
 *
 * This could be one of the Chip-8 operations, such as 'OP_CLS', or an
 * assembler instruction like 'DW'.
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
     * Declare a byte ('DB').
     */
    IT_DB,
    /**
     * Declare a word ('DW').
     */
    IT_DW,
};

/**
 * An assembler instruction.
 *
 * This is used to delay evaluation of operands until all the labels have been
 * processed.
 */
struct instruction {
    /**
     * The type of the instruction.
     */
    enum instruction_type type;
    /**
     * If the instruction type is 'IT_CHIP8_OP', the corresponding operation.
     */
    enum chip8_operation chipop;
    /**
     * The operands.
     *
     * Pseudo-operands like the 'HF' in 'LD HF, Vx' are not to be included in
     * this array; to do so would be redundant, since this information is
     * already contained in 'chipop', and would make several aspects of
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
     * The length.
     */
    size_t len;
    /**
     * The capacity.
     */
    size_t cap;
    /**
     * The underlying data.
     */
    struct instruction *data;
};

/**
 * A label table, for associating labels with addresses.
 *
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
     * The given assembler options.
     */
    struct chip8asm_options opts;
    /**
     * The labels that have been found by the assembler.
     */
    struct ltable labels;
    /**
     * The instructions that have been processed.
     *
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
    /**
     * The current IF instruction nesting level.
     *
     * This starts out at 0 and is incremented every time we reach another
     * nested IF directive.
     *
     * This is what happens when an IF directive is encountered: first, the
     * if_level is incremented.  Then, if the condition is true, we continue on
     * until we reach the matching ELSE or ENDIF.  If the condition is false,
     * we set if_skip_else to the current if_level; this tells the assembler to
     * not process anything until it reaches the matching ELSE (or the matching
     * ENDIF).
     *
     * When the corresponding ELSE directive is encountered: if if_skip_else
     * was nonzero (we were skipping to the ELSE), we reset it to 0, resulting
     * in everything in the ELSE block being processed.  If if_skip_else was
     * zero, we set if_skip_endif to the current if_level, which tells the
     * assembler to skip the ELSE block (until the ENDIF is found).
     *
     * When the corresponding ENDIF is found, we reset if_skip_else or
     * if_skip_endif to 0 if either is equal to if_level, and then decrement
     * if_level.
     */
    int if_level;
    /**
     * The IF nesting level which we should skip until reaching ELSE.
     *
     * This is set to 0 if we are not currently skipping until the next matching
     * ELSE.
     */
    int if_skip_else;
    /**
     * The IF nesting level which we should skip until reaching ENDIF.
     *
     * This is set to 0 if we are not currently skipping until the next matching
     * ENDIF.
     */
    int if_skip_endif;
};

/**
 * Adds the instruction to the internal list, completing its first pass.
 *
 * This will also process any label that might be associated with this
 * instruction.
 */
static int chip8asm_add_instruction(
    struct chip8asm *chipasm, struct instruction instr);
/**
 * Attempts to compile the given Chip-8 instruction into an opcode.
 *
 * The given instruction MUST have type 'IT_CHIP8_OP', or the results are
 * undefined.
 *
 * @param chipasm The assembler to compile with.
 * @param instr The instruction to compile.
 * @param[out] opcode The resulting opcode.
 * @return An error code.
 */
static int chip8asm_compile_chip8op(const struct chip8asm *chipasm,
    const struct instruction *instr, uint16_t *opcode);
/**
 * Processes the assignment with the given operands.
 *
 * Yes, technically we only need the first operand, but it's more consistent
 * this way.
 *
 * @return An error code.
 */
static int chip8asm_process_assignment(struct chip8asm *chipasm, char *label,
    char *operands[MAX_OPERANDS], int n_operands);
/**
 * Processes the given operation with the given operands.
 *
 * This will take the instruction, which is given to us in split form (the
 * operation and operands have been extracted for us), convert it to a 'struct
 * instruction', and add it to 'chipasm''s instruction list.
 *
 * @return An error code.
 */
static int chip8asm_process_instruction(struct chip8asm *chipasm, char *op,
    char *operands[MAX_OPERANDS], int n_operands);
/**
 * Returns whether the assembler should process anything right now.
 *
 * This will return false if, for example, we are in the middle of an IF block
 * which is being skipped. Thus, it will be necessary to process *some* things
 * even if this returns true (namely, IF, ELSE, and ENDIF instructions).
 */
static bool chip8asm_should_process(struct chip8asm *chipasm);

/**
 * Returns the hash of the given string.
 */
static unsigned long hash_str(const char *str);

/**
 * Initializes a new instruction list with the given capacity.
 *
 * The capacity may be zero.
 */
static void instructions_init(struct instructions *lst, size_t cap);

/**
 * Adds an instruction to the instruction list.
 */
static void instructions_add(
    struct instructions *lst, struct instruction instr);
/**
 * Clears the given instruction list.
 *
 * This also frees the underlying data, so it should be used for cleanup too.
 */
static void instructions_clear(struct instructions *lst);
/**
 * Grows the list, doubling its capacity.
 */
static void instructions_grow(struct instructions *lst);
/**
 * Adds the given label/address pair to the table.
 *
 * The label name will not be copied for you, since it should have already been
 * heap-allocated by a parser function (see below).
 *
 * @return Whether the label was already present in the table.
 */
static bool ltable_add(struct ltable *tab, char *label, uint16_t addr);
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
static bool ltable_get(
    const struct ltable *tab, const char *key, uint16_t *value);
/**
 * Applies the given operator to the given stack of numbers.
 */
static int operator_apply(char op, uint16_t *numstack, int *numpos, int line);
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

/*
 * Parsing functions:
 *
 * These parsing functions follow a simple set of conventions.  The ones
 * beginning with 'parse' which parse strings return a heap-allocated string
 * upon a successful parse, containing the thing that was parsed, or NULL if
 * the parse was unsuccessful.  The parsing functions which return things other
 * than strings return an error code to indicate success or failure, with an
 * output argument for the result.
 *
 * All parsing functions advance the given input string past the end of
 * whatever was parsed, if successful; if unsuccessful, the input string is
 * unchanged.
 */
/**
 * Parses an identifier, returning it if one was found.
 *
 * @return The identifier, or NULL if none was found.
 */
static char *parse_ident(const char **str);
/**
 * Parses an operand, returning it if one was found.
 *
 * @return The operand, or NULL if none was found.
 */
static char *parse_operand(const char **str);
/**
 * Parses a binary number.
 *
 * @return An error code.
 */
static int parse_num_bin(const char **str, uint16_t *num);
/**
 * Parses a decimal number.
 *
 * @return An error code.
 */
static int parse_num_dec(const char **str, uint16_t *num);
/**
 * Parses a hexadecimal number.
 *
 * @return An error code.
 */
static int parse_num_hex(const char **str, uint16_t *num);
/**
 * Consumes whitespace until a non-whitespace character is reached.
 */
static void skip_spaces(const char **str);

struct chip8asm *chip8asm_new(struct chip8asm_options opts)
{
    struct chip8asm *chipasm = xcalloc(1, sizeof *chipasm);

    chipasm->opts = opts;
    chipasm->pc = CHIP8_PROG_START;
    /* There's a good chance we'll need space for at least 128 instructions */
    instructions_init(&chipasm->instructions, 128);

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
            FAIL_MSG(
                instr->line, "invalid instruction (this should never happen)");
            return 1;
        case IT_DB:
            if ((err = chip8asm_eval(
                     chipasm, instr->operands[0], instr->line, &opcode)))
                return err;
            prog->mem[mempos] = opcode & 0xFF;
            if (mempos + 1 > prog->len)
                prog->len = mempos + 1;
            break;
        case IT_DW:
            if ((err = chip8asm_eval(
                     chipasm, instr->operands[0], instr->line, &opcode)))
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

    if (chipasm->if_level != 0)
        WARN(chipasm->line,
            "completed processing without finding matching ENDIF");
    /* All instructions emitted, so we can get rid of them */
    instructions_clear(&chipasm->instructions);

    return 0;
}

int chip8asm_eval(
    const struct chip8asm *chipasm, const char *expr, int line, uint16_t *value)
{
    /*
     * We use the shunting-yard algorithm
     * (https://en.wikipedia.org/wiki/Shunting-yard_algorithm) here, since it's
     * simple and does what we need. We do need to be a bit careful about the
     * unary '-' operator: if the last token read was an operator, then any '-'
     * should be parsed as the unary operator, but otherwise it is the binary
     * operator. Internally, we will use '_' to represent the unary '-'.
     */
    char opstack[STACK_SIZE];
    int oppos = 0;
    uint16_t numstack[STACK_SIZE];
    int numpos = 0;
    bool expecting_num = true;

    while (*expr != '\0') {
        if (expecting_num && *expr == '-') {
            int p = precedence('_');
            /* The unary - operator is right-associative */
            while (oppos > 0 && precedence(opstack[oppos - 1]) > p)
                if (operator_apply(opstack[--oppos], numstack, &numpos, line)) {
                    FAIL_MSG(line, "could not evaluate expression");
                    return 1;
                }
            opstack[oppos++] = '_';
            expr++;
        } else if (*expr == '#') {
            /* Parse hex number */
            uint16_t n;

            expr++;
            if (parse_num_hex(&expr, &n)) {
                FAIL_MSG(line, "expected hexadecimal number");
                return 1;
            }
            numstack[numpos++] = n;
            expecting_num = false;
        } else if (*expr == '$') {
            /* Parse binary number */
            uint16_t n;

            expr++;
            if (parse_num_bin(&expr, &n)) {
                FAIL_MSG(line, "expected binary number");
                return 1;
            }
            numstack[numpos++] = n;
            expecting_num = false;
        } else if (isdigit(*expr)) {
            /* Parse decimal number */
            uint16_t n;

            while (parse_num_dec(&expr, &n)) {
                FAIL_MSG(line, "expected decimal number");
                return 1;
            }
            numstack[numpos++] = n;
            expecting_num = false;
        } else if (isidentstart(*expr)) {
            /* Parse identifier */
            char *ident = parse_ident(&expr);
            uint16_t val;

            if (!ltable_get(&chipasm->labels, ident, &val)) {
                FAIL_MSG(line, "unknown identifier '%s'", ident);
                free(ident);
                return 1;
            }
            free(ident);
            numstack[numpos++] = val;
            expecting_num = false;
        } else if (*expr == '(') {
            opstack[oppos++] = *expr++;
            expecting_num = true;
        } else if (*expr == ')') {
            while (oppos > 0 && opstack[oppos - 1] != '(')
                if (operator_apply(opstack[--oppos], numstack, &numpos, line)) {
                    FAIL_MSG(line, "could not evaluate expression");
                    return 1;
                }
            /* Get rid of the '(' pseudo-operator on the stack */
            if (oppos == 0) {
                FAIL_MSG(line, "found ')' with no matching '('");
                return 1;
            }
            oppos--;
            expecting_num = false;
            expr++;
        } else if (*expr == '~') {
            /*
             * We need to treat any unary operators a little differently, since
             * they should be right-associative instead of left-associative like
             * the binary operators, and we should only process them when
             * 'expecting_num == true', since otherwise an expression like '1 ~'
             * would be parsed the same as '~ 1'.
             */
            int p = precedence(*expr);
            if (!expecting_num) {
                FAIL_MSG(line, "did not expect unary operator '~'");
                return 1;
            }
            /*
             * Note the 'precedence(...) > p' instead of >=; this is what makes
             * the operator right-associative.
             */
            while (oppos > 0 && precedence(opstack[oppos - 1]) > p)
                if (operator_apply(opstack[--oppos], numstack, &numpos, line)) {
                    FAIL_MSG(line, "could not evaluate expression");
                    return 1;
                }
            opstack[oppos++] = *expr++;
        } else {
            /* Must be a binary operator */
            int p = precedence(*expr);

            if (p < 0) {
                FAIL_MSG(line, "unknown operator '%c'", *expr);
                return 1;
            }
            while (oppos > 0 && precedence(opstack[oppos - 1]) >= p)
                if (operator_apply(opstack[--oppos], numstack, &numpos, line)) {
                    FAIL_MSG(line, "could not evaluate expression");
                    return 1;
                }
            opstack[oppos++] = *expr++;
            expecting_num = true;
        }

        if (oppos == STACK_SIZE) {
            FAIL_MSG(line, "operator stack overflowed (too many operators)");
            return 1;
        }
        if (numpos == STACK_SIZE) {
            FAIL_MSG(
                line, "number stack overflowed (too many numbers/identifiers)");
            return 1;
        }
        skip_spaces(&expr);
    }

    /* Now that we're done here, we need to use the remaining operators */
    while (oppos > 0) {
        if (opstack[--oppos] == '(') {
            FAIL_MSG(line, "found '(' with no matching ')'");
            return 1;
        }
        if (operator_apply(opstack[oppos], numstack, &numpos, line)) {
            FAIL_MSG(line, "could not evaluate expression");
            return 1;
        }
    }

    if (numpos != 1) {
        FAIL_MSG(line, "expected operation");
        return 1;
    }
    if (value)
        *value = numstack[0];
    return 0;
}

int chip8asm_process_line(struct chip8asm *chipasm, const char *line)
{
    char *tmp;
    /* The name of the operation */
    char *op;
    /* The operands to the instruction */
    char *operands[MAX_OPERANDS];
    /* The current operand being processed */
    int n_op;
    /* Whether we should process a constant assignment */
    bool is_assignment = false;

    chipasm->line++;

    /*
     * Process any labels that might be present, along with the following
     * operation.
     */
    skip_spaces(&line);
    op = NULL;
    while ((tmp = parse_ident(&line))) {
        if (*line != ':') {
            /* Found an operation, not a label. */
            op = tmp;
            break;
        } else {
            if (chipasm->line_label) {
                FAIL_MSG(chipasm->line,
                    "cannot associate more than one label with a statement; "
                    "already found label '%s'",
                    chipasm->line_label);
                free(tmp);
                return 1;
            } else {
                chipasm->line_label = tmp;
            }
            /* Prepare to find next label/operation. */
            line++;
            skip_spaces(&line);
        }
    }

    if (!op)
        return 0; /* No operation; nothing to do */

    skip_spaces(&line);
    /* If we come across an '=', that means we have a variable assignment */
    if (*line == '=') {
        is_assignment = true;
        line++;
        skip_spaces(&line);
    }

    /* Get the operands */
    n_op = 0;
    tmp = parse_operand(&line);
    if (*tmp == '\0' && *line != ',') {
        /* No operands. */
        free(tmp);
    } else {
        operands[n_op++] = tmp;
        while (*line++ == ',') {
            skip_spaces(&line);
            tmp = parse_operand(&line);
            if (*tmp == '\0') {
                FAIL_MSG(chipasm->line, "empty operand");
                goto FAIL;
            } else if (n_op >= MAX_OPERANDS) {
                FAIL_MSG(chipasm->line, "too many operands");
                goto FAIL;
            } else {
                operands[n_op++] = tmp;
            }
        }
    }

    if (is_assignment) {
        return chip8asm_process_assignment(chipasm, op, operands, n_op);
    } else {
        return chip8asm_process_instruction(chipasm, op, operands, n_op);
    }

FAIL:
    free(op);
    free(tmp);
    for (int i = 0; i < n_op; i++)
        free(operands[i]);
    return 1;
}

struct chip8asm_options chip8asm_options_default(void)
{
    return (struct chip8asm_options){
        .shift_quirks = false,
    };
}

struct chip8asm_program *chip8asm_program_new(void)
{
    return xcalloc(1, sizeof(struct chip8asm_program));
}

void chip8asm_program_destroy(struct chip8asm_program *prog)
{
    free(prog);
}

uint16_t chip8asm_program_opcode(
    const struct chip8asm_program *prog, uint16_t addr)
{
    return (uint16_t)prog->mem[addr] << 8 | prog->mem[addr + 1];
}

static int chip8asm_add_instruction(
    struct chip8asm *chipasm, struct instruction instr)
{
    /* Add label to ltable, if any */
    if (chipasm->line_label) {
        if (ltable_add(&chipasm->labels, chipasm->line_label, instr.pc)) {
            FAIL_MSG(chipasm->line, "duplicate label or variable '%s' found",
                chipasm->line_label);
            return 1;
        }
        chipasm->line_label = NULL;
    }
    instructions_add(&chipasm->instructions, instr);

    return 0;
}

static int chip8asm_compile_chip8op(const struct chip8asm *chipasm,
    const struct instruction *instr, uint16_t *opcode)
{
    struct chip8_instruction ci;
    /* Temporary variable for storing eval result */
    uint16_t value;
    /* Temporary variable for storing register numbers */
    int regno;

    ci.op = instr->chipop;
    /* Initialize fields so 'scan-build' doesn't complain */
    ci.vx = ci.vy = REG_V0;
    /*
     * We group the cases here by which arguments they have, and set the
     * corresponding fields of 'ci' appropriately. Converting 'ci'
     * to an opcode is outsourced to another function in 'instruction.h'.
     *
     * This switch statement is much simpler because we made the (smart)
     * decision not to include pseudo-operands in the instruction operands
     * array
     * ('instr->operands'), so in 'LD K, Vx', 'Vx' is the first operand in
     * the array, reducing the number of distinct cases to check below.
     */
    switch (instr->chipop) {
    case OP_INVALID:
        FAIL_MSG(instr->line, "invalid operation (this should never happen)");
        return 1;
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
        if (chip8asm_eval(chipasm, instr->operands[0], instr->line, &value)) {
            FAIL_MSG(instr->line, "could not evaluate operand '%s'",
                instr->operands[0]);
            return 1;
        }
        ci.nibble = value;
        break;
    /* Address as first operand */
    case OP_JP:
    case OP_CALL:
    case OP_LD_I:
    case OP_JP_V0:
        if (chip8asm_eval(chipasm, instr->operands[0], instr->line, &value)) {
            FAIL_MSG(instr->line, "could not evaluate operand '%s'",
                instr->operands[0]);
            return 1;
        }
        ci.addr = value;
        break;
    /* Register first, then byte */
    case OP_SE_BYTE:
    case OP_SNE_BYTE:
    case OP_LD_BYTE:
    case OP_ADD_BYTE:
    case OP_RND:
        if ((regno = register_num(instr->operands[0])) == -1) {
            FAIL_MSG(instr->line, "'%s' is not the name of a register",
                instr->operands[0]);
        }
        if (chip8asm_eval(chipasm, instr->operands[1], instr->line, &value)) {
            FAIL_MSG(instr->line, "could not evaluate operand '%s'",
                instr->operands[1]);
            return 1;
        }
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
    case OP_SHR_QUIRK:
    case OP_SUBN:
    case OP_SHL_QUIRK:
    case OP_SNE_REG:
        if ((regno = register_num(instr->operands[0])) == -1) {
            FAIL_MSG(instr->line, "'%s' is not the name of a register",
                instr->operands[0]);
            return 1;
        }
        ci.vx = regno;
        if ((regno = register_num(instr->operands[1])) == -1) {
            FAIL_MSG(instr->line, "'%s' is not the name of a register",
                instr->operands[1]);
            return 1;
        }
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
        if ((regno = register_num(instr->operands[0])) == -1) {
            FAIL_MSG(instr->line, "'%s' is not the name of a register",
                instr->operands[0]);
            return 1;
        }
        ci.vx = regno;
        break;
    /* Two registers then a nibble */
    case OP_DRW:
        if ((regno = register_num(instr->operands[0])) == -1) {
            FAIL_MSG(instr->line, "'%s' is not the name of a register",
                instr->operands[0]);
            return 1;
        }
        ci.vx = regno;
        if ((regno = register_num(instr->operands[1])) == -1) {
            FAIL_MSG(instr->line, "'%s' is not the name of a register",
                instr->operands[1]);
            return 1;
        }
        ci.vy = regno;
        if (chip8asm_eval(chipasm, instr->operands[2], instr->line, &value)) {
            FAIL_MSG(instr->line, "could not evaluate operand '%s'",
                instr->operands[2]);
            return 1;
        }
        ci.nibble = value;
        break;
    }

    /* Finally, we need to turn this into an opcode */
    if (opcode)
        *opcode = chip8_instruction_to_opcode(ci);

    return 0;
}

static int chip8asm_process_assignment(struct chip8asm *chipasm, char *label,
    char *operands[MAX_OPERANDS], int n_operands)
{
    uint16_t value;
    int retval = 0;

    /* Don't process this assignment if we're skipping things */
    if (chip8asm_should_process(chipasm)) {
        if (n_operands != 1) {
            FAIL_MSG(chipasm->line, "wrong number of operands given to '='");
            free(label);
            retval = 1;
        } else if (chip8asm_eval(chipasm, operands[0], chipasm->line, &value)) {
            FAIL_MSG(chipasm->line, "failed to evaluate expression");
            free(label);
            retval = 1;
        } else if (ltable_add(&chipasm->labels, label, value)) {
            FAIL_MSG(
                chipasm->line, "duplicate label or variable '%s' found", label);
            free(label);
            retval = 1;
        }
    }
    for (int i = 0; i < n_operands; i++)
        free(operands[i]);
    return retval;
}

static int chip8asm_process_instruction(struct chip8asm *chipasm, char *op,
    char *operands[MAX_OPERANDS], int n_operands)
{
/* This is probably bordering on preprocessor abuse... */
#define CHIPOP(name, it, nops)                                                 \
    else if (!strcasecmp(op, (name)))                                          \
    {                                                                          \
        EXPECT_OPERANDS(chipasm->line, op, (nops), n_operands);                \
        instr.type = IT_CHIP8_OP;                                              \
        instr.n_operands = (nops);                                             \
        instr.chipop = (it);                                                   \
        for (int i = 0; i < (nops); i++)                                       \
            instr.operands[i] = operands[i];                                   \
        operands_used = (nops);                                                \
    }

#define EXPECT_OPERANDS(line, op, want, got)                                   \
    do {                                                                       \
        if ((want) > (got)) {                                                  \
            FAIL_MSG((line), "too few operands to %s", (op));                  \
            retval = 1;                                                        \
            goto OUT_FREE_STRINGS;                                             \
        } else if ((got) > (want)) {                                           \
            FAIL_MSG((line), "too many operands to %s", (op));                 \
            retval = 1;                                                        \
            goto OUT_FREE_STRINGS;                                             \
        }                                                                      \
    } while (0)
    /* End preprocessor abuse */

    struct instruction instr;
    /*
     * The number of operands which were used in an instruction, and thus
     * should not be freed.
     */
    int operands_used = 0;
    int retval = 0;

    instr.line = chipasm->line;

    /*
     * We need to try processing IF, ELSE, and ENDIF first, since they will
     * determine if we should try to process anything else to come. You
     * should see the documentation for the if_level field of struct
     * chip8asm for a high-level explanation of what's going on here;
     * relevant details will be reiterated in the comments below.
     */
    if (!strcasecmp(op, "IFDEF")) {
        EXPECT_OPERANDS(chipasm->line, op, 1, n_operands);
        /*
         * It is important to keep track of the IF nesting level even if
         * we're currently not processing instructions, because we need to
         * know which nesting level any ELSE or ENDIF directives apply to.
         * However, we should only process the IF directive (by setting
         * if_skip_else) if we're currently processing instructions.
         */
        chipasm->if_level++;
        if (chip8asm_should_process(chipasm) &&
            !ltable_get(&chipasm->labels, operands[0], NULL)) {
            chipasm->if_skip_else = chipasm->if_level;
        }
        goto OUT_FREE_STRINGS;
    } else if (!strcasecmp(op, "IFNDEF")) {
        EXPECT_OPERANDS(chipasm->line, op, 1, n_operands);
        chipasm->if_level++;
        if (chip8asm_should_process(chipasm) &&
            ltable_get(&chipasm->labels, operands[0], NULL)) {
            chipasm->if_skip_else = chipasm->if_level;
        }
        goto OUT_FREE_STRINGS;
    } else if (!strcasecmp(op, "ELSE")) {
        EXPECT_OPERANDS(chipasm->line, op, 0, n_operands);
        if (chipasm->if_level == 0) {
            FAIL_MSG(chipasm->line, "unexpected ELSE");
            retval = 1;
            goto OUT_FREE_STRINGS;
        }
        /*
         * Here, we need to check if we've reached the ELSE corresponding to
         * an IF whose test returned false (so we've skipped the IF block
         * and now need to process the ELSE block). If the ELSE doesn't
         * correspond to such an IF, we need to make sure we're currently
         * processing instructions before assuming that we should set
         * if_skip_endif; if this check is not made, then a nested ELSE
         * within a skipped block will not behave correctly.
         */
        if (chipasm->if_level == chipasm->if_skip_else)
            chipasm->if_skip_else = 0;
        else if (chip8asm_should_process(chipasm))
            chipasm->if_skip_endif = chipasm->if_level;
        goto OUT_FREE_STRINGS;
    } else if (!strcasecmp(op, "ENDIF")) {
        EXPECT_OPERANDS(chipasm->line, op, 0, n_operands);
        if (chipasm->if_level == 0) {
            FAIL_MSG(chipasm->line, "unexpected ENDIF");
            retval = 1;
            goto OUT_FREE_STRINGS;
        }
        /*
         * Note that we also check if_skip_else here, since an IF block
         * without an ELSE is ended by an ENDIF, and there is no way to tell
         * in advance whether a particular IF block will have an ELSE
         * without processing it.
         */
        if (chipasm->if_level == chipasm->if_skip_else)
            chipasm->if_skip_else = 0;
        if (chipasm->if_level == chipasm->if_skip_endif)
            chipasm->if_skip_endif = 0;
        chipasm->if_level--;
        goto OUT_FREE_STRINGS;
    }

    /* Now we can determine whether we should skip the rest of this */
    if (!chip8asm_should_process(chipasm))
        goto OUT_FREE_STRINGS;

    /* Handle special assembler instructions */
    if (!strcasecmp(op, "DEFINE")) {
        EXPECT_OPERANDS(chipasm->line, op, 1, n_operands);
        ltable_add(&chipasm->labels, operands[0], 0);
        operands_used = 1;
        goto OUT_FREE_STRINGS;
    } else if (!strcasecmp(op, "DB")) {
        EXPECT_OPERANDS(chipasm->line, op, 1, n_operands);
        instr.type = IT_DB;
        instr.n_operands = 1;
        instr.operands[0] = operands[0];
        operands_used = 1;
        /* We don't have to worry about aligning pc here */
        instr.pc = chipasm->pc;
        chip8asm_add_instruction(chipasm, instr);
        chipasm->pc++;
        goto OUT_FREE_STRINGS;
    } else if (!strcasecmp(op, "DW")) {
        EXPECT_OPERANDS(chipasm->line, op, 1, n_operands);
        instr.type = IT_DW;
        instr.n_operands = 1;
        instr.operands[0] = operands[0];
        operands_used = 1;
        /* We don't have to worry about aligning pc here */
        instr.pc = chipasm->pc;
        chip8asm_add_instruction(chipasm, instr);
        chipasm->pc += 2;
        goto OUT_FREE_STRINGS;
    } else if (!strcasecmp(op, "OPTION")) {
        EXPECT_OPERANDS(chipasm->line, op, 1, n_operands);
        WARN(chipasm->line, "ignoring unrecognized option '%s'", operands[0]);
        goto OUT_FREE_STRINGS;
    }

    /*
     * Begin handling Chip-8 instructions here
     */
    /* We need to align the program counter to the nearest word */
    chipasm->pc = (chipasm->pc + 1) & ~1;
    instr.pc = chipasm->pc;

    /*
     * We need the if (0) here because the CHIPOP macro expands to have an
     * else at the beginning
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
        instr.n_operands = 1;
        /* Figure out which 'JP' we're using */
        if (n_operands == 1) {
            instr.chipop = OP_JP;
            instr.operands[0] = operands[0];
            operands_used = 1;
        } else if (n_operands == 2 && !strcasecmp(operands[0], "V0")) {
            instr.chipop = OP_JP_V0;
            free(operands[0]);
            instr.operands[0] = operands[1];
            operands_used = 2;
        }
    }
    CHIPOP("CALL", OP_CALL, 1)
    else if (!strcasecmp(op, "SE"))
    {
        EXPECT_OPERANDS(chipasm->line, op, 2, n_operands);
        instr.type = IT_CHIP8_OP;
        instr.n_operands = 2;
        instr.operands[0] = operands[0];
        instr.operands[1] = operands[1];
        operands_used = 2;
        /* Figure out which 'SE' we're using */
        if (register_num(operands[1]) != -1)
            instr.chipop = OP_SE_REG;
        else
            instr.chipop = OP_SE_BYTE;
    }
    else if (!strcasecmp(op, "SNE"))
    {
        EXPECT_OPERANDS(chipasm->line, op, 2, n_operands);
        instr.type = IT_CHIP8_OP;
        instr.n_operands = 2;
        instr.operands[0] = operands[0];
        instr.operands[1] = operands[1];
        operands_used = 2;
        /* Figure out which 'SNE' we're using */
        if (register_num(operands[1]) != -1)
            instr.chipop = OP_SNE_REG;
        else
            instr.chipop = OP_SNE_BYTE;
    }
    else if (!strcasecmp(op, "LD"))
    {
        EXPECT_OPERANDS(chipasm->line, op, 2, n_operands);
        instr.type = IT_CHIP8_OP;
        operands_used = 2;
        /*
         * Figure out which 'LD' we're using.
         * This is very ugly, owing to the large number of overloads for
         * this operation name. First, we check for special values of the
         * first operand; then, special values of the second operand,
         * including a register name. If none of those cases match, it's an
         * ordinary "load byte" instruction.
         */
        if (!strcasecmp(operands[0], "I")) {
            instr.chipop = OP_LD_I;
            instr.n_operands = 1;
            free(operands[0]);
            instr.operands[0] = operands[1];
        } else if (!strcasecmp(operands[0], "DT")) {
            instr.chipop = OP_LD_DT_REG;
            instr.n_operands = 1;
            free(operands[0]);
            instr.operands[0] = operands[1];
        } else if (!strcasecmp(operands[0], "ST")) {
            instr.chipop = OP_LD_ST;
            instr.n_operands = 1;
            free(operands[0]);
            instr.operands[0] = operands[1];
        } else if (!strcasecmp(operands[0], "F")) {
            instr.chipop = OP_LD_F;
            instr.n_operands = 1;
            free(operands[0]);
            instr.operands[0] = operands[1];
        } else if (!strcasecmp(operands[0], "HF")) {
            instr.chipop = OP_LD_HF;
            instr.n_operands = 1;
            free(operands[0]);
            instr.operands[0] = operands[1];
        } else if (!strcasecmp(operands[0], "B")) {
            instr.chipop = OP_LD_B;
            instr.n_operands = 1;
            free(operands[0]);
            instr.operands[0] = operands[1];
        } else if (!strcasecmp(operands[0], "[I]")) {
            instr.chipop = OP_LD_DEREF_I_REG;
            instr.n_operands = 1;
            free(operands[0]);
            instr.operands[0] = operands[1];
        } else if (!strcasecmp(operands[0], "R")) {
            instr.chipop = OP_LD_R_REG;
            instr.n_operands = 1;
            free(operands[0]);
            instr.operands[0] = operands[1];
        } else if (register_num(operands[1]) != -1) {
            instr.chipop = OP_LD_REG;
            instr.n_operands = 2;
            instr.operands[0] = operands[0];
            instr.operands[1] = operands[1];
        } else if (!strcasecmp(operands[1], "DT")) {
            instr.chipop = OP_LD_REG_DT;
            instr.n_operands = 1;
            instr.operands[0] = operands[0];
            operands_used = 1;
        } else if (!strcasecmp(operands[1], "K")) {
            instr.chipop = OP_LD_KEY;
            instr.n_operands = 1;
            instr.operands[0] = operands[0];
            operands_used = 1;
        } else if (!strcasecmp(operands[1], "[I]")) {
            instr.chipop = OP_LD_REG_DEREF_I;
            instr.n_operands = 1;
            instr.operands[0] = operands[0];
            operands_used = 1;
        } else if (!strcasecmp(operands[1], "R")) {
            instr.chipop = OP_LD_REG_R;
            instr.n_operands = 1;
            instr.operands[0] = operands[0];
            operands_used = 1;
        } else {
            instr.chipop = OP_LD_BYTE;
            instr.n_operands = 2;
            instr.operands[0] = operands[0];
            instr.operands[1] = operands[1];
        }
    }
    else if (!strcasecmp(op, "ADD"))
    {
        EXPECT_OPERANDS(chipasm->line, op, 2, n_operands);
        instr.type = IT_CHIP8_OP;
        /* Figure out which 'ADD' we're using */
        operands_used = 2;
        if (!strcasecmp(operands[0], "I")) {
            instr.chipop = OP_ADD_I;
            instr.n_operands = 1;
            free(operands[0]);
            instr.operands[0] = operands[1];
        } else if (register_num(operands[1]) != -1) {
            instr.chipop = OP_ADD_REG;
            instr.n_operands = 2;
            instr.operands[0] = operands[0];
            instr.operands[1] = operands[1];
        } else {
            instr.chipop = OP_ADD_BYTE;
            instr.n_operands = 2;
            instr.operands[0] = operands[0];
            instr.operands[1] = operands[1];
        }
    }
    CHIPOP("OR", OP_OR, 2)
    CHIPOP("AND", OP_AND, 2)
    CHIPOP("XOR", OP_XOR, 2)
    CHIPOP("SUB", OP_SUB, 2)
    else if (!strcasecmp(op, "SHR"))
    {
        if (chipasm->opts.shift_quirks) {
            EXPECT_OPERANDS(chipasm->line, op, 2, n_operands);
            instr.chipop = OP_SHR_QUIRK;
            instr.n_operands = 2;
            operands_used = 2;
            instr.operands[1] = operands[1];
        } else {
            EXPECT_OPERANDS(chipasm->line, op, 1, n_operands);
            instr.chipop = OP_SHR;
            instr.n_operands = 1;
            operands_used = 2;
        }
        instr.type = IT_CHIP8_OP;
        instr.operands[0] = operands[0];
    }
    CHIPOP("SUBN", OP_SUBN, 2)
    else if (!strcasecmp(op, "SHL"))
    {
        if (chipasm->opts.shift_quirks) {
            EXPECT_OPERANDS(chipasm->line, op, 2, n_operands);
            instr.chipop = OP_SHL_QUIRK;
            instr.n_operands = 2;
            operands_used = 2;
            instr.operands[1] = operands[1];
        } else {
            EXPECT_OPERANDS(chipasm->line, op, 1, n_operands);
            instr.chipop = OP_SHL;
            instr.n_operands = 1;
            operands_used = 1;
        }
        instr.type = IT_CHIP8_OP;
        instr.operands[0] = operands[0];
    }
    CHIPOP("RND", OP_RND, 2)
    CHIPOP("DRW", OP_DRW, 3)
    CHIPOP("SKP", OP_SKP, 1)
    CHIPOP("SKNP", OP_SKNP, 1)
    else
    {
        FAIL_MSG(chipasm->line, "invalid instruction (operation '%s')", op);
        retval = 1;
        goto OUT_FREE_STRINGS;
    }

    /* Every Chip-8 instruction is exactly 2 bytes long */
    chipasm->pc += 2;
    chip8asm_add_instruction(chipasm, instr);
OUT_FREE_STRINGS:
    free(op);
    for (int i = operands_used; i < n_operands; i++)
        free(operands[i]);
    return retval;

/* Nobody has to know about this */
#undef CHIPOP
}

static bool chip8asm_should_process(struct chip8asm *chipasm)
{
    return chipasm->if_skip_else == 0 && chipasm->if_skip_endif == 0;
}

static unsigned long hash_str(const char *str)
{
    /*
     * This is the "djb2" algorithm given on
     * http://www.cse.yorku.ca/~oz/hash.html
     */
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

static void instructions_init(struct instructions *lst, size_t cap)
{
    if (cap == 0)
        lst->data = NULL;
    else
        lst->data = xmalloc(cap * sizeof *lst->data);

    lst->cap = cap;
    lst->len = 0;
}

static void instructions_add(struct instructions *lst, struct instruction instr)
{
    if (lst->len >= lst->cap)
        instructions_grow(lst);

    lst->data[lst->len++] = instr;
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

static void instructions_grow(struct instructions *lst)
{
    size_t new_cap = lst->cap == 0 ? 1 : 2 * lst->cap;
    struct instruction *new_data =
        xrealloc(lst->data, new_cap * sizeof *new_data);

    lst->cap = new_cap;
    lst->data = new_data;
}

static bool ltable_add(struct ltable *tab, char *label, uint16_t addr)
{
    size_t n_bucket = hash_str(label) % LTABLE_SIZE;
    struct ltable_bucket *b;

    /* Create a new bucket list if necessary */
    if (!tab->buckets[n_bucket]) {
        b = xmalloc(sizeof *b);
        b->label = label;
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
    b->next = xmalloc(sizeof *b->next);
    b->next->label = xstrdup(label);
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

static bool ltable_get(
    const struct ltable *tab, const char *key, uint16_t *value)
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

static int operator_apply(char op, uint16_t *numstack, int *numpos, int line)
{
    if (op == '~' || op == '_') {
        if (*numpos == 0) {
            FAIL_MSG(line, "expected argument to operator");
            return 1;
        }
    } else {
        if (*numpos <= 1) {
            FAIL_MSG(line, "expected argument to operator");
            return 1;
        }
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
        FAIL_MSG(line, "unknown operator '%c'", op);
        return 1;
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

static char *parse_ident(const char **str)
{
    const char *tmp = *str;

    if (!isidentstart(*tmp))
        return NULL;
    else
        tmp++;
    while (isidentbody(*tmp))
        tmp++;
    if (tmp == *str) {
        return NULL;
    } else {
        size_t len = tmp - *str;
        char *ret = xmalloc(len + 1);

        memcpy(ret, *str, len);
        ret[len] = '\0';
        *str = tmp;

        return ret;
    }
}

static char *parse_operand(const char **str)
{
    const char *tmp = *str;
    const char *end;
    size_t len;
    char *ret;

    while (*tmp != '\0' && *tmp != '\n' && *tmp != ';' && *tmp != ',')
        tmp++;

    /* Make sure the returned string has all spaces trimmed off the end. */
    end = tmp;
    while (end > *str && isspace(end[-1]))
        end--;
    len = end - *str;

    ret = xmalloc(len + 1);
    memcpy(ret, *str, len);
    ret[len] = '\0';
    *str = tmp;

    return ret;
}

static int parse_num_bin(const char **str, uint16_t *num)
{
    const char *tmp = *str;
    uint16_t n = 0;

    while (*tmp == '0' || *tmp == '1' || *tmp == '.')
        if (*tmp == '.') {
            n = 2 * n;
            tmp++;
        } else {
            n = 2 * n + *tmp++ - '0';
        }
    if (tmp == *str) {
        return 1;
    } else {
        if (num)
            *num = n;
        *str = tmp;
        return 0;
    }
}

static int parse_num_dec(const char **str, uint16_t *num)
{
    const char *tmp = *str;
    uint16_t n = 0;

    while (isdigit(*tmp))
        n = 10 * n + *tmp++ - '0';
    if (tmp == *str) {
        return 1;
    } else {
        if (num)
            *num = n;
        *str = tmp;
        return 0;
    }
}

static int parse_num_hex(const char **str, uint16_t *num)
{
    const char *tmp = *str;
    uint16_t n = 0;

    for (;;) {
        if (isdigit(*tmp))
            n = 16 * n + *tmp++ - '0';
        else if ('A' <= *tmp && *tmp <= 'F')
            n = 16 * n + *tmp++ - 'A' + 10;
        else if ('a' <= *tmp && *tmp <= 'f')
            n = 16 * n + *tmp++ - 'a' + 10;
        else
            break;
    }
    if (tmp == *str) {
        return 1;
    } else {
        if (num)
            *num = n;
        *str = tmp;
        return 0;
    }
}

static void skip_spaces(const char **str)
{
    while (isspace(**str))
        (*str)++;
}
