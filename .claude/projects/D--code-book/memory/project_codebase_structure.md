---
name: project-codebase-structure
description: book 项目的整体目录结构、构建系统、各库之间的依赖关系
metadata: 
  node_type: memory
  type: project
  originSessionId: 79dcc910-7e0b-4040-aee9-56fe473e04bb
---

# book 项目代码库结构

**Why:** 记录了通过 `/claude-mem:learn-codebase` 系统性阅读全部源文件后获得的代码库全貌。

## 项目概述

C/C++ 算法与数据结构练习项目。CMake 3.20+、C11、C++17。无运行时依赖。

## 目录布局

```
src/
├── self_made/       # C 数据结构实现（链表、队列、栈、树、堆、排序、字符串、通用工具）
├── self_made_cpp/   # C++ 数据结构（Trie 等）
├── algo/            # 通用算法库（排序、二分查找、K-Means、距离计算、量化、分词词典、数据结构子系统）
├── leetcode/        # LeetCode 题解（c/ 放 C，cpp/ 放 C++）
├── interview/       # 面试题
├── index/           # 索引模块（向量索引 + 数据结构索引）
│   ├── vector_index/  # 向量索引（HNSW、IVF、DiskANN、BM25）
│   ├── hash/          # 哈希索引（CCEH、PG Linear Hash）
│   ├── tree/          # 树索引（B+tree、B-tree、tree_page 持久化）
│   └── data_structure/ # 跨模块数据结构（堆、visited_table、result_handler）
├── cpp/             # C++ 语言特性演示
└── redis/           # Redis 核心数据结构移植（双向链表、SDS、跳表）

include/             # 公共头文件，与 src/ 一一对应
test/                # 测试代码（GoogleTest + C++）
project/             # 独立玩具程序（贪吃蛇、2048、计算器、数独）
notes/               # 学习笔记（C、LLM、Redis、硬件、Linux、数据库）
open_source/         # Git 子模块——知名开源项目源码（faiss、redis、postgres、milvus 等）
third_part/googletest/ # vendored GoogleTest
```

## 库间依赖

```
index ──→ algo ──→ (无外部依赖)
                └── project_includes (仅头文件)

self_made ──→ project_includes
self_made_cpp ──→ project_includes
leetcode ──→ project_includes
interview ──→ project_includes
```

## CMake 构建

- 根 CMakeLists.txt 是唯一入口
- `cmake/ProjectUtils.cmake` 提供 `add_project_test()` 和 `add_project_library()` 辅助函数
- `project_includes` 是 INTERFACE 库，携带 `include/` 目录
- 测试二进制输出到源码目录
- GoogleTest vendored 在 `third_part/googletest/`

## 代码风格

- C11 + C++17
- 使用 `clang-format` 格式化
- 中文注释
