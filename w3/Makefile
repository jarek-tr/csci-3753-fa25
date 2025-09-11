# Makefile for thread_demo.c
# Week 3 OS Recitation: Threads, Thread Safety, Reentrant Code

CC     = gcc
CFLAGS = -Wall -Wextra -O2 -pthread
TARGET = thread_demo

# Default target: build the program
all: $(TARGET)

# Build rule: compile source into executable
$(TARGET): thread_demo.c
	$(CC) $(CFLAGS) -o $(TARGET) thread_demo.c

# Run the program
run: $(TARGET)
	./$(TARGET)

# Clean up compiled files
clean:
	rm -f $(TARGET)