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
/**
 * @file
 * The Chip-8 assembler.
 */
#ifndef CHIP8_ASSEMBLER_H
#define CHIP8_ASSEMBLER_H

#include <stdint.h>

#include "instruction.h"

/**
 * The maximum size of a Chip-8 program, in bytes.
 */
#define CHIP8_PROG_SIZE (CHIP8_MEM_SIZE - CHIP8_PROG_START)

/**
 * An assembled Chip-8 program.
 */
struct chip8asm_program {
    /**
     * The bytes of the assembled program.
     */
    uint8_t mem[CHIP8_PROG_SIZE];
};

/**
 * Contains the state of the assembler.
 */
struct chip8asm;

/**
 * Initializes a new assembler.
 */
struct chip8asm *chip8asm_new(void);
/**
 * Destroys the given assembler.
 */
void chip8asm_destroy(struct chip8asm *chipasm);

/**
 * Emits parsed assembly code into the given program.
 * This will take all instructions which have been processed using
 * `chip8asm_process_line`, run the second pass (which substitutes labels for
 * their corresponding addresses, etc.), and put the corresponding binary code
 * into the given program buffer.
 * Note that only labels and constants which have been defined using invocations
 * of `chip8asm_process_line` up to this point will be defined; if an attempt is
 * made to reference a label or constant which has not yet been defined by some
 * line processed by this assembler, an error will result.
 *
 * @return An error code.
 */
int chip8asm_emit(struct chip8asm *chipasm, struct chip8asm_program *prog);
/**
 * Processes the given line of assembly code as part of the first pass.
 * More specifically, the line will be parsed into an internal "instruction"
 * format and added to the list of instructions to be processed during the
 * second pass (which can be triggered using `chip8_emit`).
 *
 * @return An error code.
 */
int chip8asm_process_line(struct chip8asm *chipasm, const char *line);

#endif
