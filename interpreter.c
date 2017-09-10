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

#include <stdlib.h>
#include <string.h>

struct chip8 *chip8_new(void)
{
    struct chip8 *chip = malloc(sizeof *chip);

    memset(chip->mem, 0, sizeof chip->mem);
    for (int i = 0; i < CHIP8_DISPLAY_WIDTH; i++)
        for (int j = 0; j < CHIP8_DISPLAY_HEIGHT; j++)
            chip->display[i][j] = false;
    memset(chip->regs, 0, sizeof chip->regs);
    chip->reg_i = chip->reg_dt = chip->reg_st = 0;
    /* Start program at beginning of usable memory */
    chip->pc = 0x200;
    chip->halted = false;
    chip->highres = false;
    chip->needs_refresh = true;

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
    return chip8_instruction_from_opcode(opcode);
}

void chip8_execute(struct chip8 *chip, struct chip8_instruction inst)
{
    /* TODO */
}

void chip8_run(struct chip8 *chip)
{
    while (!chip->halted)
        chip8_step(chip);
}

void chip8_step(struct chip8 *chip)
{
    chip8_execute(chip, chip8_current_instr(chip));
}
