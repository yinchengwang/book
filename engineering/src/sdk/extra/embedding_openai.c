// engineering/src/sdk/extra/embedding_openai.c
#include "sdk/mmdb_embedding.h"
#include "sdk/mmdb_error.h"

#include <stdio.h>
#include <stdlib.h>

/* OpenAI embedding adapter 占位实现
 *
 * 行为（P4-T4.1 升级）：
 *   1. 探测环境变量 MMDB_OPENAI_API_KEY
 *      - 未设置：stderr 给出警告，返回 MMDB_ERR_NOT_IMPLEMENTED (-11)
 *        （首次使用 mmdb_error.h 中的 MMDB_ERR_NOT_IMPLEMENTED 宏）
 *      - 已设置：当前仍返回 MMDB_ERR_NOT_IMPLEMENTED + stderr 提示
 *        "HTTP 集成留待后续 plan"，避免引入 libcurl 依赖。
 *
 * 后续实际 HTTP 调用计划：
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

    const char* api_key = getenv("MMDB_OPENAI_API_KEY");
    if (!api_key || api_key[0] == '\0') {
        fprintf(stderr,
                "[mmdb/embedding_openai] MMDB_OPENAI_API_KEY 未设置；"
                "OpenAI embedding 仍为占位实现，返回 MMDB_ERR_NOT_IMPLEMENTED。\n");
        return MMDB_ERR_NOT_IMPLEMENTED;
    }

    /* 检测到 API key，但仍为占位实现，避免引入 libcurl 依赖。
     * 真正 HTTP 集成留待后续 plan。 */
    fprintf(stderr,
            "[mmdb/embedding_openai] 检测到 MMDB_OPENAI_API_KEY；"
            "HTTP 集成尚未实现，返回 MMDB_ERR_NOT_IMPLEMENTED（待后续 plan）。\n");
    return MMDB_ERR_NOT_IMPLEMENTED;
}
