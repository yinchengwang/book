#include "db/sha256.h"

#include <string.h>

static const uint32_t sha256_round_constants[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};

static uint32_t rotate_right(uint32_t value, unsigned int amount) {
    return (value >> amount) | (value << (32U - amount));
}

static uint32_t load_be32(const uint8_t *bytes) {
    return ((uint32_t)bytes[0] << 24U) | ((uint32_t)bytes[1] << 16U) | ((uint32_t)bytes[2] << 8U) |
           (uint32_t)bytes[3];
}

static void store_be32(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)(value >> 24U);
    bytes[1] = (uint8_t)(value >> 16U);
    bytes[2] = (uint8_t)(value >> 8U);
    bytes[3] = (uint8_t)value;
}

static void sha256_transform(sha256_ctx_t *ctx, const uint8_t block[SHA256_BLOCK_SIZE]) {
    uint32_t words[64];
    uint32_t a = ctx->state[0];
    uint32_t b = ctx->state[1];
    uint32_t c = ctx->state[2];
    uint32_t d = ctx->state[3];
    uint32_t e = ctx->state[4];
    uint32_t f = ctx->state[5];
    uint32_t g = ctx->state[6];
    uint32_t h = ctx->state[7];

    for (size_t i = 0; i < 16; ++i) {
        words[i] = load_be32(block + i * 4U);
    }
    for (size_t i = 16; i < 64; ++i) {
        uint32_t s0 = rotate_right(words[i - 15], 7U) ^ rotate_right(words[i - 15], 18U) ^ (words[i - 15] >> 3U);
        uint32_t s1 = rotate_right(words[i - 2], 17U) ^ rotate_right(words[i - 2], 19U) ^ (words[i - 2] >> 10U);
        words[i] = words[i - 16] + s0 + words[i - 7] + s1;
    }
    for (size_t i = 0; i < 64; ++i) {
        uint32_t s1 = rotate_right(e, 6U) ^ rotate_right(e, 11U) ^ rotate_right(e, 25U);
        uint32_t choice = (e & f) ^ (~e & g);
        uint32_t temp1 = h + s1 + choice + sha256_round_constants[i] + words[i];
        uint32_t s0 = rotate_right(a, 2U) ^ rotate_right(a, 13U) ^ rotate_right(a, 22U);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

void sha256_init(sha256_ctx_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    ctx->state[0] = 0x6a09e667U;
    ctx->state[1] = 0xbb67ae85U;
    ctx->state[2] = 0x3c6ef372U;
    ctx->state[3] = 0xa54ff53aU;
    ctx->state[4] = 0x510e527fU;
    ctx->state[5] = 0x9b05688cU;
    ctx->state[6] = 0x1f83d9abU;
    ctx->state[7] = 0x5be0cd19U;
    ctx->bit_count = 0;
    ctx->buffer_len = 0;
}

void sha256_update(sha256_ctx_t *ctx, const void *data, size_t len) {
    const uint8_t *input = (const uint8_t *)data;
    if (ctx == NULL || (data == NULL && len != 0U)) {
        return;
    }
    ctx->bit_count += (uint64_t)len * 8U;
    while (len != 0U) {
        size_t available = SHA256_BLOCK_SIZE - ctx->buffer_len;
        size_t copied = len < available ? len : available;
        memcpy(ctx->buffer + ctx->buffer_len, input, copied);
        ctx->buffer_len += copied;
        input += copied;
        len -= copied;
        if (ctx->buffer_len == SHA256_BLOCK_SIZE) {
            sha256_transform(ctx, ctx->buffer);
            ctx->buffer_len = 0;
        }
    }
}

void sha256_final(sha256_ctx_t *ctx, uint8_t digest[SHA256_DIGEST_SIZE]) {
    uint64_t bit_count;
    if (ctx == NULL || digest == NULL) {
        return;
    }
    bit_count = ctx->bit_count;
    ctx->buffer[ctx->buffer_len++] = 0x80U;
    if (ctx->buffer_len > 56U) {
        memset(ctx->buffer + ctx->buffer_len, 0, SHA256_BLOCK_SIZE - ctx->buffer_len);
        sha256_transform(ctx, ctx->buffer);
        ctx->buffer_len = 0;
    }
    memset(ctx->buffer + ctx->buffer_len, 0, 56U - ctx->buffer_len);
    for (unsigned int i = 0; i < 8U; ++i) {
        ctx->buffer[56U + i] = (uint8_t)(bit_count >> (56U - i * 8U));
    }
    sha256_transform(ctx, ctx->buffer);
    for (size_t i = 0; i < 8; ++i) {
        store_be32(digest + i * 4U, ctx->state[i]);
    }
    memset(ctx, 0, sizeof(*ctx));
}

void sha256_compute(const void *data, size_t len, uint8_t digest[SHA256_DIGEST_SIZE]) {
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, digest);
}
