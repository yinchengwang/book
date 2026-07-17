# IVF-HNSW 优化设计

## Context

### 背景

当前 IVF 索引实现了两级聚类（nlist × nlist2）和残差量化（PQ/SQ/LVQ/RQ），搜索流程为：

```
查询向量 → 暴力扫描找 nprobe 个一级簇 → 展开二级桶 → 桶内扫描 → top-k
```

粗排阶段复杂度 O(nlist × dims) 已成为性能瓶颈，尤其当 nlist 达到 10000+ 时。

### 现有代码结构

```
include/index/vector_index/ivf/     # 现有 IVF 实现
├── IndexIVF.h
├── inverted_lists.h
├── direct_map.h
└── ivf_log.h

src/index/vector_index/ivf/         # IVF 实现
├── faiss_ivf_core.c               # 创建/销毁
├── faiss_ivf_train.c              # 训练
├── faiss_ivf_add.c                # 添加向量
├── faiss_ivf_search.c             # 搜索（粗排在这里）
├── faiss_ivf_utils.c
└── faiss_ivf_private.h            # 内部结构
```

### 约束

- C 语言实现，保持与现有代码风格一致
- 不修改现有 IVF/HNSW 接口，保证向后兼容
- 所有参数可配置，不硬编码魔法值
- 内存占用优先：HNSW 图只索引一级中心（数量少）

## Goals / Non-Goals

**Goals:**
- 实现 HNSW-IVF 复合索引，用 HNSW 替代暴力粗排
- 实现 Multi-assignment 支持，允许向量分配到多个桶
- 精度优先，性能作为约束条件
- 完整的基准测试验证

**Non-Goals:**
- 不修改现有 IVF/HNSW 实现（独立实现）
- 不实现磁盘级存储（IVF 的适用范围 100w-1000w）
- 不实现增量更新（训练后数据只读）

## Decisions

### Decision 1: 独立目录 vs 代码复用

**选择：独立目录 `ivf_hnsw/`**

| 选项 | 优点 | 缺点 |
|------|------|------|
| 复用现有 HNSW | 代码量少 | 语义冲突：HNSW-IVF 索引一级中心，现有 HNSW 索引原始向量 |
| 独立目录 | 语义清晰，易维护 | 代码量稍多 |

**结论**：选择独立目录，但复用：
- HNSW 图结构设计思路
- 距离计算模块
- K-Means 训练模块

### Decision 2: HNSW 图存储格式

**选择：扁平化邻接数组（与现有 HNSW 一致）**

```
结构:
├── levels[nlist]          一级中心的层高
├── offsets[nlist]        邻接表偏移
└── nbs[...]              扁平化邻接表
```

**理由**：
- 内存连续，缓存友好
- 实现简单，与现有 HNSW 一致
- 一级中心数量少（nlist=1000~10000），内存开销可忽略

### Decision 3: Multi-assignment 分配策略

**选择：最近 k 个桶（可配置）**

```c
// 每个向量记录分配到的桶
typedef struct {
    int32_t bucket_id;    // 桶编号
    int32_t offset;        // 桶内偏移
} assign_entry_t;

// 存储布局
assign_entry_t assignments[n_total][max_assignments];
```

**分配算法**：
1. 计算向量到所有一级中心的距离
2. 排序，取最近的 k 个
3. 同时将向量添加到这 k 个桶的倒排列表

### Decision 4: HNSW 构建时机

**选择：训练时同步构建**

```c
int32_t faiss_ivf_hnsw_index_train(faiss_ivf_hnsw_t *index, int32_t n, const float *vectors) {
    // 1. 一级 K-Means 聚类
    // 2. 二级 K-Means 聚类
    // 3. 量化器训练
    // 4. HNSW 图构建（新增）
    return hnsw_build_from_centroids(index);
}
```

**理由**：一次遍历，训练效率高

### Decision 5: 量化支持

**选择：复用现有量化框架**

- IVF-HNSW 使用独立的 quantizer（索引中心点，无量化）
- 二级桶内向量使用独立 quantizer（可配置 PQ/SQ/LVQ/RQ）
- 两者互不干扰

## Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                    HNSW-IVF 完整架构                                  │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │                     训练阶段                                     │  │
│  │                                                               │  │
│  │  原始向量 ──▶ 一级 K-Means ──▶ 一级中心                        │  │
│  │                │                                              │  │
│  │                ▼                                              │  │
│  │  二级 K-Means ──▶ 二级桶                                       │  │
│  │                │                                              │  │
│  │                ▼                                              │  │
│  │  HNSW 图构建 ──▶ HNSW(一级中心)                               │  │
│  │                │                                              │  │
│  │                ▼                                              │  │
│  │  量化器训练 ──▶ PQ/SQ 量化器                                   │  │
│  │                │                                              │  │
│  │                ▼                                              │  │
│  │  Multi-assignment ──▶ 分配表                                   │  │
│  └───────────────────────────────────────────────────────────────┘  │
│                              │                                       │
│                              ▼                                       │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │                     搜索阶段                                     │  │
│  │                                                               │  │
│  │  查询向量 ──▶ HNSW 搜索 ──▶ nprobe 个一级簇                    │  │
│  │                             │                                   │  │
│  │                             ▼                                   │  │
│  │                    展开二级桶（按距离排序）                       │  │
│  │                             │                                   │  │
│  │                             ▼                                   │  │
│  │  Multi-assignment ──▶ 额外展开分配的桶                         │  │
│  │                             │                                   │  │
│  │                             ▼                                   │  │
│  │                    桶内扫描 ──▶ top-k                          │  │
│  └───────────────────────────────────────────────────────────────┘  │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

## 目录结构

```
include/index/vector_index/
├── ivf/
│   ├── IndexIVF.h              # 现有 IVF
│   └── ...
└── ivf_hnsw/                   # 新建
    ├── IndexIVFHNSW.h          # 公共接口
    └── faiss_ivf_hnsw_private.h # 内部结构

src/index/vector_index/
├── ivf/                        # 现有 IVF
│   ├── faiss_ivf_*.c
│   └── ...
└── ivf_hnsw/                   # 新建
    ├── faiss_ivf_hnsw_index.c  # 创建/销毁
    ├── faiss_ivf_hnsw_train.c  # 训练 + HNSW 构建
    ├── faiss_ivf_hnsw_add.c    # 添加 + Multi-assignment
    ├── faiss_ivf_hnsw_search.c  # HNSW 粗排 + 桶扫描
    ├── faiss_hnsw_quantizer.c  # HNSW 图量化器
    └── faiss_ivf_hnsw_private.h # 内部结构 + 工具函数

test/vector_index/
└── ivf_hnsw/                   # 新建
    └── ivf_hnsw_test.cpp       # 基准测试
```

## Risks / Trade-offs

| Risk | Description | Mitigation |
|------|-------------|------------|
| HNSW 构建耗时 | 一级中心数量大时构建慢 | nlist 通常 1000~10000，可接受 |
| Multi-assignment 内存 | k 增大时内存线性增长 | 默认 k=1，用户显式配置 |
| 精度 vs 速度 | ef_search 增大时速度下降 | 用户可调，精度优先场景用大值 |
| 与现有代码差异 | 独立实现导致代码重复 | 抽取公共模块复用（如 K-Means） |

## Open Questions

1. **ef_search 默认值**：建议 ef_search = nprobe × 2，是否合理？
2. **HNSW M 参数**：建议 M=16~32（根据维度选择），是否需要动态调整？
3. **桶内是否需要 HNSW**：当桶内向量数量很大时，是否在桶内也构建 HNSW？

## 参数配置建议

| 参数 | 建议值 | 说明 |
|------|--------|------|
| nlist | 1000~10000 | 数据量/1000 |
| nlist2 | 50~200 | 每簇内二级数 |
| nprobe | nlist/20~nlist/10 | 典型 1%~10% |
| M | 16~32 | HNSW 邻居数 |
| ef_search | nprobe × 2~5 | 越大越精确 |
| k_assignments | 1~3 | Multi-assignment 数 |
