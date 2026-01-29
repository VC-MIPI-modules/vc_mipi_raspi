# Makefile for vc-set-format


CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11
LDFLAGS =
TARGET = vc-set-format
SOURCES = vc-set-format.c
BUILDDIR = build
OBJECTS = $(addprefix $(BUILDDIR)/, $(SOURCES:.c=.o))

# Installation paths
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin

.PHONY: all clean install uninstall builddir

all: builddir $(BUILDDIR)/$(TARGET)

builddir:
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

$(BUILDDIR)/%.o: %.c | builddir
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILDDIR)

install: $(BUILDDIR)/$(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(BUILDDIR)/$(TARGET) $(DESTDIR)$(BINDIR)/

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)

# Development targets
debug: CFLAGS += -g -DDEBUG
debug: clean all

test: $(BUILDDIR)/$(TARGET)
	@echo "Testing vc-set-format..."
	@$(BUILDDIR)/$(TARGET) --help

.DEFAULT_GOAL := all
