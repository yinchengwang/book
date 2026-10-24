/**
 * @file rag_executor.h
 * @brief RAG 执行器接口定义
 *
 * 将 RAG 系统的分块和检索模块深度集成到 DB 执行管线，
 * 让"RAG 查询"成为 DB 的一等公民操作。
 */

#ifndef DB_RAG_EXECUTOR_H
#define DB_RAG_EXECUTOR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** RAG 执行器默认配置值 */
#define RAG_DEFAULT_CHUNK_SIZE     512
#define RAG_DEFAULT_CHUNK_OVERLAP  64
#define RAG_DEFAULT_MIN_CHUNK_SIZE 32
#define RAG_DEFAULT_TOP_K          10
#define RAG_DEFAULT_RECALL_K       50
#define RAG_DEFAULT_RRF_K          60
#define RAG_DEFAULT_HYBRID_WEIGHT  0.5f

/** 分块策略类型 */
typedef enum rag_chunk_strategy {
    RAG_CHUNK_FIXED = 0,       /**< 固定大小分块 */
    RAG_CHUNK_RECURSIVE = 1,   /**< 递归字符分块 */
    RAG_CHUNK_CODE = 2,        /**< 代码感知分块 */
    RAG_CHUNK_SEMANTIC = 3     /**< 语义分块（预留） */
} rag_chunk_strategy_t;

/** 检索策略类型 */
typedef enum rag_retrieve_strategy {
    RAG_RETRIEVE_HNSW = 0,     /**< 向量检索 */
    RAG_RETRIEVE_BM25 = 1,     /**< 全文检索 */
    RAG_RETRIEVE_HYBRID = 2    /**< 混合检索 */
} rag_retrieve_strategy_t;

/** 融合策略类型 */
typedef enum rag_fusion_strategy {
    RAG_FUSION_RRF = 0,        /**< Reciprocal Rank Fusion */
    RAG_FUSION_WEIGHTED = 1    /**< 加权融合 */
} rag_fusion_strategy_t;

/* ============================================================
 * RAG 执行上下文
 * ============================================================ */

/**
 * @brief RAG 配置结构
 */
typedef struct rag_config {
    /** 分块配置 */
    rag_chunk_strategy_t chunk_strategy;  /**< 分块策略 */
    int                  chunk_size;      /**< 块大小（字符数） */
    int                  chunk_overlap;   /**< 块重叠大小 */
    int                  min_chunk_size;  /**< 最小块大小 */

    /** 检索配置 */
    rag_retrieve_strategy_t retrieve_strategy;  /**< 检索策略 */
    int                    top_k;               /**< 返回结果数 */
    int                    recall_k;            /**< 召回数量 */
    float                  hybrid_weight;       /**< 混合权重（0-1） */
    float                  rrf_k;               /**< RRF 参数 k */

    /** 增强功能配置 */
    int    enable_rerank;     /**< 是否启用重排序 */
    int    enable_mmr;        /**< 是否启用 MMR 多样性 */
    float  mmr_lambda;        /**< MMR lambda 参数 */

    /** 代码分块专用 */
    char   language[32];      /**< 代码语言（如 "cpp", "python"） */

    /** 融合策略配置 */
    rag_fusion_strategy_t fusion_strategy;  /**< 融合策略 */

    /** 预留扩展字段 */
    void  *extra;
} rag_config_t;

/**
 * @brief RAG 状态结构
 */
typedef struct rag_state {
    int    in_transaction;    /**< 是否在事务中 */
    uint32_t txn_id;          /**< 事务 ID */
    int    queries_executed;  /**< 已执行查询数 */
    char  *error_msg;         /**< 错误消息 */
} rag_state_t;

/**
 * @brief RAG 执行上下文
 */
typedef struct rag_context {
    rag_config_t config;      /**< 配置 */
    rag_state_t  state;       /**< 状态 */
    void        *catalog;     /**< Catalog 指针（预留） */
    void        *buffer_pool; /**< Buffer Pool 指针（预留） */
    void        *wal;         /**< WAL 指针（预留） */
    void        *vector_engine;   /**< 向量引擎指针 */
    void        *doc_engine;      /**< 文档引擎指针（BM25） */
} rag_context_t;

/* ============================================================
 * RAG 算子基类
 * ============================================================ */

/**
 * @brief RAG 算子类型
 */
typedef enum rag_operator_type {
    RAG_OP_CHUNK = 0,     /**< 分块算子 */
    RAG_OP_RETRIEVE = 1,  /**< 检索算子 */
    RAG_OP_EMBED = 2,     /**< Embedding 算子 */
    RAG_OP_GENERATE = 3,  /**< 生成算子 */
    RAG_OP_PIPELINE = 4   /**< 完整管线算子 */
} rag_operator_type_t;

/* 前向声明 */
typedef struct rag_operator rag_operator_t;

/**
 * @brief RAG 算子操作函数指针
 */
typedef int (*rag_op_init_fn)(rag_operator_t *op, rag_context_t *ctx);
typedef int (*rag_op_execute_fn)(rag_operator_t *op, void *input, void **output);
typedef int (*rag_op_cleanup_fn)(rag_operator_t *op);

/**
 * @brief RAG 算子基类
 */
typedef struct rag_operator {
    rag_operator_type_t     type;       /**< 算子类型 */
    char                    name[64];   /**< 算子名称 */

    rag_op_init_fn          init;       /**< 初始化函数 */
    rag_op_execute_fn       execute;    /**< 执行函数 */
    rag_op_cleanup_fn       cleanup;    /**< 清理函数 */

    rag_context_t          *context;    /**< 执行上下文 */
    void                   *state;      /**< 算子内部状态 */
} rag_operator_t;

/* ============================================================
 * 分块算子输入输出
 * ============================================================ */

/**
 * @brief 分块输入
 */
typedef struct chunk_input {
    const char *text;         /**< 待分块文本 */
    const char *document_id;  /**< 文档 ID */
    int         text_len;     /**< 文本长度（0 表示 null 终止） */

    /** 元数据（可选） */
    const char *metadata;     /**< JSON 格式元数据 */
    void       *extra;        /**< 扩展数据 */
} chunk_input_t;

/**
 * @brief 单个 Chunk
 */
typedef struct rag_chunk {
    char   id[64];            /**< Chunk ID（UUID） */
    char   document_id[64];   /**< 文档 ID */
    char  *content;           /**< Chunk 内容 */
    int    content_len;       /**< 内容长度 */
    int    chunk_index;       /**< 块索引 */
    int    start_char;        /**< 起始字符位置 */
    int    end_char;          /**< 结束字符位置 */
    void  *metadata;          /**< 元数据 */
} rag_chunk_t;

/**
 * @brief 分块输出
 */
typedef struct chunk_output {
    rag_chunk_t *chunks;      /**< 分块数组 */
    int          nchunks;     /**< 块数量 */
    int          nchunks_capacity;  /**< 数组容量 */

    /** 统计信息 */
    int64_t      processing_time_ms;  /**< 处理时间（毫秒） */
    int          total_tokens;        /**< 总 token 数（估算） */
} chunk_output_t;

/* ============================================================
 * 检索算子输入输出
 * ============================================================ */

/**
 * @brief 检索输入
 */
typedef struct retrieve_input {
    const char *query;        /**< 查询文本 */
    int         query_len;    /**< 查询长度（0 表示 null 终止） */
    int         top_k;        /**< 返回结果数（0 表示使用默认值） */

    /** 检索选项 */
    int         enable_rerank;    /**< 是否启用重排序 */
    int         enable_mmr;       /**< 是否启用 MMR */
    float       mmr_lambda;       /**< MMR lambda */

    /** 预留扩展字段 */
    void       *extra;
} retrieve_input_t;

/**
 * @brief 单个检索结果
 */
typedef struct retrieve_result {
    rag_chunk_t chunk;        /**< 相关 Chunk */
    float       score;        /**< 综合分数 */
    float       vector_score; /**< 向量相似度分数 */
    float       bm25_score;   /**< BM25 分数 */
    int         rank;         /**< 排名（从 1 开始） */
    char        source[16];   /**< 来源："hnsw", "bm25", "hybrid" */
} retrieve_result_t;

/**
 * @brief 检索输出
 */
typedef struct retrieve_output {
    retrieve_result_t *results;   /**< 结果数组 */
    int                nresults;  /**< 结果数量 */
    int                nresults_capacity;  /**< 数组容量 */

    /** 统计信息 */
    int64_t            processing_time_ms;
    int                hnsw_results_count;
    int                bm25_results_count;
} retrieve_output_t;

/* ============================================================
 * RAG 管线
 * ============================================================ */

/**
 * @brief RAG 管线状态
 */
typedef enum rag_pipeline_state {
    RAG_PIPELINE_IDLE = 0,
    RAG_PIPELINE_RUNNING = 1,
    RAG_PIPELINE_DONE = 2,
    RAG_PIPELINE_ERROR = 3
} rag_pipeline_state_t;

/**
 * @brief RAG 管线
 */
typedef struct rag_pipeline {
    rag_context_t          *context;     /**< 执行上下文 */
    rag_operator_t         *chunk_op;    /**< 分块算子 */
    rag_operator_t         *retrieve_op; /**< 检索算子 */
    rag_pipeline_state_t    state;       /**< 管线状态 */
    int                     step;        /**< 当前步骤 */
} rag_pipeline_t;

/* ============================================================
 * RAG 执行器 API
 * ============================================================ */

/**
 * @brief 创建 RAG 执行上下文
 * @return 上下文指针，失败返回 NULL
 */
rag_context_t *rag_context_create(void);

/**
 * @brief 销毁 RAG 执行上下文
 * @param ctx 上下文指针
 */
void rag_context_destroy(rag_context_t *ctx);

/**
 * @brief 初始化 RAG 执行上下文
 * @param ctx 上下文指针
 * @param config 配置（NULL 使用默认配置）
 * @return 0 成功，-1 失败
 */
int rag_context_init(rag_context_t *ctx, const rag_config_t *config);

/**
 * @brief 设置默认配置
 * @param config 配置结构指针
 */
void rag_config_init_defaults(rag_config_t *config);

/**
 * @brief 创建分块算子
 * @param strategy 分块策略
 * @param config 配置
 * @return 算子指针，失败返回 NULL
 */
rag_operator_t *rag_chunk_op_create(rag_chunk_strategy_t strategy,
                                     const rag_config_t *config);

/**
 * @brief 创建检索算子
 * @param strategy 检索策略
 * @param config 配置
 * @param ctx 执行上下文
 * @return 算子指针，失败返回 NULL
 */
rag_operator_t *rag_retrieve_op_create(rag_retrieve_strategy_t strategy,
                                        const rag_config_t *config,
                                        rag_context_t *ctx);

/**
 * @brief 销毁 RAG 算子
 * @param op 算子指针
 */
void rag_operator_destroy(rag_operator_t *op);

/**
 * @brief 初始化 RAG 算子
 * @param op 算子指针
 * @param ctx 执行上下文
 * @return 0 成功，-1 失败
 */
int rag_operator_init(rag_operator_t *op, rag_context_t *ctx);

/**
 * @brief 执行 RAG 算子
 * @param op 算子指针
 * @param input 输入数据
 * @param output 输出数据（需要调用方释放）
 * @return 0 成功，-1 失败
 */
int rag_operator_execute(rag_operator_t *op, void *input, void **output);

/**
 * @brief 清理 RAG 算子
 * @param op 算子指针
 * @return 0 成功，-1 失败
 */
int rag_operator_cleanup(rag_operator_t *op);

/**
 * @brief 创建 RAG 管线
 * @param ctx 执行上下文
 * @param chunk_config 分块配置
 * @param retrieve_config 检索配置
 * @return 管线指针，失败返回 NULL
 */
rag_pipeline_t *rag_pipeline_create(rag_context_t *ctx,
                                     const rag_config_t *chunk_config,
                                     const rag_config_t *retrieve_config);

/**
 * @brief 销毁 RAG 管线
 * @param pipeline 管线指针
 */
void rag_pipeline_destroy(rag_pipeline_t *pipeline);

/**
 * @brief 执行 RAG 查询（端到端）
 * @param pipeline 管线指针
 * @param query 查询文本
 * @param query_len 查询长度（0 表示 null 终止）
 * @param results 输出结果（需要调用方释放）
 * @return 结果数量，-1 失败
 */
int rag_pipeline_execute(rag_pipeline_t *pipeline,
                          const char *query,
                          int query_len,
                          retrieve_output_t **results);

/**
 * @brief 释放 Chunk 输出
 * @param output 输出指针
 */
void chunk_output_free(chunk_output_t *output);

/**
 * @brief 释放检索输出
 * @param output 输出指针
 */
void retrieve_output_free(retrieve_output_t *output);

/**
 * @brief 获取错误消息
 * @param ctx 执行上下文
 * @return 错误消息（不需要释放）
 */
const char *rag_context_errmsg(const rag_context_t *ctx);

/* ============================================================
 * 便捷函数
 * ============================================================ */

/**
 * @brief 执行分块（便捷函数）
 * @param ctx 执行上下文
 * @param text 待分块文本
 * @param document_id 文档 ID
 * @return 分块输出（需要调用方释放），失败返回 NULL
 */
chunk_output_t *rag_chunk(rag_context_t *ctx,
                          const char *text,
                          const char *document_id);

/**
 * @brief 执行检索（便捷函数）
 * @param ctx 执行上下文
 * @param query 查询文本
 * @param top_k 返回结果数
 * @return 检索输出（需要调用方释放），失败返回 NULL
 */
retrieve_output_t *rag_retrieve(rag_context_t *ctx,
                                 const char *query,
                                 int top_k);

/**
 * @brief 执行端到端 RAG 查询
 * @param ctx 执行上下文
 * @param query 查询文本
 * @param top_k 返回结果数
 * @return 检索输出（需要调用方释放），失败返回 NULL
 */
retrieve_output_t *rag_query(rag_context_t *ctx,
                              const char *query,
                              int top_k);

/* ============================================================
 * Embedding 策略类型（Wave 3-⑨）
 * ============================================================ */

/**
 * @brief Embedding 策略
 */
typedef enum rag_embed_strategy {
    RAG_EMBED_SIMPLE = 0,     /**< 确定性种子向量（用于测试/降级） */
    RAG_EMBED_OLLAMA = 1,     /**< Ollama API 调用 */
    RAG_EMBED_OPENAI = 2,     /**< OpenAI API（预留） */
    RAG_EMBED_COHERE = 3      /**< Cohere API（预留） */
} rag_embed_strategy_t;

/**
 * @brief LLM 策略
 */
typedef enum rag_llm_strategy {
    RAG_LLM_SIMPLE = 0,       /**< 关键词模拟（用于测试/降级） */
    RAG_LLM_OLLAMA = 1,       /**< Ollama API 调用 */
    RAG_LLM_OPENAI = 2,       /**< OpenAI API（预留） */
    RAG_LLM_ANTHROPIC = 3     /**< Anthropic API（预留） */
} rag_llm_strategy_t;

/* ============================================================
 * Embedding 算子（Wave 3-⑨）
 * ============================================================ */

/**
 * @brief Embedding 配置
 */
typedef struct rag_embed_config {
    rag_embed_strategy_t strategy;     /**< Embedding 策略 */
    char                 model[64];    /**< 模型名称，如 "nomic-embed-text" */
    char                 base_url[256]; /**< API 地址，如 "http://localhost:11434" */
    int                  dimension;    /**< 向量维度，默认 768 */
    int                  cache_size;   /**< 缓存大小，默认 1000 */
    int                  timeout_sec;  /**< 超时秒数，默认 30 */
    int                  max_retries;  /**< 最大重试次数，默认 3 */
} rag_embed_config_t;

/**
 * @brief Embedding 输入
 */
typedef struct rag_embed_input {
    const char *text;         /**< 待编码文本 */
    int         text_len;     /**< 文本长度（0 表示 null 终止） */
    int         use_cache;    /**< 是否使用缓存 */
} rag_embed_input_t;

/**
 * @brief Embedding 输出
 */
typedef struct rag_embed_output {
    float   *vector;          /**< 向量数据（需要调用方释放） */
    int      dimension;       /**< 向量维度 */
    int64_t  processing_time_ms; /**< 处理时间 */
    int      cache_hit;       /**< 是否命中缓存 */
} rag_embed_output_t;

/**
 * @brief Embedding 缓存统计
 */
typedef struct rag_embed_cache_stats {
    int  size;                /**< 缓存条目数 */
    int  hits;                /**< 命中次数 */
    int  misses;              /**< 未命中次数 */
    double hit_rate;          /**< 命中率 */
} rag_embed_cache_stats_t;

/**
 * @brief 创建 Embedding 算子
 * @param strategy Embedding 策略
 * @param config 配置
 * @return 算子指针，失败返回 NULL
 */
rag_operator_t *rag_embed_op_create(rag_embed_strategy_t strategy,
                                     const rag_embed_config_t *config);

/**
 * @brief 初始化 Embedding 算子
 * @param op 算子指针
 * @param ctx 执行上下文
 * @return 0 成功，-1 失败
 */
int rag_embed_op_init(rag_operator_t *op, rag_context_t *ctx);

/**
 * @brief 执行 Embedding
 * @param op 算子指针
 * @param text 输入文本
 * @param use_cache 是否使用缓存
 * @return Embedding 输出（需要调用方释放），失败返回 NULL
 */
rag_embed_output_t *rag_embed_execute(rag_operator_t *op,
                                       const char *text,
                                       int use_cache);

/**
 * @brief 批量执行 Embedding
 * @param op 算子指针
 * @param texts 文本数组
 * @param ntexts 文本数量
 * @param vectors 输出向量数组（调用方分配，dimension × ntexts）
 * @return 0 成功，-1 失败
 */
int rag_embed_execute_batch(rag_operator_t *op,
                             const char **texts,
                             int ntexts,
                             float *vectors);

/**
 * @brief 获取 Embedding 向量维度
 * @param op 算子指针
 * @return 向量维度
 */
int rag_embed_dimension(const rag_operator_t *op);

/**
 * @brief 检查 Embedding 服务是否就绪
 * @param op 算子指针
 * @return true 就绪，false 未就绪
 */
int rag_embed_is_ready(const rag_operator_t *op);

/**
 * @brief 获取 Embedding 缓存统计
 * @param op 算子指针
 * @param stats 输出统计
 */
void rag_embed_cache_stats(const rag_operator_t *op,
                           rag_embed_cache_stats_t *stats);

/**
 * @brief 清除 Embedding 缓存
 * @param op 算子指针
 */
void rag_embed_cache_clear(rag_operator_t *op);

/**
 * @brief 释放 Embedding 输出
 * @param output 输出指针
 */
void rag_embed_output_free(rag_embed_output_t *output);

/* ============================================================
 * LLM 生成算子（Wave 3-⑨）
 * ============================================================ */

/**
 * @brief LLM 生成配置
 */
typedef struct rag_llm_config {
    rag_llm_strategy_t strategy;       /**< LLM 策略 */
    char               model[64];      /**< 模型名称，如 "llama3.2" */
    char               base_url[256];  /**< API 地址 */
    int                max_tokens;     /**< 最大生成长度，默认 1024 */
    float              temperature;    /**< 温度参数，默认 0.7 */
    float              top_p;          /**< Top-p 采样，默认 0.9 */
    int                top_k;          /**< Top-k 采样，默认 40 */
    float              repeat_penalty; /**< 重复惩罚，默认 1.1 */
    int                timeout_sec;    /**< 超时秒数，默认 120 */
    int                max_retries;    /**< 最大重试次数，默认 3 */
} rag_llm_config_t;

/**
 * @brief LLM 生成输入
 */
typedef struct rag_generate_input {
    const char *prompt;        /**< 提示词 */
    int         prompt_len;    /**< 提示词长度（0 表示 null 终止） */
    int         max_tokens;    /**< 最大 token 数（0 使用默认值） */
    float       temperature;   /**< 温度参数（0 使用默认值） */
} rag_generate_input_t;

/**
 * @brief LLM 生成输出
 */
typedef struct rag_generate_output {
    char   *text;              /**< 生成的文本（需要调用方释放） */
    int     text_len;          /**< 文本长度 */
    int     tokens_generated;  /**< 生成的 token 数 */
    int64_t processing_time_ms; /**< 处理时间 */
    int     finished;          /**< 是否完成 */
    char    finish_reason[32]; /**< 完成原因：stop/eos/error */
} rag_generate_output_t;

/**
 * @brief LLM 流式生成回调
 * @param token 新生成的 token
 * @param complete 是否完成
 * @param user_data 用户数据
 */
typedef void (*rag_stream_callback_t)(const char *token,
                                      int complete,
                                      void *user_data);

/**
 * @brief 创建 LLM 生成算子
 * @param strategy LLM 策略
 * @param config 配置
 * @return 算子指针，失败返回 NULL
 */
rag_operator_t *rag_generate_op_create(rag_llm_strategy_t strategy,
                                        const rag_llm_config_t *config);

/**
 * @brief 初始化 LLM 生成算子
 * @param op 算子指针
 * @param ctx 执行上下文
 * @return 0 成功，-1 失败
 */
int rag_generate_op_init(rag_operator_t *op, rag_context_t *ctx);

/**
 * @brief 执行 LLM 生成
 * @param op 算子指针
 * @param prompt 提示词
 * @return 生成输出（需要调用方释放），失败返回 NULL
 */
rag_generate_output_t *rag_generate_execute(rag_operator_t *op,
                                             const char *prompt);

/**
 * @brief 执行 LLM 流式生成
 * @param op 算子指针
 * @param prompt 提示词
 * @param callback 回调函数
 * @param user_data 用户数据
 * @return 0 成功，-1 失败
 */
int rag_generate_execute_stream(rag_operator_t *op,
                                 const char *prompt,
                                 rag_stream_callback_t callback,
                                 void *user_data);

/**
 * @brief 检查 LLM 服务是否就绪
 * @param op 算子指针
 * @return true 就绪，false 未就绪
 */
int rag_generate_is_ready(const rag_operator_t *op);

/**
 * @brief 释放 LLM 生成输出
 * @param output 输出指针
 */
void rag_generate_output_free(rag_generate_output_t *output);

/* ============================================================
 * Prompt 模板管理（Wave 3-⑨）
 * ============================================================ */

/**
 * @brief Prompt 模板配置
 */
typedef struct rag_prompt_template {
    char system_prompt[1024];   /**< 系统提示词 */
    char user_template[2048];   /**< 用户模板，支持 {context} 占位符 */
    char context_separator[128]; /**< 上下文分隔符 */
} rag_prompt_template_t;

/**
 * @brief 构建带上下文的 Prompt
 * @param tmpl 模板配置
 * @param query 用户查询
 * @param chunks 上下文块
 * @param nchunks 块数量
 * @param output 输出缓冲区
 * @param output_size 输出缓冲区大小
 * @return 0 成功，-1 失败
 */
int rag_build_prompt(const rag_prompt_template_t *tmpl,
                     const char *query,
                     const rag_chunk_t *chunks,
                     int nchunks,
                     char *output,
                     int output_size);

/**
 * @brief 设置默认 Prompt 模板
 * @param tmpl 模板配置
 */
void rag_prompt_template_defaults(rag_prompt_template_t *tmpl);

/* ============================================================
 * 完整 RAG 管线（Wave 3-⑨）
 * ============================================================ */

/**
 * @brief 完整 RAG 管线配置
 */
typedef struct rag_pipeline_config {
    /** Embedding 配置 */
    rag_embed_config_t embed_config;

    /** LLM 配置 */
    rag_llm_config_t llm_config;

    /** 检索配置（复用 Wave 3-⑧） */
    rag_retrieve_strategy_t retrieve_strategy;
    int                     top_k;
    float                   hybrid_weight;
    float                   rrf_k;

    /** Prompt 模板 */
    rag_prompt_template_t prompt_template;

    /** 管线选项 */
    int enable_streaming;       /**< 启用流式输出 */
    int enable_cache;           /**< 启用 Embedding 缓存 */
} rag_pipeline_config_t;

/**
 * @brief 完整 RAG 管线
 */
typedef struct rag_full_pipeline {
    rag_context_t          *context;       /**< 执行上下文 */
    rag_operator_t         *embed_op;      /**< Embedding 算子 */
    rag_operator_t         *generate_op;   /**< 生成算子 */
    rag_operator_t         *chunk_op;      /**< 分块算子 */
    rag_operator_t         *retrieve_op;   /**< 检索算子 */
    rag_pipeline_state_t    state;         /**< 管线状态 */
    rag_pipeline_config_t   config;        /**< 配置 */

    /** 内部实现 */
    void                   *impl;          /**< 内部实现数据 */

    /** 统计信息 */
    int64_t total_queries;
    int64_t total_time_ms;
    int     cache_hits;
    int     cache_misses;
} rag_full_pipeline_t;

/**
 * @brief 创建完整 RAG 管线
 * @param ctx 执行上下文
 * @param config 管线配置
 * @return 管线指针，失败返回 NULL
 */
rag_full_pipeline_t *rag_full_pipeline_create(rag_context_t *ctx,
                                               const rag_pipeline_config_t *config);

/**
 * @brief 销毁完整 RAG 管线
 * @param pipeline 管线指针
 */
void rag_full_pipeline_destroy(rag_full_pipeline_t *pipeline);

/**
 * @brief 执行完整 RAG 查询
 * @param pipeline 管线指针
 * @param query 用户查询
 * @param top_k 检索 Top-K
 * @return 生成输出（需要调用方释放），失败返回 NULL
 */
rag_generate_output_t *rag_full_pipeline_execute(rag_full_pipeline_t *pipeline,
                                                  const char *query,
                                                  int top_k);

/**
 * @brief 执行 RAG 查询（仅检索，不生成）
 * @param pipeline 管线指针
 * @param query 用户查询
 * @param top_k 检索 Top-K
 * @return 检索输出（需要调用方释放），失败返回 NULL
 */
retrieve_output_t *rag_full_pipeline_retrieve(rag_full_pipeline_t *pipeline,
                                               const char *query,
                                               int top_k);

/**
 * @brief 获取管线统计信息
 * @param pipeline 管线指针
 * @param total_queries 输出总查询数
 * @param total_time_ms 输出总时间
 * @param cache_hits 输出缓存命中数
 * @param cache_misses 输出缓存未命中数
 */
void rag_full_pipeline_stats(const rag_full_pipeline_t *pipeline,
                             int64_t *total_queries,
                             int64_t *total_time_ms,
                             int *cache_hits,
                             int *cache_misses);

/**
 * @brief 设置 Prompt 模板
 * @param pipeline 管线指针
 * @param system_prompt 系统提示词（NULL 保持不变）
 * @param user_template 用户模板（NULL 保持不变）
 * @return 0 成功，-1 失败
 */
int rag_full_pipeline_set_prompt(rag_full_pipeline_t *pipeline,
                                 const char *system_prompt,
                                 const char *user_template);

/* ============================================================
 * 事务集成（Wave 3-⑨）
 * ============================================================ */

/**
 * @brief RAG 事务
 *
 * 用于在 DB 事务内执行 RAG 操作
 */
typedef struct rag_transaction {
    void *db_txn;               /**< 底层数据库事务 */

    /**
     * @brief 提交事务
     */
    int (*commit)(struct rag_transaction *txn);

    /**
     * @brief 回滚事务
     */
    int (*rollback)(struct rag_transaction *txn);

    /**
     * @brief 清除事务内的 Embedding 缓存
     */
    void (*clear_cache)(struct rag_transaction *txn);

    /** 内部实现 */
    void *impl;                 /**< 内部实现数据 */
} rag_transaction_t;

/**
 * @brief 创建 RAG 事务
 * @param db_txn 底层数据库事务指针
 * @return 事务指针，失败返回 NULL
 */
rag_transaction_t *rag_transaction_create(void *db_txn);

/**
 * @brief 销毁 RAG 事务
 * @param txn 事务指针
 */
void rag_transaction_destroy(rag_transaction_t *txn);

#ifdef __cplusplus
}
#endif

#endif /* DB_RAG_EXECUTOR_H */
