## Process this file with automake to produce Makefile.in
bin_PROGRAMS = chip8 chip8asm
check_PROGRAMS = chip8test

chip8_SOURCES = audio.c chip8.c instruction.c interpreter.c
chip8asm_SOURCES = assembler.c chip8asm.c instruction.c
chip8test_SOURCES = assembler.c chip8test.c instruction.c interpreter.c

TESTS = chip8test

chip8_CFLAGS = \
	$(AM_CFLAGS) \
	$(PTHREAD_CFLAGS) \
	$(SDL_CFLAGS)

chip8_LDADD = \
	$(PTHREAD_LIBS) \
	$(SDL_LIBS)

chip8asm_CFLAGS = \
	$(AM_CFLAGS)

chip8test_CFLAGS = \
	$(AM_CFLAGS) \
	$(PTHREAD_CFLAGS)

chip8test_LDADD = \
	$(PTHREAD_LIBS)
