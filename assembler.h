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
int chip8asm_process(struct chip8asm *chipasm, const char *line);

#endif
