CC = gcc
CFLAGS = -std=c11 -Wall -O2
TARGET = veth_demo
$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c
run: $(TARGET)
	sudo ./$(TARGET)
clean:
	rm -f $(TARGET)
