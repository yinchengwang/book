CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O0 -std=c11 -fsanitize=undefined -fsanitize=address
# 关闭 demonstration 期间 UBSAN 拦截；只允许 main 主动触发
CFLAGS_DEMO ?= -Wall -Wextra -O0 -std=c11

TARGET  = ub_demo
SRC     = main.c

.PHONY: all run clean debug

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS_DEMO) -o $@ $<

debug: $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET)_ubsan $<

run: $(TARGET)
	./$(TARGET)
	@echo "---"
	@echo "To trigger UB: ./$(TARGET) run  (would segfault or print UB-detected trace)"

clean:
	rm -f $(TARGET) $(TARGET)_ubsan
