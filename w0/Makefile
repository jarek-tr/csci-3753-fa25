CC ?= gcc
CFLAGS_DEBUG = -std=c17 -Wall -Wextra -pedantic -O0 -g
CFLAGS_SAN   = -std=c17 -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all
CFLAGS_REL   = -std=c17 -O2 -DNDEBUG
TARGET = demo
SRC    = demo.c

.PHONY: all debug sanitize release clang-debug clang-sanitize clean

all: debug sanitize release clang-debug clang-sanitize

debug:
	$(CC) $(CFLAGS_DEBUG) $(SRC) -o $(TARGET)_debug

sanitize:
	$(CC) $(CFLAGS_SAN) $(SRC) -o $(TARGET)_asan

release:
	$(CC) $(CFLAGS_REL) $(SRC) -o $(TARGET)_release

clang-debug:
	clang $(CFLAGS_DEBUG) $(SRC) -o $(TARGET)_clang_debug

clang-sanitize:
	clang $(CFLAGS_SAN) $(SRC) -o $(TARGET)_clang_asan

clean:
	rm -f $(TARGET)_debug $(TARGET)_asan $(TARGET)_release $(TARGET)_clang_debug $(TARGET)_clang_asan
