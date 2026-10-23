---
name: mystl-handwritten-impl
description: learning/scaffold/cpp/stl 24 个组件的纯手写实现进度
metadata: 
  node_type: memory
  type: project
  originSessionId: 3edcf4b1-66e9-442a-82c6-7d3543fb658d
---

**项目位置**：`learning/scaffold/cpp/stl/`

**当前状态**：6 阶段全部完成，所有 24 个组件从 placeholder 重写为对标 libstdc++ 的纯手写实现。零 `std::` 依赖，0/24 仍为占位符。

**实现概览**：

基础设施（6）：type_traits, utility, iterator, allocator, functional, memory
序列容器（5）：array, vector, list, forward_list, deque
适配器（3）：stack, queue, priority_queue
有序关联（5）：rb_tree + set/multiset/map/multimap
无序关联（5）：hash_table + unordered_set/map/multiset/multimap
算法（2）：algorithm, numeric

**验证**：mini_test 15/15 通过，mystl_test 编译链接通过，stl_demo 与 std 兼容。

**已知次要缺陷**：
1. `pair<const T1, T2>::operator=(const pair&)` T1 为 const 时被删（按 std 规则）
2. `make_integer_sequence` 简化实现，不真正展开 N
3. `swap` SFINAE 移除，可能抛异常但不保证 noexcept

**Why**：用户反馈原实现大量 wrapper / 继承空壳 / placeholder。要求完整重写，符合 libstdc++ 模式 + 全部纯手写 + 中文注释 + 学习导向。

**How to apply**：在这个 stl 目录新增项目时用 `#include "mystl.h"` 即可，namespace `mystl`。每个组件的 .h 文件独立可包含，mtld 包括 include/mystl 作为内部细节。修改需重编译整个单元（无 .tcc 拆分时）。