# makefile scaffold

Makefile 八件套演示：变量、模式规则、自动依赖生成、`.PHONY` 完整示例 + 一个 4 文件真实 mini 工程（mathlib + demo + test）。

## 复现命令（本机 MinGW-w64 实测）

```bash
cd learning/scaffold/makefile

# 由于本机无 make，我们直接用 gcc 复现：
mkdir -p build
gcc -Wall -Wextra -O2 -std=c11 -MMD -MP -c main.c   -o build/main.o
gcc -Wall -Wextra -O2 -std=c11 -MMD -MP -c mathlib.c -o build/mathlib.o
gcc -Wall -Wextra -O2 -std=c11 -MMD -MP -c test.c    -o build/test.o
gcc -o build/mathapp build/main.o build/mathlib.o -lm
gcc -o build/test_mathlib build/test.o build/mathlib.o -lm

./build/mathapp           # 输出数学 demo
./build/test_mathlib      # 跑 6 个断言
```

## 真实 Linux/WSL/macOS 用户用 make

```bash
make            # all: build/mathapp
make run        # build + 跑 demo
make test       # 跑 test_mathlib
make install    # PREFIX=/usr/local sudo make install
make vars       # 打印关键变量
make -n test    # 干跑（dry-run，不真执行）
make clean      # rm -rf build/
```

## Makefile 八件套

| 概念 | 用途 |
|---|---|
| `CC/CFLAGS/LDFLAGS` | 变量定义 + `?=` 可被命令行覆盖 |
| `:=` 立即展开 vs `=` 递归展开 | SRC_DEMO 用 `:=`，自定义函数用 `=` |
| **自动依赖**（`-MMD -MP` + `-include *.d`） | 头文件改动时自动重编译 |
| **模式规则** `$(OBJDIR)/%.o: %.c` | 一行搞定所有 .c→.o |
| **`.PHONY`** | 防止目录同名 target 与文件冲突 |
| **`$(addprefix)/$(addsuffix)`** | 字符串函数做批量替换 |
| **`$(OBJDIR):` mkdir -p** | 目录存在性 order-only prerequisite |
| **`install -m 0755`** | 部署到 PREFIX/bin/ |
| **`make -n` 干跑** | 验证 Makefile 不真副作用执行 |

详见 NOTES.md 工程对照段。
