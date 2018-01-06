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
#include "instruction.h"

#include <stdio.h>

/**
 * Returns the `Vx` part of the given opcode.
 */
#define OPCODE2VX(opcode) (((opcode)&0xF00) >> 8)
/**
 * Returns the `Vy` part of the given opcode.
 */
#define OPCODE2VY(opcode) (((opcode)&0xF0) >> 4)
/**
 * Returns the `addr` part of the given opcode.
 */
#define OPCODE2ADDR(opcode) ((opcode)&0xFFF)
/**
 * Returns the `byte` part of the given opcode.
 */
#define OPCODE2BYTE(opcode) ((opcode)&0xFF)
/**
 * Returns the `nibble` part of the given opcode.
 */
#define OPCODE2NIBBLE(opcode) ((opcode)&0xF)

/**
 * Returns the opcode bits corresponding to `Vx`.
 */
#define VX2OPCODE(vx) (((vx)&0xF) << 8)
/**
 * Returns the opcode bits corresponding to `Vy`.
 */
#define VY2OPCODE(vy) (((vy)&0xF) << 4)
/**
 * Returns the opcode bits corresponding to `addr`.
 */
#define ADDR2OPCODE(addr) ((addr)&0xFFF)
/**
 * Returns the opcode bits corresponding to `byte`.
 */
#define BYTE2OPCODE(byte) ((byte)&0xFF)
/**
 * Returns the opcode bits corresponding to `nibble`.
 */
#define NIBBLE2OPCODE(nibble) ((nibble)&0xF)

struct chip8_instruction chip8_instruction_from_opcode(uint16_t opcode,
                                                       bool shift_quirks)
{
    struct chip8_instruction ins;

    /*
     * If we don't detect an instruction in the switch below, the opcode is
     * invalid.
     */
    ins.op = OP_INVALID;
    ins.opcode = opcode;

    switch ((opcode & 0xF000) >> 12) {
    case 0x0:
        if ((opcode & 0xF0) == 0xC0) {
            ins.op = OP_SCD;
            ins.nibble = OPCODE2NIBBLE(opcode);
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
        ins.addr = OPCODE2ADDR(opcode);
        break;
    case 0x2:
        ins.op = OP_CALL;
        ins.addr = OPCODE2ADDR(opcode);
        break;
    case 0x3:
        ins.op = OP_SE_BYTE;
        ins.vx = OPCODE2VX(opcode);
        ins.byte = OPCODE2BYTE(opcode);
        break;
    case 0x4:
        ins.op = OP_SNE_BYTE;
        ins.vx = OPCODE2VX(opcode);
        ins.byte = OPCODE2BYTE(opcode);
        break;
    case 0x5:
        if ((opcode & 0xF) == 0) {
            ins.op = OP_SE_REG;
            ins.vx = OPCODE2VX(opcode);
            ins.vy = OPCODE2VY(opcode);
        }
        break;
    case 0x6:
        ins.op = OP_LD_BYTE;
        ins.vx = OPCODE2VX(opcode);
        ins.byte = OPCODE2BYTE(opcode);
        break;
    case 0x7:
        ins.op = OP_ADD_BYTE;
        ins.vx = OPCODE2VX(opcode);
        ins.byte = OPCODE2BYTE(opcode);
        break;
    case 0x8:
        switch (opcode & 0xF) {
        case 0x0:
            ins.op = OP_LD_REG;
            ins.vx = OPCODE2VX(opcode);
            ins.vy = OPCODE2VY(opcode);
            break;
        case 0x1:
            ins.op = OP_OR;
            ins.vx = OPCODE2VX(opcode);
            ins.vy = OPCODE2VY(opcode);
            break;
        case 0x2:
            ins.op = OP_AND;
            ins.vx = OPCODE2VX(opcode);
            ins.vy = OPCODE2VY(opcode);
            break;
        case 0x3:
            ins.op = OP_XOR;
            ins.vx = OPCODE2VX(opcode);
            ins.vy = OPCODE2VY(opcode);
            break;
        case 0x4:
            ins.op = OP_ADD_REG;
            ins.vx = OPCODE2VX(opcode);
            ins.vy = OPCODE2VY(opcode);
            break;
        case 0x5:
            ins.op = OP_SUB;
            ins.vx = OPCODE2VX(opcode);
            ins.vy = OPCODE2VY(opcode);
            break;
        case 0x6:
            if (shift_quirks) {
                ins.op = OP_SHR;
                ins.vx = OPCODE2VX(opcode);
                ins.vy = OPCODE2VY(opcode);
            } else {
                if ((opcode & 0xF0) == 0x00) {
                    ins.op = OP_SHR;
                    ins.vx = OPCODE2VX(opcode);
                }
            }
            break;
        case 0x7:
            ins.op = OP_SUBN;
            ins.vx = OPCODE2VX(opcode);
            ins.vy = OPCODE2VY(opcode);
            break;
        case 0xE:
            if (shift_quirks) {
                ins.op = OP_SHL;
                ins.vx = OPCODE2VX(opcode);
                ins.vy = OPCODE2VY(opcode);
            } else {
                if ((opcode & 0xF0) == 0x00) {
                    ins.op = OP_SHL;
                    ins.vx = OPCODE2VX(opcode);
                }
            }
            break;
        }
        break;
    case 0x9:
        if ((opcode & 0xF) == 0) {
            ins.op = OP_SNE_REG;
            ins.vx = OPCODE2VX(opcode);
            ins.vy = OPCODE2VY(opcode);
        }
        break;
    case 0xA:
        ins.op = OP_LD_I;
        ins.addr = OPCODE2ADDR(opcode);
        break;
    case 0xB:
        ins.op = OP_JP_V0;
        ins.addr = OPCODE2ADDR(opcode);
        break;
    case 0xC:
        ins.op = OP_RND;
        ins.vx = OPCODE2VX(opcode);
        ins.byte = OPCODE2BYTE(opcode);
        break;
    case 0xD:
        ins.op = OP_DRW;
        ins.vx = OPCODE2VX(opcode);
        ins.vy = OPCODE2VY(opcode);
        ins.nibble = OPCODE2NIBBLE(opcode);
        break;
    case 0xE:
        switch (opcode & 0xFF) {
        case 0x9E:
            ins.op = OP_SKP;
            ins.vx = OPCODE2VX(opcode);
            break;
        case 0xA1:
            ins.op = OP_SKNP;
            ins.vx = OPCODE2VX(opcode);
            break;
        }
        break;
    case 0xF:
        switch (opcode & 0xFF) {
        case 0x07:
            ins.op = OP_LD_REG_DT;
            ins.vx = OPCODE2VX(opcode);
            break;
        case 0x0A:
            ins.op = OP_LD_KEY;
            ins.vx = OPCODE2VX(opcode);
            break;
        case 0x15:
            ins.op = OP_LD_DT_REG;
            ins.vx = OPCODE2VX(opcode);
            break;
        case 0x18:
            ins.op = OP_LD_ST;
            ins.vx = OPCODE2VX(opcode);
            break;
        case 0x1E:
            ins.op = OP_ADD_I;
            ins.vx = OPCODE2VX(opcode);
            break;
        case 0x29:
            ins.op = OP_LD_F;
            ins.vx = OPCODE2VX(opcode);
            break;
        case 0x30:
            ins.op = OP_LD_HF;
            ins.vx = OPCODE2VX(opcode);
            break;
        case 0x33:
            ins.op = OP_LD_B;
            ins.vx = OPCODE2VX(opcode);
            break;
        case 0x55:
            ins.op = OP_LD_DEREF_I_REG;
            ins.vx = OPCODE2VX(opcode);
            break;
        case 0x65:
            ins.op = OP_LD_REG_DEREF_I;
            ins.vx = OPCODE2VX(opcode);
            break;
        case 0x75:
            ins.op = OP_LD_R_REG;
            ins.vx = OPCODE2VX(opcode);
            break;
        case 0x85:
            ins.op = OP_LD_REG_R;
            ins.vx = OPCODE2VX(opcode);
            break;
        }
    }

    return ins;
}

uint16_t chip8_instruction_to_opcode(struct chip8_instruction instr,
                                     bool shift_quirks)
{
    switch (instr.op) {
    case OP_INVALID:
        return instr.opcode;
    case OP_SCD:
        return 0x00C0 | NIBBLE2OPCODE(instr.nibble);
    case OP_CLS:
        return 0x00E0;
    case OP_RET:
        return 0x00EE;
    case OP_SCR:
        return 0x00FB;
    case OP_SCL:
        return 0x00FC;
    case OP_EXIT:
        return 0x00FD;
    case OP_LOW:
        return 0x00FE;
    case OP_HIGH:
        return 0x00FF;
    case OP_JP:
        return 0x1000 | ADDR2OPCODE(instr.addr);
    case OP_CALL:
        return 0x2000 | ADDR2OPCODE(instr.addr);
    case OP_SE_BYTE:
        return 0x3000 | VX2OPCODE(instr.vx) | BYTE2OPCODE(instr.byte);
    case OP_SNE_BYTE:
        return 0x4000 | VX2OPCODE(instr.vx) | BYTE2OPCODE(instr.byte);
    case OP_SE_REG:
        return 0x5000 | VX2OPCODE(instr.vx) | VY2OPCODE(instr.vy);
    case OP_LD_BYTE:
        return 0x6000 | VX2OPCODE(instr.vx) | BYTE2OPCODE(instr.byte);
    case OP_ADD_BYTE:
        return 0x7000 | VX2OPCODE(instr.vx) | BYTE2OPCODE(instr.byte);
    case OP_LD_REG:
        return 0x8000 | VX2OPCODE(instr.vx) | VY2OPCODE(instr.vy);
    case OP_OR:
        return 0x8001 | VX2OPCODE(instr.vx) | VY2OPCODE(instr.vy);
    case OP_AND:
        return 0x8002 | VX2OPCODE(instr.vx) | VY2OPCODE(instr.vy);
    case OP_XOR:
        return 0x8003 | VX2OPCODE(instr.vx) | VY2OPCODE(instr.vy);
    case OP_ADD_REG:
        return 0x8004 | VX2OPCODE(instr.vx) | VY2OPCODE(instr.vy);
    case OP_SUB:
        return 0x8005 | VX2OPCODE(instr.vx) | VY2OPCODE(instr.vy);
    case OP_SHR:
        if (shift_quirks)
            return 0x8006 | VX2OPCODE(instr.vx) | VY2OPCODE(instr.vy);
        else
            return 0x8006 | VX2OPCODE(instr.vx);
    case OP_SUBN:
        return 0x8007 | VX2OPCODE(instr.vx) | VY2OPCODE(instr.vy);
    case OP_SHL:
        if (shift_quirks)
            return 0x800E | VX2OPCODE(instr.vx) | VY2OPCODE(instr.vy);
        else
            return 0x800E | VX2OPCODE(instr.vx);
    case OP_SNE_REG:
        return 0x9000 | VX2OPCODE(instr.vx) | VY2OPCODE(instr.vy);
    case OP_LD_I:
        return 0xA000 | ADDR2OPCODE(instr.addr);
    case OP_JP_V0:
        return 0xB000 | ADDR2OPCODE(instr.addr);
    case OP_RND:
        return 0xC000 | VX2OPCODE(instr.vx) | BYTE2OPCODE(instr.byte);
    case OP_DRW:
        return 0xD000 | VX2OPCODE(instr.vx) | VY2OPCODE(instr.vy) |
               NIBBLE2OPCODE(instr.nibble);
    case OP_SKP:
        return 0xE09E | VX2OPCODE(instr.vx);
    case OP_SKNP:
        return 0xE0A1 | VX2OPCODE(instr.vx);
    case OP_LD_REG_DT:
        return 0xF007 | VX2OPCODE(instr.vx);
    case OP_LD_KEY:
        return 0xF00A | VX2OPCODE(instr.vx);
    case OP_LD_DT_REG:
        return 0xF015 | VX2OPCODE(instr.vx);
    case OP_LD_ST:
        return 0xF018 | VX2OPCODE(instr.vx);
    case OP_ADD_I:
        return 0xF01E | VX2OPCODE(instr.vx);
    case OP_LD_F:
        return 0xF029 | VX2OPCODE(instr.vx);
    case OP_LD_HF:
        return 0xF030 | VX2OPCODE(instr.vx);
    case OP_LD_B:
        return 0xF033 | VX2OPCODE(instr.vx);
    case OP_LD_DEREF_I_REG:
        return 0xF055 | VX2OPCODE(instr.vx);
    case OP_LD_REG_DEREF_I:
        return 0xF065 | VX2OPCODE(instr.vx);
    case OP_LD_R_REG:
        return 0xF075 | VX2OPCODE(instr.vx);
    case OP_LD_REG_R:
        return 0xF085 | VX2OPCODE(instr.vx);
    }

    return 0;
}

void chip8_instruction_format(struct chip8_instruction instr, char *dest,
                              size_t sz, bool shift_quirks)
{
    switch (instr.op) {
    case OP_INVALID:
        snprintf(dest, sz, "INVALID (DW #%04X)", instr.opcode);
        break;
    case OP_SCD:
        snprintf(dest, sz, "SCD %u", instr.nibble);
        break;
    case OP_CLS:
        snprintf(dest, sz, "CLS");
        break;
    case OP_RET:
        snprintf(dest, sz, "RET");
        break;
    case OP_SCR:
        snprintf(dest, sz, "SCR");
        break;
    case OP_SCL:
        snprintf(dest, sz, "SCL");
        break;
    case OP_EXIT:
        snprintf(dest, sz, "EXIT");
        break;
    case OP_LOW:
        snprintf(dest, sz, "LOW");
        break;
    case OP_HIGH:
        snprintf(dest, sz, "HIGH");
        break;
    case OP_JP:
        snprintf(dest, sz, "JP #%03X", instr.addr);
        break;
    case OP_CALL:
        snprintf(dest, sz, "CALL #%03X", instr.addr);
        break;
    case OP_SE_BYTE:
        snprintf(dest, sz, "SE V%X, #%02X", instr.vx, instr.byte);
        break;
    case OP_SNE_BYTE:
        snprintf(dest, sz, "SNE V%X, #%02X", instr.vx, instr.byte);
        break;
    case OP_SE_REG:
        snprintf(dest, sz, "SE V%X, V%X", instr.vx, instr.vy);
        break;
    case OP_LD_BYTE:
        snprintf(dest, sz, "LD V%X, #%02X", instr.vx, instr.byte);
        break;
    case OP_ADD_BYTE:
        snprintf(dest, sz, "ADD V%X, #%02X", instr.vx, instr.byte);
        break;
    case OP_LD_REG:
        snprintf(dest, sz, "LD V%X, V%X", instr.vx, instr.vy);
        break;
    case OP_OR:
        snprintf(dest, sz, "OR V%X, V%X", instr.vx, instr.vy);
        break;
    case OP_AND:
        snprintf(dest, sz, "AND V%X, V%X", instr.vx, instr.vy);
        break;
    case OP_XOR:
        snprintf(dest, sz, "XOR V%X, V%X", instr.vx, instr.vy);
        break;
    case OP_ADD_REG:
        snprintf(dest, sz, "ADD V%X, V%X", instr.vx, instr.vy);
        break;
    case OP_SUB:
        snprintf(dest, sz, "SUB V%X, V%X", instr.vx, instr.vy);
        break;
    case OP_SHR:
        if (shift_quirks)
            snprintf(dest, sz, "SHR V%X, V%X", instr.vx, instr.vy);
        else
            snprintf(dest, sz, "SHR V%X", instr.vx);
        break;
    case OP_SUBN:
        snprintf(dest, sz, "SUBN V%X, V%X", instr.vx, instr.vy);
        break;
    case OP_SHL:
        if (shift_quirks)
            snprintf(dest, sz, "SHL V%X, V%X", instr.vx, instr.vy);
        else
            snprintf(dest, sz, "SHL V%X", instr.vx);
        break;
    case OP_SNE_REG:
        snprintf(dest, sz, "SNE V%X, V%X", instr.vx, instr.vy);
        break;
    case OP_LD_I:
        snprintf(dest, sz, "LD I, #%03X", instr.addr);
        break;
    case OP_JP_V0:
        snprintf(dest, sz, "JP V0, #%03X", instr.addr);
        break;
    case OP_RND:
        snprintf(dest, sz, "RND V%X, #%02X", instr.vx, instr.byte);
        break;
    case OP_DRW:
        snprintf(dest, sz, "DRW V%X, V%X, %u", instr.vx, instr.vy,
                 instr.nibble);
        break;
    case OP_SKP:
        snprintf(dest, sz, "SKP V%X", instr.vx);
        break;
    case OP_SKNP:
        snprintf(dest, sz, "SKNP V%X", instr.vx);
        break;
    case OP_LD_REG_DT:
        snprintf(dest, sz, "LD V%X, DT", instr.vx);
        break;
    case OP_LD_KEY:
        snprintf(dest, sz, "LD V%X, K", instr.vx);
        break;
    case OP_LD_DT_REG:
        snprintf(dest, sz, "LD DT, V%X", instr.vx);
        break;
    case OP_LD_ST:
        snprintf(dest, sz, "LD ST, V%X", instr.vx);
        break;
    case OP_ADD_I:
        snprintf(dest, sz, "ADD I, V%X", instr.vx);
        break;
    case OP_LD_F:
        snprintf(dest, sz, "LD F, V%X", instr.vx);
        break;
    case OP_LD_HF:
        snprintf(dest, sz, "LD HF, V%X", instr.vx);
        break;
    case OP_LD_B:
        snprintf(dest, sz, "LD B, V%X", instr.vx);
        break;
    case OP_LD_DEREF_I_REG:
        snprintf(dest, sz, "LD [I], V%X", instr.vx);
        break;
    case OP_LD_REG_DEREF_I:
        snprintf(dest, sz, "LD V%X, [I]", instr.vx);
        break;
    case OP_LD_R_REG:
        snprintf(dest, sz, "LD R, V%X", instr.vx);
        break;
    case OP_LD_REG_R:
        snprintf(dest, sz, "LD V%X, R", instr.vx);
        break;
    }
}
