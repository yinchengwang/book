CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2 -std=c11

TARGET  = error_demo
SRC     = main.c

.PHONY: all run check-errno clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC)

run: $(TARGET)
	./$(TARGET)

# 演示 NDEBUG 禁用 assert
check-errno:
	@echo "=== 编译启用 NDEBUG (禁用 assert) ==="
	$(CC) $(CFLAGS) -DNDEBUG -o $(TARGET)_nodebug $(SRC)
	./$(TARGET)_nodebug | head -30
	rm -f $(TARGET)_nodebug

clean:
	rm -f $(TARGET)