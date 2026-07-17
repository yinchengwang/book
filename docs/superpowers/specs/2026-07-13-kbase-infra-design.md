# kbase 项目设计文档

> 轻量级推理引擎（infra）+ Obsidian 知识库（kbase）

## 1. 整体架构

```
┌──────────────────────────────────────────────────────────────────────┐
│                      kbase — 知识库系统                               │
│                                                                      │
│  ┌─────────────────────────┐ ┌────────────────────────────────────┐  │
│  │      CLI 应用层          │ │      learning/notes/              │  │
│  │  (apps/kbase/)          │ │      Obsidian 笔记仓库              │  │
│  │                         │ │                                    │  │
│  │  kbase search <query>   │ │  ├── LLM/transformer/             │  │
│  │  kbase ask <question>   │ │  ├── learn_deep/                  │  │
│  │  kbase summarize <path> │ │  ├── C/CPP/DS/...                 │  │
│  │  kbase index            │ │  └── ...                          │  │
│  └──────────┬──────────────┘ └───────────────┬────────────────────┘  │
│             │                                 │                       │
│  ┌──────────▼─────────────────────────────────▼────────────────────┐ │
│  │                     知识库核心层 (src/kbase/)                    │ │
│  │                                                               │ │
│  │  ┌──────────────┐ ┌────────────┐ ┌──────────┐ ┌────────────┐  │ │
│  │  │ kbase_index  │ │kbase_search│ │kbase_rag │ │kbase_utils │  │ │
│  │  │ 索引构建器    │ │ 语义搜索    │ │ 问答     │ │ 工具函数    │  │ │
│  │  └──────┬───────┘ └─────┬──────┘ └────┬─────┘ └────────────┘  │ │
│  └─────────┼───────────────┼─────────────┼────────────────────────┘ │
│            │               │             │                            │
└────────────┼───────────────┼─────────────┼────────────────────────────┘
             │               │             │
             ▼               ▼             ▼
┌──────────────────────────────────────────────────────────────────────┐
│                 infra — 轻量级推理引擎                                │
│                                                                      │
│  ┌──────────────────────────────────────────────────────────────┐    │
│  │                   推理引擎核心层                                │    │
│  │                                                              │    │
│  │  ┌────────────┐ ┌───────────┐ ┌──────────┐ ┌─────────────┐  │    │
│  │  │ 模型加载器  │ │ 计算图     │ │ 算子库   │ │ 张量/内存    │  │    │
│  │  │ GGUF/ONNX  │ │ 调度器     │ │ MatMul/  │ │ 管理         │  │    │
│  │  │ 解析器      │ │ 优化器     │ │ Attn/... │ │             │  │    │
│  │  └────────────┘ └───────────┘ └──────────┘ └─────────────┘  │    │
│  └──────────────────────────────────────────────────────────────┘    │
│                                                                      │
│  ┌──────────────────────────────────────────────────────────────┐    │
│  │                    优化层                                      │    │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌────────────────┐  │    │
│  │  │ SIMD     │ │ 量化引擎 │ │ 算子融合 │ │ 多线程运行时    │  │    │
│  │  │ SSE/AVX  │ │ INT8/INT4│ │          │ │ 线程池          │  │    │
│  │  └──────────┘ └──────────┘ └──────────┘ └────────────────┘  │    │
│  └──────────────────────────────────────────────────────────────┘    │
│                                                                      │
│  ┌──────────────────────────────────────────────────────────────┐    │
│  │                    生态层                                      │    │
│  │  ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌───────────┐  │    │
│  │  │ Python     │ │ 模型转换   │ │ ONNX 支持  │ │ Tokenizer │  │    │
│  │  │ 绑定       │ │ 工具       │ │           │ │            │  │    │
│  │  └────────────┘ └────────────┘ └────────────┘ └───────────┘  │    │
│  └──────────────────────────────────────────────────────────────┘    │
│                                                                      │
│  ┌──────────────────────────────────────────────────────────────┐    │
│  │                    3 阶段演进                                  │    │
│  │  ┌──────────────────┐ ┌──────────────────┐ ┌──────────────┐  │    │
│  │  │ Phase 1: 核心能力│ │ Phase 2: 性能优化 │ │ Phase 3:    │  │    │
│  │  │ GGUF + Embedding │ │ SIMD + 量化      │ │ ONNX + 生成  │  │    │
│  │  └──────────────────┘ └──────────────────┘ └──────────────┘  │    │
│  └──────────────────────────────────────────────────────────────┘    │
└──────────────────────────────────────────────────────────────────────┘
```

## 2. 项目结构

```
engineering/
├── CMakeLists.txt                  # 新增 kbase 子目录
├── include/
│   └── kbase/
│       ├── infra/
│       │   ├── infra.h             # 主头文件，统一 API 入口
│       │   ├── types.h             # 类型定义（infra_dtype_t, infra_status_t）
│       │   ├── tensor.h            # 张量系统
│       │   ├── model.h             # 模型加载接口
│       │   ├── graph.h             # 计算图定义
│       │   ├── op.h                # 算子注册与执行
│       │   ├── memory.h            # 内存池
│       │   └── quant.h             # 量化接口
│       ├── kbase_index.h           # 笔记索引
│       ├── kbase_search.h          # 语义搜索
│       ├── kbase_rag.h             # RAG 问答
│       └── kbase_utils.h           # 工具函数
│
├── src/kbase/
│   ├── CMakeLists.txt              # 构建 kbase 库
│   ├── infra/
│   │   ├── model_loader.c          # 模型加载器
│   │   ├── model_gguf.c            # GGUF 格式解析
│   │   ├── model_onnx.c            # ONNX 格式解析（Phase 3）
│   │   ├── tensor.c                # 张量实现
│   │   ├── graph.c                 # 计算图调度
│   │   ├── graph_optimizer.c       # 计算图优化器
│   │   ├── memory.c                # 内存池
│   │   ├── quant.c                 # 量化推理引擎
│   │   ├── ops/
│   │   │   ├── op_matmul.c         # MatMul 算子
│   │   │   ├── op_add.c            # 逐元素加
│   │   │   ├── op_mul.c            # 逐元素乘
│   │   │   ├── op_activations.c    # ReLU, GELU, SiLU
│   │   │   ├── op_norm.c           # LayerNorm, RMSNorm
│   │   │   ├── op_softmax.c        # Softmax
│   │   │   ├── op_reshape.c        # Reshape
│   │   │   ├── op_transpose.c      # Transpose
│   │   │   └── op_attention.c      # Attention（Phase 3）
│   │   ├── simd/
│   │   │   ├── simd_matmul_sse.c   # SSE 矩阵乘法
│   │   │   └── simd_matmul_avx.c   # AVX 矩阵乘法
│   │   └── tokenizer.c             # Tokenizer（Phase 3）
│   ├── kbase_index.c               # 笔记索引构建
│   ├── kbase_search.c              # 语义搜索
│   ├── kbase_rag.c                 # RAG 问答
│   └── kbase_utils.c               # 工具函数
│
├── apps/kbase/
│   ├── CMakeLists.txt
│   ├── main.c                      # CLI 入口
│   ├── cmd_index.c                 # kbase index 命令
│   ├── cmd_search.c                # kbase search 命令
│   ├── cmd_ask.c                   # kbase ask 命令
│   ├── cmd_summarize.c             # kbase summarize 命令
│   └── cmd_embed.c                 # infra embed 命令
│
└── test/kbase/
    ├── CMakeLists.txt
    ├── test_infra_tensor.cpp       # 张量系统测试
    ├── test_infra_ops.cpp          # 算子精度测试
    ├── test_infra_model.cpp        # 模型加载测试
    ├── test_infra_embed.cpp        # Embedding 端到端测试
    ├── test_infra_quant.cpp        # 量化精度测试
    ├── test_infra_simd.cpp         # SIMD 正确性测试
    ├── test_kbase_index.cpp        # 索引构建测试
    ├── test_kbase_search.cpp       # 语义搜索测试
    └── test_kbase_rag.cpp          # RAG 问答测试

reference/models/                   # 模型文件（gitignored）
├── .gitkeep
└── download_model.sh               # 模型下载脚本

learning/notes/                     # 现有 Obsidian 笔记仓库（不变）
```

## 3. 推理引擎核心设计

### 3.1 类型系统

```c
// 数据类型枚举
typedef enum {
    INFRA_DTYPE_F32   = 0,    // 32-bit 浮点
    INFRA_DTYPE_F16   = 1,    // 16-bit 浮点（存储用）
    INFRA_DTYPE_Q8_0  = 2,    // 8-bit 量化 (block 32)
    INFRA_DTYPE_Q4_0  = 3,    // 4-bit 量化 (block 32)
    INFRA_DTYPE_Q4_1  = 4,    // 4-bit 量化 (block 32, 偏移)
    INFRA_DTYPE_I32   = 5,    // 32-bit 整数
    INFRA_DTYPE_I64   = 6,    // 64-bit 整数
} infra_dtype_t;

// 状态码
typedef enum {
    INFRA_OK           = 0,
    INFRA_ERR_LOAD     = -1,  // 模型加载失败
    INFRA_ERR_FORMAT   = -2,  // 格式不支持
    INFRA_ERR_MEMORY   = -3,  // 内存不足
    INFRA_ERR_GRAPH    = -4,  // 计算图错误
    INFRA_ERR_OP       = -5,  // 算子执行错误
    INFRA_ERR_PARAM    = -6,  // 参数错误
} infra_status_t;
```

### 3.2 张量系统

```c
typedef struct infra_tensor {
    infra_dtype_t dtype;          // 数据类型
    int32_t ndim;                  // 维度数
    int64_t dims[INFRA_MAX_DIMS]; // 各维度尺寸
    int64_t strides[INFRA_MAX_DIMS]; // 各维度步长
    size_t nelems;                 // 元素总数
    size_t nbytes;                 // 数据字节数
    void* data;                    // 数据指针
    int owns_data;                 // 是否拥有数据所有权
    const char* name;              // 张量名称（用于调试）
} infra_tensor_t;

// 创建/销毁
infra_tensor_t* infra_tensor_create(const int64_t* dims, int ndim, infra_dtype_t dtype);
void infra_tensor_free(infra_tensor_t* t);
void infra_tensor_free_all(infra_tensor_t** tensors, int n);

// 操作
infra_status_t infra_tensor_reshape(infra_tensor_t* t, const int64_t* new_dims, int new_ndim);
infra_status_t infra_tensor_copy(infra_tensor_t* dst, const infra_tensor_t* src);
int infra_tensor_is_contiguous(const infra_tensor_t* t);
size_t infra_tensor_sizeof_dtype(infra_dtype_t dtype);
```

### 3.3 算子系统

算子注册表模式：

```c
// 算子函数签名
typedef infra_status_t (*infra_op_func_t)(
    infra_tensor_t** inputs, int num_inputs,
    infra_tensor_t** outputs, int num_outputs,
    const void* params);

// 算子描述符
typedef struct {
    const char* name;              // 算子名（如 "matmul"）
    infra_op_func_t func;          // 实现函数
    int min_inputs;                // 最少输入数
    int max_inputs;                // 最大输入数
    int min_outputs;               // 最少输出数
    int max_outputs;               // 最大输出数
    const char* description;       // 描述
} infra_op_t;

// 算子注册与查找
void infra_op_register(const infra_op_t* op);
const infra_op_t* infra_op_find(const char* name);
void infra_op_register_all(void);  // 注册所有内置算子
```

### 3.4 计算图

```c
// 计算图节点
typedef struct infra_graph_node {
    const infra_op_t* op;               // 绑定的算子
    infra_tensor_t* output;              // 输出张量
    struct infra_graph_node** inputs;    // 输入节点
    int num_inputs;
    void* params;                        // 算子参数（如 MatMul 的转置标志）
    size_t params_size;                  // 参数字节数
    const char* name;                    // 节点名称（调试用）
} infra_graph_node_t;

// 计算图
typedef struct {
    infra_graph_node_t** nodes;          // 所有节点
    int num_nodes;
    int capacity;
    int* exec_order;                     // 拓扑排序后的执行顺序
    int num_exec_nodes;
} infra_graph_t;

// 构建与执行
infra_graph_t* infra_graph_create(int capacity);
void infra_graph_free(infra_graph_t* g);
infra_graph_node_t* infra_graph_add_node(infra_graph_t* g,
    const char* op_name, infra_tensor_t* output);
infra_status_t infra_graph_add_edge(infra_graph_t* g,
    infra_graph_node_t* from, infra_graph_node_t* to, int input_idx);
infra_status_t infra_graph_build(infra_graph_t* g);
infra_status_t infra_graph_execute(infra_graph_t* g);
```

### 3.5 模型加载

```c
// 模型格式
typedef enum {
    INFRA_MODEL_GGUF = 0,    // GGUF 格式
    INFRA_MODEL_ONNX = 1,    // ONNX 格式（Phase 3）
} infra_model_format_t;

// 模型架构
typedef enum {
    INFRA_ARCH_BERT     = 0,  // BERT 类（Embedding）
    INFRA_ARCH_Llama    = 1,  // LLaMA 类（文本生成，Phase 3）
    INFRA_ARCH_UNKNOWN  = -1,
} infra_model_arch_t;

// 模型句柄
typedef struct infra_model {
    infra_model_format_t format;
    infra_model_arch_t arch;
    infra_graph_t* graph;
    struct infra_model_gguf* gguf_data;  // GGUF 内部数据
    struct infra_model_onnx* onnx_data;  // ONNX 内部数据
    // 模型参数
    int n_embd;              // 嵌入维度
    int n_head;              // 注意力头数
    int n_layer;             // Transformer 层数
    int n_ctx;               // 上下文长度
    // 内部状态
    infra_memory_pool_t* pool;
    int loaded;
} infra_model_t;

// 模型加载 API
infra_model_t* infra_model_load(const char* path, infra_model_format_t fmt);
void infra_model_free(infra_model_t* model);
infra_status_t infra_model_validate(infra_model_t* model);
```

### 3.6 内存管理

```c
// 内存池（预分配，避免频繁 malloc/free）
typedef struct infra_memory_pool infra_memory_pool_t;

infra_memory_pool_t* infra_memory_pool_create(size_t initial_size, size_t max_size);
void infra_memory_pool_destroy(infra_memory_pool_t* pool);
void* infra_memory_pool_alloc(infra_memory_pool_t* pool, size_t size);
void* infra_memory_pool_calloc(infra_memory_pool_t* pool, size_t size);
void infra_memory_pool_reset(infra_memory_pool_t* pool);
```

### 3.7 统一推理 API

```c
// Embedding 推理
infra_status_t infra_embed(
    infra_model_t* model,
    const char** texts, int num_texts,
    float** embeddings_out, int* dim_out);

// 释放 Embedding 结果
void infra_embed_free(float* embeddings, int num_texts);

// 文本生成（Phase 3）
typedef struct {
    int max_tokens;          // 最大生成 token 数
    float temperature;       // 温度
    float top_p;             // Top-P 采样
    int top_k;               // Top-K 采样
    int repeat_penalty;      // 重复惩罚
} infra_gen_params_t;

infra_status_t infra_generate(
    infra_model_t* model,
    const char* prompt,
    char* output, int max_len,
    const infra_gen_params_t* params);
```

## 4. 知识库核心设计

### 4.1 笔记索引

```c
// 笔记条目
typedef struct {
    char* path;              // 相对路径
    char* title;             // 标题（从文件名或 # 提取）
    char* content;           // 全文内容
    float* embedding;        // 向量（384 维）
    size_t content_len;      // 内容长度
    int embedding_dim;       // 向量维度
} kbase_note_t;

// 索引
typedef struct kbase_index {
    kbase_note_t* notes;
    int num_notes;
    int capacity;
    char* notes_dir;         // 笔记根目录
    // 子模块
    struct vector_index* vec_idx;  // VDB 向量索引
    int dim;                      // 向量维度
} kbase_index_t;

// 索引构建
kbase_index_t* kbase_index_create(const char* notes_dir);
void kbase_index_destroy(kbase_index_t* idx);
int kbase_index_scan(kbase_index_t* idx);                     // 扫描文件
infra_status_t kbase_index_build(kbase_index_t* idx, infra_model_t* model);  // 生成 Embedding
infra_status_t kbase_index_save(kbase_index_t* idx, const char* path);
infra_status_t kbase_index_load(kbase_index_t* idx, const char* path);
```

### 4.2 语义搜索

```c
// 搜索结果
typedef struct {
    kbase_note_t* note;
    float score;
    int rank;
} kbase_result_t;

// 搜索
kbase_result_t* kbase_search(
    kbase_index_t* idx,
    infra_model_t* model,
    const char* query,
    int top_k,
    int* num_results);

void kbase_search_free(kbase_result_t* results, int num_results);
```

### 4.3 RAG 问答

```c
// 问答
char* kbase_ask(
    kbase_index_t* idx,
    infra_model_t* model,
    const char* question,
    int top_k);
```

## 5. GGUF 格式解析

### 5.1 GGUF 文件结构

```
┌──────────────────────┐
│ GGUF Header (21 字节)│
│  魔数: "GGUF"        │
│  版本: 3             │
│  张量数: T           │
│  metadata KV 数: K   │
├──────────────────────┤
│ Metadata KV 数组      │
│  每个 KV:             │
│  key_len | key | type | value     │
│  (支持: uint32/int32/float32/     │
│   string/array)       │
├──────────────────────┤
│ Tensor Info 数组      │
│  每个 Tensor:         │
│  name_len | name | ndim | dims |  │
│  type | offset        │
├──────────────────────┤
│ Tensor Data 区域      │
│  对齐到 32 字节       │
│  按 offset 索引       │
└──────────────────────┘
```

### 5.2 GGUF 解析器设计

```c
// GGUF 头
#define INFRA_GGUF_MAGIC   0x46554747  // "GGUF"

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint64_t tensor_count;
    uint64_t metadata_kv_count;
} infra_gguf_header_t;

// GGUF 张量信息
typedef struct {
    char* name;
    int ndim;
    int64_t dims[4];
    int32_t dtype;        // GGUF 张量类型
    uint64_t offset;      // 文件偏移
} infra_gguf_tensor_info_t;

// GGUF 模型数据
typedef struct infra_model_gguf {
    infra_gguf_header_t header;
    infra_gguf_tensor_info_t* tensor_infos;
    uint64_t* tensor_offsets;  // 排序后的偏移数组
    char* file_data;           // mmap 文件映射
    size_t file_size;
    int fd;                    // 文件描述符
} infra_model_gguf_t;

// 解析 API
infra_status_t infra_gguf_load(infra_model_gguf_t* gguf, const char* path);
void infra_gguf_free(infra_model_gguf_t* gguf);
infra_gguf_tensor_info_t* infra_gguf_find_tensor(infra_model_gguf_t* gguf, const char* name);
void* infra_gguf_tensor_data(infra_model_gguf_t* gguf, infra_gguf_tensor_info_t* info);
```

### 5.3 MiniLM-L6 模型架构

MiniLM-L6 是 6 层 Transformer Encoder，输入输出流程：

```
Input: "hello world"
    │
    ├── Tokenizer (WordPiece) → [101, 7592, 2088, 102]
    │
    ├── Token Embedding + Position Embedding + TokenType Embedding
    │
    ├── [Layer 1-6] Transformer Encoder Block
    │   ├── Multi-Head Self-Attention (12 heads, 384 dim)
    │   │   ├── QKV Projection (MatMul + Bias)
    │   │   ├── Attention Score (Softmax(Q·K^T / sqrt(d_k)))
    │   │   ├── Weighted Sum (Score · V)
    │   │   └── Output Projection (MatMul + Bias)
    │   ├── Residual + LayerNorm
    │   └── FFN (Linear → GELU → Linear)
    │       └── Residual + LayerNorm
    │
    ├── Pooler (第一 token 的线性变换 + Tanh)
    │
    Output: 384-dim 向量
```

## 6. 数据流

### 6.1 索引构建流程

```
kbase index --dir learning/notes/ --model model.gguf
    │
    1. 扫描目录，收集所有 .md 文件
    2. 解析 frontmatter / 标题 / 正文
    3. 对每个笔记调用 infra_embed() 生成向量
    4. 写入 VDB HNSW 索引
    5. 保存索引元数据到文件
```

### 6.2 搜索流程

```
kbase search "什么是 Transformer"
    │
    1. infra_embed("什么是 Transformer") → 查询向量
    2. VDB HNSW search → Top-K 笔记 ID
    3. 按分数排序，返回结果
    4. 显示：笔记标题 + 摘要 + 相似度分数
```

### 6.3 问答流程

```
kbase ask "解释一下 Transformer 的注意力机制"
    │
    1. infra_embed(question) → 查询向量
    2. VDB HNSW search → Top-K 相关笔记
    3. 拼接相关笔记内容作为上下文
    4. 构建 prompt: [指令] + [上下文] + [问题]
    5. infra_generate(prompt) → 生成回答
    6. 显示回答 + 引用来源
```

## 7. Phase 演进

### Phase 1: 核心能力

| 组件 | 交付物 | 验证标准 |
|------|--------|---------|
| 张量系统 | tensor.h/c | 创建/reshape/复制正确 |
| 核心算子 | MatMul, Add, Mul, ReLU/GELU, Softmax, LayerNorm, Reshape, Transpose | 与 Python 参考实现误差 < 1e-5 |
| 计算图 | graph.h/c | 拓扑排序正确，执行顺序正确 |
| GGUF 解析 | model_gguf.c | 能加载 MiniLM-L6 GGUF 模型 |
| Embedding 推理 | infra_embed() | 输入文本 → 384 维向量，与 Python 版误差 < 1e-3 |
| 内存池 | memory.h/c | 无泄漏，预分配生效 |
| 知识库索引 | kbase_index.c | 能扫描 100+ 笔记文件 |
| 语义搜索 | kbase_search.c | 搜索功能正确 |
| CLI 工具 | apps/kbase/ | 命令行可用 |
| 集成测试 | 精度验证 | 端到端测试通过 |

### Phase 2: 性能优化

| 组件 | 交付物 | 验证标准 |
|------|--------|---------|
| SSE MatMul | simd_matmul_sse.c | 比标量快 2x+ |
| AVX MatMul | simd_matmul_avx.c | 比标量快 4x+ |
| INT8 量化 | quant.c Q8_0 | 精度损失 < 2% |
| INT4 量化 | quant.c Q4_0/Q4_1 | 精度损失 < 5% |
| 算子融合 | graph_optimizer.c | 推理速度提升 |
| 多线程 | 线程池 + 并行推理 | 推理速度提升 |
| 基准测试 | 性能对比报告 | 量化 vs 浮点对比 |

### Phase 3: 生态扩展

| 组件 | 交付物 | 验证标准 |
|------|--------|---------|
| ONNX 解析 | model_onnx.c | 能加载 ONNX 模型 |
| 文本生成 | infra_generate() + Tokenizer | 能生成连贯文本 |
| Python 绑定 | kbase.py ctypes 封装 | Python 可调用 |
| 模型转换工具 | convert_hf_to_gguf.py | HuggingFace 模型可转换 |

## 8. 测试策略

### 8.1 算子精度测试

```
test_infra_ops.cpp:
  - test_matmul_f32:   随机矩阵 MatMul，对比参考实现
  - test_add_f32:      逐元素加
  - test_mul_f32:      逐元素乘
  - test_relu:         ReLU
  - test_gelu:         GELU
  - test_softmax:      Softmax
  - test_layernorm:    LayerNorm
  - test_reshape:      Reshape
  - test_transpose:    Transpose
```

### 8.2 模型加载测试

```
test_infra_model.cpp:
  - test_gguf_header:    GGUF head 解析正确性
  - test_gguf_metadata:  metadata KV 解析
  - test_gguf_tensors:   张量信息读取
  - test_gguf_load:      完整加载流程
```

### 8.3 Embedding 端到端测试

```
test_infra_embed.cpp:
  - test_embed_single:    单文本 Embedding
  - test_embed_batch:     批量 Embedding
  - test_embed_compare:   与 Python 版输出对比
  - test_embed_empty:     空输入处理
```

### 8.4 知识库集成测试

```
test_kbase_search.cpp:
  - test_index_build:     索引构建
  - test_search:          语义搜索
  - test_search_empty:    空搜索处理

test_kbase_rag.cpp:
  - test_rag_qa:          RAG 问答
```

## 9. 构建配置

### CMakeLists.txt 结构

```cmake
# engineering/CMakeLists.txt 新增
add_subdirectory(src/kbase)

# engineering/src/kbase/CMakeLists.txt
add_project_library(kbase)
target_link_libraries(kbase PUBLIC project_includes db_vector)

# engineering/apps/kbase/CMakeLists.txt
add_executable(kbase_cli main.c cmd_*.c)
target_link_libraries(kbase_cli kbase)

# engineering/test/kbase/CMakeLists.txt
add_project_test(kbase_test)
target_link_libraries(kbase_test kbase)
```

## 10. 模型管理

### 模型下载

```bash
# reference/models/download_model.sh
# 下载 MiniLM-L6 GGUF 格式
wget https://huggingface.co/.../minilm-l6-v2-gguf/.../ggml-model-q4_0.gguf

# 下载 BGE-small-zh GGUF 格式（中文优化）
wget https://huggingface.co/.../bge-small-zh-gguf/.../ggml-model-q4_0.gguf
```

### Git 忽略规则

```gitignore
# .gitignore
reference/models/*.gguf
reference/models/*.onnx
reference/models/*.bin
```

## 11. 与现有系统的集成

### 11.1 VDB 集成

知识库的语义搜索不直接依赖 VDB 的 C API，而是通过 `vector_index` 接口使用 HNSW 索引：

```c
#include "db/storage/vector/vector_index.h"

// 构建索引时
vector_index_t* idx = vector_index_create(hnsw, dim, metric_cosine);
vector_index_insert(idx, note_id, embedding, dim);

// 搜索时
vector_index_search(idx, query_embedding, top_k, results);
```

### 11.2 RAG 集成

知识库的问答模块使用 infra 引擎的文本生成能力（Phase 3），prompt 模板：

```
你是一个知识库助手，根据以下笔记内容回答问题。

笔记内容：
{context}

问题：{question}

请用中文回答，并注明参考来源。
```

## 12. 性能目标

| 场景 | Phase 1 目标 | Phase 2 目标 |
|------|-------------|-------------|
| 单文本 Embedding（标量） | < 100ms | - |
| 单文本 Embedding（SIMD） | - | < 30ms |
| 批量 100 条 Embedding | < 3s | < 1s |
| 1000 笔记索引构建 | < 30s | < 10s |
| 语义搜索（1000 笔记） | < 10ms | < 5ms |
| 模型加载时间 | < 2s | < 1s |
| 内存占用 | < 500MB | < 300MB |