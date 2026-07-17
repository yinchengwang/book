# cross_platform 学习笔记

## 概念地图

C 语言是"可移植的汇编"——它在任何平台上都能编译，但不同平台的 API 完全不同。跨平台 C 编程 = 找到"最大公约数"，用 `#ifdef` 封装差异：

- **三层次适配策略**：
  1. **只用 C 标准库**（最高可移植性）：`fopen`、`malloc`、`printf`——所有平台的 C 编译器都提供
  2. **POSIX 兼容层**（中可移植性）：`open`、`sysconf`、`dlopen`——Linux/macOS 原生支持，Windows 通过 MinGW/Cygwin 提供
  3. **平台专有 API + `#ifdef` 封装**（最低可移植性，最高性能）：`GetSystemInfo`(Windows) vs `sysconf`(POSIX)——通过 `platform_cpu_count()` 抽象
- **编译期 vs 运行时检测**：
  - `#ifdef _WIN32` — 编译期，死代码在预处理阶段就被裁掉，最终二进制只包含当前平台的路径
  - `is_little_endian()` — 运行时，但在常量折叠优化下等同于 `return 1`（x86 上编译器直接替换为常量）
- **CMake 是 C 编译的跨平台"元层"**：`CMakeLists.txt` 生成平台原生构建文件——Windows 生成 `.sln`/`Makefile`，Linux 生成 `Makefile`，macOS 生成 `Xcode project`。`cmake/ProjectUtils.cmake` 中的 `if(WIN32)` 和 `if(UNIX)` 就是 `#ifdef _WIN32` 的构建层对应
- **ABI（Application Binary Interface）差异**：即使代码跨平台（源码级兼容），二进制也不兼容——Windows 的 `.dll` 和 Linux 的 `.so` 是不同的文件格式（PE vs ELF），函数调用约定不同（Windows x64 用 RCX/RDX，Linux x64 用 RDI/RSI）

## 踩坑记录

1. **`long` 的宽度是最常见的跨平台 bug**：在 `struct` 中存 `long` 然后序列化到文件——Windows 写 4 字节、Linux 读 8 字节，数据错乱。用 `int32_t`/`int64_t`（`<stdint.h>`）替代所有"我认为是 N 字节"的整数类型
2. **Windows 头文件的宏污染**：`<windows.h>` 定义了 `min` / `max` 宏——会破坏 C++ 的 `std::min`/`std::max`。在包含前 `#define NOMINMAX` 关闭
3. **路径分隔符的运行时处理**：`#ifdef _WIN32` 决定的是**代码编译时**的路径分隔符——如果你在 Windows 上编译了一个程序然后拿到 Wine 上跑，路径分隔符仍是 `\`。运行时路径用 `/`（Windows API 内部也接受 `/`），不依赖编译期宏
4. **`sysconf` 的返回值**：`sysconf(_SC_NPROCESSORS_ONLN)` 返回 `long`——如果系统有超过 2³¹ 个核心（暂时不太可能），返回值是 -1。生产代码需检查 `errno` 是否为 0
5. **MinGW 是"伪装成 Linux on Windows"**：MinGW 提供 POSIX API（`sysconf`、`dlopen`），但它链接的是 Windows 原生 DLL——`dlopen("libc.so.6")` 在 MinGW 下会失败，因为 Linux 的 `libc.so.6` 在 Windows 上不存在。本卡的 `platform_dlopen_test` 在 Windows 上用 `LoadLibrary("kernel32.dll")` 正确演示了这个差异

## 工程对照（≥100 字硬约束）

跨平台 C 编程在本仓库的多处代码中有实践体现：

1. **Redis zmalloc — 跨平台内存分配器**：`engineering/src/redis/redis_zmalloc.c` 中用 `#ifdef HAVE_MALLOC_SIZE` + `#ifdef JEMALLOC` 做两级适配——`HAVE_MALLOC_SIZE` 决定是否能用 `malloc_usable_size()`（glibc 扩展），`JEMALLOC` 决定是否使用 jemalloc 的高性能分配器。这是生产级跨平台 C 代码的范本——不是"能编译就行"，而是"用最优的 allocator"
2. **双 CMake 构建系统**：`engineering/CMakeLists.txt` 和 `learning/CMakeLists.txt` 使用不同的构建系统——`engineering/` 用 CMake 生成平台原生构建文件（`cmake -G "Visual Studio"` 或 `cmake -G "Unix Makefiles"`），`learning/` 用 Makefile 的 `?=` 覆盖模式让用户自定义 CC/CFLAGS。两者都要求"无论什么平台，一条命令构建"
3. **scaffold Makefile 的 `?=` 覆盖模式**：`learning/scaffold/` 下所有 24+8 张卡的 Makefile 都用 `CC ?= gcc` + `CFLAGS ?= ...`——允许用户在 macOS 上 `make CC=clang`，在 Linux 上 `make CC=gcc-13`，在 Windows MinGW 上不用 make 而直接写 gcc 命令。这个 `?=` 是跨平台构建的"最小公约数"
4. **`cmake/ProjectUtils.cmake` 的平台检测**：`engineering/cmake/ProjectUtils.cmake` 中的 `if(WIN32)` / `if(APPLE)` 分支与 `platform.h` 的 `#ifdef` 对应——CMake 在**构建时**决定编译哪个文件、链接哪个库，`#ifdef` 在**编译时**决定展开哪段代码。两者协同但不可互相替代
5. **`<stdint.h>` 已在仓库中广泛使用**：搜索 `engineering/src/` 下的 `int32_t`/`uint64_t` 可找到所有使用固定宽度类型的代码——这是跨平台 C 编程的第一条规则：永远不用 `int`/`long` 来存储需要确定宽度的数据

学完本卡后能动手的事：扫描 `learning/scaffold/` 下所有 Makefile，统一添加 `UNAME_S ?= $(shell uname -s)` 平台检测——在 `clean` target 中用 `ifneq ($(UNAME_S),Windows_NT)` 选 `rm -f` vs `del /f`，实现"所有卡 Makefile 在所有平台上都能用"的最终目标。
