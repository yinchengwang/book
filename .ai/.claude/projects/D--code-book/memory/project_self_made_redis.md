---
name: project-self-made-redis
description: self_made/ 和 self_made_cpp/ 数据结构、redis/ 移植、uthash 库、cpp/ 演示、通用工具头文件
metadata: 
  node_type: memory
  type: project
  originSessionId: 79dcc910-7e0b-4040-aee9-56fe473e04bb
---

# self_made / redis / 通用库

**Why:** 记录了项目的手写数据结构、Redis 核心移植和第三方工具库。

## self_made/ — C 数据结构

实现的数据结构：
- **链表**: 单/双向链表、Floyd 环检测、K 组反转、回文判断
- **队列**: 多种队列实现（数组队列、链表队列、环形队列等）
- **栈**: 内联数据优化栈
- **优先队列**: 含性能基准测试
- **树**: 二叉树遍历（前/中/后/层序）
- **堆**: 堆实现
- **排序**: 各排序算法
- **字符串**: 字符串算法（KMP 等）
- **通用工具**: common.h、compare.h 等

## self_made_cpp/ — C++ 数据结构

- **Trie**: 使用 `unordered_map` 作为子节点容器
- 其他 C++ 练习代码

## redis/ — Redis 核心数据结构移植

从 Redis 源码中提取并移植的核心数据结构：

### 双向链表 (adlist)
- 函数指针多态（dup/free/match）
- 迭代器支持（正向/反向）

### SDS (Simple Dynamic String)
- 5 种头部类型：sdshdr5/8/16/32/64
- `s[-1]` 技巧访问 flags 字节（存储类型和容量）
- O(1) 取长度、预分配、惰性释放

### 跳表 (skiplist)
- 仅移植头文件（skiplist.h）
- 多层次索引结构

### zmalloc
- 空桩实现（占位符，非完整移植）

## uthash — 第三方侵入式哈希库

位于 include/ 目录，提供：
- **uthash**: 侵入式哈希表，按插入顺序迭代
- **utarray**: 动态数组
- **utlist**: 链表宏（含归并排序）
- **utringbuffer**: 环形缓冲区（无槽位浪费设计）
- **utstack**: 栈宏
- **utstring**: 动态字符串（含 KMP 搜索）
- **bloom filter**: 布隆过滤器

uthash 的特点：头文件纯宏实现、侵入式（结构体内嵌 hh 字段）、无需外部依赖。

## cpp/ — C++ 语言特性演示

C++17 特性演示代码，与核心库独立。

## 通用工具头文件

- `common.h`: 通用宏和类型定义
- `compare.h`: 比较函数封装
