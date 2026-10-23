---
name: project-algo-library
description: algo/ 通用算法库的完整结构——ds/ 子系统、距离计算 SIMD、量化 PQ/LVQ、K-Means、分词词典、排序、二分查找
metadata: 
  node_type: memory
  type: project
  originSessionId: 79dcc910-7e0b-4040-aee9-56fe473e04bb
---

# algo/ 通用算法库

**Why:** 记录了 algo/ 库的完整模块结构，这是 index/ 和项目其他部分的基础依赖。

## 目录结构

```
algo/
├── ds/              # 数据结构子系统（教学用）
│   ├── array/ string/ linked_list/ stack/ queue/
│   ├── hash_table/ deque/ monotonic_stack/
│   ├── binary_tree/ bst/ avl_tree/ rb_tree/
│   ├── segment_tree/ fenwick_tree/ trie/ union_find/
│   └── graph/
├── Kmeans/          # K-Means 聚类（KMeans++ 初始化 + xorshift PRNG）
├── dict/            # 中文分词词典（Trie + DP 最大匹配）
├── distance/        # 距离计算（NEON/AVX/SSE/scalar 多后端 SIMD）
├── quantization/    # 向量量化（PQ 乘积量化 + LVQ，统一接口）
├── sort/            # 10 种排序算法
└── binary_search/   # 通用二分查找（lower_bound/upper_bound）
```

## ds/ 数据结构子系统

教学导向的数据结构实现，每个结构有独立目录，包含头文件（教程式注释）和实现：
- **线性结构**: array, string, linked_list, stack, queue, deque, monotonic_stack
- **哈希**: hash_table
- **树**: binary_tree, bst, avl_tree, rb_tree, segment_tree, fenwick_tree, trie
- **图**: graph, union_find

以下仅为文档存根（无实现）：skip_list, lru_cache, lfu_cache, state_compression, bitmap, persistent_ds, distributed_index

## Kmeans 模块

- KMeans++ 初始化（概率正比于到最近中心距离的平方）
- xorshift PRNG 用于随机数生成
- 供 IVF 索引的两级聚类训练使用

## dict 分词模块

- 中文分词词典，基于 Trie + DP 最大匹配算法
- 供 BM25 索引的文本分词使用
- 支持词项归一化

## distance 模块

多后端 SIMD 距离计算，编译时根据架构选择：
- **NEON** (ARM)
- **AVX** (x86 256-bit)
- **SSE** (x86 128-bit)
- **scalar** (通用回退)

支持 L2 距离和内积 (IP)，提供批量 4 个一组的 `batch4_from_query` 优化。

## quantization 模块

统一量化接口，支持两种量化器：
- **PQ (Product Quantization)**: 将向量分为 M 个子空间，每子空间 KMeans 聚类，ADC 近似距离
- **LVQ (Locally-adapted Vector Quantization)**: 局部自适应量化

被 HNSW、DiskANN、IVF 三个向量索引模块共用。

## sort 模块

实现 10 种排序算法：冒泡、选择、插入、希尔、归并、快速、堆、计数、基数、桶排序。

## binary_search 模块

泛型 lower_bound 和 upper_bound 实现。
