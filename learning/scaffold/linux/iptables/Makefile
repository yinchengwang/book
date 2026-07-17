CC = gcc
CFLAGS = -std=c11 -Wall -O2
TARGET = iptables_demo

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c

run: $(TARGET)
	./$(TARGET)

# 以模拟模式运行（默认）
simulate: $(TARGET)
	./$(TARGET)

# 以实际模式运行（需要 root 权限）
apply: $(TARGET)
	sudo ./$(TARGET) --apply

clean:
	rm -f $(TARGET)

.PHONY: run simulate apply clean
