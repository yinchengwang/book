---
name: project-index-structures
description: index/ 模块的完整结构——向量索引(BM25/HNSW/DiskANN/IVF)、哈希索引(CCEH/PG Linear Hash)、树索引(B+tree/B-tree)、跨模块数据结构
metadata: 
  node_type: memory
  type: project
  originSessionId: 79dcc910-7e0b-4040-aee9-56fe473e04bb
---

# index/ 索引模块

**Why:** 记录了 index/ 下所有索引和数据结构的全景，是项目最复杂的模块。

## 整体结构

```
index/
├── vector_index/       # 向量索引（ANN + 文本检索）
│   ├── BM25/           # 倒排索引，精确文本检索
│   ├── hnsw/           # 分层可导航小世界图，近似最近邻
│   ├── diskann/        # 磁盘友好 ANN 图索引
│   └── ivf/            # 倒排文件索引，两级聚类
├── hash/               # 哈希索引
│   ├── CCEH/           # 无锁并发可扩展哈希（RCU + COW 段）
│   └── pg_linear_hash/ # PG 风格线性哈希（Litwin 1980）
├── tree/               # 树索引
│   ├── B+tree/         # B+树（max_keys+1 溢出槽、叶子链表、借位/合并）
│   ├── B-tree/         # B-树（自顶向下分裂、值在所有节点中）
│   └── tree_page/      # 槽式页面持久化（槽向前增长、负载向后增长，FNV-1a 校验）
└── data_structure/     # 跨模块共用数据结构
    ├── faiss_heap/          # 1-based 大顶堆
    ├── faiss_minimax_heap/  # 双语义堆（max-heap + O(n) pop_min）
    ├── faiss_visited_table/ # 访问标记表（自增 visno 模式，避免 O(size) 清零）
    └── faiss_result_handler/# 基于阈值的结果处理器
```

## 向量索引模块 (vector_index/)

4 个向量索引各有 DESIGN.md 设计文档（位于各自目录下），覆盖算法原理、数据结构、存储设计、DML/DQL 流程、优化策略。

## 哈希索引

### CCEH (无锁并发可扩展哈希)
- RCU 风格并发控制：COW (Copy-On-Write) 段
- FNV-1a 哈希函数
- 8-bit 指纹用于快速过滤
- 乐观无锁读取
- CLFLUSH/WB 持久化支持

### PG Linear Hash
- Litwin 1980 算法
- high_mask/low_mask 分裂跟踪
- 填充因子触发分裂
- 溢出页支持

## 树索引

### B+tree
- max_keys+1 溢出槽位（分裂前暂存）
- 叶子节点链表（范围扫描）
- 借位左/右 + 合并
- 槽式页面持久化 + 叶子链持久化

### B-tree
- 自顶向下分裂（下降前分裂，保证父节点有空间）
- 值在所有节点中（非仅叶子）
- 与 B+tree 共用 tree_page 持久化

### tree_page (共享持久化层)
- 槽位向前增长，负载向后增长
- FNV-1a 校验和
- 碎片整理/压缩

## 跨模块数据结构

### faiss_heap — 1-based 大顶堆
### faiss_minimax_heap — 双语义堆：按最小距离 pop + 自动拒绝差候选
### faiss_visited_table — 自增 visno 避免 O(size) 清零
### faiss_result_handler — 基于阈值的 top-k 结果处理
