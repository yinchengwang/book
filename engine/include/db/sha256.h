/**
 * @file sha256.h
 * @brief 纯 C SHA-256 流式计算接口。
 */
#ifndef DB_SHA256_H
#define DB_SHA256_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
/* C++ 调用时保持 C 链接约定。 */
extern "C" {
#endif

#define SHA256_DIGEST_SIZE 32
#define SHA256_BLOCK_SIZE 64

typedef struct sha256_ctx_s {
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t buffer[SHA256_BLOCK_SIZE];
    size_t buffer_len;
} sha256_ctx_t;

void sha256_init(sha256_ctx_t *ctx);
void sha256_update(sha256_ctx_t *ctx, const void *data, size_t len);
void sha256_final(sha256_ctx_t *ctx, uint8_t digest[SHA256_DIGEST_SIZE]);
void sha256_compute(const void *data, size_t len, uint8_t digest[SHA256_DIGEST_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* DB_SHA256_H */
