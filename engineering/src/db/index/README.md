# 索引子系统 — 实现文档

> 20+ 种索引实现，覆盖向量索引、传统 B 树、Hash 索引、空间索引等。

---

## 📐 索引架构

```
┌─────────────────────────────────────────────────────────────────┐
│                       索引子系统                                  │
├──────────────────┬──────────────────────────────────────────────┤
│                  │                                              │
│   传统索引        │           向量索引（ANN）                    │
│                  │                                              │
│   B-Tree         │   ┌──────────────────────────────────────┐  │
│   B+Tree         │   │   图索引                              │  │
│   Hash           │   │   HNSW / NSW / SSG / ScaNN           │  │
│   ART            │   ├──────────────────────────────────────┤  │
│   Radix Tree     │   │   倒排索引                            │  │
│   R-Tree         │   │   IVF-Flat / IVF-PQ / IVF-HNSW      │  │
│   Skip List      │   ├──────────────────────────────────────┤  │
│   T-Tree         │   │   哈希索引                            │  │
│   GIN / GiST     │   │   LSH / Multi-Probe / ITQ            │  │
│   BRIN           │   ├──────────────────────────────────────┤  │
│   Fulltext       │   │   树索引                              │  │
│                  │   │   KD-Tree / Ball-Tree                 │  │
│                  │   ├──────────────────────────────────────┤  │
│                  │   │   量化方法                            │  │
│                  │   │   PQ / SQ / OPQ / RQ / LVQ / BQ     │  │
│                  │   ├──────────────────────────────────────┤  │
│                  │   │   磁盘索引                            │  │
│                  │   │   DiskANN (Vamana + PQ)              │  │
│                  │   └──────────────────────────────────────┘  │
└──────────────────┴──────────────────────────────────────────────┘
```

---

## 📁 模块目录

```
index/
├── vector_index/          # 向量索引（20+）
│   ├── hnsw_pq/          # HNSW + PQ 混合
│   ├── nsw/              # NSW (Navigable Small World)
│   ├── ssg/              # SSG (Satire Graph)
│   ├── scann/            # ScaNN
│   ├── diskann/          # DiskANN (磁盘)
│   ├── ivf_flat/         # IVF-Flat
│   ├── ivf_pq/           # IVF-PQ
│   ├── ivf_hnsw/         # IVF-HNSW
│   ├── milvus_ivf/       # Milvus IVF 兼容
│   ├── faiss_ivf_compat/ # Faiss IVF 兼容
│   ├── lsh/              # LSH
│   ├── lsh_multiprobe/   # Multi-Probe LSH
│   ├── itq/              # ITQ (Iterative Quantization)
│   ├── spectral_hash/    # Spectral Hashing
│   ├── pq/               # PQ (Product Quantization)
│   ├── sq/               # SQ (Scalar Quantization)
│   ├── opq/              # OPQ (Optimized PQ)
│   ├── rq/               # RQ (Residual Quantization)
│   ├── lvq/              # LVQ (Learning Vector Quantization)
│   ├── bq/               # BQ (Binary Quantization)
│   ├── kd_tree/          # KD-Tree
│   ├── ball_tree/        # Ball-Tree
│   ├── annoy/            # Annoy
│   ├── reranker/         # 重排序器
│   ├── streaming/        # 流式索引
│   ├── delete/           # 删除策略
│   ├── persist/          # 持久化
│   ├── multimodal/       # 多模态索引
│   └── hybrid_search.c   # 混合检索
│
├── btree/                # B-Tree
├── bplus_tree/           # B+Tree
├── hash/                 # Hash 索引
│   ├── bloom/            # Bloom Filter
│   ├── cceh/             # CCEH
│   ├── cuckoo/           # Cuckoo Hash
│   ├── pg_hash/          # PostgreSQL Hash
│   └── xor_filter/       # XOR Filter
├── art/                  # Adaptive Radix Tree
├── radix_tree/           # Radix Tree
├── rtree/                # R-Tree (空间)
├── skip_list/            # Skip List
├── ttree/                # T-Tree
├── brin/                 # Block Range Index
├── gin/                  # GIN 索引
├── gist/                 # GiST 索引
├── fulltext/             # 全文索引
├── hilbert/              # Hilbert 曲线索引
├── bitmap/               # 位图索引
└── shared/               # 共享组件
    └── tree_page.c       # 树页面管理
```

---

## 🟢 向量索引详解

### 1. HNSW（分层可导航小世界图）

```
Layer 2:  ────────●────────●         (稀疏层，最快搜索)
Layer 1:  ────●───●───●───●───●     (中层)
Layer 0:  ●●●●●●●●●●●●●●●●●●●●●●   (底层，所有点)

搜索：从顶层入口点开始，每层贪心搜索最近邻，最后在底层精排
```

**参数**：
- `M`：每个节点最大连接数（16-64）
- `efConstruction`：构建搜索范围（100-400）
- `efSearch`：搜索搜索范围（50-1000）

**文件**：`hnsw_pq/hnsw_pq.c`

### 2. IVF-PQ（倒排文件 + 产品量化）

```
┌─────────────────────────────────────────────┐
│  原始向量: [0.1, 0.3, 0.5, 0.7, ...]       │
│         ↓  Product Quantization             │
│  聚类中心码本: 256 个，聚类维度 4            │
│         ↓                                   │
│  压缩后: [42, 128, 97, 201] (16 bytes)      │
└─────────────────────────────────────────────┘

搜索：
1. PQ 压缩 query 向量
2. 在倒排列表中找最近的几个聚类中心
3. 在这些聚类的向量中暴力搜索
```

**文件**：`ivf_flat/`, `ivf_pq/`

### 3. DiskANN（磁盘优化索引）

专为大规模向量磁盘存储设计：
- **PQ 预处理**：将向量压缩为 4-32 bytes
- **Vamana 图**：优化磁盘 I/O 的图结构
- **搜索缓存**：热点数据缓存加速

**文件**：`diskann/diskann_*.c`

### 4. NSW（导航可扩展小世界）

无分层结构的图索引，适合中小规模数据。

**文件**：`nsw/nsw.c`

### 5. LSH（局部敏感哈希）

```
原始向量 → 多个哈希函数 → 桶内搜索
```

**文件**：`lsh/lsh.c`, `lsh_multiprobe/lsh_multiprobe.c`

### 6. Reranker（重排序器）

| 重排序器 | 说明 |
|----------|------|
| `reranker.c` | 基础重排序 |
| `mmr_reranker.c` | MMR（最大边际相关性） |
| `precise_reranker.c` | 精确重排序 |
| `multi_metric_reranker.c` | 多指标重排序 |
| `two_stage_search.c` | 两阶段搜索 |

---

## 🟢 量化方法对比

| 方法 | 压缩率 | 精度损失 | 适用场景 | 文件 |
|------|--------|----------|----------|------|
| PQ | 8-64x | 中 | 大规模高维 | `pq/pq.c` |
| SQ | 4x | 高 | 小规模高精度 | `sq/sq.c` |
| OPQ | 8-64x | 低 | 高精度需求 | `opq/opq.c` |
| RQ | 可调 | 低 | 极高精度 | `rq/rq.c` |
| LVQ | 可调 | 低 | 监督学习 | `lvq/lvq.c` |
| BQ | 32x | 中 | 快速近似 | `bq/bq.c` |

---

## 🟢 传统索引详解

### B-Tree / B+Tree

```
        [10 | 20]
       /    |    \
    [5]   [15]   [25 | 30]
    / \    / \    / | \
  [3][7][12][18][23][27][35]
```

- **B-Tree**：所有节点存储数据
- **B+Tree**：仅叶子节点存储数据，叶子链表连接

**文件**：`btree/btree_*.c`, `bplus_tree/bptree_*.c`

### Hash 索引

| 类型 | 特点 | 文件 |
|------|------|------|
| Cuckoo | 高负载因子 | `cuckoo/cuckoo_core.c` |
| PG Hash | PostgreSQL 兼容 | `pg_hash/pg_hash_*.c` |
| CCEH | 并发友好 | `cceh/cceh_hash_*.c` |
| Bloom | 概率型 | `bloom/bloom_core.c` |
| XOR Filter | 空间高效 | `xor_filter/xor_filter_core.c` |

### 其他索引

| 索引 | 说明 | 文件 |
|------|------|------|
| ART | 自适应基数树 | `art/art_*.c` |
| R-Tree | 空间索引 | `rtree/rtree_*.c` |
| Skip List | 跳表 | `skip_list/skip_list_*.c` |
| GIN | 倒排索引 | `gin/gin.c` |
| GiST | 通用搜索树 | `gist/gist.c` |
| BRIN | 块范围索引 | `brin/brin.c` |

---

## 📊 索引选择指南

| 数据类型 | 推荐索引 |
|----------|----------|
| 高维向量（稠密） | HNSW, IVF-PQ |
| 高维向量（稀疏） | IVF-Flat, LSH |
| 超大规模向量 | DiskANN |
| 整数/字符串键 | B+Tree, Hash |
| 地理位置 | R-Tree, Hilbert |
| 时序数据 | BRIN |
| 全文检索 | GIN, BM25 |

---

## 🔧 构建

```bash
# 构建所有索引
cd engineering
cmake -B build -DBUILD_TESTING=ON
cmake --build build --parallel 4

# 运行索引测试
ctest --test-dir build -R index_ --output-on-failure
```

---

## 🔗 相关模块

- [存储层](../storage/README.md) — 索引的底层存储
- [RAG 系统](../../../rag/README.md) — 向量索引的应用场景
- [向量存储](../storage/vector/) — 向量页面管理
