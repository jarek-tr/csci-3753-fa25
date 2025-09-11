# Makefile for syscall_demo.c
# Week 1 OS Recitation: User vs Kernel Mode

# Compiler and flags
CC     = gcc
CFLAGS = -Wall -Wextra -O2

# Target program name
TARGET = syscall_demo

# Default target: build the program
all: $(TARGET)

# Build rule: compile syscall_demo.c into syscall_demo
$(TARGET): syscall_demo.c
	$(CC) $(CFLAGS) -o $(TARGET) syscall_demo.c

# Run the program
run: $(TARGET)
	./$(TARGET)

# Clean up compiled files
clean:
	rm -f $(TARGET)