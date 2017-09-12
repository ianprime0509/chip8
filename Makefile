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

EXECUTABLE = chip8
LIBS = -lSDL2 -pthread
OBJS_COMMON = instruction.o interpreter.o
OBJS_MAIN = $(OBJS_COMMON) chip8.o
OBJS_TEST = $(OBJS_COMMON) test.o
OBJS_ALL = $(OBJS_MAIN) $(OBJS_TEST)

.PHONY: all clean test

all: $(EXECUTABLE)

clean:
	$(RM) -rf doc $(OBJS_ALL) $(EXECUTABLE)

test: $(OBJS_TEST)
	$(CC) $(CFLAGS) $(LDFLAGS) -o chip8_test $^ $(LIBS)
	./chip8_test

$(EXECUTABLE): $(OBJS_MAIN)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
