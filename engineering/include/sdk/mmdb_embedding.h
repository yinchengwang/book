/* SDK embedding 抽象接口
 * 定义统一的文本→向量编码抽象，支持多种实现：
 *   - MMDB_EMBED_HASH        本地确定性 hash（零依赖，用于测试）
 *   - MMDB_EMBED_AVERAGE_POOL 平均池化占位
 *   - MMDB_EMBED_OPENAI       OpenAI HTTP 占位（T2.2 实现）
 */
#ifndef MMDB_EMBEDDING_H
#define MMDB_EMBEDDING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MMDB_EMBED_HASH = 0,          /* 本地确定性 hash（零依赖，测试用） */
    MMDB_EMBED_AVERAGE_POOL = 1,  /* 平均池化（占位，不实际编码） */
    MMDB_EMBED_OPENAI = 2         /* OpenAI HTTP（占位，T2.2 stub） */
} mmdb_embedding_kind_t;

typedef struct mmdb_embedding mmdb_embedding_t;

/* 创建 embedding 句柄
 * @param kind 编码器种类
 * @param dim  输出向量维度
 * @return 非 NULL 成功；NULL 表示参数无效或 OOM
 */
mmdb_embedding_t* mmdb_embedding_create(mmdb_embedding_kind_t kind, size_t dim);

/* 释放句柄 */
void mmdb_embedding_drop(mmdb_embedding_t* emb);

/* 文本编码为定长 float 向量
 * @param emb        embedding 句柄
 * @param text       输入文本（不需要 NUL 结尾）
 * @param text_len   输入文本长度（字节）
 * @param out_vec    输出向量缓冲区
 * @param out_dim    输出向量维度（必须等于 emb 创建时的 dim）
 * @return 0 成功；负数错误码
 *   -1: 参数无效 / dim 不匹配
 *   -2: AVERAGE_POOL 暂未实现
 *   -3: OPENAI 暂未实现
 */
int mmdb_embed_text(
    mmdb_embedding_t* emb,
    const char* text, size_t text_len,
    float* out_vec, size_t out_dim);

#ifdef __cplusplus
}
#endif

#endif  /* MMDB_EMBEDDING_H */
