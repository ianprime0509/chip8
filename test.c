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
 * Simple unit tests for the project.
 * Tests are just functions with the signature `int test_func(void)`, which
 * return 0 on success or 1 on failure, and they should be registered in the
 * `main` function of this file.
 */

#include <stdint.h>
#include <stdio.h>

#include "assembler.h"
#include "interpreter.h"

#define TEST_RUN(test) testing_run(#test, test)
#define ASSERT(cond)                                                           \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "ERR: assertion failed on line %d: " #cond "\n",   \
                    __LINE__);                                                 \
            return 1;                                                          \
        }                                                                      \
    } while (0)
#define ASSERT_EQ(lhs, rhs)                                                    \
    do {                                                                       \
        if ((lhs) != (rhs)) {                                                  \
            fprintf(stderr, "ERR: assertion failed on line %d: " #lhs          \
                            " == " #rhs "\n",                                  \
                    __LINE__);                                                 \
            fprintf(stderr, "     LHS = ");                                    \
            fprintf(stderr, FMTSTRING(lhs), (lhs));                            \
            fprintf(stderr, "\n");                                             \
            fprintf(stderr, "     RHS = ");                                    \
            fprintf(stderr, FMTSTRING(rhs), (rhs));                            \
            fprintf(stderr, "\n");                                             \
            return 1;                                                          \
        }                                                                      \
    } while (0)
/**
 * Returns the proper format string for the type of the given value.
 * TODO: add more cases as needed.
 */
#define FMTSTRING(expr)                                                        \
    _Generic((expr), signed char                                               \
             : "%hhd", unsigned char                                           \
             : "%hhu", short                                                   \
             : "%hd", unsigned short                                           \
             : "%hu", int                                                      \
             : "%d", unsigned int                                              \
             : "%u")

static int n_failures;

static void testing_run(const char *name, int (*test)(void));
static void testing_setup(void);
static int testing_teardown(void);

static struct chip8_options chip8_options_testing(void);

/**
 * Tests Chip-8 arithmetic instruction evaluation.
 */
int test_arithmetic(void);
/**
 * Tests assembly of instructions.
 */
int test_asm(void);
/**
 * Tests assembly instruction alignment.
 */
int test_asm_align(void);
/**
 * Tests evaluation of expressions in assembler.
 */
int test_asm_eval(void);
/**
 * Tests Chip-8 comparison instruction evaluation.
 */
int test_comparison(void);
/**
 * Tests Chip-8 memory instruction evaluation.
 */
int test_ld(void);

int main(void)
{
    testing_setup();
    TEST_RUN(test_arithmetic);
    TEST_RUN(test_asm);
    TEST_RUN(test_asm_align);
    TEST_RUN(test_asm_eval);
    TEST_RUN(test_comparison);
    TEST_RUN(test_ld);
    return testing_teardown();
}

int test_arithmetic(void)
{
    struct chip8_options opts = chip8_options_testing();
    struct chip8 *chip = chip8_new(opts);

    /* LD V0, #66 */
    chip8_execute_opcode(chip, 0x6066);
    /* ADD V0, #0A */
    chip8_execute_opcode(chip, 0x700A);
    ASSERT_EQ(chip->regs[REG_V0], 0x70);
    ASSERT_EQ(chip->regs[REG_VF], 0);
    /* ADD V0, 0xFF */
    chip8_execute_opcode(chip, 0x70FF);
    ASSERT_EQ(chip->regs[REG_V0], 0x6F);
    ASSERT_EQ(chip->regs[REG_VF], 1);
    /* LD V1, 0x10 */
    chip8_execute_opcode(chip, 0x6110);
    /* OR V0, V1 */
    chip8_execute_opcode(chip, 0x8011);
    ASSERT_EQ(chip->regs[REG_V0], 0x7F);
    /* LD V2, 0xF0 */
    chip8_execute_opcode(chip, 0x62F0);
    /* AND V0, V2 */
    chip8_execute_opcode(chip, 0x8022);
    ASSERT_EQ(chip->regs[REG_V0], 0x70);
    /* XOR V0, V2 */
    chip8_execute_opcode(chip, 0x8023);
    ASSERT_EQ(chip->regs[REG_V0], 0x80);
    /* ADD V0, V2 */
    chip8_execute_opcode(chip, 0x8024);
    ASSERT_EQ(chip->regs[REG_V0], 0x70);
    ASSERT_EQ(chip->regs[REG_VF], 1);
    /* LD V1, 0x70 */
    chip8_execute_opcode(chip, 0x6170);
    /* ADD V0, V1 */
    chip8_execute_opcode(chip, 0x8014);
    ASSERT_EQ(chip->regs[REG_V0], 0xE0);
    ASSERT_EQ(chip->regs[REG_VF], 0);
    /* SUB V0, V1 */
    chip8_execute_opcode(chip, 0x8015);
    ASSERT_EQ(chip->regs[REG_V0], 0x70);
    ASSERT_EQ(chip->regs[REG_VF], 1);
    /* SUB V0, V2 */
    chip8_execute_opcode(chip, 0x8025);
    ASSERT_EQ(chip->regs[REG_V0], 0x80);
    ASSERT_EQ(chip->regs[REG_VF], 0);
    /* LD V3, #07 */
    chip8_execute_opcode(chip, 0x6307);
    /* SHR V3 */
    chip8_execute_opcode(chip, 0x8306);
    ASSERT_EQ(chip->regs[REG_V3], 0x03);
    ASSERT_EQ(chip->regs[REG_VF], 1);
    /* SHL V3 */
    chip8_execute_opcode(chip, 0x830E);
    ASSERT_EQ(chip->regs[REG_V3], 0x06);
    ASSERT_EQ(chip->regs[REG_VF], 0);
    /* SUBN V3, V0 */
    chip8_execute_opcode(chip, 0x8307);
    ASSERT_EQ(chip->regs[REG_V3], 0x7A);
    ASSERT_EQ(chip->regs[REG_VF], 1);

    chip8_destroy(chip);
    return 0;
}

int test_asm(void)
{
#define TEST_INSTR(instr, opcode)                                              \
    ASSERT(!chip8asm_process_line(chipasm, (instr)));                          \
    ASSERT(!chip8asm_emit(chipasm, prog));                                     \
    ASSERT_EQ(chip8asm_program_opcode(prog, prog->len - 2), (opcode))

    struct chip8asm *chipasm = chip8asm_new(chip8asm_options_default());
    struct chip8asm_program *prog = chip8asm_program_new();

    ASSERT(!chip8asm_process_line(chipasm, "program_start:"));
    TEST_INSTR("SCD 7", 0x00C7);
    TEST_INSTR("CLS", 0x00E0);
    TEST_INSTR("RET", 0x00EE);
    TEST_INSTR("SCR", 0x00FB);
    TEST_INSTR("SCL", 0x00FC);
    TEST_INSTR("EXIT", 0x00FD);
    TEST_INSTR("LOW", 0x00FE);
    TEST_INSTR("HIGH", 0x00FF);
    TEST_INSTR("JP program_start", 0x1200);
    TEST_INSTR("CALL program_start", 0x2200);
    TEST_INSTR("SE V8, #45", 0x3845);
    TEST_INSTR("SNE VA, #90", 0x4A90);
    TEST_INSTR("SE VE, V0", 0x5E00);
    TEST_INSTR("LD V1, $1101", 0x610D);
    TEST_INSTR("ADD V4, 10", 0x740A);
    TEST_INSTR("LD V7, VB", 0x87B0);
    TEST_INSTR("OR VD, VC", 0x8DC1);
    TEST_INSTR("AND VB, V6", 0x8B62);
    TEST_INSTR("XOR V5, V0", 0x8503);
    TEST_INSTR("ADD V5, V9", 0x8594);
    TEST_INSTR("SUB VD, VA", 0x8DA5);
    TEST_INSTR("SHR V3", 0x8306);
    TEST_INSTR("SUBN V9, VC", 0x89C7);
    TEST_INSTR("SHL VF", 0x8F0E);
    TEST_INSTR("SNE V8, V2", 0x9820);
    TEST_INSTR("LD I, program_start", 0xA200);
    TEST_INSTR("JP V0, program_start", 0xB200);
    TEST_INSTR("RND V0, #F5", 0xC0F5);
    TEST_INSTR("DRW V0, V1, 10", 0xD01A);
    TEST_INSTR("SKP V4", 0xE49E);
    TEST_INSTR("SKNP VD", 0xEDA1);
    TEST_INSTR("LD V8, DT", 0xF807);
    TEST_INSTR("LD VD, K", 0xFD0A);
    TEST_INSTR("LD DT, VA", 0xFA15);
    TEST_INSTR("LD ST, V6", 0xF618);
    TEST_INSTR("ADD I, V3", 0xF31E);
    TEST_INSTR("LD F, V8", 0xF829);
    TEST_INSTR("LD HF, VE", 0xFE30);
    TEST_INSTR("LD B, V6", 0xF633);
    TEST_INSTR("LD [I], V2", 0xF255);
    TEST_INSTR("LD V7, [I]", 0xF765);
    TEST_INSTR("LD R, V1", 0xF175);
    TEST_INSTR("LD VB, R", 0xFB85);

    /* Do a couple checks for case-insensitivity */
    TEST_INSTR("lD v5, [i]", 0xF565);
    TEST_INSTR("Ld sT, Va", 0xFA18);
    TEST_INSTR("hIgH", 0x00FF);

    /* But labels are case-sensitive */
    ASSERT(!chip8asm_process_line(chipasm, "JP PROGRAM_START"));
    ASSERT(chip8asm_emit(chipasm, prog));

    chip8asm_program_destroy(prog);
    chip8asm_destroy(chipasm);
    return 0;

#undef TEST_INSTR
}

int test_asm_align(void)
{
    struct chip8asm *chipasm = chip8asm_new(chip8asm_options_default());
    struct chip8asm_program *prog = chip8asm_program_new();

    ASSERT(!chip8asm_process_line(chipasm, "DW #1234"));
    ASSERT(!chip8asm_process_line(chipasm, "DB #56"));
    ASSERT(!chip8asm_process_line(chipasm, "DW #789A"));
    ASSERT(!chip8asm_process_line(chipasm, "JP #200"));
    ASSERT(!chip8asm_process_line(chipasm, "DB #BC"));
    /* Labels must also be aligned */
    ASSERT(!chip8asm_process_line(chipasm, "lbl:"));
    ASSERT(!chip8asm_process_line(chipasm, "JP lbl"));
    ASSERT(!chip8asm_emit(chipasm, prog));

    ASSERT_EQ(prog->mem[0], 0x12);
    ASSERT_EQ(prog->mem[1], 0x34);
    ASSERT_EQ(prog->mem[2], 0x56);
    ASSERT_EQ(prog->mem[3], 0x78);
    ASSERT_EQ(prog->mem[4], 0x9A);
    /* The JP instruction must be aligned */
    ASSERT_EQ(prog->mem[5], 0x00);
    ASSERT_EQ(prog->mem[6], 0x12);
    ASSERT_EQ(prog->mem[7], 0x00);
    ASSERT_EQ(prog->mem[8], 0xBC);
    ASSERT_EQ(prog->mem[9], 0x00);
    ASSERT_EQ(prog->mem[10], 0x12);
    ASSERT_EQ(prog->mem[11], 0x0A);

    chip8asm_program_destroy(prog);
    chip8asm_destroy(chipasm);
    return 0;
}

int test_asm_eval(void)
{
    struct chip8asm *chipasm = chip8asm_new(chip8asm_options_default());
    uint16_t value;

    chip8asm_eval(chipasm, "2 + #F - $10", 1, &value);
    ASSERT_EQ(value, 15);
    chip8asm_eval(chipasm, "-1", 2, &value);
    ASSERT_EQ(value, UINT16_MAX);
    chip8asm_eval(chipasm, "2 + 3 * 1", 3, &value);
    ASSERT_EQ(value, 5);
    chip8asm_eval(chipasm, "((4 + 4) * (#0a - $00000010))", 4, &value);
    ASSERT_EQ(value, 64);
    chip8asm_eval(chipasm, "~$01010101 | $01010101 ^ $00001111 & $10101010", 5,
                  &value);
    ASSERT_EQ(value, 0xFFFF);
    chip8asm_eval(chipasm, "7 > 2 < 2", 6, &value);
    ASSERT_EQ(value, 4);
    chip8asm_eval(chipasm, "13 % 8 / 2", 7, &value);
    ASSERT_EQ(value, 2);
    chip8asm_eval(chipasm, "~--~45", 8, &value);
    ASSERT_EQ(value, 45);
    chip8asm_process_line(chipasm, "THE_ANSWER = 42");
    chip8asm_eval(chipasm, "THE_ANSWER - 2", 9, &value);
    ASSERT_EQ(value, 40);
    chip8asm_process_line(chipasm, "_crazyIDENT1234 = #FFFF");
    chip8asm_eval(chipasm, "~_crazyIDENT1234", 10, &value);
    ASSERT_EQ(value, 0);
    /* Test for failure on invalid expressions */
    ASSERT(chip8asm_eval(chipasm, "~1234~", 11, &value));
    ASSERT(chip8asm_eval(chipasm, "123+", 12, &value));
    ASSERT(chip8asm_eval(chipasm, "undefined", 13, &value));

    chip8asm_destroy(chipasm);
    return 0;
}

int test_comparison(void)
{
    struct chip8_options opts = chip8_options_testing();
    struct chip8 *chip = chip8_new(opts);
    uint16_t pc;

    /* LD V0, #45 */
    chip8_execute_opcode(chip, 0x6045);
    pc = chip->pc;
    /* SE V0, #45 */
    chip8_execute_opcode(chip, 0x3045);
    ASSERT_EQ(chip->pc, pc + 4);
    pc = chip->pc;
    /* SNE V0, #45 */
    chip8_execute_opcode(chip, 0x4045);
    ASSERT_EQ(chip->pc, pc + 2);
    /* LD V1, #39 */
    chip8_execute_opcode(chip, 0x6139);
    pc = chip->pc;
    /* SE V0, V1 */
    chip8_execute_opcode(chip, 0x5010);
    ASSERT_EQ(chip->pc, pc + 2);
    pc = chip->pc;
    /* SNE V0, V1 */
    chip8_execute_opcode(chip, 0x9010);
    ASSERT_EQ(chip->pc, pc + 4);

    chip8_destroy(chip);
    return 0;
}

int test_ld(void)
{
    struct chip8_options opts = chip8_options_testing();
    struct chip8 *chip = chip8_new(opts);

    /* LD V5, #67 */
    chip8_execute_opcode(chip, 0x6567);
    ASSERT_EQ(chip->regs[REG_V5], 0x67);
    /* LD VA, V5 */
    chip8_execute_opcode(chip, 0x8A50);
    ASSERT_EQ(chip->regs[REG_VA], chip->regs[REG_V5]);
    /* LD I, #600 */
    chip8_execute_opcode(chip, 0xA600);
    ASSERT_EQ(chip->reg_i, 0x600);
    /* LD DT, V5 */
    chip8_execute_opcode(chip, 0xF515);
    ASSERT_EQ(chip->reg_dt, chip->regs[REG_V5]);
    /* LD V0, DT */
    chip8_execute_opcode(chip, 0xF007);
    ASSERT_EQ(chip->regs[REG_V0], chip->reg_dt);
    /* LD ST, V0 */
    chip8_execute_opcode(chip, 0xF018);
    ASSERT_EQ(chip->reg_st, chip->regs[REG_V0]);
    /* LD B, V5 */
    chip8_execute_opcode(chip, 0xF533);
    ASSERT_EQ(chip->mem[0x600], 1);
    ASSERT_EQ(chip->mem[0x601], 0);
    ASSERT_EQ(chip->mem[0x602], 3);

    chip8_destroy(chip);
    return 0;
}

static void testing_run(const char *name, int (*test)(void))
{
    int res;

    printf("=====Running test %s\n", name);
    if ((res = test())) {
        printf("-----Test FAILED with status %d\n", res);
        n_failures++;
    } else {
        printf("+++++Test PASSED\n\n");
    }
}

static void testing_setup(void)
{
    printf("Starting tests\n");
}

static int testing_teardown(void)
{
    printf("All tests finished with %d failure%s\n", n_failures,
           n_failures == 1 ? "" : "s");
    if (n_failures != 0)
        return 1;
    return 0;
}

static struct chip8_options chip8_options_testing(void)
{
    struct chip8_options opts = chip8_options_default();

    opts.enable_timer = false;
    opts.delay_draws = false;
    return opts;
}
