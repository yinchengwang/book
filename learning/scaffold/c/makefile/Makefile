# c-makefile scaffold — 展示 8 个常用 Make 概念
#
# 用法:
#   make            # 默认 all：build demo
#   make run        # build + 跑 demo
#   make test       # build + 跑测试
#   make install    # 安装到 PREFIX
#   make clean      # 清理 build 产物
#   make distclean  # 清理更多
#   make vars       # 打印关键变量
#   make -n test    # 干跑，预览命令不真执行

CC       ?= gcc
CFLAGS   ?= -Wall -Wextra -O2 -std=c11 -MMD -MP
LDFLAGS  ?=

# 自动依赖：-MMD -MP 生成 .d 文件，-include 把它们纳入 make 解析
# 当头文件改动时，GCC 重生成 .d，下次 make 自动重编依赖它的 .c

SRC_DEMO := main.c mathlib.c
SRC_TEST := test.c mathlib.c

OBJDIR   := build
TARGET   := $(OBJDIR)/mathapp
TEST_BIN := $(OBJDIR)/test_mathlib

# 模式规则：从 .c 到 .o
$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# 链接可执行
$(TARGET): $(addprefix $(OBJDIR)/, $(SRC_DEMO:.c=.o))
	$(CC) -o $@ $^ $(LDFLAGS)

$(TEST_BIN): $(addprefix $(OBJDIR)/, $(SRC_TEST:.c=.o))
	$(CC) -o $@ $^ $(LDFLAGS)

# 让 .d 文件被纳入判断
-include $(OBJDIR)/*.d

.PHONY: all run test install clean distclean vars

all: $(TARGET)

run: $(TARGET)
	./$(TARGET)

test: $(TEST_BIN)
	./$(TEST_BIN)

install: $(TARGET)
	install -m 0755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/mathapp

clean:
	rm -rf $(OBJDIR)

distclean: clean
	rm -f Makefile.bak

vars:
	@echo "CC       = $(CC)"
	@echo "CFLAGS   = $(CFLAGS)"
	@echo "OBJDIR   = $(OBJDIR)"
	@echo "TARGET   = $(TARGET)"
	@echo "SRC_DEMO = $(SRC_DEMO)"
	@echo "OBJ files = $(addprefix $(OBJDIR)/, $(SRC_DEMO:.c=.o))"

$(OBJDIR):
	mkdir -p $(OBJDIR)
