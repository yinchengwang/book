# Sanitizer 快速参考

## 概述

Sanitizer 是 GCC/Clang 内置的运行时检测工具集，用于在程序运行时发现各种内存和并发问题。相比 Valgrind，Sanitizer 集成在编译器中，性能开销更小（~2x vs ~10-50x）。

## 四大 Sanitizer

| Sanitizer | 标志 | 主要检测 | 性能开销 | 编译器支持 |
|-----------|------|----------|----------|------------|
| AddressSanitizer | `-fsanitize=address` | 内存错误（溢出、UAF、双free） | ~2x | GCC 4.8+, Clang 3.1+ |
| MemorySanitizer | `-fsanitize=memory` | 未初始化内存读取 | ~3x | Clang only |
| ThreadSanitizer | `-fsanitize=thread` | 数据竞争 | ~5-10x | GCC 6+, Clang 3.1+ |
| UndefinedBehaviorSanitizer | `-fsanitize=undefined` | 未定义行为 | ~1.5x | GCC 4.9+, Clang 3.3+ |

## 快速使用

```bash
# GCC/Clang 编译选项
g++ -fsanitize=address -g -O1 -o program program.cpp

# 运行即可自动检测
./program
```

## 常用编译组合

```bash
# 内存 + 未定义行为（推荐组合）
-fsanitize=address,undefined

# 全套检测（性能开销大）
-fsanitize=address,memory,thread,undefined

# 仅泄漏检测（轻量）
-fsanitize=leak
```

## 检测能力详解

### AddressSanitizer (ASan)

| 错误类型 | 示例 |
|----------|------|
| 堆缓冲区溢出 | `buf[10]` 当 `malloc(10)` |
| 栈缓冲区溢出 | `int arr[4]; arr[10] = 0;` |
| 全局缓冲区溢出 | `global_arr[100]` 超出定义 |
| 使用后释放 | `free(p); printf("%s", p);` |
| 双重释放 | `free(p); free(p);` |
| 内存泄漏 | 分配后未 free |
| 返回栈地址 | `return local_var;` |
| 错位释放 | `malloc` + `delete` |

### MemorySanitizer (MSan)

| 错误类型 | 示例 |
|----------|------|
| 使用未初始化变量 | `int x; printf("%d", x);` |
| 从未初始化内存读取 | `char buf[32]; printf("%s", buf);` |
| 条件分支使用 | `if (uninit > 0)` |
| 逻辑运算使用 | `int c = a && b;` (a 未初始化) |
| 传递参数使用 | `func(uninit_var);` |

### ThreadSanitizer (TSan)

| 错误类型 | 示例 |
|----------|------|
| 数据竞争 | 两线程同时读写同一变量 |
| 释放后使用 | 线程A delete，指针线程B仍用 |
| 锁顺序死锁 | 线程1锁A→B，线程2锁B→A |

**TSan 黄金规则**：两个线程同时访问同一内存，至少一个写操作，无锁保护。

### UndefinedBehaviorSanitizer (USan)

| 错误类型 | 示例 |
|----------|------|
| 有符号整数溢出 | `INT_MAX + 1` |
| 除零 | `x / 0` |
| 空指针解引用 | `S *p = nullptr; p->value;` |
| 位移越界 | `1 << 100` |
| 浮点 NaN | `(int)(0.0/0.0)` |
| 对齐违反 | `int* p = (int*)0x1;` |

## 常见错误信息

```
# ASan
AddressSanitizer: heap-buffer-overflow on address 0x...
WRITE of size 1 at 0x... thread T0

# MSan
MemorySanitizer: use-of-uninitialized-value on address 0x...

# TSan
WARNING: ThreadSanitizer: data race (pid=12345)
  Write at 0x... by thread T1:
  Previous write at 0x... by thread T0:

# USan
UndefinedBehaviorSanitizer: undefined-behavior program.cpp:10:5
runtime error: signed integer overflow: 2147483647 + 1 cannot be represented
```

## 调试符号与优化级别

```bash
# 必须使用 -g 生成调试符号
-g

# 建议使用 -O1 或 -O0，避免优化消除检测代码
# 不要使用 -O2/-O3（可能改变错误行为）
-g -O1
```

## 与 GDB 联用

```bash
# 编译
g++ -fsanitize=address -g -O1 -o program program.cpp

# GDB 调试（ASan 在断点处仍生效）
gdb ./program
(gdb) run
# ASan 在错误点中断，可检查内存
(gdb) bt  # 查看调用栈
(gdb) p variable_name
```

## CMake 集成

```cmake
# CMakeLists.txt
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fsanitize=address -g -O1)
    add_link_options(-fsanitize=address)
endif()
```

## 项目级配置（参考 engineering/CMakeLists.txt）

```cmake
# engineering/CMakeLists.txt 第 52-60 行
if(NOT MSVC)
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-sign-compare)

    if(ENABLE_COVERAGE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-fprofile-arcs -ftest-coverage)
        add_link_options(-fprofile-arcs -ftest-coverage)
    endif()
endif()

# 如需启用 Sanitizer，可添加：
if(ENABLE_SANITIZER)
    add_compile_options(-fsanitize=address -g -O1)
    add_link_options(-fsanitize=address)
endif()
```

## 限制与注意事项

1. **MSan 仅 Clang**：GCC 不支持 MemorySanitizer
2. **TSan 独占**：TSan 与 ASan/MSan 不能同时启用
3. **性能开销**：生产环境不建议开启 Sanitizer
4. **优化级别**：使用 `-O1` 避免优化干扰检测
5. **调试符号**：`-g` 必须开启
6. **MSVC 限制**：MSVC 对 Sanitizer 支持有限，建议用 MinGW/Clang

## 参考资料

- [AddressSanitizer](https://clang.llvm.org/docs/AddressSanitizer.html)
- [MemorySanitizer](https://clang.llvm.org/docs/MemorySanitizer.html)
- [ThreadSanitizer](https://clang.llvm.org/docs/ThreadSanitizer.html)
- [UndefinedBehaviorSanitizer](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html)
- [Sanitizer Cookbook](https://github.com/google/sanitizers/wiki/SanitizerCommonFlags)
