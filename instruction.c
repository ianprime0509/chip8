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
#include "instruction.h"

/**
 * Returns the `Vx` part of the given opcode.
 */
#define OPCODE_VX(opcode) (((opcode)&0xF00) >> 8)
/**
 * Returns the `Vy` part of the given opcode.
 */
#define OPCODE_VY(opcode) (((opcode)&0xF0) >> 4)
/**
 * Returns the `addr` part of the given opcode.
 */
#define OPCODE_ADDR(opcode) ((opcode)&0xFFF)
/**
 * Returns the `byte` part of the given opcode.
 */
#define OPCODE_BYTE(opcode) ((opcode)&0xFF)
/**
 * Returns the `nibble` part of the given opcode.
 */
#define OPCODE_NIBBLE(opcode) ((opcode)&0xF)

struct chip8_instruction chip8_instruction_from_opcode(uint16_t opcode)
{
    struct chip8_instruction ins;

    /*
     * If we don't detect an instruction in the switch below, the opcode is
     * invalid.
     */
    ins.op = OP_INVALID;

    switch ((opcode & 0xF000) >> 12) {
    case 0x0:
        if ((opcode & 0xF0) == 0xC0) {
            ins.op = OP_SCD;
            ins.nibble = OPCODE_NIBBLE(opcode);
        } else {
            switch (opcode & 0xFF) {
            case 0xE0:
                ins.op = OP_CLS;
                break;
            case 0xEE:
                ins.op = OP_RET;
                break;
            case 0xFB:
                ins.op = OP_SCR;
                break;
            case 0xFC:
                ins.op = OP_SCL;
                break;
            case 0xFD:
                ins.op = OP_EXIT;
                break;
            case 0xFE:
                ins.op = OP_LOW;
                break;
            case 0xFF:
                ins.op = OP_HIGH;
                break;
            }
        }
        break;
    case 0x1:
        ins.op = OP_JP;
        ins.addr = OPCODE_ADDR(opcode);
        break;
    case 0x2:
        ins.op = OP_CALL;
        ins.addr = OPCODE_ADDR(opcode);
        break;
    case 0x3:
        ins.op = OP_SE_BYTE;
        ins.vx = OPCODE_VX(opcode);
        ins.byte = OPCODE_BYTE(opcode);
        break;
    case 0x4:
        ins.op = OP_SNE_BYTE;
        ins.vx = OPCODE_VX(opcode);
        ins.byte = OPCODE_BYTE(opcode);
        break;
    case 0x5:
        if ((opcode & 0xF) == 0) {
            ins.op = OP_SE_REG;
            ins.vx = OPCODE_VX(opcode);
            ins.byte = OPCODE_BYTE(opcode);
        }
        break;
    case 0x6:
        ins.op = OP_LD_BYTE;
        ins.vx = OPCODE_VX(opcode);
        ins.byte = OPCODE_BYTE(opcode);
        break;
    case 0x7:
        ins.op = OP_ADD_BYTE;
        ins.vx = OPCODE_VX(opcode);
        ins.byte = OPCODE_BYTE(opcode);
        break;
    case 0x8:
        switch (opcode & 0xF) {
        case 0x0:
            ins.op = OP_LD_REG;
            ins.vx = OPCODE_VX(opcode);
            ins.vy = OPCODE_VY(opcode);
            break;
        case 0x1:
            ins.op = OP_OR;
            ins.vx = OPCODE_VX(opcode);
            ins.vy = OPCODE_VY(opcode);
            break;
        case 0x2:
            ins.op = OP_AND;
            ins.vx = OPCODE_VX(opcode);
            ins.vy = OPCODE_VY(opcode);
            break;
        case 0x3:
            ins.op = OP_XOR;
            ins.vx = OPCODE_VX(opcode);
            ins.vy = OPCODE_VY(opcode);
            break;
        case 0x4:
            ins.op = OP_ADD_REG;
            ins.vx = OPCODE_VX(opcode);
            ins.vy = OPCODE_VY(opcode);
            break;
        case 0x5:
            ins.op = OP_SUB;
            ins.vx = OPCODE_VX(opcode);
            ins.vy = OPCODE_VY(opcode);
            break;
        case 0x6:
            if ((opcode & 0xF0) == 0x00) {
                ins.op = OP_SHR;
                ins.vx = OPCODE_VX(opcode);
            }
            break;
        case 0x7:
            ins.op = OP_SUBN;
            ins.vx = OPCODE_VX(opcode);
            ins.vy = OPCODE_VY(opcode);
            break;
        case 0xE:
            if ((opcode & 0xF0) == 0x00) {
                ins.op = OP_SHL;
                ins.vx = OPCODE_VX(opcode);
            }
            break;
        }
        break;
    case 0x9:
        if ((opcode & 0xF) == 0) {
            ins.op = OP_SNE_REG;
            ins.vx = OPCODE_VX(opcode);
            ins.vy = OPCODE_VY(opcode);
        }
        break;
    case 0xA:
        ins.op = OP_LD_I;
        ins.addr = OPCODE_ADDR(opcode);
        break;
    case 0xB:
        ins.op = OP_JP_V0;
        ins.addr = OPCODE_ADDR(opcode);
        break;
    case 0xC:
        ins.op = OP_RND;
        ins.vx = OPCODE_VX(opcode);
        ins.byte = OPCODE_BYTE(opcode);
        break;
    case 0xD:
        ins.op = OP_DRW;
        ins.vx = OPCODE_VX(opcode);
        ins.vy = OPCODE_VY(opcode);
        ins.nibble = OPCODE_NIBBLE(opcode);
        break;
    case 0xE:
        switch (opcode & 0xFF) {
        case 0x9E:
            ins.op = OP_SKP;
            ins.vx = OPCODE_VX(opcode);
            break;
        case 0xA1:
            ins.op = OP_SKNP;
            ins.vx = OPCODE_VX(opcode);
            break;
        }
        break;
    case 0xF:
        switch (opcode & 0xFF) {
        case 0x07:
            ins.op = OP_LD_REG_DT;
            ins.vx = OPCODE_VX(opcode);
            break;
        case 0x0A:
            ins.op = OP_LD_KEY;
            ins.vx = OPCODE_VX(opcode);
            break;
        case 0x15:
            ins.op = OP_LD_DT_REG;
            ins.vx = OPCODE_VX(opcode);
            break;
        case 0x18:
            ins.op = OP_LD_ST;
            ins.vx = OPCODE_VX(opcode);
            break;
        case 0x1E:
            ins.op = OP_ADD_I;
            ins.vx = OPCODE_VX(opcode);
            break;
        case 0x29:
            ins.op = OP_LD_F;
            ins.vx = OPCODE_VX(opcode);
            break;
        case 0x30:
            ins.op = OP_LD_HF;
            ins.vx = OPCODE_VX(opcode);
            break;
        case 0x33:
            ins.op = OP_LD_B;
            ins.vx = OPCODE_VX(opcode);
            break;
        case 0x55:
            ins.op = OP_LD_DEREF_I_REG;
            ins.vx = OPCODE_VX(opcode);
            break;
        case 0x65:
            ins.op = OP_LD_REG_DEREF_I;
            ins.vx = OPCODE_VX(opcode);
            break;
        }
    }

    return ins;
}
