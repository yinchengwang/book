# 多模态向量索引

## Purpose

实现多模态向量索引，支持文本、图像、音频等多模态数据的跨模态联合检索。

## Requirements

### Requirement: 多模态索引基础操作

多模态索引 SHALL 支持以下核心操作：

| 操作 | 函数签名 | 说明 |
|------|----------|------|
| 创建 | `multimodal_index_t *multimodal_create(void)` | 创建多模态索引 |
| 注册模态 | `int multimodal_register_modality(multimodal_index_t *idx, const char *name, index_type_t type)` | 注册模态索引 |
| 添加向量 | `int multimodal_add(multimodal_index_t *idx, int id, const char *modality, const float *vector)` | 添加单模态向量 |
| 跨模态搜索 | `int multimodal_cross_search(multimodal_index_t *idx, const char *query_modality, const float *query_vec, int k, int *ids, float *scores)` | 跨模态搜索 |
| 融合搜索 | `int multimodal_fusion_search(multimodal_index_t *idx, float **query_vecs, const char **modalities, int n_modalities, int k, int *ids, float *scores)` | 多模态融合搜索 |
| 混合搜索 | `int multimodal_hybrid_search(multimodal_index_t *idx, const char *text_query, const float *vector_query, float alpha, int k, int *ids, float *scores)` | 稀疏+密集混合搜索 |
| 保存 | `int multimodal_save(multimodal_index_t *idx, const char *path)` | 保存索引 |
| 加载 | `multimodal_index_t *multimodal_load(const char *path)` | 加载索引 |
| 销毁 | `void multimodal_destroy(multimodal_index_t *idx)` | 释放资源 |

### Requirement: 多模态索引核心结构

```c
// 模态类型
typedef enum {
    MODALITY_TEXT = 0,
    MODALITY_IMAGE = 1,
    MODALITY_AUDIO = 2,
    MODALITY_VIDEO = 3,
    MODALITY_OTHER = 99,
} modality_type_t;

// 单模态索引
typedef struct modality_index {
    char name[64];                 // 模态名称
    modality_type_t type;          // 模态类型
    index_type_t index_type;      // 底层索引类型
    void *index;                   // 底层索引指针
    int dims;                      // 向量维度
    int n_vectors;                 // 向量数量
} modality_index_t;

// 多模态索引
typedef struct multimodal_index {
    modality_index_t **modalities;  // 各模态索引
    int n_modalities;               // 模态数量
    int max_id;                     // 最大 ID

    // 跨模态映射（可选）
    void *cross_modal_map;          // 跨模态映射表

    // 融合参数
    float fusion_alpha;             // 融合权重
} multimodal_index_t;
```

### Requirement: 跨模态检索模式

| 模式 | 说明 | 实现方式 |
|------|------|----------|
| Late Interaction | 各模态独立编码，检索时交互 | COLBERT 风格 |
| Early Fusion | 多模态向量拼接后编码 | Concat + Encode |
| Cross Attention | 跨模态注意力融合 | Transformer |

### Requirement: 稀疏-密集混合搜索

混合搜索 SHALL 支持 BM25（稀疏）+ HNSW/DiskANN（密集）组合：

```c
// 混合搜索参数
typedef struct hybrid_search_params {
    // 密集向量部分
    const float *dense_vector;
    index_type_t dense_index_type;
    float dense_weight;            // 密集向量权重

    // 稀疏向量部分
    const char *text_query;        // 或 BM25SparseVector
    float sparse_weight;           // 稀疏向量权重

    // 融合方式
    enum {
        FUSION_RRF,               // Reciprocal Rank Fusion
        FUSION_WEIGHTED_SUM,       // 加权和
        FUSION_COMPOSE,           // 分层组合
    } fusion_type;
} hybrid_search_params_t;
```

### Requirement: Reciprocal Rank Fusion (RRF)

RRF 融合 SHALL 按以下公式计算：

```
RRF_score(d) = Σ weight / (k + rank_d)
```

| 参数 | 说明 | 默认值 |
|------|------|--------|
| weight | 模态权重 | 1.0 |
| k | RRF 平滑因子 | 60 |
| rank_d | 文档 d 在该模态结果中的排名 | - |

### Requirement: 典型应用场景

| 场景 | Query 模态 | 检索模态 | 说明 |
|------|------------|----------|------|
| 图文搜索 | 文本 | 图像 | 用文字搜索图片 |
| 商品推荐 | 图像 | 商品 | 以图搜商品 |
| 多模态问答 | 文本+图像 | 文本 | 融合多模态信息 |
| 视频检索 | 音频 | 视频 | 音频搜索视频 |

#### Scenario: 以图搜图

- **WHEN** 用户上传一张图片
- **AND** 多模态索引包含图像模态
- **THEN** 系统 SHALL 使用图像向量进行相似度搜索
- **AND** 返回相似图片列表

#### Scenario: 文本搜索图片

- **WHEN** 用户输入"一只橘色的猫"
- **AND** 多模态索引包含文本模态和图像模态
- **THEN** 系统 SHALL 先将文本转为向量
- **AND** 在图像模态中检索相似向量
- **AND** 返回匹配的图片

#### Scenario: 混合搜索

- **WHEN** 用户输入"红色的裙子"和图片向量
- **AND** 使用 alpha=0.7（文本权重 0.7，图像权重 0.3）
- **THEN** 系统 SHALL 计算 `0.7 * text_score + 0.3 * image_score`
- **AND** 返回综合得分最高的文档
