CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2 -std=c11

TARGET  = git_demo
SRC     = main.c

.PHONY: all run clean git-log git-diff bisect-demo

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

run: $(TARGET)
	./$(TARGET) "hello world"
	@echo ""
	./$(TARGET)

# ---------- Git 专属 target ----------

git-log:
	@command -v git >/dev/null 2>&1 && \
		git log --oneline -5 -- $(SRC) || \
		echo "[skip] git 不可用"

git-diff:
	@command -v git >/dev/null 2>&1 && \
		git diff HEAD~1 -- $(SRC) 2>/dev/null || \
		echo "[skip] git 不可用或无足够历史"

bisect-demo:
	@echo "=== git bisect 演示（模拟）==="
	@echo ""
	@echo "场景: v1.1 引入了 NULL 指针 bug，但你不确定是哪个 commit 引入的。"
	@echo ""
	@echo "  $$ git bisect start"
	@echo "  $$ git bisect bad HEAD          # 当前版本有 bug"
	@echo "  $$ git bisect good v1.0         # v1.0 是好的"
	@echo "  $$ git bisect run ./test.sh     # 自动二分查找"
	@echo ""
	@echo "Git 会自动 checkout 到中间 commit，运行 test.sh，"
	@echo "根据退出码判断这个 commit 是 good 还是 bad，"
	@echo "最终定位到引入 bug 的那次 commit。"
	@echo ""
	@echo "这是 Git 最强大的调试工具之一——O(log n) 次定位任意回归 bug。"

clean:
	rm -f $(TARGET)
