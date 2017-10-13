## Process this file with automake to produce Makefile.in
bin_PROGRAMS = chip8 chip8asm
check_PROGRAMS = chip8test

chip8_SOURCES = \
	audio.h \
	audio.c \
	chip8.c \
	instruction.h \
	instruction.c \
	interpreter.h \
	interpreter.c

chip8asm_SOURCES = \
	assembler.h \
	assembler.c \
	chip8asm.c \
	instruction.h \
	instruction.c

chip8test_SOURCES = \
	assembler.h \
	assembler.c \
	chip8test.c \
	instruction.h \
	instruction.c \
	interpreter.h \
	interpreter.c

TESTS = chip8test

chip8_CFLAGS = \
	$(AM_CFLAGS) \
	$(SDL_CFLAGS)

chip8_LDADD = \
	$(SDL_LIBS)

chip8asm_CFLAGS = \
	$(AM_CFLAGS)

chip8test_CFLAGS = \
	$(AM_CFLAGS)
