CC ?= gcc
RM ?= rm

# For debugging only
# TODO: release mode flags
CFLAGS ?= -g -O0

CFLAGS := $(CFLAGS) -std=gnu11 -Wall -Wextra
CPPFLAGS := $(CPPFLAGS)
LDFLAGS := $(LDFLAGS)

EXECUTABLE = chip8
LIBS = -lSDL2 -pthread
OBJS_COMMON = instruction.o interpreter.o
OBJS_MAIN = $(OBJS_COMMON) main.o
OBJS_TEST = $(OBJS_COMMON) test.o

.PHONY: all clean test

all: $(EXECUTABLE)

clean:
	$(RM) -rf doc $(OBJS) $(EXECUTABLE)

test: $(OBJS_TEST)
	$(CC) $(CFLAGS) $(LDFLAGS) -o chip8_test $^ $(LIBS)
	./chip8_test

$(EXECUTABLE): $(OBJS_MAIN)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
