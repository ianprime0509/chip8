#!/bin/sh
# Test that binary -> disassemble -> assemble is a no-op.

# Copyright 2018 Ian Johnson

# This is free software, distributed under the MIT license.  A copy of the
# license can be found in the LICENSE file in the project root, or at
# https://opensource.org/licenses/MIT.

TMPFILE1=$(mktemp)
TMPFILE2=$(mktemp)
PROGS="fonttest spritetest"
RETVAL=0

cd "$TESTDIR"
echo "Working in '$PWD'"
echo "chip8asm is '$CHIP8ASM'"
echo "chip8disasm is '$CHIP8DISASM'"

for prog in $PROGS; do
    "$CHIP8DISASM" ${prog}.check.bin -o "$TMPFILE1"
    if [ $? -ne 0]; then
        echo "ERROR: $prog disassembly failed"
    fi
    "$CHIP8ASM" "$TMPFILE1" -o "$TMPFILE2"
    if [ $? -ne 0]; then
        echo "ERROR: $prog assembly failed"
    fi
    diff "$TMPFILE2" ${prog}.check.bin >/dev/null
    if [ $? -ne 0 ]; then
        echo "ERROR: $prog disassembly -> assembly failed expectations"
        RETVAL=1
    else
        echo "$prog disassembly -> assembly succeeded"
    fi
done

rm "$TMPFILE1" "$TMPFILE2"
exit $RETVAL
