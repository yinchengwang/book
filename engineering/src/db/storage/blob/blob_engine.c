/**
 * @file blob_engine.c
 * @brief Blob 存储引擎实现（C3-1）
 *
 * 分块存储 + SHA-256 内容寻址 + KV catalog 元数据
 * chunk 文件：{data_dir}/chunks/{chunk_id_hex}.bin
 * metadata：{data_dir}/blob_meta/
 */
#include "db/blob_engine.h"
#include "db/core/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir_path(path) _mkdir(path)
#else
#include <unistd.h>
#define mkdir_path(path) mkdir(path, 0755)
#endif

struct blob_engine_s {
    char data_dir[512];
};

static void ensure_dir(const char *path) {
    mkdir_path(path);
}

/* SHA-256 简化哈希：djb2 哈希 + 64 字节拼接模拟（生产环境应真正 SHA-256） */
static void simple_hash(const void *data, size_t len, uint8_t out[32]) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t h1 = 5381, h2 = 5381 + 1, h3 = 5381 + 2, h4 = 5381 + 3;
    for (size_t i = 0; i < len; i++) {
        h1 = ((h1 << 5) + h1) ^ p[i];
        h2 = ((h2 << 5) + h2) ^ p[i];
        h3 = ((h3 << 5) + h3) ^ p[i];
        h4 = ((h4 << 5) + h4) ^ p[i];
    }
    memcpy(out + 0, &h1, 4);
    memcpy(out + 4, &h2, 4);
    memcpy(out + 8, &h3, 4);
    memcpy(out + 12, &h4, 4);
    memset(out + 16, 0, 16);
}

static void hex_str(const uint8_t *bin, char *hex, size_t bin_len) {
    static const char hexc[] = "0123456789abcdef";
    for (size_t i = 0; i < bin_len; ++i) {
        hex[2*i]   = hexc[bin[i] >> 4];
        hex[2*i+1] = hexc[bin[i] & 0x0f];
    }
    hex[2*bin_len] = '\0';
}

blob_engine_t *blob_engine_create(const char *data_dir) {
    if (!data_dir) return NULL;
    blob_engine_t *e = calloc(1, sizeof(blob_engine_t));
    if (!e) return NULL;
    strncpy(e->data_dir, data_dir, sizeof(e->data_dir) - 1);
    ensure_dir(data_dir);
    char chunk_dir[600];
    snprintf(chunk_dir, sizeof(chunk_dir), "%s/chunks", data_dir);
    ensure_dir(chunk_dir);
    LOG_INFO("Blob 引擎创建: %s", data_dir);
    return e;
}

blob_engine_t *blob_engine_open(const char *data_dir) {
    return blob_engine_create(data_dir);
}

void blob_engine_close(blob_engine_t *engine) {
    free(engine);
}

int blob_put(blob_engine_t *engine, const void *data, size_t len,
             uint8_t out_blob_id[BLOB_SHA256_SIZE]) {
    if (!engine || !data || len == 0 || !out_blob_id) return -1;

    /* 计算整体 blob_id（简化为 hash of data） */
    simple_hash(data, len, out_blob_id);

    /* 分块写入 */
    char chunk_dir[600];
    snprintf(chunk_dir, sizeof(chunk_dir), "%s/chunks", engine->data_dir);
    ensure_dir(chunk_dir);

    size_t written = 0;
    while (written < len) {
        size_t chunk_len = len - written;
        if (chunk_len > BLOB_MAX_CHUNK_SIZE) chunk_len = BLOB_MAX_CHUNK_SIZE;

        /* 计算 chunk_id */
        uint8_t chunk_id[BLOB_SHA256_SIZE];
        simple_hash((const uint8_t *)data + written, chunk_len, chunk_id);

        char hex[65];
        hex_str(chunk_id, hex, BLOB_SHA256_SIZE);
        char path[700];
        snprintf(path, sizeof(path), "%s/%s.bin", chunk_dir, hex);

        /* 检查去重：文件已存在则跳过 */
        struct stat st;
        if (stat(path, &st) == 0) { written += chunk_len; continue; }

        FILE *fp = fopen(path, "wb");
        if (!fp) return -1;
        fwrite((const uint8_t *)data + written, 1, chunk_len, fp);
        fflush(fp);
        /* C3-1 T8：chunk 写入后 fsync（POSIX fsync / Windows _commit） */
#ifdef _WIN32
        _commit(_fileno(fp));
#else
        fsync(fileno(fp));
#endif
        fclose(fp);
        written += chunk_len;
    }

    return 0;
}

int blob_get(blob_engine_t *engine, const uint8_t blob_id[BLOB_SHA256_SIZE],
             void *out_buf, size_t buf_len, size_t *out_read) {
    if (!engine || !blob_id || !out_buf || !out_read) return -1;

    /* 简化：整体存储模式（chunk0 = 整个 blob）
     * 完整模式：读 catalog → chunk 列表 → 逐块组装 */
    char hex[65];
    hex_str(blob_id, hex, BLOB_SHA256_SIZE);
    char path[600];
    snprintf(path, sizeof(path), "%s/chunks/%s.bin", engine->data_dir, hex);
    FILE *fp = fopen(path, "rb");
    if (!fp) { *out_read = 0; return -1; }
    *out_read = fread(out_buf, 1, buf_len, fp);
    fclose(fp);
    return 0;
}

int blob_delete(blob_engine_t *engine, const uint8_t blob_id[BLOB_SHA256_SIZE]) {
    if (!engine || !blob_id) return -1;
    char hex[65];
    hex_str(blob_id, hex, BLOB_SHA256_SIZE);
    char path[600];
    snprintf(path, sizeof(path), "%s/chunks/%s.bin", engine->data_dir, hex);
    remove(path);
    return 0;
}

int blob_stat(blob_engine_t *engine, const uint8_t blob_id[BLOB_SHA256_SIZE],
              size_t *out_len) {
    if (!engine || !blob_id || !out_len) return -1;
    char hex[65];
    hex_str(blob_id, hex, BLOB_SHA256_SIZE);
    char path[600];
    snprintf(path, sizeof(path), "%s/chunks/%s.bin", engine->data_dir, hex);
    struct stat st;
    if (stat(path, &st) != 0) { *out_len = 0; return -1; }
    *out_len = (size_t)st.st_size;
    return 0;
}

int blob_range_get(blob_engine_t *engine, const uint8_t blob_id[BLOB_SHA256_SIZE],
                   size_t offset, size_t len,
                   void *out_buf, size_t buf_len, size_t *out_read) {
    if (!engine || !blob_id || !out_buf || !out_read) return -1;
    *out_read = 0;
    char hex[65];
    hex_str(blob_id, hex, BLOB_SHA256_SIZE);
    char path[600];
    snprintf(path, sizeof(path), "%s/chunks/%s.bin", engine->data_dir, hex);
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    if (fseek(fp, (long)offset, SEEK_SET) != 0) { fclose(fp); return -1; }
    *out_read = fread(out_buf, 1, len < buf_len ? len : buf_len, fp);
    fclose(fp);
    return 0;
}
