/*
 * Copyright 2018 Ian Johnson
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
 * Backend of the disassembler.
 */
#ifndef CHIP8_DISASSEMBLER_H
#define CHIP8_DISASSEMBLER_H

#include <stdio.h>

/**
 * Contains the state of the disassembler.
 */
struct chip8disasm;

/**
 * Creates a new disassembler from the given program file.
 *
 * @return The new disassembler, or NULL if creation failed.
 */
struct chip8disasm *chip8disasm_from_file(const char *fname);
/**
 * Destroys the given disassembler.
 */
void chip8disasm_destroy(struct chip8disasm *disasm);

#endif
