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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NS_PER_SECOND (1000 * 1000 * 1000)

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
 * The timer thread function.
 */
static void *timer_func(void *arg);

struct chip8 *chip8_new(void)
{
    struct chip8 *chip = malloc(sizeof *chip);

    memset(chip->mem, 0, sizeof chip->mem);
    for (int i = 0; i < CHIP8_DISPLAY_WIDTH; i++)
        for (int j = 0; j < CHIP8_DISPLAY_HEIGHT; j++)
            chip->display[i][j] = false;
    memset(chip->regs, 0, sizeof chip->regs);
    chip->reg_dt = chip->reg_st = chip->reg_i = 0;
    /* Start program at beginning of usable memory */
    chip->pc = 0x200;
    chip->halted = false;
    chip->highres = false;
    chip->needs_refresh = true;
    chip->should_stop_thread = false;
    chip->timer_latch = true;

    chip8_timer_start(chip);

    return chip;
}

void chip8_destroy(struct chip8 *chip)
{
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

static uint16_t chip8_execute(struct chip8 *chip, struct chip8_instruction inst)
{
    switch (inst.op) {
    case OP_INVALID:
        fprintf(stderr,
                "Invalid instruction encountered and ignored (opcode 0x%hX)\n",
                inst.opcode);
        break;
    }

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

static void *timer_func(void *arg)
{
    struct chip8 *chip = arg;

    while (!chip->should_stop_thread) {
        if (chip->reg_dt)
            chip->reg_dt--;
        if (chip->reg_st)
            chip->reg_st--;
        chip->timer_latch = true;
        nanosleep(
            &(struct timespec){.tv_nsec = NS_PER_SECOND / CHIP8_TIMER_FREQ},
            NULL);
    }

    return NULL;
}
