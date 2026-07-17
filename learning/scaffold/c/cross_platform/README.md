# cross_platform scaffold

跨平台 C 编程实践——`#ifdef` 平台检测 + `platform.h` 抽象层 + 运行时字节序检测。本机为 Windows (MinGW)，其他平台代码通过 `#ifdef` 保证理论上可编译。

## 文件结构

```
cross_platform/
├── main.c          # 跨平台入口（打印所有平台信息）
├── platform.h      # 平台检测宏 + API 声明
├── platform.c      # 平台特定实现（Windows/Linux/macOS 三分支）
├── Makefile        # 含 check-platform / cross-check target
├── README.md       # 本文件
├── NOTES.md        # 学习笔记
└── CHEATSHEET.md   # 跨平台速查表
```

## 复现命令

```bash
cd learning/scaffold/cross_platform

# 编译运行（多文件联编）
gcc -Wall -Wextra -O2 -std=c11 -o cross_demo main.c platform.c
./cross_demo

# 查看预定义平台宏
gcc -dM -E - < /dev/null | grep -E '_WIN|__linux|__APPLE'
```

## 预期输出（Windows MinGW）

```
=== 跨平台 C 编程演示 ===

--- 1. 系统信息 ---
[platform] OS:       Windows
[platform] Compiler: GCC 14.2

--- 2. 硬件信息 ---
[platform] CPU cores: 8
[platform] Page size: 4096 bytes

--- 3. 字节序 ---
[platform] Endianness: Little-Endian
[platform] 0x12345678 in memory: 78 56 34 12

--- 4. 路径 ---
[platform] Path separator: '\'
[platform] Example path: usr\local\bin

--- 5. Sleep ---
[platform] Sleeping 100ms... done

--- 6. 动态库 ---
[platform] Shared lib extension: .dll
[platform] Dlopen test: PASS
```

## 关键点

- **`long` 的宽度是跨平台最大的坑**：Windows 4 字节、Linux/macOS 64-bit 8 字节。永远用 `int32_t`/`int64_t`
- **`#ifdef` 不是 if-else**：预处理器只认 `#ifdef`/`#if defined()`——不能用 `#ifdef _WIN32 || __linux__`
- **运行时字节序检测最可靠**：`unsigned int x=1; *(char*)&x` 是零开销的——编译器在编译期就能算出结果（常量折叠）
- **namespace 隔离保护**：`platform_cpu_count()` 封装了 Windows `GetSystemInfo()` 和 POSIX `sysconf()` 的差异——上层代码只调一个函数

详见 NOTES.md 工程对照段和 CHEATSHEET.md 速查表。
