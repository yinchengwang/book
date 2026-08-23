// engineering/src/sdk/extra/embedding_openai.c
#include "sdk/mmdb_embedding.h"

/* OpenAI embedding adapter 占位实现
 *
 * 当前返回 MMDB_EMBED_OPENAI 错误码（-3）。
 * 实际 HTTP 调用计划：
 *   - 引入 libcurl 依赖（third_part/libcurl/）
 *   - POST https://api.openai.com/v1/embeddings
 *     Body: {"input": text, "model": "text-embedding-3-small"}
 *     Headers: Authorization: Bearer ${api_key}
 *   - 解析 JSON 响应（使用 third_part/cjson/）
 *
 * 触发此实现的接口：mmdb_embedding_create_openai(api_key, model)
 * 待 T2.x 添加。
 */

int mmdb_embed_text_openai_stub(
    const char* text, size_t text_len,
    float* out_vec, size_t out_dim) {
    (void)text; (void)text_len; (void)out_vec; (void)out_dim;
    return -3;  /* MMDB_ERR_NOT_IMPLEMENTED for OPENAI */
}
