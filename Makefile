# Makefile information
# Targets:
#   all        build everything in release mode
#   clean      clean up build files
#   debug      build everything in debug mode
#   doc        build Doxygen documentation
#   test       build and run test binary

BUILDDIR ?= build
CC ?= gcc
DOXYGEN ?= doxygen
MKDIR ?= mkdir
RM ?= rm

CFLAGS := $(CFLAGS) -std=gnu11 -Wall -Wextra
CPPFLAGS := $(CPPFLAGS)
LDFLAGS := $(LDFLAGS)

CFLAGS_DEBUG = $(CFLAGS) -g -O0
CFLAGS_RELEASE = $(CFLAGS) -O2

ASSEMBLER = chip8asm
DEBUGDIR = $(BUILDDIR)/debug
EXECUTABLE = chip8
RELEASEDIR = $(BUILDDIR)/release

LIBS = -lSDL2 -pthread
OBJS_COMMON = assembler.o instruction.o interpreter.o
OBJS_ASSEMBLER = $(OBJS_COMMON) chip8asm.o
OBJS_EMULATOR = $(OBJS_COMMON) audio.o chip8.o
OBJS_TEST = $(OBJS_COMMON) test.o

.PHONY: all clean debug test

all: $(RELEASEDIR)/$(ASSEMBLER) $(RELEASEDIR)/$(EXECUTABLE)

clean:
	$(RM) -rf doc $(BUILDDIR)

debug: $(DEBUGDIR)/$(ASSEMBLER) $(DEBUGDIR)/$(EXECUTABLE)

# Note: this is not a phony target, it is the name of a directory
doc: Doxyfile
	$(DOXYGEN) $<

test: $(OBJS_TEST:%=$(DEBUGDIR)/%)
	$(CC) $(CFLAGS_DEBUG) $(LDFLAGS) -o $(BUILDDIR)/chip8_test $^ $(LIBS)
	$(BUILDDIR)/chip8_test

$(DEBUGDIR):
	$(MKDIR) -p $@

$(DEBUGDIR)/$(ASSEMBLER): $(OBJS_ASSEMBLER:%=$(DEBUGDIR)/%)
	$(CC) $(CFLAGS_DEBUG) $(LDFLAGS) -o $@ $^ $(LIBS)

$(DEBUGDIR)/$(EXECUTABLE): $(OBJS_EMULATOR:%=$(DEBUGDIR)/%)
	$(CC) $(CFLAGS_DEBUG) $(LDFLAGS) -o $@ $^ $(LIBS)

$(DEBUGDIR)/%.o: %.c | $(DEBUGDIR)
	$(CC) $(CFLAGS_DEBUG) $(CPPFLAGS) -c -o $@ $<

$(RELEASEDIR):
	$(MKDIR) -p $@

$(RELEASEDIR)/$(ASSEMBLER): $(OBJS_ASSEMBLER:%=$(RELEASEDIR)/%)
	$(CC) $(CFLAGS_RELEASE) $(LDFLAGS) -o $@ $^ $(LIBS)

$(RELEASEDIR)/$(EXECUTABLE): $(OBJS_EMULATOR:%=$(RELEASEDIR)/%)
	$(CC) $(CFLAGS_RELEASE) $(LDFLAGS) -o $@ $^ $(LIBS)

$(RELEASEDIR)/%.o: %.c | $(RELEASEDIR)
	$(CC) $(CFLAGS_RELEASE) $(CPPFLAGS) -c -o $@ $<
