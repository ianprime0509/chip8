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
/**
 * @file
 * The Chip-8 interpreter.
 */
#ifndef CHIP8_H
#define CHIP8_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "instruction.h"

#define CHIP8_DISPLAY_WIDTH 128
#define CHIP8_DISPLAY_HEIGHT 64

/**
 * A node in a linked list which functions as a call stack.
 */
struct chip8_call_node;

/**
 * Options which can be given to the interpreter.
 */
struct chip8_options {
    /**
     * Whether to delay draw instructions (default true).
     */
    bool delay_draws;
    /**
     * Whether to enable the timer (default true).
     * The only reason you should set this to false is if you're trying to run
     * the interpreter in a test environment where you don't want to have the
     * timer ticking while tests are being run. Obviously, if the timer is
     * disabled, the delay timer and sound timer won't work.
     */
    bool enable_timer;
    /**
     * Whether to enable shift quirks mode (default false).
     * In shift quirks mode, the instructions `SHR` and `SHL` take two register
     * arguments rather than one; the first register will be set to the shifted
     * value of the second. The default is to just shift the single register
     * given as an argument.
     */
    bool shift_quirks;
    /**
     * The frequency at which to run the game (default 60Hz).
     */
    long timer_freq;
};

/**
 * Contains the state of the interpreter.
 */
struct chip8 {
    /**
     * Options to use for emulation.
     */
    struct chip8_options opts;
    /**
     * The internal memory.
     */
    uint8_t mem[CHIP8_MEM_SIZE];
    /**
     * The display.
     * Each element in the display array is a boolean value specifying whether
     * that pixel is on or off.
     */
    bool display[CHIP8_DISPLAY_WIDTH][CHIP8_DISPLAY_HEIGHT];
    /**
     * The general-purpose registers `V0`-`VF`.
     */
    uint8_t regs[16];
    /**
     * The special register `I`.
     */
    uint16_t reg_i;
    /**
     * The delay timer register.
     */
    uint8_t reg_dt;
    /**
     * The sound timer register.
     */
    uint8_t reg_st;
    /**
     * The program counter.
     */
    uint16_t pc;
    /**
     * Whether the interpreter has been halted.
     */
    bool halted;
    /**
     * Whether we are in high resolution (128x64) mode.
     */
    bool highres;
    /**
     * Whether the external display needs to be refreshed.
     */
    bool needs_refresh;
    /**
     * Set to `true` on every timer clock cycle (for delaying until a cycle).
     */
    bool timer_latch;
    /**
     * Whether we are waiting for a latch change (for delaying until a cycle).
     */
    bool timer_waiting;
    /**
     * The internal timer, in ticks.
     * The frequency of these ticks is configured in the options passed to the
     * `chip8_new` function.
     */
    unsigned long timer_ticks;
    /**
     * The call stack (for returning from subroutines).
     */
    struct chip8_call_node *call_stack;
    /**
     * Which keys are currently being pressed.
     * Each bit (0x0-0xF) represents the state of the corresponding key 0-F.
     */
    uint16_t key_states;
};

/**
 * Returns the default set of options.
 */
struct chip8_options chip8_options_default(void);

struct chip8 *chip8_new(struct chip8_options opts);
void chip8_destroy(struct chip8 *chip);

/**
 * Returns the current instruction.
 */
struct chip8_instruction chip8_current_instr(struct chip8 *chip);
/**
 * Executes the given opcode at the current program location.
 */
void chip8_execute_opcode(struct chip8 *chip, uint16_t opcode);
/**
 * Loads a program in binary format from the given byte array.
 *
 * @return A non-zero value if there was an error.
 */
int chip8_load_from_bytes(struct chip8 *chip, uint8_t *bytes, size_t len);
/**
 * Loads a program in binary format from the given file.
 *
 * @return A non-zero value if there was an error.
 */
int chip8_load_from_file(struct chip8 *chip, FILE *file);
/**
 * Executes the next instruction.
 */
void chip8_step(struct chip8 *chip);

#endif