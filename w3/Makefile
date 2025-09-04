CC=gcc
CFLAGS=-Wall -Wextra -O2
TARGET=copy_sim_easy

all: $(TARGET)
$(TARGET): copy_sim.c
	$(CC) $(CFLAGS) -o $(TARGET) copy_sim.c
run: $(TARGET)
	./$(TARGET)
clean:
	rm -f $(TARGET)