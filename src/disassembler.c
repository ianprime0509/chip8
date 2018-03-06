/*
 * Copyright 2017-2018 Ian Johnson
 *
 * This is free software, distributed under the MIT license.  A copy of the
 * license can be found in the LICENSE file in the project root, or at
 * https://opensource.org/licenses/MIT.
 */
#include "disassembler.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "instruction.h"
#include "log.h"

/**
 * An ordered collection of jump and return points.
 *
 * This is a rather strange structure, so it is a good idea to go over a few
 * points.  A "jump" point is the address of a jump instruction, and a "return"
 * point is the address to which the jump instruction points.  Since any valid
 * instruction address in the Chip-8 is even (all instructions must be aligned
 * on a 16-bit boundary), this allows us to use pointer tagging to distinguish
 * these two types of address.  We declare that the address of a return point
 * will be unmodified, while the address of a jump point will have its lowest
 * bit set.
 *
 * This decision is not arbitrary, and actually makes things easier to deal
 * with.  The goal of knowing where the jump and return points are is to be
 * able to detect sections of the program that should be disassembled as data
 * rather than as instructions.  Such a data section must be unreachable when
 * the program is executed, and in all normal programs a data section will be
 * precisely the area between a jump point and a return point (exclusive on
 * both ends).  Thus, it is advantageous to order return points after jump
 * points of the same address in order to make this detection easy (this
 * situation may occur if there is an infinite loop of some kind).
 *
 * Naturally, this list should always be sorted in ascending order, and there
 * must be no duplicate elements.  We use a "vector" (in C++ terminology) since
 * a linked list or even a heap seem like overkill given the relatively small
 * number of points we could possibly be dealing with.
 *
 * Also, by convention, the addresses stored in this list should not include
 * the 512-byte interpreter area offset; that is, the address 0x200 in a
 * program will be represented as 0x0 in the list for the disassembler's
 * convenience.
 */
struct jpret_list {
    /**
     * The capacity of the list.
     */
    size_t cap;
    /**
     * The length of the list.
     */
    size_t len;
    /**
     * The actual list data (jump/return point addresses).
     */
    uint16_t *data;
};

struct chip8disasm {
    /**
     * Options to use with this instance of the disassembler.
     */
    struct chip8disasm_options opts;
    /**
     * The program being disassembled.
     *
     * Keep in mind that this is only the program memory (it does not include
     * the reserved 512 bytes of interpreter memory), but that all the
     * addresses referred to by instructions in the program *will* be offset by
     * these 512 bytes.
     */
    uint8_t *mem;
    /**
     * The length of the program being disassembled.
     */
    size_t proglen;
    /**
     * The list of jump/return points.
     */
    struct jpret_list jpret_list;
    /**
     * The list of labelled locations.
     *
     * Every address which is accessed or used by some instruction should be
     * put in here, so that a nice label name can be displayed instead of an
     * indecipherable address.
     */
    struct jpret_list label_list;
};

/**
 * Popuates the jpret_list and label_list with the addresses it can find.
 *
 * @return An error code.
 */
static int chip8disasm_populate_lists(struct chip8disasm *disasm);

/**
 * Initializes a new jpret_list with the given capacity.
 *
 * The capacity is allowed to be zero.
 *
 * @return An error code.
 */
static int jpret_list_init(struct jpret_list *lst, size_t cap);

/**
 * Adds the given entry to the list.
 *
 * This should be the address as it appears in the program (that is, including
 * the 512-byte offset), but with the lowest bit set according to whether it is
 * a jump or return address.
 *
 * @return An error code.
 */
static int jpret_list_add(struct jpret_list *lst, uint16_t addr);
/**
 * Clears the given list, freeing the list memory.
 *
 * It's fine to pass NULL to this function; it will do nothing in that case.
 */
static void jpret_list_clear(struct jpret_list *lst);
/**
 * Finds the index of the given element in the list.
 *
 * @return The index, or -1 if the element is not in the list.
 */
static int jpret_list_find(const struct jpret_list *lst, uint16_t addr);
/**
 * Finds the index of the first equal or greater element in the list.
 *
 * @return The index, or the length of the list if there was no such element.
 */
static int jpret_list_find_ge(const struct jpret_list *lst, uint16_t addr);
/**
 * Grows the list, doubling its capacity.
 *
 * @return An error code.
 */
static int jpret_list_grow(struct jpret_list *lst);
/**
 * Returns whether the given address is in a data section.
 */
static bool jpret_list_in_data(const struct jpret_list *lst, uint16_t addr);
/**
 * Removes the list element at the specified index.
 */
static void jpret_list_remove(struct jpret_list *lst, size_t idx);

struct chip8disasm *chip8disasm_from_file(
    struct chip8disasm_options opts, const char *fname)
{
    struct chip8disasm *disasm = calloc(1, sizeof *disasm);
    struct stat stats;
    FILE *input;

    if (!disasm)
        return NULL;
    disasm->opts = opts;
    /* Figure out how long the program is */
    if (stat(fname, &stats)) {
        log_error("Could not stat '%s': %s", fname, strerror(errno));
        goto FAIL;
    }
    disasm->proglen = stats.st_size;
    if (disasm->proglen > CHIP8_PROG_SIZE) {
        log_error("Input program is too large");
        goto FAIL;
    }
    disasm->mem = malloc(disasm->proglen);
    /* Read in the program data */
    if (!(input = fopen(fname, "r"))) {
        log_error("Could not open input file '%s': %s", fname, strerror(errno));
        goto FAIL;
    }
    fread(disasm->mem, 1, disasm->proglen, input);
    if (ferror(input)) {
        log_error("Error reading from file '%s': %s", fname, strerror(errno));
        fclose(input);
        goto FAIL;
    }
    fclose(input);

    if (jpret_list_init(&disasm->jpret_list, 64)) {
        log_error("Could not create internal address list");
        goto FAIL;
    }
    if (jpret_list_init(&disasm->label_list, 64)) {
        log_error("Could not create internal label list");
        goto FAIL;
    }
    if (chip8disasm_populate_lists(disasm))
        goto FAIL;

    return disasm;

FAIL:
    chip8disasm_destroy(disasm);
    return NULL;
}

void chip8disasm_destroy(struct chip8disasm *disasm)
{
    if (!disasm)
        return;
    free(disasm->mem);
    jpret_list_clear(&disasm->jpret_list);
    jpret_list_clear(&disasm->label_list);
    free(disasm);
}

int chip8disasm_dump(const struct chip8disasm *disasm, FILE *out)
{
    char buf[64];
    char label[8];

    for (int i = 0; i < (int)disasm->proglen; i += 2) {
        uint16_t opcode = ((uint16_t)disasm->mem[i] << 8) + disasm->mem[i + 1];

        /* Print label if necessary */
        if (jpret_list_find(&disasm->label_list, i) != -1)
            fprintf(out, "L%03X: ", (unsigned)i);
        else
            fprintf(out, "      ");
        if (ferror(out)) {
            log_error("Could not dump disassembly to output file: %s",
                strerror(errno));
            return 1;
        }

        if (jpret_list_in_data(&disasm->jpret_list, i)) {
            fprintf(out, "DW #%04X\n", opcode);
        } else {
            struct chip8_instruction instr = chip8_instruction_from_opcode(
                opcode, disasm->opts.shift_quirks);
            /*
             * Don't forget to check that the address actually corresponds to a
             * label!
             */
            bool use_addr = chip8_instruction_uses_addr(instr) &&
                jpret_list_find(
                    &disasm->label_list, instr.addr - CHIP8_PROG_START) != -1;

            /*
             * If the instruction uses an address, we need to construct the
             * corresponding label for nice output.
             */
            if (use_addr)
                snprintf(label, sizeof label, "L%03X",
                    instr.addr - CHIP8_PROG_START);
            chip8_instruction_format(instr, use_addr ? label : NULL, buf,
                sizeof buf, disasm->opts.shift_quirks);
            fprintf(out, "%s\n", buf);
        }
        if (ferror(out)) {
            log_error("Could not dump disassembly to output file: %s",
                strerror(errno));
            return 1;
        }
    }

    return 0;
}

struct chip8disasm_options chip8disasm_options_default(void)
{
    return (struct chip8disasm_options){.shift_quirks = false};
}

static int chip8disasm_populate_lists(struct chip8disasm *disasm)
{
    /*
     * OK, so basically what's happening here is this: we start off with a
     * temporary list of addresses that contains only 0x0, the beginning of
     * execution.  Then, until that temporary list is empty, we start looking
     * at instructions in sequence, starting at one of the addresses in the
     * list, and adding all the return points we find along the way to the
     * temporary list.  When we find a jump point (note that we need to be
     * careful, since a jump instruction after an instruction like SE is not a
     * jump point, being only conditional), we add it to the list in 'disasm'
     * and continue to the next start of execution in our temporary list.
     *
     * The idea behind all of this is that we are effectively simulating
     * execution along all possible paths in order to see which instructions
     * are reachable.  Of course, we can check to make sure that we don't check
     * the same path twice, which should make things a little faster.
     *
     * At the same time, we populate the label_list with all referenced
     * addresses.
     */
    /* A list of addresses to start searching at */
    struct jpret_list starts;
    /* Whether we have just passed a "skip" instruction */
    bool after_skip = false;
    int retval = 0;

    if (jpret_list_init(&starts, 64)) {
        log_error("Could not create temporary address list");
        return 1;
    }
    /*
     * Remember that addresses in the list ignore the 512-byte interpreter
     * area, so the start-of-execution address is stored as 0x0.
     */
    jpret_list_add(&starts, 0x0);

    while (starts.len != 0) {
        uint16_t pc = starts.data[starts.len - 1];

        after_skip = false;
        jpret_list_remove(&starts, starts.len - 1);
        /* We can skip this path if we've already gone down it */
        if (jpret_list_find(&disasm->jpret_list, pc) != -1)
            goto BREAK_FOR;
        /* All the starting points of this loop are return points */
        if (jpret_list_add(&disasm->jpret_list, pc)) {
            log_error("Could not add item to internal address list");
            retval = 1;
            goto EXIT;
        }

        /* Traverse the code from the starting point */
        for (;; pc += 2) {
            struct chip8_instruction inst;

            if (pc >= disasm->proglen) {
                log_warning("Control path went out of program bounds");
                break;
            }
            /* This is the instruction we're currently looking at */
            inst = chip8_instruction_from_opcode(
                ((uint16_t)disasm->mem[pc] << 8) + disasm->mem[pc + 1],
                disasm->opts.shift_quirks);

            /*
             * Add to the label list if we need to.  Note that we need to check
             * not only whether the instruction takes an address as an operand,
             * but whether that address actually refers to a location in the
             * current file; if it doesn't refer to such a location, it
             * shouldn't be added to the label list since it's just an address.
             */
            if (chip8_instruction_uses_addr(inst) &&
                inst.addr - CHIP8_PROG_START < disasm->proglen) {
                if (jpret_list_add(
                        &disasm->label_list, inst.addr - CHIP8_PROG_START)) {
                    log_error("Could not add item to internal label list");
                    retval = 1;
                    goto EXIT;
                }
            }

            switch (inst.op) {
            case OP_RET:
            case OP_EXIT:
                if (!after_skip) {
                    /*
                     * Remember that we shouldn't call something a "jump point"
                     * if it's only conditional.
                     */
                    if (jpret_list_add(&disasm->jpret_list, pc | 1)) {
                        log_error(
                            "Could not add item to internal address list");
                        retval = 1;
                        goto EXIT;
                    }
                    goto BREAK_FOR;
                }
                after_skip = false;
                break;
            case OP_CALL:
                if (inst.addr % 2 != 0) {
                    log_error("Misaligned CALL operand encountered");
                    retval = 1;
                    goto EXIT;
                }
                /*
                 * CALL instructions always keep executing after RET in the
                 * subroutine, so they're not jump points.
                 */
                if (jpret_list_add(&starts, inst.addr - CHIP8_PROG_START)) {
                    log_error("Could not add item to temporary address list");
                    retval = 1;
                    goto EXIT;
                }
                after_skip = false;
                break;
            case OP_JP:
                if (inst.addr % 2 != 0) {
                    log_error("Misaligned JP operand encountered");
                    retval = 1;
                    goto EXIT;
                }
                if (jpret_list_add(&starts, inst.addr - CHIP8_PROG_START)) {
                    log_error("Could not add item to temporary address list");
                    retval = 1;
                    goto EXIT;
                }
                if (!after_skip) {
                    if (jpret_list_add(&disasm->jpret_list, pc | 1)) {
                        log_error(
                            "Could not add item to internal address list");
                        retval = 1;
                        goto EXIT;
                    }
                    goto BREAK_FOR;
                }
                after_skip = false;
                break;
            case OP_SE_BYTE:
            case OP_SNE_BYTE:
            case OP_SE_REG:
            case OP_SNE_REG:
            case OP_SKP:
            case OP_SKNP:
                after_skip = true;
                break;
            case OP_JP_V0:
                log_warning("The disassembler doesn't support JP V0 yet");
                if (!after_skip) {
                    if (jpret_list_add(&disasm->jpret_list, pc | 1)) {
                        log_error(
                            "Could not add item to internal address list");
                        retval = 1;
                        goto EXIT;
                    }
                    goto BREAK_FOR;
                }
                after_skip = false;
                break;
            default:
                after_skip = false;
                break;
            }
        }
    BREAK_FOR:;
    }

EXIT:
    jpret_list_clear(&starts);
    return retval;
}

static int jpret_list_init(struct jpret_list *lst, size_t cap)
{
    if (cap == 0)
        lst->data = NULL;
    else if (!(lst->data = malloc(cap * sizeof *lst->data)))
        return 1;

    lst->cap = cap;
    lst->len = 0;

    return 0;
}

static int jpret_list_add(struct jpret_list *lst, uint16_t addr)
{
    int pos = jpret_list_find_ge(lst, addr);

    if (pos < (int)lst->len && lst->data[pos] == addr)
        return 0; /* Nothing to do */
    /* Try to grow the list if needed */
    if (lst->len == lst->cap)
        if (jpret_list_grow(lst))
            return 1;
    /* Move the elements at the end of the list */
    for (int i = lst->len; i > pos; i--)
        lst->data[i] = lst->data[i - 1];
    /* Finally, we can store our element */
    lst->data[pos] = addr;
    lst->len++;

    return 0;
}

static void jpret_list_clear(struct jpret_list *lst)
{
    if (!lst)
        return;
    free(lst->data);
    lst->cap = 0;
    lst->len = 0;
}

static int jpret_list_find(const struct jpret_list *lst, uint16_t addr)
{
    int ge = jpret_list_find_ge(lst, addr);

    if (ge < lst->len && lst->data[ge] == addr)
        return ge;
    else
        return -1;
}

static int jpret_list_find_ge(const struct jpret_list *lst, uint16_t addr)
{
    int start, end;

    if (lst->len == 0)
        return 0;

    start = 0;
    end = lst->len;
    while (start < end) {
        int mid = (start + end) / 2;

        if (lst->data[mid] < addr)
            start = mid + 1;
        else if (lst->data[mid] > addr)
            end = mid;
        else
            return mid;
    }

    return end;
}

static int jpret_list_grow(struct jpret_list *lst)
{
    size_t new_cap = lst->cap == 0 ? 1 : 2 * lst->cap;
    uint16_t *new_data = realloc(lst->data, new_cap * sizeof *new_data);

    if (!new_data)
        return 1;
    lst->cap = new_cap;
    lst->data = new_data;

    return 0;
}

static bool jpret_list_in_data(const struct jpret_list *lst, uint16_t addr)
{
    int pos = jpret_list_find_ge(lst, addr);

    if (pos == 0)
        return false;

    return (lst->data[pos - 1] & 1) == 1 &&
        (pos == lst->len || addr < lst->data[pos]);
}

static void jpret_list_remove(struct jpret_list *lst, size_t idx)
{
    if (idx >= lst->len)
        return;
    for (size_t i = idx; i < lst->len - 1; i++)
        lst->data[i] = lst->data[i + 1];
    lst->len--;
}
