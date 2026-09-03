# unit_test scaffold

多文件 C 项目的单元测试实践——mathlib 为被测试模块，test.c 用 20 行手工断言宏实现零依赖测试框架。

## 文件结构

```
unit_test/
├── main.c          # 演示入口（调用 mathlib 函数）
├── mathlib.h       # 被测试模块的头文件
├── mathlib.c       # 被测试模块的实现
├── test.c          # 单元测试（19 个用例）
├── Makefile        # 多 target（all/test/test-verbose/coverage）
├── README.md       # 本文件
├── NOTES.md        # 学习笔记
└── CHEATSHEET.md   # 测试框架速查表
```

## 复现命令

```bash
cd learning/scaffold/unit_test

# 1. 编译运行 demo
gcc -Wall -Wextra -O2 -std=c11 -o utest_demo main.c mathlib.c
./utest_demo

# 2. 编译运行单元测试
gcc -Wall -Wextra -O2 -std=c11 -o test_runner test.c mathlib.c
./test_runner
# 预期: 18/18 passed, 0 failed

# 3. 查看某个测试用例失败时的输出
# （可临时修改 ASSERT_EQ 中的预期值来模拟失败）
```

## 预期输出

```
=== 单元测试：mathlib ===

[test] factorial_zero           ... PASS
[test] factorial_one            ... PASS
[test] factorial_five           ... PASS
...
[test] power_neg_exp            ... PASS

=== result: 18/18 passed, 0 failed ===
```

## 关键点

- **C 没有标准测试框架**：C11 标准库不含测试/断言模块——这是历史原因（C 诞生时还没有 TDD 概念）。实际项目中要么用第三方框架（Unity/CppUTest/Check），要么像本仓库一样用 Google Test（C++ 框架测试 C 代码）
- **手工断言宏的巧妙之处**：`TEST()` 生成函数名（`test_factorial_zero`），`RUN_TEST()` 调用它并统计 PASS/FAIL。失败时 `return` 跳出当前测试函数，不中断后续用例
- **测试覆盖率 = 信心度**：19 个用例覆盖了 factorial/is_prime/gcd/power 的边界值（0/1/-1/大数）和正常输入——如果某次重构改了 mathlib.c 的算法但测试仍然全绿，说明重构没有破坏行为
- **AAA 模式**：Arrange（准备数据）→ Act（调用函数）→ Assert（验证结果）——所有测试用例都遵循这个三段式

详见 NOTES.md 工程对照段和 CHEATSHEET.md 命令速查表。
