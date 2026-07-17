# XDP 演示 Makefile
CC = gcc
CFLAGS = -std=c11 -Wall -O2
TARGET = xdp_demo

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
