CC = gcc
CFLAGS = -O2 -Wall -Wextra -Wno-unused-result
CFLAGS += $(shell pkg-config --cflags sdl2)
LDFLAGS = -lm
LDFLAGS += $(shell pkg-config --libs sdl2)

.PHONY: all
all: build

.PHONY: build
build:
	$(CC) emul8tor.c -o emul8tor $(LDFLAGS)

.PHONY: clean
clean:
	rm -rf emul8tor
