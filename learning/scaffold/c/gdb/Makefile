CC      ?= gcc
CFLAGS  ?= -g -O0 -Wall -Wextra -std=c11

# 默认加 ASAN，读者可改 BACKEND=plain 走纯 gdb 路径
BACKEND ?= asan
ifeq ($(BACKEND),asan)
  CFLAGS  += -fsanitize=address
  LDFLAGS += -fsanitize=address
endif

TARGET  = buggy
SRC     = main.c

.PHONY: all run clean plain asan

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

run: $(TARGET)
	./$(TARGET) "Hello World"

# 默认行为：要么有 argv，要么触发 Bug C
crash: $(TARGET)
	./$(TARGET) || echo "exit code $$?"

clean:
	rm -f $(TARGET)

plain:
	$(MAKE) BACKEND=plain

asan:
	$(MAKE) BACKEND=asan
