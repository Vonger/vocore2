CFLAGS += -Wall -Wextra

PREFIX ?= /usr/local

all: mt7628tool

mt7628tool:
	$(CC) $(CFLAGS) mt7628tool.c -o mt7628tool

clean:
	rm -fv mt7628tool
