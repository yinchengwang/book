/* SDK embedding 接口的本地实现
 * 提供三种编码器的最小骨架：
 *   - HASH: 基于 FNV-1a + xorshift32 的确定性伪随机向量 + L2 归一化
 *   - AVERAGE_POOL / OPENAI: 占位返回错误码，后续 Task 补完
 */
#include "sdk/mmdb_embedding.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

/* embedding 句柄内部结构（不透明指针） */
struct mmdb_embedding {
    mmdb_embedding_kind_t kind;
    size_t dim;
};

/* FNV-1a 32-bit hash：稳定、零依赖、分布尚可 */
static uint32_t fnv1a(const uint8_t* data, size_t len) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        h ^= data[i];
        h *= 16777619u;
    }
    return h;
}

/* xorshift32 PRNG：状态 1 个 uint32，速度快 */
static uint32_t xorshift32(uint32_t* state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

mmdb_embedding_t* mmdb_embedding_create(mmdb_embedding_kind_t kind, size_t dim) {
    if (dim == 0) return NULL;
    mmdb_embedding_t* e = (mmdb_embedding_t*)calloc(1, sizeof(mmdb_embedding_t));
    if (!e) return NULL;
    e->kind = kind;
    e->dim = dim;
    return e;
}

void mmdb_embedding_drop(mmdb_embedding_t* emb) {
    free(emb);
}

int mmdb_embed_text(
    mmdb_embedding_t* emb,
    const char* text, size_t text_len,
    float* out_vec, size_t out_dim) {
    if (!emb || !text || !out_vec) return -1;
    if (out_dim != emb->dim) return -1;

    switch (emb->kind) {
        case MMDB_EMBED_HASH: {
            /* 用文本 hash 作为种子，生成 dim 个伪随机 float */
            uint32_t state = fnv1a((const uint8_t*)text, text_len);
            if (state == 0) state = 1;  /* xorshift 不能用 0 作种子 */

            float* vec = out_vec;
            for (size_t i = 0; i < emb->dim; i++) {
                uint32_t r = xorshift32(&state);
                /* 映射到 [-1, 1] */
                vec[i] = ((float)(r & 0xFFFFu) / 32768.0f) - 1.0f;
            }
            /* L2 归一化 */
            float norm = 0.0f;
            for (size_t i = 0; i < emb->dim; i++) norm += vec[i] * vec[i];
            norm = sqrtf(norm);
            if (norm > 0.0f) {
                for (size_t i = 0; i < emb->dim; i++) vec[i] /= norm;
            }
            return 0;
        }
        case MMDB_EMBED_AVERAGE_POOL:
            return -2;
        case MMDB_EMBED_OPENAI:
            return -3;
        default:
            return -1;
    }
}