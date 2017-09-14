CC ?= gcc
RM ?= rm

ifeq ($(DEBUG), 1)
CFLAGS ?= -g -O0
else
CFLAGS ?= -O2
endif

CFLAGS := $(CFLAGS) -std=gnu11 -Wall -Wextra
CPPFLAGS := $(CPPFLAGS)
LDFLAGS := $(LDFLAGS)

ASSEMBLER = chip8asm
EXECUTABLE = chip8
LIBS = -lSDL2 -pthread
OBJS_COMMON = assembler.o instruction.o interpreter.o
OBJS_ASSEMBLER = $(OBJS_COMMON) chip8asm.o
OBJS_EMULATOR = $(OBJS_COMMON) audio.o chip8.o
OBJS_TEST = $(OBJS_COMMON) test.o
OBJS_ALL = $(OBJS_ASSEMBLER) $(OBJS_EMULATOR) $(OBJS_TEST)

.PHONY: all clean test

all: $(ASSEMBLER) $(EXECUTABLE)

clean:
	$(RM) -rf doc $(OBJS_ALL) $(ASSEMBLER) $(EXECUTABLE) *.o */*.o */*/*.o

test: $(OBJS_TEST)
	$(CC) $(CFLAGS) $(LDFLAGS) -o chip8_test $^ $(LIBS)
	./chip8_test

$(ASSEMBLER): $(OBJS_ASSEMBLER)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

$(EXECUTABLE): $(OBJS_EMULATOR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
