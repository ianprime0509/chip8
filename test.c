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
 * Simple unit tests for the project.
 * Tests are just functions with the signature `int test_func(void)`, which
 * return 0 on success or 1 on failure, and they should be registered in the
 * `main` function of this file.
 */

#include <stdint.h>
#include <stdio.h>

#include "interpreter.h"

#define TEST_RUN(test) test_run(#test, test)
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
             : "%ud", int                                                      \
             : "%d", unsigned int                                              \
             : "%u")

static int n_failures;

static void test_run(const char *name, int (*test)(void));
static void test_setup(void);
static int test_teardown(void);

static struct chip8_options chip8_options_testing(void);

int test_comparison(void);
int test_ld(void);

int main(void)
{
    test_setup();
    TEST_RUN(test_comparison);
    TEST_RUN(test_ld);
    return test_teardown();
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

    return 0;
}

static void test_run(const char *name, int (*test)(void))
{
    int res;

    printf("=====Running test %s\n", name);
    if ((res = test())) {
        printf("-----Test FAILED with status %d\n", res);
        n_failures++;
    } else {
        printf("+++++Test PASSED\n");
    }
}

static void test_setup(void)
{
    printf("Starting tests\n");
}

static int test_teardown(void)
{
    printf("\nAll tests finished with %d failure%s\n", n_failures,
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
