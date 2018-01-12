#!/bin/sh
# Test that binary -> disassemble -> assemble is a no-op.

# Copyright 2018 Ian Johnson

# This file is part of Chip-8.

# Chip-8 is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# Chip-8 is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with Chip-8.  If not, see <http://www.gnu.org/licenses/>.

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
