/*
 * Copyright 2018 Ian Johnson
 *
 * This is free software, distributed under the MIT license.  A copy of the
 * license can be found in the LICENSE file in the project root, or at
 * https://opensource.org/licenses/MIT.
 */
/**
 * @file
 * Backend of the disassembler.
 */
#ifndef CHIP8_DISASSEMBLER_H
#define CHIP8_DISASSEMBLER_H

#include <stdbool.h>
#include <stdio.h>

/**
 * Options which can be set for the disassembler.
 */
struct chip8disasm_options {
    /**
     * Whether to use shift quirks mode.
     */
    bool shift_quirks;
};

/**
 * Contains the state of the disassembler.
 */
struct chip8disasm;

/**
 * Creates a new disassembler from the given program file.
 *
 * @return The new disassembler, or NULL if creation failed.
 */
struct chip8disasm *chip8disasm_from_file(
    struct chip8disasm_options opts, const char *fname);
/**
 * Destroys the given disassembler.
 */
void chip8disasm_destroy(struct chip8disasm *disasm);

/**
 * Dumps the disassembly to the given file.
 *
 * @return An error code.
 */
int chip8disasm_dump(const struct chip8disasm *disasm, FILE *out);

/**
 * Returns the default set of options for the disassembler.
 */
struct chip8disasm_options chip8disasm_options_default(void);

#endif
