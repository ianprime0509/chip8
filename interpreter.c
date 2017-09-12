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
#include "interpreter.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * The address of the low-resolution hex digit sprites in memory.
 * All the sprites must reside below 0x200.
 */
#define CHIP8_HEX_LOW_ADDR 0x0000
/**
 * The height (in pixels) of a low-resolution hex digit sprite.
 */
#define CHIP8_HEX_LOW_HEIGHT 5
/**
 * The address of the high-resolution hex digit sprites in memory.
 * All the sprites must reside below 0x200.
 */
#define CHIP8_HEX_HIGH_ADDR 0x0100
/**
 * The height (in pixels) of a high-resolution hex digit sprite.
 */
#define CHIP8_HEX_HIGH_HEIGHT 10
/**
 * The address where programs should be loaded.
 */
#define CHIP8_PROG_START 0x200

/* Test assumptions on hex digit sprite locations */
static_assert(CHIP8_HEX_LOW_ADDR + 16 * CHIP8_HEX_LOW_HEIGHT <=
                  CHIP8_PROG_START,
              "Low-resolution hex digit sprites overlap program memory");
static_assert(CHIP8_HEX_HIGH_ADDR + 16 * CHIP8_HEX_HIGH_HEIGHT <=
                  CHIP8_PROG_START,
              "High-resolution hex digit sprites overlap program memory");

#define NS_PER_SECOND (1000 * 1000 * 1000)

/**
 * Aborts the given interpreter with the given message.
 */
#define ABORT(chip, ...)                                                       \
    do {                                                                       \
        fprintf(stderr, "ABORT: " __VA_ARGS__);                                \
        fprintf(stderr, "\n");                                                 \
        chip8_dump_regs(chip, stderr);                                         \
        exit(EXIT_FAILURE);                                                    \
    } while (0)

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
 * I'm not entirely sure what the "official" versions of these are, so I made my
 * own, which aren't really the best
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
 * Dumps the state of the interpreter registers to the given file.
 * Errors in writing will be ignored.
 */
static void chip8_dump_regs(const struct chip8 *chip, FILE *output);
/**
 * Executes the given instruction in the interpreter.
 *
 * @return The new value of the program counter after execution.
 */
static uint16_t chip8_execute(struct chip8 *chip,
                              struct chip8_instruction inst);
/**
 * Signals the timer thread to stop and waits for it to do so.
 */
static void chip8_timer_end(struct chip8 *chip);
/**
 * Starts the timer thread.
 */
static void chip8_timer_start(struct chip8 *chip);
/**
 * Clears the timer latch and waits until it is reset.
 */
static void chip8_wait_cycle(struct chip8 *chip);
/**
 * Returns a random byte.
 */
static uint8_t rand_byte(void);
/**
 * The timer thread function.
 */
static void *timer_func(void *arg);

struct chip8_options chip8_options_default(void)
{
    return (struct chip8_options){
        .timer_freq = 60, .delay_draws = true, .enable_timer = true,
    };
}

struct chip8 *chip8_new(struct chip8_options opts)
{
    struct chip8 *chip = malloc(sizeof *chip);

    chip->opts = opts;
    memset(chip->mem, 0, sizeof chip->mem);
    memset(chip->display, 0, sizeof chip->display);
    memset(chip->regs, 0, sizeof chip->regs);
    chip->reg_dt = chip->reg_st = chip->reg_i = 0;
    /* Start program at beginning of usable memory */
    chip->pc = 0x200;
    chip->halted = false;
    chip->highres = false;
    chip->needs_refresh = true;
    chip->should_stop_thread = false;
    chip->timer_latch = true;
    chip->call_stack = NULL;
    chip->key_states = 0;

    /* Load low-resolution hex sprites into memory */
    memcpy(chip->mem + CHIP8_HEX_LOW_ADDR, chip8_hex_low, sizeof chip8_hex_low);
    /* Load high-resolution hex sprites into memory */
    memcpy(chip->mem + CHIP8_HEX_HIGH_ADDR, chip8_hex_high,
           sizeof chip8_hex_high);

    if (opts.enable_timer)
        chip8_timer_start(chip);
    srand(time(NULL));

    return chip;
}

void chip8_destroy(struct chip8 *chip)
{
    if (chip->opts.enable_timer)
        chip8_timer_end(chip);
    free(chip);
}

struct chip8_instruction chip8_current_instr(struct chip8 *chip)
{
    /* The Chip-8 is big-endian */
    uint16_t opcode = ((uint16_t)chip->mem[chip->pc] << 8) |
                      (uint16_t)chip->mem[chip->pc + 1];
    return chip8_instruction_from_opcode(opcode);
}

void chip8_execute_opcode(struct chip8 *chip, uint16_t opcode)
{
    chip->mem[chip->pc] = (opcode & 0xFF00) >> 8;
    chip->mem[chip->pc + 1] = opcode & 0xFF;
    chip8_step(chip);
}

int chip8_load_from_bytes(struct chip8 *chip, uint8_t *bytes, size_t len)
{
    int mempos = CHIP8_PROG_START;

    while (len--) {
        if (mempos >= CHIP8_MEM_SIZE) {
            fprintf(stderr, "Input program is too big\n");
            return -1;
        }
        chip->mem[mempos++] = *bytes++;
    }
    return 0;
}

int chip8_load_from_file(struct chip8 *chip, FILE *file)
{
    int c, err;
    int mempos = CHIP8_PROG_START;

    while ((c = getc(file)) != EOF) {
        if (mempos >= CHIP8_MEM_SIZE) {
            fprintf(stderr, "Input program is too big\n");
            return -1;
        }
        chip->mem[mempos++] = c;
    }

    if ((err = ferror(file))) {
        fprintf(stderr, "Error reading from game file (code %d)\n", err);
        return err;
    }
    return 0;
}

void chip8_step(struct chip8 *chip)
{
    if (!chip->halted) {
        if (chip->pc >= CHIP8_MEM_SIZE) {
            fprintf(stderr, "Program counter went out of bounds\n");
            chip->halted = true;
        } else {
            chip->pc = chip8_execute(chip, chip8_current_instr(chip));
        }
    }
}

static void chip8_dump_regs(const struct chip8 *chip, FILE *output)
{
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            int reg = 4 * i + j;
            fprintf(output, "V%X = %02X%s", reg, chip->regs[reg],
                    j == 3 ? "\n" : "    ");
        }
    fprintf(output, "\nDT = %02X    ST = %02X     I = %04X  PC = %04X\n",
            chip->reg_dt, chip->reg_st, chip->reg_i, chip->pc);
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
            }

    chip->needs_refresh = true;
    return collision;
}

static uint16_t chip8_execute(struct chip8 *chip, struct chip8_instruction inst)
{
    /* Wait for delay timer to run out */
    if (chip->opts.enable_timer)
        while (chip->reg_dt)
            ;

    switch (inst.op) {
    case OP_INVALID:
        fprintf(stderr,
                "Invalid instruction encountered and ignored (opcode 0x%hX)\n",
                inst.opcode);
        break;
    case OP_SCD:
        chip8_wait_cycle(chip);
        for (int y = 0; y < CHIP8_DISPLAY_HEIGHT - inst.nibble; y++)
            for (int x = 0; x < CHIP8_DISPLAY_WIDTH; x++)
                chip->display[x][y] = chip->display[x][y + inst.nibble];
        break;
    case OP_CLS:
        memset(chip->display, 0, sizeof chip->display);
        break;
    case OP_RET:
        if (chip->call_stack) {
            struct chip8_call_node *node = chip->call_stack;
            /* Be sure to increment PAST the caller address! */
            uint16_t retval = node->call_addr + 2;
            chip->call_stack = node->next;
            free(node);
            return retval;
        } else {
            ABORT(chip,
                  "Tried to return from subroutine, but was never called");
        }
        break;
    case OP_SCR:
        chip8_wait_cycle(chip);
        for (int x = 0; x < CHIP8_DISPLAY_WIDTH - 4; x++)
            memcpy(&chip->display[x], &chip->display[x + 4],
                   sizeof chip->display[x]);
        break;
    case OP_SCL:
        chip8_wait_cycle(chip);
        for (int x = CHIP8_DISPLAY_WIDTH - 5; x > 0; x--)
            memcpy(&chip->display[x + 4], &chip->display[x],
                   sizeof chip->display[x]);
        break;
    case OP_EXIT:
        chip->halted = true;
        break;
    case OP_LOW:
        chip->highres = false;
        break;
    case OP_HIGH:
        chip->highres = true;
        break;
    case OP_JP:
        if (inst.addr % 2 == 0)
            return inst.addr;
        else
            ABORT(chip, "Attempted to jump to misaligned memory address "
                        "0x%hX; aborting",
                  inst.addr);
        break;
    case OP_CALL:
        if (inst.addr % 2 == 0) {
            struct chip8_call_node *node = malloc(sizeof *node);
            if (!node)
                ABORT(chip, "Failed to allocate new call node; aborting");
            node->call_addr = chip->pc;
            node->next = chip->call_stack;
            chip->call_stack = node;
            return inst.addr;
        } else {
            ABORT(chip, "Attempted to call subroutine at misaligned memory "
                        "address 0x%hX; aborting",
                  inst.addr);
        }
        break;
    case OP_SE_BYTE:
        if (chip->regs[inst.vx] == inst.byte)
            return chip->pc + 4;
        break;
    case OP_SNE_BYTE:
        if (chip->regs[inst.vx] != inst.byte)
            return chip->pc + 4;
        break;
    case OP_SE_REG:
        if (chip->regs[inst.vx] == chip->regs[inst.vy])
            return chip->pc + 4;
        break;
    case OP_LD_BYTE:
        chip->regs[inst.vx] = inst.byte;
        break;
    case OP_ADD_BYTE:
        /* Check for carry */
        chip->regs[REG_VF] = inst.byte > 255 - chip->regs[inst.vx];
        chip->regs[inst.vx] += inst.byte;
        break;
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
    case OP_ADD_REG:
        /* Check for carry */
        chip->regs[REG_VF] = chip->regs[inst.vy] > 255 - chip->regs[inst.vx];
        chip->regs[inst.vx] += chip->regs[inst.vy];
        break;
    case OP_SUB:
        /* Check for borrow */
        chip->regs[REG_VF] = chip->regs[inst.vy] < chip->regs[inst.vx];
        chip->regs[inst.vx] -= chip->regs[inst.vy];
        break;
    case OP_SHR:
        chip->regs[REG_VF] = chip->regs[inst.vx] & 0x1;
        chip->regs[inst.vx] >>= 1;
        break;
    case OP_SUBN:
        /* Check for borrow */
        chip->regs[REG_VF] = chip->regs[inst.vx] < chip->regs[inst.vy];
        chip->regs[inst.vy] -= chip->regs[inst.vx];
        break;
    case OP_SHL:
        chip->regs[REG_VF] = (chip->regs[inst.vx] & 0x80) >> 7;
        chip->regs[inst.vx] <<= 1;
        break;
    case OP_SNE_REG:
        if (chip->regs[inst.vx] != chip->regs[inst.vy])
            return chip->pc + 4;
        break;
    case OP_LD_I:
        chip->reg_i = inst.addr;
        break;
    case OP_JP_V0: {
        uint32_t jpto = (uint32_t)inst.addr + chip->regs[REG_V0];
        if (jpto % 2 == 0) {
            if (jpto < CHIP8_MEM_SIZE)
                return jpto;
            else
                ABORT(chip, "Attempted to jump to out of bounds memory "
                            "address 0x%X; aborting",
                      jpto);
        } else {
            ABORT(chip, "Attempted to jump to misaligned memory address "
                        "0x%X; aborting",
                  jpto);
        }
    } break;
    case OP_RND:
        chip->regs[inst.vx] = rand_byte() & inst.byte;
        break;
    case OP_DRW:
        chip8_wait_cycle(chip);
        chip->regs[REG_VF] =
            chip8_draw_sprite(chip, chip->regs[inst.vx], chip->regs[inst.vy],
                              chip->reg_i, inst.nibble);
        break;
    case OP_SKP:
        if (chip->key_states & (1 << (chip->regs[inst.vx] & 0xF)))
            return chip->pc + 4;
        break;
    case OP_SKNP:
        if (!(chip->key_states & (1 << (chip->regs[inst.vx] & 0xF))))
            return chip->pc + 4;
        break;
    case OP_LD_REG_DT:
        chip->regs[inst.vx] = chip->reg_dt;
        break;
    case OP_LD_KEY:
        /* Wait for key press */
        while (chip->key_states == 0)
            ;
        /* Get the lowest set bit, and make sure it's 0-indexed */
        chip->regs[inst.vx] = ffs(chip->key_states) - 1;
        break;
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

        if (chip->reg_i + cpy_len >= CHIP8_MEM_SIZE)
            ABORT(chip, "Tried to write to out of bounds memory; aborting");
        memcpy(chip->mem + chip->reg_i, chip->regs, cpy_len);
    } break;
    case OP_LD_REG_DEREF_I: {
        size_t cpy_len = sizeof chip->regs[0] * (inst.vx + 1);

        if (chip->reg_i + cpy_len >= CHIP8_MEM_SIZE)
            ABORT(chip, "Tried to read from out of bounds memory; aborting");
        memcpy(chip->regs, chip->mem + chip->reg_i, cpy_len);
    } break;
    case OP_LD_R_REG:
    case OP_LD_REG_R:
        ABORT(chip, "I have no idea what this does");
        break;
    }

    /*
     * Most instructions just increment the program counter to the next
     * instruction; the ones that don't will have already returned
     */
    return chip->pc + 2;
}

static void chip8_timer_end(struct chip8 *chip)
{
    chip->should_stop_thread = true;
    pthread_join(chip->timer_thread, NULL);
    chip->should_stop_thread = false;
}

static void chip8_timer_start(struct chip8 *chip)
{
    if (pthread_create(&chip->timer_thread, NULL, timer_func, chip)) {
        fprintf(stderr, "Could not spawn timer thread; aborting\n");
        exit(EXIT_FAILURE);
    }
}

static void chip8_wait_cycle(struct chip8 *chip)
{
    if (!chip->opts.delay_draws)
        return;
    chip->timer_latch = false;
    while (!chip->timer_latch)
        ;
}

static uint8_t rand_byte(void)
{
    return rand();
}

static void *timer_func(void *arg)
{
    struct chip8 *chip = arg;

    while (!chip->should_stop_thread) {
        if (chip->reg_dt)
            chip->reg_dt--;
        if (chip->reg_st)
            chip->reg_st--;
        chip->timer_latch = true;
        nanosleep(&(struct timespec){.tv_nsec =
                                         NS_PER_SECOND / chip->opts.timer_freq},
                  NULL);
    }

    return NULL;
}
