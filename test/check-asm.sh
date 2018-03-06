#!/bin/sh
# Test assembly output against known good results.

# Copyright 2018 Ian Johnson

# This is free software, distributed under the MIT license.  A copy of the
# license can be found in the LICENSE file in the project root, or at
# https://opensource.org/licenses/MIT.

TMPFILE=$(mktemp)
PROGS="fonttest spritetest"
RETVAL=0

cd "$TESTDIR"
echo "Working in '$PWD'"
echo "chip8asm is '$CHIP8ASM'"
echo "chip8disasm is '$CHIP8DISASM'"

for prog in $PROGS; do
    "$CHIP8ASM" check-asm/${prog}.c8 -o "$TMPFILE"
    if [ $? -ne 0]; then
        echo "ERROR: $prog assembly failed"
    fi
    diff "$TMPFILE" check-asm/${prog}.asm.bin >/dev/null
    if [ $? -ne 0 ]; then
        echo "ERROR: $prog assembly failed expectations"
        RETVAL=1
    else
        echo "$prog assembly succeeded"
    fi
done

rm "$TMPFILE"
exit $RETVAL
