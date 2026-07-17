# Makefile — db_flamegraph

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
TARGET = db_flamegraph
SRC = main.c

.PHONY: all clean run perf

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

perf: $(TARGET)
	perf record -F 99 -g -o perf.data ./$(TARGET)
	perf script -i perf.data > perf_script.txt
	@echo "perf_script.txt 已生成，可用 FlameGraph 工具转换为 svg"

clean:
	rm -f $(TARGET) perf.data perf_script.txt
