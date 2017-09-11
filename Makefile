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
OBJS = instruction.o interpreter.o main.o

.PHONY: all clean

all: $(EXECUTABLE)

clean:
	$(RM) -rf doc $(OBJS) $(EXECUTABLE)

$(EXECUTABLE): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
