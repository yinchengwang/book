# 工程对照：Sanitizer 配置与 PostgreSQL 存储引擎

## 概述

本文档对照学习材料中的 Sanitizer 演示代码与 `engineering/CMakeLists.txt` 中的工程级配置，分析如何将 Sanitizer 集成到实际 C/C++ 项目中。

## engineering/CMakeLists.txt 关键配置解析

### 1. 当前工程构建配置

```cmake
# engineering/CMakeLists.txt 第 49-60 行
if(MSVC)
    add_compile_options(/W4 /WX)
    add_compile_options(/utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    # GCC/Clang（与 AGENTS.md 第 54 行保持一致）
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-sign-compare)

    if(ENABLE_COVERAGE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-fprofile-arcs -ftest-coverage)
        add_link_options(-fprofile-arcs -ftest-coverage)
    endif()
endif()
```

**分析**：当前工程未启用 Sanitizer，仅启用了代码覆盖率（Coverage）。这符合生产环境的默认配置。

### 2. Sanitizer 集成方案

如需在工程中启用 Sanitizer，建议采用 CMake 选项控制：

```cmake
# 建议方案：在 engineering/CMakeLists.txt 中添加
option(ENABLE_SANITIZER "Enable AddressSanitizer" OFF)

if(ENABLE_SANITIZER AND NOT MSVC)
    # ASan 标志（可用于 Debug 构建）
    add_compile_options(-fsanitize=address -g -O1)
    add_link_options(-fsanitize=address)

    # 可选：添加 USan
    # add_compile_options(-fsanitize=undefined)
    # add_link_options(-fsanitize=undefined)

    message(STATUS "Sanitizer enabled: ASan")
endif()
```

### 3. Debug vs Release 构建差异

```cmake
# engineering/CMakeLists.txt 第 62-78 行
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(NOT MSVC)
        add_compile_options(-O0 -g3)
    endif()
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(NOT MSVC)
        add_compile_options(-O2 -DNDEBUG)
    endif()
endif()
```

**Sanitizer 兼容性**：

| 构建类型 | 优化级别 | Sanitizer 兼容性 | 说明 |
|----------|----------|------------------|------|
| Debug | `-O0` | 部分兼容 | 优化太少可能改变代码结构 |
| Debug | `-O1` | **最佳** | Sanitizer 推荐级别 |
| Release | `-O2/-O3` | 可能干扰 | 优化可能消除检测代码 |

## 数据库存储引擎中的潜在问题

### 1. Buffer Pool (bufmgr.c) 的内存管理

```c
// engineering/src/db/storage/buffer/bufmgr.c
// 潜在 Sanitizer 问题场景

// 问题1: 指针算术溢出
buffer_pool->buffers[i] = (char *)calloc(1, BUF_PAGE_SIZE);
// 如果 i 超出范围，ASan 可检测

// 问题2: 释放后使用
void buf_drop_tmp(BufferDesc *buf) {
    buf->refcount--;
    if (buf->refcount == 0) {
        // 如果 buf 已在其他线程释放，TSan 会报警
    }
}

// 问题3: 未初始化内存
typedef struct BufHashEntry_s {
    uint32_t relfilenode;      // 可能未初始化
    BlockNumber blocknum;      // 可能未初始化
    // ...
} BufHashEntry;
// 如果 MSan 可用，应检查所有字段初始化
```

### 2. WAL 日志的并发问题

```c
// engineering/src/db/wal/wal.c
// TSan 可能检测的数据竞争

static void wal_write_record(wal_buf_t *wal, const char *record, size_t len) {
    // 多个线程同时写入 WAL 时可能存在竞争
    // 应使用互斥锁保护共享状态
}

// TSan 检测规则：
// 两个线程同时访问 shared state
// 至少一个写操作
// 无锁保护
```

### 3. 未定义行为风险

```c
// 数据库代码中常见的未定义行为

// 问题1: 有符号整数溢出
uint32_t nbuffers = 1024;
nbuffers = nbuffers * 2;  // 如果 nbuffers 是有符号数，可能溢出

// 问题2: 指针转换
page_t *page = (page_t *)((char *)buf + offset);
// 如果 offset 计算错误，USan 可能检测对齐问题

// 问题3: 除零
size_t page_size = 0;
size_t n_pages = total_size / page_size;  // 除零！
```

## Sanitizer 在数据库项目中的实际应用

### 1. 开发阶段启用 ASan

```bash
# 在项目根目录
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZER=ON ..
make

# 或直接指定编译器标志
CFLAGS="-fsanitize=address -g -O1" CXXFLAGS="-fsanitize=address -g -O1" cmake ..
```

### 2. CI/CD 集成

```yaml
# .github/workflows/sanitize.yml 示例
name: Sanitizer Tests

on: [push, pull_request]

jobs:
  asan-test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Configure with ASan
        run: |
          mkdir build && cd build
          cmake -DCMAKE_BUILD_TYPE=Debug \
                -DCMAKE_C_FLAGS="-fsanitize=address -g -O1" \
                -DCMAKE_CXX_FLAGS="-fsanitize=address -g -O1" \
                ..
      - name: Build and Test
        run: |
          cmake --build .
          ctest --output-on-failure
```

### 3. TSan 在并发模块中的使用

```bash
# 编译数据库存储引擎（启用 TSan）
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_C_FLAGS="-fsanitize=thread -pthread -g -O1" \
      -DCMAKE_CXX_FLAGS="-fsanitize=thread -pthread -g -O1" \
      ..
make

# 运行测试
ctest --output-on-failure

# TSan 输出示例
# WARNING: ThreadSanitizer: data race (pid=12345)
#   Write at 0x... by thread T1:
#   Previous write at 0x... by thread T0:
```

## 最佳实践总结

1. **开发阶段启用 ASan**：每次开发新模块后运行 ASan 检测
2. **并发模块启用 TSan**：数据库的 Buffer Pool、WAL 等并发模块应定期用 TSan 测试
3. **保持 `-O1` 优化**：避免 `-O0`（代码结构改变）和 `-O2`（优化干扰）
4. **确保 `-g` 调试符号**：否则错误信息缺少行号
5. **不混用 TSan + ASan**：TSan 与 ASan 不能同时启用
6. **CI 中添加 Sanitizer 构建**：作为回归检测的一部分

## 代码位置索引

| 文件 | 路径 | Sanitizer 相关 |
|------|------|----------------|
| 工程 CMake 配置 | `engineering/CMakeLists.txt` | 第 49-78 行（编译器选项） |
| Buffer Pool | `engineering/src/db/storage/buffer/bufmgr.c` | 内存管理 |
| WAL 日志 | `engineering/src/db/wal/wal.c` | 并发写入 |
| 页面管理 | `engineering/src/db/storage/page.c` | 指针操作 |
| 测试入口 | `engineering/test/` | gtest 测试套件 |

## 扩展阅读

- [PostgreSQL ASan 集成文档](https://www.postgresql.org/docs/current/sanitize.html)
- [Clang Sanitizer 文档](https://clang.llvm.org/docs/index.html#sanitizers)
- [数据库存储引擎架构](../db/storage_overview/README.md)
