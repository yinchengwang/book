CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -g

TARGET = acid
SRC = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

# 模拟故障测试原子性
failure: $(SRC)
	$(CC) $(CFLAGS) -DSIMULATE_FAILURE -o acid_failure $(SRC)

clean:
	rm -f $(TARGET) acid_failure

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run failure