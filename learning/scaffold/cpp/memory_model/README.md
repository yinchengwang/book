# C++ Memory Model 学习脚手架

## 概述

本目录演示 C++ 内存模型的核心概念，包括内存序（memory order）、happens-before 关系、同步原语以及数据竞争的识别与消除。

## 文件列表

| 文件 | 说明 |
|------|------|
| `main.cpp` | 演示代码（约 120 行） |
| `Makefile` | 编译脚本 |
| `README.md` | 本文件 |
| `NOTES.md` | Linux 内存屏障与 C++ 内存序对照表 |

## 编译与运行

```bash
make        # 编译
./main      # 运行（Linux/macOS）
make clean  # 清理
```

## 演示内容

1. **数据竞争（Data Race）**：展示未同步并发访问导致的未定义行为
2. **六种内存序**：
   - `seq_cst`：顺序一致性，最严格
   `acq_rel`：获取/释放语义
   - `consume`：依赖顺序（C++20 前实现不一致）
   - `relaxed`：仅保证原子性
3. **happens-before 关系**：验证程序顺序与内存序的传递律
4. **同步原语**：`std::mutex` + `std::condition_variable`

## 依赖

- C++ 编译器（g++ / clang++）
- C++17 标准库
