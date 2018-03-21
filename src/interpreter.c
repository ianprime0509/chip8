/*
 * Copyright 2017-2018 Ian Johnson
 *
 * This is free software, distributed under the MIT license.  A copy of the
 * license can be found in the LICENSE file in the project root, or at
 * https://opensource.org/licenses/MIT.
 */
#include "interpreter.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "log.h"
#include "util.h"

/**
 * The address of the low-resolution hex digit sprites in memory.
 *
 * All the sprites must reside below 0x200.
 */
#define CHIP8_HEX_LOW_ADDR 0x0000
/**
 * The height (in pixels) of a low-resolution hex digit sprite.
 */
#define CHIP8_HEX_LOW_HEIGHT 5
/**
 * The address of the high-resolution hex digit sprites in memory.
 *
 * All the sprites must reside below 0x200.
 */
#define CHIP8_HEX_HIGH_ADDR 0x0100
/**
 * The height (in pixels) of a high-resolution hex digit sprite.
 */
#define CHIP8_HEX_HIGH_HEIGHT 10

/**
 * The low-resolution hex digit sprites.
 */
static uint8_t chip8_hex_low[16][CHIP8_HEX_LOW_HEIGHT] = {
    {0xF0, 0x90, 0x90, 0x90, 0xF0}, {0x20, 0x60, 0x20, 0x20, 0x70},
    {0xF0, 0x10, 0xF0, 0x80, 0xF0}, {0xF0, 0x10, 0xF0, 0x10, 0xF0},
    {0x90, 0x90, 0xF0, 0x10, 0x10}, {0xF0, 0x80, 0xF0, 0x10, 0xF0},
    {0xF0, 0x80, 0xF0, 0x90, 0xF0}, {0xF0, 0x10, 0x20, 0x40, 0x40},
    {0xF0, 0x90, 0xF0, 0x90, 0xF0}, {0xF0, 0x90, 0xF0, 0x10, 0xF0},
    {0xF0, 0x90, 0xF0, 0x90, 0x90}, {0xE0, 0x90, 0xE0, 0x90, 0xE0},
    {0xF0, 0x80, 0x80, 0x80, 0xF0}, {0xE0, 0x90, 0x90, 0x90, 0xE0},
    {0xF0, 0x80, 0xF0, 0x80, 0xF0}, {0xF0, 0x80, 0xF0, 0x80, 0x80},
};

/**
 * The high-resolution hex digit sprites.
 *
 * I'm not entirely sure what the "official" versions of these are, so I made
 * my own, which aren't really the best.
 */
static uint8_t chip8_hex_high[16][CHIP8_HEX_HIGH_HEIGHT] = {
    {0x3C, 0x42, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x42, 0x3C},
    {0x18, 0x28, 0x48, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x7F},
    {0x3C, 0x42, 0x81, 0x81, 0x02, 0x0C, 0x30, 0x40, 0x80, 0xFF},
    {0x7C, 0x82, 0x01, 0x01, 0x1E, 0x01, 0x01, 0x01, 0x82, 0x7C},
    {0x81, 0x81, 0x81, 0x81, 0xFF, 0x01, 0x01, 0x01, 0x01, 0x01},
    {0xFF, 0x80, 0x80, 0x80, 0xFC, 0x02, 0x01, 0x01, 0x02, 0xFC},
    {0x3E, 0x40, 0x80, 0x80, 0x80, 0xFE, 0x81, 0x81, 0x81, 0x7E},
    {0xFF, 0x01, 0x02, 0x04, 0x08, 0x08, 0x10, 0x10, 0x20, 0x20},
    {0x3C, 0x42, 0x81, 0x42, 0x3C, 0x42, 0x81, 0x81, 0x42, 0x3C},
    {0x7E, 0x81, 0x81, 0x81, 0x7F, 0x01, 0x01, 0x01, 0x02, 0x7C},
    {0x18, 0x24, 0x24, 0x24, 0x42, 0x7E, 0x42, 0x81, 0x81, 0x81},
    {0xFC, 0x82, 0x82, 0x84, 0xF8, 0x84, 0x82, 0x82, 0x82, 0xFC},
    {0x3C, 0x42, 0x81, 0x80, 0x80, 0x80, 0x80, 0x81, 0x42, 0x3C},
    {0xFC, 0x82, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x82, 0xFC},
    {0xFF, 0x80, 0x80, 0x80, 0xFC, 0x80, 0x80, 0x80, 0x80, 0xFF},
    {0xFF, 0x80, 0x80, 0x80, 0xFC, 0x80, 0x80, 0x80, 0x80, 0x80},
};

/**
 * An element in the call stack.
 */
struct chip8_call_node {
    uint16_t call_addr;
    struct chip8_call_node *next;
};

/**
 * Draws a (low-resolution) sprite at the given position.
 *
 * @return Whether there was a collision.
 */
static bool chip8_draw_sprite(struct chip8 *chip, int x, int y,
    uint16_t sprite_start, uint16_t sprite_len);
/**
 * Draws a (high-resolution) sprite at the given position.
 *
 * @return Whether there was a collision.
 */
static bool chip8_draw_sprite_high(
    struct chip8 *chip, int x, int y, uint16_t sprite_start);
/**
 * Logs the state of the registers (using level LOG_DEBUG).
 */
static void chip8_log_regs(const struct chip8 *chip);
/**
 * Executes the given instruction in the interpreter.
 *
 * @param chip The interpreter to use for execution.
 * @param inst The instruction to execute.
 * @param[out] pc The new program counter value to use.
 *
 * @return An error code.
 */
static int chip8_execute(
    struct chip8 *chip, struct chip8_instruction inst, uint16_t *pc);
/**
 * Updates the internal timer value and other internal timers.
 */
static void chip8_timer_update(struct chip8 *chip);
/**
 * Updates just the internal timer.
 */
static void chip8_timer_update_ticks(struct chip8 *chip);
/**
 * Clears the timer latch and waits until it is reset.
 * This method is meant to be called several times, returning `true` when the
 * latch is reset.
 *
 * @return Whether the requested delay has been achieved.
 */
static bool chip8_wait_cycle(struct chip8 *chip);
/**
 * Returns a random byte.
 */
static uint8_t rand_byte(void);

struct chip8_options chip8_options_default(void)
{
    return (struct chip8_options){
        .delay_draws = true,
        .enable_timer = true,
        .shift_quirks = false,
        .timer_freq = 60,
    };
}

struct chip8 *chip8_new(struct chip8_options opts)
{
    struct chip8 *chip = calloc(1, sizeof *chip);

    chip->opts = opts;
    /* Start program at beginning of usable memory */
    chip->pc = 0x200;
    chip->halted = false;
    chip->highres = false;
    chip->needs_refresh = true;
    chip->timer_latch = true;
    chip->timer_waiting = false;
    chip8_timer_update_ticks(chip);
    chip->call_stack = NULL;
    chip->key_states = 0;

    /* Load low-resolution hex sprites into memory */
    memcpy(chip->mem + CHIP8_HEX_LOW_ADDR, chip8_hex_low, sizeof chip8_hex_low);
    /* Load high-resolution hex sprites into memory */
    memcpy(
        chip->mem + CHIP8_HEX_HIGH_ADDR, chip8_hex_high, sizeof chip8_hex_high);

    srand(time(NULL));

    return chip;
}

void chip8_destroy(struct chip8 *chip)
{
    free(chip);
}

struct chip8_instruction chip8_current_instr(struct chip8 *chip)
{
    /* The Chip-8 is big-endian */
    uint16_t opcode = ((uint16_t)chip->mem[chip->pc] << 8) |
        (uint16_t)chip->mem[chip->pc + 1];
    return chip8_instruction_from_opcode(opcode, chip->opts.shift_quirks);
}

int chip8_execute_opcode(struct chip8 *chip, uint16_t opcode)
{
    chip->mem[chip->pc] = (opcode & 0xFF00) >> 8;
    chip->mem[chip->pc + 1] = opcode & 0xFF;
    return chip8_step(chip);
}

int chip8_load_from_bytes(struct chip8 *chip, uint8_t *bytes, size_t len)
{
    int mempos = CHIP8_PROG_START;

    while (len--) {
        if (mempos >= CHIP8_MEM_SIZE) {
            log_error("Input program is too big");
            return -1;
        }
        chip->mem[mempos++] = *bytes++;
    }

    return 0;
}

int chip8_load_from_file(struct chip8 *chip, FILE *file)
{
    int c;
    int mempos = CHIP8_PROG_START;

    while ((c = getc(file)) != EOF) {
        if (mempos >= CHIP8_MEM_SIZE) {
            log_error("Input program is too big");
            return -1;
        }
        chip->mem[mempos++] = c;
    }
    if (ferror(file)) {
        log_error("Error reading from game file: ", strerror(errno));
        return 1;
    }

    return 0;
}

int chip8_step(struct chip8 *chip)
{
    if (!chip->halted) {
        if (chip->pc >= CHIP8_MEM_SIZE) {
            log_error("Program counter went out of bounds");
            chip->halted = true;
        } else if (chip8_execute(chip, chip8_current_instr(chip), &chip->pc)) {
            log_error("Aborting execution");
            return 1;
        }
    } else {
        log_warning(
            "Attempted to execute an instruction in a halted interpreter");
    }

    return 0;
}

static bool chip8_draw_sprite(struct chip8 *chip, int x, int y,
    uint16_t sprite_start, uint16_t sprite_len)
{
    bool collision = false;

    /*
     * Low-resolution sprites are always 8 pixels wide. Here, j will be the
     * column of the sprite to draw, so it is an index from the most significant
     * byte of the sprite at row i.
     */
    for (int i = 0; i < sprite_len && y + i < CHIP8_DISPLAY_HEIGHT; i++)
        for (int j = 0; j < 8 && x + j < CHIP8_DISPLAY_WIDTH; j++)
            if (chip->mem[sprite_start + i] & (1 << (7 - j))) {
                int dispx = x + j, dispy = y + i;
                /* If the pixel on screen is set, we have a collision */
                collision = collision || chip->display[dispx][dispy];
                chip->display[dispx][dispy] = !chip->display[dispx][dispy];
                chip->needs_refresh = true;
            }

    return collision;
}

static bool chip8_draw_sprite_high(
    struct chip8 *chip, int x, int y, uint16_t sprite_start)
{
    bool collision = false;

    /*
     * High-resolution sprites are always 16x16. Here, i is the row of the
     * sprite to draw and j is the column.
     */
    for (int i = 0; i < 16 && y + i < CHIP8_DISPLAY_HEIGHT; i++)
        for (int j = 0; j < 16 && x + j < CHIP8_DISPLAY_WIDTH; j++) {
            int bytepos = sprite_start + 2 * i + j / 8;
            int bitpos = 7 - j % 8;
            if (chip->mem[bytepos] & (1 << bitpos)) {
                int dispx = x + j, dispy = y + i;
                /* If the pixel on screen is set, we have a collision */
                collision = collision || chip->display[dispx][dispy];
                chip->display[dispx][dispy] = !chip->display[dispx][dispy];
                chip->needs_refresh = true;
            }
        }

    return collision;
}

static void chip8_log_regs(const struct chip8 *chip)
{
    log_message_begin(LOG_DEBUG);
    log_message_part("Register values: ");
    for (int i = 0; i < 16; i++)
        log_message_part("V%X = %02X; ", i, chip->regs[i]);
    log_message_part("DT = %02X; ST = %02X; I = %04X; PC = %04X", chip->reg_dt,
        chip->reg_st, chip->reg_i, chip->pc);
    log_message_end();
}

static int chip8_execute(
    struct chip8 *chip, struct chip8_instruction inst, uint16_t *pc)
{
    uint16_t new_pc;

    if (chip->opts.enable_timer)
        chip8_timer_update(chip);

    new_pc = chip->pc + 2;
    switch (inst.op) {
    case OP_INVALID:
        log_warning(
            "Invalid instruction encountered and ignored (opcode 0x%hX)",
            inst.opcode);
        break;
    case OP_SCD:
        if (!chip8_wait_cycle(chip)) {
            new_pc = chip->pc;
            break;
        }
        for (int y = CHIP8_DISPLAY_HEIGHT - 1; y >= inst.nibble; y--)
            for (int x = 0; x < CHIP8_DISPLAY_WIDTH; x++)
                chip->display[x][y] = chip->display[x][y - inst.nibble];
        for (int x = 0; x < CHIP8_DISPLAY_WIDTH; x++)
            memset(chip->display[x], 0, inst.nibble);
        chip->needs_refresh = true;
        break;
    case OP_CLS:
        memset(chip->display, 0, sizeof chip->display);
        chip->needs_refresh = true;
        break;
    case OP_RET:
        if (chip->call_stack) {
            struct chip8_call_node *node = chip->call_stack;
            /* Be sure to increment PAST the caller address! */
            uint16_t retval = node->call_addr + 2;
            chip->call_stack = node->next;
            free(node);
            new_pc = retval;
        } else {
            log_error("Tried to return from subroutine, but there is nothing "
                      "to return to");
            chip8_log_regs(chip);
            return 1;
        }
        break;
    case OP_SCR:
        if (!chip8_wait_cycle(chip)) {
            new_pc = chip->pc;
            break;
        }
        for (int x = CHIP8_DISPLAY_WIDTH - 1; x >= 4; x--)
            memcpy(&chip->display[x], &chip->display[x - 4],
                sizeof chip->display[x]);
        for (int x = 0; x < 4; x++)
            memset(chip->display[x], 0, sizeof chip->display[x]);
        chip->needs_refresh = true;
        break;
    case OP_SCL:
        if (!chip8_wait_cycle(chip)) {
            new_pc = chip->pc;
            break;
        }
        for (int x = 0; x < CHIP8_DISPLAY_WIDTH - 4; x++)
            memcpy(&chip->display[x], &chip->display[x + 4],
                sizeof chip->display[x]);
        for (int x = CHIP8_DISPLAY_WIDTH - 4; x < CHIP8_DISPLAY_WIDTH; x++)
            memset(chip->display[x], 0, sizeof chip->display[x]);
        chip->needs_refresh = true;
        break;
    case OP_EXIT:
        chip->halted = true;
        break;
    case OP_LOW:
        chip->highres = false;
        chip->needs_refresh = true;
        break;
    case OP_HIGH:
        chip->highres = true;
        chip->needs_refresh = true;
        break;
    case OP_JP:
        if (inst.addr % 2 == 0) {
            new_pc = inst.addr;
        } else {
            log_error("Attempted to jump to misaligned memory address 0x%03X",
                inst.addr);
            chip8_log_regs(chip);
            return 1;
        }
        break;
    case OP_CALL:
        if (inst.addr % 2 == 0) {
            struct chip8_call_node *node = malloc(sizeof *node);
            if (!node) {
                log_error(
                    "Could not allocate new call stack node (out of memory)");
                return 1;
            }
            node->call_addr = chip->pc;
            node->next = chip->call_stack;
            chip->call_stack = node;
            new_pc = inst.addr;
        } else {
            log_error("Attempted to call subroutine at misaligned memory "
                      "address 0x%hX",
                inst.addr);
            chip8_log_regs(chip);
            return 1;
        }
        break;
    case OP_SE_BYTE:
        if (chip->regs[inst.vx] == inst.byte)
            new_pc = chip->pc + 4;
        break;
    case OP_SNE_BYTE:
        if (chip->regs[inst.vx] != inst.byte)
            new_pc = chip->pc + 4;
        break;
    case OP_SE_REG:
        if (chip->regs[inst.vx] == chip->regs[inst.vy])
            new_pc = chip->pc + 4;
        break;
    case OP_LD_BYTE:
        chip->regs[inst.vx] = inst.byte;
        break;
    case OP_ADD_BYTE: {
        /* Check for carry */
        uint8_t carry = inst.byte > 255 - chip->regs[inst.vx];
        chip->regs[inst.vx] += inst.byte;
        chip->regs[REG_VF] = carry;
    } break;
    case OP_LD_REG:
        chip->regs[inst.vx] = chip->regs[inst.vy];
        break;
    case OP_OR:
        chip->regs[inst.vx] |= chip->regs[inst.vy];
        break;
    case OP_AND:
        chip->regs[inst.vx] &= chip->regs[inst.vy];
        break;
    case OP_XOR:
        chip->regs[inst.vx] ^= chip->regs[inst.vy];
        break;
    case OP_ADD_REG: {
        /* Check for carry */
        uint8_t carry = chip->regs[inst.vy] > 255 - chip->regs[inst.vx];
        chip->regs[inst.vx] += chip->regs[inst.vy];
        chip->regs[REG_VF] = carry;
    } break;
    case OP_SUB: {
        /* Check for borrow */
        uint8_t borrow = chip->regs[inst.vy] <= chip->regs[inst.vx];
        chip->regs[inst.vx] -= chip->regs[inst.vy];
        chip->regs[REG_VF] = borrow;
    } break;
    case OP_SHR: {
        uint8_t low = chip->regs[inst.vx] & 0x1;
        chip->regs[inst.vx] >>= 1;
        chip->regs[REG_VF] = low;
    } break;
    case OP_SHR_QUIRK: {
        uint8_t low = chip->regs[inst.vy] & 0x1;
        chip->regs[inst.vx] = chip->regs[inst.vy] >> 1;
        chip->regs[REG_VF] = low;
    } break;
    case OP_SUBN: {
        /* Check for borrow */
        uint8_t borrow = chip->regs[inst.vx] <= chip->regs[inst.vy];
        chip->regs[inst.vx] = chip->regs[inst.vy] - chip->regs[inst.vx];
        chip->regs[REG_VF] = borrow;
    } break;
    case OP_SHL: {
        uint8_t high = (chip->regs[inst.vx] & 0x80) >> 7;
        chip->regs[inst.vx] <<= 1;
        chip->regs[REG_VF] = high;
    } break;
    case OP_SHL_QUIRK: {
        uint8_t high = (chip->regs[inst.vy] & 0x80) >> 7;
        chip->regs[inst.vx] = chip->regs[inst.vy] << 1;
        chip->regs[REG_VF] = high;
    } break;
    case OP_SNE_REG:
        if (chip->regs[inst.vx] != chip->regs[inst.vy])
            new_pc = chip->pc + 4;
        break;
    case OP_LD_I:
        chip->reg_i = inst.addr;
        break;
    case OP_JP_V0: {
        uint32_t jpto = (uint32_t)inst.addr + chip->regs[REG_V0];
        if (jpto % 2 == 0) {
            if (jpto < CHIP8_MEM_SIZE) {
                new_pc = jpto;
            } else {
                log_error("Attempted to jump to out of bounds memory "
                          "address 0x%X",
                    jpto);
                chip8_log_regs(chip);
                return 1;
            }
        } else {
            log_error("Attempted to jump to misaligned memory address "
                      "0x%X",
                jpto);
            chip8_log_regs(chip);
            return 1;
        }
    } break;
    case OP_RND:
        chip->regs[inst.vx] = rand_byte() & inst.byte;
        break;
    case OP_DRW:
        if (!chip8_wait_cycle(chip)) {
            new_pc = chip->pc;
            break;
        }
        if (inst.nibble == 0)
            chip->regs[REG_VF] = chip8_draw_sprite_high(
                chip, chip->regs[inst.vx], chip->regs[inst.vy], chip->reg_i);
        else
            chip->regs[REG_VF] = chip8_draw_sprite(chip, chip->regs[inst.vx],
                chip->regs[inst.vy], chip->reg_i, inst.nibble);
        break;
    case OP_SKP:
        if (chip->key_states & (1 << (chip->regs[inst.vx] & 0xF)))
            new_pc = chip->pc + 4;
        break;
    case OP_SKNP:
        if (!(chip->key_states & (1 << (chip->regs[inst.vx] & 0xF))))
            new_pc = chip->pc + 4;
        break;
    case OP_LD_REG_DT:
        chip->regs[inst.vx] = chip->reg_dt;
        break;
    case OP_LD_KEY: {
        int key;
        /*
         * Wait for key press; if none is pressed right now, we just wait until
         * the next step to check for one
         */
        if (chip->key_states == 0) {
            new_pc = chip->pc;
            break;
        }
        key = lowest_set_bit(chip->key_states);
        chip->regs[inst.vx] = key;
        /* Now we need to clear it so that we don't read it twice */
        chip->key_states &= ~(1 << key);
    } break;
    case OP_LD_DT_REG:
        chip->reg_dt = chip->regs[inst.vx];
        break;
    case OP_LD_ST:
        chip->reg_st = chip->regs[inst.vx];
        break;
    case OP_ADD_I:
        chip->reg_i += chip->regs[inst.vx];
        break;
    case OP_LD_F:
        chip->reg_i = CHIP8_HEX_LOW_ADDR +
            CHIP8_HEX_LOW_HEIGHT * (chip->regs[inst.vx] & 0xF);
        break;
    case OP_LD_HF:
        chip->reg_i = CHIP8_HEX_HIGH_ADDR +
            CHIP8_HEX_HIGH_HEIGHT * (chip->regs[inst.vx] & 0xF);
        break;
    case OP_LD_B:
        /* Note that register Vx is only a byte, so it's 3 digits or fewer */
        chip->mem[chip->reg_i] = chip->regs[inst.vx] / 100;
        chip->mem[chip->reg_i + 1] = (chip->regs[inst.vx] / 10) % 10;
        chip->mem[chip->reg_i + 2] = chip->regs[inst.vx] % 10;
        break;
    case OP_LD_DEREF_I_REG: {
        size_t cpy_len = sizeof chip->regs[0] * (inst.vx + 1);

        if (chip->reg_i + cpy_len >= CHIP8_MEM_SIZE) {
            log_error("Tried to write to out of bounds memory");
            chip8_log_regs(chip);
            return 1;
        }
        memcpy(chip->mem + chip->reg_i, chip->regs, cpy_len);
        if (chip->opts.load_quirks)
            chip->reg_i += 2 * (inst.vx + 1);
    } break;
    case OP_LD_REG_DEREF_I: {
        size_t cpy_len = sizeof chip->regs[0] * (inst.vx + 1);

        if (chip->reg_i + cpy_len >= CHIP8_MEM_SIZE) {
            log_error("Tried to read from out of bounds memory");
            chip8_log_regs(chip);
            return 1;
        }
        memcpy(chip->regs, chip->mem + chip->reg_i, cpy_len);
        if (chip->opts.load_quirks)
            chip->reg_i += 2 * (inst.vx + 1);
    } break;
    case OP_LD_R_REG:
    case OP_LD_REG_R:
        log_warning("Encountered one of those weird HP instructions");
        break;
    }

    if (pc)
        *pc = new_pc;
    return 0;
}

static void chip8_timer_update(struct chip8 *chip)
{
    unsigned long old_ticks = chip->timer_ticks;
    unsigned long elapsed;

    chip8_timer_update_ticks(chip);
    elapsed = chip->timer_ticks - old_ticks;

    /* Update internal timers */
    if (chip->reg_dt >= elapsed)
        chip->reg_dt -= elapsed;
    else
        chip->reg_dt = 0;

    if (chip->reg_st >= elapsed)
        chip->reg_st -= elapsed;
    else
        chip->reg_st = 0;

    if (elapsed != 0)
        chip->timer_latch = true;
}

static void chip8_timer_update_ticks(struct chip8 *chip)
{
    chip->timer_ticks = clock_seconds() * chip->opts.timer_freq;
}

static bool chip8_wait_cycle(struct chip8 *chip)
{
    if (!chip->opts.delay_draws)
        return true;
    if (chip->timer_waiting) {
        if (chip->timer_latch) {
            chip->timer_waiting = false;
            return true;
        } else {
            return false;
        }
    } else {
        chip->timer_waiting = true;
        chip->timer_latch = false;
        return false;
    }
}

static uint8_t rand_byte(void)
{
    return rand();
}
