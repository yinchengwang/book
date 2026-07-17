## Context

当前 index 模块已有以下实现：
- **树索引**：B-Tree、B+Tree
- **哈希索引**：CCEH、PostgreSQL Hash
- **向量索引**：HNSW、DiskANN、IVF、IVF-HNSW、BM25
- **量化器**：PQ、LVQ、SQ、RQ

缺少的索引类型：
- 传统索引：GIN、GiST、FULLTEXT、BRIN、Bitmap、R-Tree（用户指定放其他项目）
- 向量索引：IVF-PQ、LSH、OPQ、多模态索引

## Goals / Non-Goals

**Goals:**
- 实现完整的传统索引体系（GIN、GiST、FULLTEXT、BRIN、Bitmap）
- 完善向量索引能力（IVF-PQ、LSH、OPQ、多模态）
- 保持与现有代码风格一致（C 语言，CMake 构建）
- 复用已有模块（B-Tree/PQ/BM25 等）

**Non-Goals:**
- R-Tree 空间索引（用户指定放其他项目）
- 数据库完整实现（只实现索引算法，不实现 SQL 引擎）
- GUI 或 Web 界面

## Decisions

### 决策 1：目录结构设计

```
src/index/
├── inverted/              # 倒排索引族
│   ├── gin/               #   GIN 通用倒排索引
│   ├── gist/              #   GiST 广义搜索树
│   ├── fulltext/          #   全文索引
│   └── bitmap/             #   Bitmap 位图索引
├── block/                 # 块级索引
│   └── brin/               #   BRIN 块范围索引
└── vector_index/          # 向量索引（扩展）
    ├── ivf_pq/            #   IVF-PQ
    ├── lsh/               #   LSH
    ├── opq/                #   OPQ
    └── multimodal/         #   多模态索引
```

**理由**：按索引范式分类，与现有 tree/hash/vector_index 平级。

### 决策 2：传统索引接口设计

采用统一的索引接口模式：

```c
// 通用索引接口
typedef struct index_ops {
    int (*insert)(void *ctx, const void *key, const void *value);
    int (*search)(void *ctx, const void *query, void *results);
    int (*delete)(void *ctx, const void *key);
    void (*destroy)(void *ctx);
} index_ops_t;

// GIN 特定接口
typedef struct gin_index gin_index_t;
gin_index_t *gin_create(int max_entries);
int gin_insert(gin_index_t *idx, const char *key, int doc_id);
int gin_search(gin_index_t *idx, const char *query, int *results, int *count);

// GiST 特定接口
typedef struct gist_index gist_index_t;
gist_index_t *gist_create(void);
int gist_insert(gist_index_t *idx, const void *key, const void *bbox);
int gist_search(gist_index_t *idx, const void *query_bbox, void **results);
```

### 决策 3：IVF-PQ 实现策略

**复用优先**：
1. 复用现有的 `ivf/` 模块（聚类、倒排列表管理）
2. 复用现有的 `pq/` 量化器（编码、解码、距离表）
3. 新增 `ivf_pq/` 只需组合两者

```c
// IVF-PQ 核心结构
typedef struct ivf_pq_index {
    ivf_index_t *ivf;           // 底层 IVF
    quantizer_t *pq;            // PQ 量化器
    int nprobe;                 // 查询探针数
} ivf_pq_index_t;
```

### 决策 4：LSH 实现选择

**多策略支持**：
- Bitwise LSH：二值向量（汉明距离）
- p-stable LSH：L2 距离
- SimHash：Cosine 相似度

```c
// LSH 类型
typedef enum lsh_type {
    LSH_BITWISE,    // Bitwise LSH
    LSH_PSTABLE,    // p-stable LSH (L2)
    LSH_SIMHASH,    // SimHash (Cosine)
} lsh_type_t;
```

### 决策 5：OPQ 实现

OPQ = PQ + PCA 旋转。在 PQ 训练前对数据进行 PCA 变换，使各子空间方差均衡。

```c
typedef struct opq_quantizer {
    quantizer_t *pq;           // 底层 PQ
    float *rotation_matrix;   // PCA 旋转矩阵
    int dims;                  // 向量维度
} opq_quantizer_t;
```

## Risks / Trade-offs

| 风险 | 描述 | 缓解措施 |
|------|------|----------|
| 实现复杂度 | GiST 支持自定义操作符，复杂度高 | 分阶段实现，先完成 B-Tree 操作符 |
| 内存占用 | Bitmap 索引在大基数列上存储膨胀 | 限制单索引列基数，或使用 RLE 压缩 |
| 查询精度 | LSH 是概率索引，结果有误差 | 多哈希函数 + OR 组合提高召回 |
| 中文分词 | FULLTEXT 需要中文分词支持 | 复用现有分词器或接入开源方案 |

## Open Questions

1. **GiST 操作符体系**：PostgreSQL 的 GiST 支持 9 种几何关系操作符，是否需要全部实现？
   - 建议：先实现基本的 Within/Contains/Intersects
2. **Bitmap 压缩格式**：Roaring Bitmap vs WAH vs BBC？
   - 建议：先用简单 RLE，后续可扩展
3. **多模态索引方案**：COLBERT 式 Late Interaction vs Early Fusion？
   - 建议：先实现 Late Interaction（BM25 + HNSW 组合）
