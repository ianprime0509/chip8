#!/bin/sh
# Test expected disassembly output (after assembling and then disassembling)
# against actual.

# Copyright 2018 Ian Johnson

# This is free software, distributed under the MIT license.  A copy of the
# license can be found in the LICENSE file in the project root, or at
# https://opensource.org/licenses/MIT.

TMPFILE1=$(mktemp)
TMPFILE2=$(mktemp)
PROGS="db pathological"
RETVAL=0

cd "$TESTDIR"
echo "Working in '$PWD'"
echo "chip8asm is '$CHIP8ASM'"
echo "chip8disasm is '$CHIP8DISASM'"

for prog in $PROGS; do
    "$CHIP8ASM" check-disasm/${prog}.c8 -o "$TMPFILE1"
    if [ $? -ne 0 ]; then
        echo "ERROR: $prog assembly failed"
    fi
    "$CHIP8DISASM" "$TMPFILE1" -o "$TMPFILE2"
    if [ $? -ne 0 ]; then
        echo "ERROR: $prog disassembly failed"
    fi
    diff "$TMPFILE2" check-disasm/${prog}.disasm.c8 >/dev/null
    if [ $? -ne 0 ]; then
        echo "ERROR: $prog assembly -> disassembly failed expectations"
        echo "Got disassembly:"
        cat "$TMPFILE2"
        RETVAL=1
    else
        echo "$prog disassembly -> assembly succeeded"
    fi
done

rm "$TMPFILE1" "$TMPFILE2"
exit $RETVAL
