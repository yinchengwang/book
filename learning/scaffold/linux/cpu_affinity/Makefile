CC = gcc
CFLAGS = -std=c11 -Wall -O2
TARGET = cpu_affinity

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
