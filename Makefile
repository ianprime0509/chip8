CC ?= gcc
RM ?= rm

CFLAGS := $(CFLAGS) -std=c11 -Wall -Wextra
CPPFLAGS := $(CPPFLAGS)
LDFLAGS := $(LDFLAGS)

EXECUTABLE = chip8
OBJS = instruction.o main.o

.PHONY: all clean

all: $(EXECUTABLE)

clean:
	$(RM) -rf doc $(OBJS) $(EXECUTABLE)

$(EXECUTABLE): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $<
