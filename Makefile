CC ?= gcc
RM ?= rm

CFLAGS := $(CFLAGS) -std=c11 -Wall -Wextra
CPPFLAGS := $(CPPFLAGS)
LDFLAGS := $(LDFLAGS)

EXECUTABLE = chip8
LIBS = -lSDL2
OBJS = instruction.o main.o

.PHONY: all clean

all: $(EXECUTABLE)

clean:
	$(RM) -rf doc $(OBJS) $(EXECUTABLE)

$(EXECUTABLE): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
