# preprocessor 学习笔记

## 概念地图

C 预处理器（CPP）是 C 编译的第一步——它在编译器看到代码之前完成文本替换。它**不理解 C 语法**，只是机械替换：

- **三大核心操作**：
  1. **对象宏** `#define NAME value`：纯文本替换，C 语言没有真正的常量声明（C99 `const` 不参与宏替换）
  2. **函数宏** `#define NAME(args) body`：文本替换形式函数，但**不评估类型**——`MAX(a,b)` 中 `a`/`b` 可为任何有 `>` 操作符的类型
  3. **包含** `#include`：纯文本插入文件内容，分 `<>`（系统路径）和 `""`（当前路径优先）两种
- **三个"魔法"操作符**：
  - `#x` 字符串化：把宏参数转为字符串字面量
  - `a##b` 记号拼接：把两个记号粘在一起形成新记号
  - `__VA_ARGS__` 变参：捕获 `...` 部分
- **条件编译**：`#if/#elif/#else/#endif` + `defined()` 是平台/特性选择的核心——同一份代码在 Linux/Windows/macOS 上编译出不同产物
- **头文件守卫两种风格**：
  - **传统**：`#ifndef HEADER_NAME_H` / `#define HEADER_NAME_H` / `#endif`
  - **现代**：`#pragma once`（GCC/Clang/MSVC 三家都支持，更简洁）

## 踩坑记录

1. **函数宏优先级坑**：`#define SQUARE(x) x*x` 时 `SQUARE(1+2)` 变成 `1+2*1+2=5` 而非 `9`。**必须 `((x)*(x))`**
2. **`#` 不展开宏**：`#define PI 3.14` 然后 `STR(PI)` 得 `"PI"` 而非 `"3.14"`——CPP 只字符串化参数，不二次展开
3. **`##` 优先级 vs 标识符**：必须 `CONCAT(a,b)` 中 `a` 是裸标识符，否则 `CONCAT(struct, foo)` 无法形成 `struct foo`
4. **`__VA_ARGS__` 0 参 GCC 警告**：`LOG("hello")` 在 strict 模式下 GCC 警告 `"hello"` 后面缺逗号。用 `LOG(fmt, ...)` + `fprintf(stderr, fmt, ##__VA_ARGS__)` 兼容
5. **宏的副作用**：`MAX(++a, ++b)` 中 `a`/`b` 可能被多次 `++`。**不要把带副作用的表达式传给函数宏**
6. **`#pragma once` 不是 C 标准**：是 GCC 扩展，但 GCC/Clang/MSVC 都支持。如果需要支持古董编译器才用传统 guard

## 工程对照（≥100 字硬约束）

预处理器宏在本仓库 `engineering/` 中是平台/特性适配的核心工具：

1. **`engineering/src/redis/redis_zmalloc.c` 的两级平台宏**：通过 `#ifdef HAVE_MALLOC_SIZE` 决定是否能用 `malloc_usable_size()`（glibc 扩展），通过 `#ifdef JEMALLOC` 决定是否用 jemalloc 高性能分配器。这是生产级跨平台代码的范本——**不是"能编译就行"，而是"用最优的 allocator"**
2. **`engineering/include/` 全部使用传统头文件守卫**：搜索 `#ifndef` 在 `engineering/include/` 下的密度极高——例如 `engineering/include/db/catalog.h`、`engineering/include/db/buf.h` 等都是 `#ifndef DB_CATALOG_H` + `#define DB_CATALOG_H` 模式
3. **变参日志宏**：`engineering/include/db/logger.h` 定义了类似 `elog(LOG_LEVEL, fmt, ...)` 的变参宏——所有 `engineering/src/db/**/*.c` 都通过它打日志（如 `engineering/src/db/storage/page/disk.c` 调 `elog(ERROR, "page read failed: %d", errno)`）
4. **`__VA_ARGS__` 的工程化**：`grep -rn "__VA_ARGS__" engineering/include/` 可发现多个变参日志/校验宏——这是工程代码消除样板代码的标准武器
5. **`#pragma once` 缺席**：工程中目前用传统 guard 居多（兼容性更好），但**新版头文件可以混用**——GCC 14 完全支持
6. **`#ifdef __GNUC__` 编译器特性开关**：`grep -rn "__GNUC__\|__attribute__" engineering/src/` 可发现大量 GCC 扩展使用，例如 `__attribute__((unused))`、`__attribute__((format(printf, ...)))`——这些都在预处理期决策

学完本卡能动手的事：把 `learning/scaffold/syntax/` 等所有 main.c 中的 `#include <stdio.h>` 改为用 `LOG_INFO(...)` 变参宏的封装，验证条件编译与变参宏的实战用法。