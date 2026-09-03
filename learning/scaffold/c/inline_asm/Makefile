CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2 -std=gnu11

TARGET  = inasm_demo
SRC     = main.c

.PHONY: all run intel-syntax clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC)

run: $(TARGET)
	./$(TARGET)

# 用 Intel 语法重编（GCC 加 -masm=intel）
intel-syntax:
	@echo "=== 用 Intel 语法编译 ==="
	$(CC) $(CFLAGS) -masm=intel -o $(TARGET)_intel $(SRC)
	./$(TARGET)_intel

clean:
	rm -f $(TARGET) $(TARGET)_intel