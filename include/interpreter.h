/*
 * Copyright 2017 Ian Johnson
 *
 * This is free software, distributed under the MIT license.  A copy of the
 * license can be found in the LICENSE file in the project root, or at
 * https://opensource.org/licenses/MIT.
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
     *
     * The only reason you should set this to false is if you're trying to run
     * the interpreter in a test environment where you don't want to have the
     * timer ticking while tests are being run.  Obviously, if the timer is
     * disabled, the delay timer and sound timer won't work.
     */
    bool enable_timer;
    /**
     * Whether to enable load quirks mode (default false).
     *
     * In load quirks mode, the I register is incremented when the LD [I], VX or
     * LD VX, [I] instructions are used.  The default is to not modify the I
     * register at all.
     */
    bool load_quirks;
    /**
     * Whether to enable shift quirks mode (default false).
     *
     * In shift quirks mode, the instructions `SHR` and `SHL` take two register
     * arguments rather than one; the first register will be set to the shifted
     * value of the second.  The default is to just shift the single register
     * given as an argument.
     */
    bool shift_quirks;
    /**
     * The frequency at which to run the game (default 60Hz).
     */
    unsigned long timer_freq;
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
     *
     * Each element in the display array is a boolean value specifying whether
     * that pixel is on or off.
     */
    bool display[CHIP8_DISPLAY_WIDTH][CHIP8_DISPLAY_HEIGHT];
    /**
     * The general-purpose registers `V0`-`VF`.
     */
    uint8_t regs[16];
    /**
     * The RPL flags.
     */
    uint8_t rpl[8];
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
     * The function to be called every time a pixel needs to be redrawn.  It
     * accepts as parameters the x and y coordinates of the pixel, whether the
     * pixel should be turned on and whether it should be drawn in
     * high-resolution mode.
     */
    void (*draw_callback)(int x, int y, bool on, bool high);
    /**
     * Whether the external display needs to be completely redrawn (e.g. after
     * changing between high- and low-resolution modes.
     */
    bool needs_full_redraw;
    /**
     * The internal timer, in ticks.
     *
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
     *
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
 * Inserts and executes the given opcode at the current program location.
 *
 * @return An error code.
 */
int chip8_execute_opcode(struct chip8 *chip, uint16_t opcode);
/**
 * Loads a program in binary format from the given byte array.
 *
 * @return An error code.
 */
int chip8_load_from_bytes(struct chip8 *chip, uint8_t *bytes, size_t len);
/**
 * Loads a program in binary format from the given file.
 *
 * @return An error code.
 */
int chip8_load_from_file(struct chip8 *chip, FILE *file);
/**
 * Executes the next instruction.
 *
 * @return An error code.
 */
int chip8_step(struct chip8 *chip);

#endif
