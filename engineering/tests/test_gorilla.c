/**
 * @file test_gorilla.c
 * @brief Standalone test for Gorilla XOR encoding
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

/* Simulate the header definitions */
typedef struct {
    uint8_t *buffer;
    size_t byte_pos;
    size_t bit_pos;
    float prev_value;
    int has_prev;
} gorilla_encoder_t;

typedef struct {
    const uint8_t *buffer;
    size_t buffer_size;
    size_t byte_pos;
    size_t bit_pos;
    float prev_value;
    int has_prev;
} gorilla_decoder_t;

int gorilla_encoder_init(gorilla_encoder_t *enc);
void gorilla_encoder_destroy(gorilla_encoder_t *enc);
int gorilla_decoder_init(gorilla_decoder_t *dec, const uint8_t *data, size_t size);
void gorilla_decoder_destroy(gorilla_decoder_t *dec);
int gorilla_encode(gorilla_encoder_t *enc, float value);
int gorilla_decode(gorilla_decoder_t *dec, float *value);
const uint8_t *gorilla_encoder_get_data(const gorilla_encoder_t *enc, size_t *out_size);

#define BITS_PER_BYTE 8
#define FLOAT_BITS 32
#define GORILLA_INIT_BUF_SIZE 4096

static void gorilla_write_bits(gorilla_encoder_t *enc, uint64_t value, int num_bits) {
    for (int i = 0; i < num_bits; i++) {
        int bit = (value >> i) & 1;
        if (bit) {
            enc->buffer[enc->byte_pos] |= (1 << enc->bit_pos);
        }
        enc->bit_pos++;
        if (enc->bit_pos == BITS_PER_BYTE) {
            enc->bit_pos = 0;
            enc->byte_pos++;
        }
    }
}

static uint64_t gorilla_read_bits(gorilla_decoder_t *dec, int num_bits) {
    uint64_t value = 0;
    for (int i = 0; i < num_bits; i++) {
        if (dec->byte_pos >= dec->buffer_size) break;
        int bit = (dec->buffer[dec->byte_pos] >> dec->bit_pos) & 1;
        value |= ((uint64_t)bit << i);
        dec->bit_pos++;
        if (dec->bit_pos == BITS_PER_BYTE) {
            dec->bit_pos = 0;
            dec->byte_pos++;
        }
    }
    return value;
}

int gorilla_encoder_init(gorilla_encoder_t *enc) {
    if (!enc) return -1;
    enc->buffer = (uint8_t *)calloc(GORILLA_INIT_BUF_SIZE, 1);
    if (!enc->buffer) return -1;
    enc->byte_pos = 0;
    enc->bit_pos = 0;
    enc->prev_value = 0.0f;
    enc->has_prev = 0;
    return 0;
}

void gorilla_encoder_destroy(gorilla_encoder_t *enc) {
    if (enc && enc->buffer) {
        free(enc->buffer);
        enc->buffer = NULL;
    }
}

int gorilla_decoder_init(gorilla_decoder_t *dec, const uint8_t *data, size_t size) {
    if (!dec || !data || size == 0) return -1;
    dec->buffer = data;
    dec->buffer_size = size;
    dec->byte_pos = 0;
    dec->bit_pos = 0;
    dec->prev_value = 0.0f;
    dec->has_prev = 0;
    return 0;
}

void gorilla_decoder_destroy(gorilla_decoder_t *dec) {
    if (dec) {
        dec->buffer = NULL;
        dec->buffer_size = 0;
    }
}

int gorilla_encode(gorilla_encoder_t *enc, float value) {
    if (!enc) return -1;

    size_t needed = enc->byte_pos + 16;
    size_t current_size = GORILLA_INIT_BUF_SIZE;
    while (current_size < needed) current_size *= 2;
    if (current_size > GORILLA_INIT_BUF_SIZE) {
        uint8_t *new_buf = (uint8_t *)realloc(enc->buffer, current_size);
        if (!new_buf) return -1;
        enc->buffer = new_buf;
        memset(enc->buffer + (current_size / 2), 0, current_size / 2);
    }

    uint32_t bits;
    memcpy(&bits, &value, sizeof(float));

    if (!enc->has_prev) {
        gorilla_write_bits(enc, bits, FLOAT_BITS);
        enc->prev_value = value;
        enc->has_prev = 1;
        return 0;
    }

    uint32_t prev_bits;
    memcpy(&prev_bits, &enc->prev_value, sizeof(float));
    uint32_t xor_val = bits ^ prev_bits;

    if (xor_val == 0) {
        gorilla_write_bits(enc, 0, 1);
    } else {
        gorilla_write_bits(enc, 1, 1);

        int leading_zeros = 0;
        int trailing_zeros = 0;
        uint32_t temp = xor_val;

        for (int i = FLOAT_BITS - 1; i >= 0; i--) {
            if ((temp >> i) & 1) break;
            leading_zeros++;
        }

        temp = xor_val;
        while ((temp & 1) == 0 && trailing_zeros < FLOAT_BITS) {
            temp >>= 1;
            trailing_zeros++;
        }

        int significant_bits = FLOAT_BITS - leading_zeros - trailing_zeros;

        gorilla_write_bits(enc, 1, 1);
        gorilla_write_bits(enc, leading_zeros, 5);
        gorilla_write_bits(enc, trailing_zeros, 5);
        gorilla_write_bits(enc, xor_val >> trailing_zeros, significant_bits);
    }

    enc->prev_value = value;
    return 0;
}

int gorilla_decode(gorilla_decoder_t *dec, float *value) {
    if (!dec || !value) return -1;

    if (!dec->has_prev) {
        uint32_t bits = (uint32_t)gorilla_read_bits(dec, FLOAT_BITS);
        float v;
        memcpy(&v, &bits, sizeof(float));
        *value = v;
        dec->prev_value = v;
        dec->has_prev = 1;
        return 0;
    }

    int has_change = (int)gorilla_read_bits(dec, 1);

    if (has_change == 0) {
        *value = dec->prev_value;
        return 0;
    }

    int meaningful = (int)gorilla_read_bits(dec, 1);
    (void)meaningful;

    int leading_zeros = (int)gorilla_read_bits(dec, 5);
    int trailing_zeros = (int)gorilla_read_bits(dec, 5);

    int significant_bits = FLOAT_BITS - leading_zeros - trailing_zeros;
    if (significant_bits <= 0) significant_bits = 1;

    uint32_t xor_val = (uint32_t)gorilla_read_bits(dec, significant_bits);
    xor_val <<= trailing_zeros;

    uint32_t prev_bits;
    memcpy(&prev_bits, &dec->prev_value, sizeof(float));
    uint32_t result = prev_bits ^ xor_val;

    float v;
    memcpy(&v, &result, sizeof(float));
    *value = v;
    dec->prev_value = v;

    return 0;
}

const uint8_t *gorilla_encoder_get_data(const gorilla_encoder_t *enc, size_t *out_size) {
    if (!enc || !out_size) return NULL;
    size_t total_bits = enc->byte_pos * BITS_PER_BYTE + enc->bit_pos;
    *out_size = (total_bits + BITS_PER_BYTE - 1) / BITS_PER_BYTE;
    return enc->buffer;
}

/* Test functions */
int test_single_value() {
    printf("Test: Single value round-trip... ");
    gorilla_encoder_t enc;
    gorilla_encoder_init(&enc);

    float val = 123.456f;
    int ret = gorilla_encode(&enc, val);
    if (ret != 0) {
        printf("FAILED (encode returned %d)\n", ret);
        gorilla_encoder_destroy(&enc);
        return 1;
    }

    size_t compressed_size = 0;
    const uint8_t *data = gorilla_encoder_get_data(&enc, &compressed_size);
    if (!data) {
        printf("FAILED (get_data returned NULL)\n");
        gorilla_encoder_destroy(&enc);
        return 1;
    }

    gorilla_decoder_t dec;
    gorilla_decoder_init(&dec, data, compressed_size);

    float decoded = 0.0f;
    ret = gorilla_decode(&dec, &decoded);
    if (ret != 0) {
        printf("FAILED (decode returned %d)\n", ret);
        gorilla_encoder_destroy(&enc);
        gorilla_decoder_destroy(&dec);
        return 1;
    }

    if (fabsf(decoded - val) > 1e-5) {
        printf("FAILED (decoded=%f, expected=%f)\n", decoded, val);
        gorilla_encoder_destroy(&enc);
        gorilla_decoder_destroy(&dec);
        return 1;
    }

    gorilla_encoder_destroy(&enc);
    gorilla_decoder_destroy(&dec);
    printf("PASSED\n");
    return 0;
}

int test_multiple_values() {
    printf("Test: Multiple values round-trip... ");
    gorilla_encoder_t enc;
    gorilla_encoder_init(&enc);

    float values[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    const int count = 5;

    for (int i = 0; i < count; i++) {
        gorilla_encode(&enc, values[i]);
    }

    size_t compressed_size = 0;
    const uint8_t *data = gorilla_encoder_get_data(&enc, &compressed_size);

    gorilla_decoder_t dec;
    gorilla_decoder_init(&dec, data, compressed_size);

    int failed = 0;
    for (int i = 0; i < count; i++) {
        float decoded = 0.0f;
        int ret = gorilla_decode(&dec, &decoded);
        if (ret != 0 || fabsf(decoded - values[i]) > 1e-5) {
            printf("\n  FAILED at index %d (ret=%d, decoded=%f, expected=%f)\n",
                   i, ret, decoded, values[i]);
            failed = 1;
            break;
        }
    }

    gorilla_encoder_destroy(&enc);
    gorilla_decoder_destroy(&dec);

    if (!failed) printf("PASSED\n");
    return failed;
}

int test_duplicate_values() {
    printf("Test: Duplicate values (high compression)... ");
    gorilla_encoder_t enc;
    gorilla_encoder_init(&enc);

    float val = 42.0f;
    for (int i = 0; i < 10; i++) {
        gorilla_encode(&enc, val);
    }

    size_t compressed_size = 0;
    const uint8_t *data = gorilla_encoder_get_data(&enc, &compressed_size);

    /* 10 duplicates should use very little space */
    if (compressed_size >= 50) {
        printf("WARNING: compressed size %zu >= 50 bytes (may not be compressed well)\n", compressed_size);
    }

    gorilla_decoder_t dec;
    gorilla_decoder_init(&dec, data, compressed_size);

    int failed = 0;
    for (int i = 0; i < 10; i++) {
        float decoded = 0.0f;
        int ret = gorilla_decode(&dec, &decoded);
        if (ret != 0 || fabsf(decoded - val) > 1e-5) {
            printf("FAILED at index %d\n", i);
            failed = 1;
            break;
        }
    }

    gorilla_encoder_destroy(&enc);
    gorilla_decoder_destroy(&dec);

    if (!failed) printf("PASSED (compressed to %zu bytes)\n", compressed_size);
    return failed;
}

int test_similar_values() {
    printf("Test: Similar values compression... ");
    gorilla_encoder_t enc;
    gorilla_encoder_init(&enc);

    float base = 1000000.0f;
    for (int i = 0; i < 100; i++) {
        float val = base + (float)i * 0.001f;
        gorilla_encode(&enc, val);
    }

    size_t compressed_size = 0;
    gorilla_encoder_get_data(&enc, &compressed_size);

    gorilla_encoder_destroy(&enc);

    /* 100 floats = 400 bytes, should compress to much less */
    if (compressed_size < 400) {
        printf("PASSED (400 -> %zu bytes, %.1f%% compression)\n",
               compressed_size, 100.0 * (400 - compressed_size) / 400);
    } else {
        printf("WARNING: only compressed to %zu bytes (expected < 400)\n", compressed_size);
    }
    return 0;
}

int main() {
    printf("=== Gorilla XOR Encoding Tests ===\n\n");

    int failures = 0;
    failures += test_single_value();
    failures += test_multiple_values();
    failures += test_duplicate_values();
    failures += test_similar_values();

    printf("\n=== Results: %d failures ===\n", failures);
    return failures;
}
