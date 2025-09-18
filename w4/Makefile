# Makefile for io_demo.c
# Students can build and run the program using:
#   make        (compiles the program)
#   make run    (runs the program with sample arguments)
#   make clean  (removes the compiled file)

CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = io_demo

# Default target to build the program
all: $(TARGET)

# Rule to build the program from the source file
$(TARGET): io_demo.c
	$(CC) $(CFLAGS) -o $(TARGET) io_demo.c

# Run the program with some sample arguments
# Students should edit the arguments to experiment
run: $(TARGET)
	./$(TARGET) arg1 arg2 arg3

# Clean up compiled files
clean:
	rm -f $(TARGET)
