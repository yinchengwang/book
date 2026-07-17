#ifndef RAG_MULTIMODAL_CHUNK_H
#define RAG_MULTIMODAL_CHUNK_H

/**
 * @file multimodal_chunk.h
 * @brief VDB C SDK 多模态 chunk 接口
 *
 * 提供多模态 RAG 系统的 C 层 API：文本/表格/图像/代码 chunk 的
 * 插入、删除、检索接口，以及多模态检索结果结构体。
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 类型定义 ==================== */

/** Chunk 类型枚举 */
typedef enum {
    CHUNK_TYPE_TEXT = 0,
    CHUNK_TYPE_TABLE = 1,
    CHUNK_TYPE_IMAGE = 2,
    CHUNK_TYPE_CODE = 3,
} chunk_type_t;

/** 文本块 */
typedef struct {
    uint64_t chunk_id;
    uint64_t doc_id;
    uint64_t tenant_id;
    char    *content;
    char    *metadata;      /* JSON 格式 */
    float   *embedding;
    int      embedding_dim;
} text_chunk_t;

/** 表格块 */
typedef struct {
    uint64_t chunk_id;
    uint64_t doc_id;
    uint64_t tenant_id;
    char    *title;
    char    *headers;       /* JSON 数组 */
    char    *rows;          /* JSON 数组 */
    char    *caption;
    char    *metadata;      /* JSON 格式 */
    float   *embedding;
    int      embedding_dim;
} table_chunk_t;

/** 图像块 */
typedef struct {
    uint64_t chunk_id;
    uint64_t doc_id;
    uint64_t tenant_id;
    char    *image_path;
    char    *caption;
    char    *ocr_text;
    char    *metadata;      /* JSON 格式 */
    float   *siglip_embedding;
    int      siglip_dim;
    float   *text_embedding;  /* 可选的文本向量 */
    int      text_dim;
} image_chunk_t;

/** 代码块 */
typedef struct {
    uint64_t chunk_id;
    uint64_t doc_id;
    uint64_t tenant_id;
    char    *language;
    char    *code;
    char    *name;          /* 函数名/类名 */
    char    *docstring;
    char    *metadata;      /* JSON 格式 */
    float   *embedding;
    int      embedding_dim;
} code_chunk_t;

/** 多模态检索结果 */
typedef struct {
    uint64_t     chunk_id;
    chunk_type_t type;
    char        *content;      /* 原始内容 */
    char        *metadata;     /* JSON 元数据 */
    float        score;
    char        *highlight;    /* 高亮片段（可选） */
} multimodal_result_t;

/* ==================== API 函数声明 ==================== */

/**
 * 插入文本块
 * @param chunk  文本块指针
 * @return 0 成功，非 0 失败
 */
int multimodal_insert_text_chunk(text_chunk_t *chunk);

/**
 * 插入表格块
 * @param chunk  表格块指针
 * @return 0 成功，非 0 失败
 */
int multimodal_insert_table_chunk(table_chunk_t *chunk);

/**
 * 插入图像块
 * @param chunk  图像块指针
 * @return 0 成功，非 0 失败
 */
int multimodal_insert_image_chunk(image_chunk_t *chunk);

/**
 * 插入代码块
 * @param chunk  代码块指针
 * @return 0 成功，非 0 失败
 */
int multimodal_insert_code_chunk(code_chunk_t *chunk);

/**
 * 多模态检索
 * @param tenant_id       租户 ID
 * @param query_text      查询文本
 * @param query_embedding 查询向量
 * @param embedding_dim   向量维度
 * @param target_types    要检索的类型数组，NULL 表示全部
 * @param num_types       类型数组长度
 * @param top_k           返回 top-K 结果
 * @param results         输出结果数组指针
 * @param num_results     输出结果数量
 * @return 0 成功，非 0 失败
 */
int multimodal_search(
    uint64_t          tenant_id,
    const char       *query_text,
    const float      *query_embedding,
    int               embedding_dim,
    chunk_type_t     *target_types,
    int               num_types,
    int               top_k,
    multimodal_result_t **results,
    int              *num_results
);

/**
 * 删除文档及其所有 chunk
 * @param doc_id 文档 ID
 * @return 0 成功，非 0 失败
 */
int multimodal_delete_by_doc_id(uint64_t doc_id);

/**
 * 更新 chunk
 * @param chunk_id  chunk ID
 * @param type      chunk 类型
 * @param chunk     新 chunk 数据指针
 * @return 0 成功，非 0 失败
 */
int multimodal_update_chunk(uint64_t chunk_id, chunk_type_t type, void *chunk);

#ifdef __cplusplus
}
#endif

#endif /* RAG_MULTIMODAL_CHUNK_H */