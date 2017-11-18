# Chip8

This is a [Chip-8](https://en.wikipedia.org/wiki/CHIP-8) emulator, written in
C. Currently, all the Chip-8 games I have tested are functional, and some
Super-Chip games are as well (in particular, BLINKY). Many Super-Chip games
don't quite work yet, though.

## General Chip-8 documentation

Some helpful documentation can be found in the following:

* [Cowgod's Chip-8 technical
  reference](http://devernay.free.fr/hacks/chip8/C8TECH10.HTM)
* [Peter Miller's Chip-8 site](http://chip8.sourceforge.net/)
* [David Winter's Chip-8 emulation page](http://www.pong-story.com/chip8/)

I am also working on a sort of "reference guide" to collect information
scattered around various sources, as well as to document this project in a more
complete manner than this README.

## Assembler syntax

The assembler syntax is meant to be compatible with the game sources
given for the games provided on [David Winter's
site](http://www.pong-story.com/chip8/), but perhaps with some
extensions in the future. Assembler input is assumed to be encoded as
UTF-8, but it doesn't (currently) check for validity, so really any
ASCII-compatible character encoding will probably work.

Right now, the syntax is not very formal, and is really just notes for
myself so I can implement it.

### "Formal" syntax

A typical non-empty line has the format

```
[LABEL:] INSTRUCTION [OPERAND1], [OPERAND2], ...
```

That is, it may optionally begin with a label followed by a colon, an
instruction, and (optionally) one or several operands. These are
described in more detail below. Also, the character `;` can be used to
start a comment: any characters including and following the first `;`
on a line will be ignored.

Alternatively, a line may have the format

```
LABEL = EXPRESSION
```

which causes `LABEL` to be replaced by `EXPRESSION` in any expression
context (e.g. in an operand).

### Labels

A label may consist of any character except for blank space and the
colon (`:`) which ends the label. Be warned, however, that if you name
a label something like `V0` (or any other register or pseudo-operand),
it might be confused with another usage of that name.

### Instructions

The names of the allowed programmatic instructions are summarized in
the documentation for `instruction.h` (which should be in the same
directory as this file). The case of the letters does not matter, so
that `ADD` is the same as `add` or `aDd`.

The instructions `DB` and `DW` are used to insert a byte or a word (16
bits), respectively, directly into the assembled code. Keep in mind
that the Chip-8 is a big-endian machine.

### Operands

The typical names of the registers (`V0`-`VF`, `I`, `DT`, and `ST`)
are retained, and (like the instructions) are case-insensitive. The
"pseudo-operands" `F`, `B`, `[I]`, `HF`, and `R` are also recognized
in certain contexts (see the instruction reference), and are
case-insensitive. A label can also be used as an operand, as can
numeric literals. A numeric literal is either a plain decimal number
like `123`; a hexadecimal number prefixed with `#`, like `#AF` (the
hexadecimal digits are case-insensitive); or a binary number prefixed
with `$`, like `$01001`.

An operand can also be an expression, like `1 + 2`. TODO: document
this a bit more thoroughly.
