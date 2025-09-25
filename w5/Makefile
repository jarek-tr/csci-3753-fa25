# Makefile for thread_recitation
CC = gcc
CFLAGS_COMMON = -pthread -Wall -Wextra
CFLAGS_OPT = -O2
CFLAGS_DEBUG = -O0 -g

TARGET = thread_test
SRC = thread_test.c

# Default values if not provided at make time
THREADS ?= 8
ITERATIONS ?= 100000

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS_COMMON) $(CFLAGS_OPT) -DTHREADS=$(THREADS) -DITERATIONS=$(ITERATIONS) -o $(TARGET) $(SRC)

debug:
	$(CC) $(CFLAGS_COMMON) $(CFLAGS_DEBUG) -DTHREADS=$(THREADS) -DITERATIONS=$(ITERATIONS) -o $(TARGET) $(SRC)

fast:
	$(CC) $(CFLAGS_COMMON) -O3 -march=native -DTHREADS=$(THREADS) -DITERATIONS=$(ITERATIONS) -o $(TARGET) $(SRC)

run: debug
	./$(TARGET)

clean:
	rm -f $(TARGET)
