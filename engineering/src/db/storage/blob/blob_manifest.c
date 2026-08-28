/**
 * @file blob_manifest.c
 * @brief Blob Chunk/Manifest 固定格式与原子发布实现（Task 2+3）
 *
 * 实现 Chunk 和 Manifest 文件的写入、校验和原子发布逻辑。
 *
 * Chunk 格式：header(56) + payload(payload_size) + payload_checksum(4)
 * Manifest 格式：header(28) + content_type + metadata + chunks[]
 *
 * 所有多字节字段按 little-endian 写入。
 */
#include "db/blob_manifest.h"
#include "db/sha256.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define mkdir_path(path) _mkdir(path)
#else
#include <unistd.h>
#define mkdir_path(path) mkdir(path, 0755)
#endif

/* ========================================================================
 * 日志宏（简化版本，避免引入额外依赖）
 * ======================================================================== */

#ifdef BLOB_ENABLE_LOG
#include "db/log.h"
#define LOG_DEBUG(...)  log_debug(__VA_ARGS__)
#define LOG_WARN(...)   log_warn(__VA_ARGS__)
#define LOG_ERROR(...)  log_error(__VA_ARGS__)
#else
#define LOG_DEBUG(...)  ((void)0)
#define LOG_WARN(...)   ((void)0)
#define LOG_ERROR(...)  ((void)0)
#endif

/* ========================================================================
 * CRC32 查找表（CRC-32/ISO-HDLC，多项式 0xEDB88320）
 * ======================================================================== */

static uint32_t crc32_table[256];
static bool crc32_table_initialized = false;

static void crc32_init_table(void) {
    if (crc32_table_initialized) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
        crc32_table[i] = crc;
    }
    crc32_table_initialized = true;
}

uint32_t blob_chunk_payload_checksum(const void *data, size_t len) {
    crc32_init_table();
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

/* ========================================================================
 * 小端序读写辅助
 * ======================================================================== */

static void put_le32(uint8_t *buf, uint32_t val) {
    buf[0] = (uint8_t)(val);
    buf[1] = (uint8_t)(val >> 8);
    buf[2] = (uint8_t)(val >> 16);
    buf[3] = (uint8_t)(val >> 24);
}

static void put_le64(uint8_t *buf, uint64_t val) {
    buf[0] = (uint8_t)(val);
    buf[1] = (uint8_t)(val >> 8);
    buf[2] = (uint8_t)(val >> 16);
    buf[3] = (uint8_t)(val >> 24);
    buf[4] = (uint8_t)(val >> 32);
    buf[5] = (uint8_t)(val >> 40);
    buf[6] = (uint8_t)(val >> 48);
    buf[7] = (uint8_t)(val >> 56);
}

static uint32_t get_le32(const uint8_t *buf) {
    return (uint32_t)buf[0]
         | ((uint32_t)buf[1] << 8)
         | ((uint32_t)buf[2] << 16)
         | ((uint32_t)buf[3] << 24);
}

static uint64_t get_le64(const uint8_t *buf) {
    return (uint64_t)buf[0]
         | ((uint64_t)buf[1] << 8)
         | ((uint64_t)buf[2] << 16)
         | ((uint64_t)buf[3] << 24)
         | ((uint64_t)buf[4] << 32)
         | ((uint64_t)buf[5] << 40)
         | ((uint64_t)buf[6] << 48)
         | ((uint64_t)buf[7] << 56);
}

/* ========================================================================
 * SHA-256 转十六进制字符串
 * ======================================================================== */

void blob_sha256_to_hex(const uint8_t digest[BLOB_BLOB_ID_SIZE],
                        char hex[BLOB_BLOB_ID_SIZE * 2 + 1]) {
    static const char hexc[] = "0123456789abcdef";
    for (size_t i = 0; i < BLOB_BLOB_ID_SIZE; i++) {
        hex[2 * i]     = hexc[digest[i] >> 4];
        hex[2 * i + 1] = hexc[digest[i] & 0x0F];
    }
    hex[BLOB_BLOB_ID_SIZE * 2] = '\0';
}

/* ========================================================================
 * 路径构造
 * ======================================================================== */

int blob_chunk_tmp_path(const char *dir, const char *upload_id,
                        char *path_buf, size_t buf_size) {
    int n = snprintf(path_buf, buf_size, "%s/.tmp.%s", dir, upload_id);
    if (n < 0 || (size_t)n >= buf_size) return BLOB_ERR_INVAL;
    return BLOB_OK;
}

int blob_chunk_final_path(const char *dir,
                          const uint8_t chunk_id[BLOB_CHUNK_ID_SIZE],
                          char *path_buf, size_t buf_size) {
    char hex[BLOB_CHUNK_ID_SIZE * 2 + 1];
    blob_sha256_to_hex(chunk_id, hex);
    int n = snprintf(path_buf, buf_size, "%s/%s.chunk", dir, hex);
    if (n < 0 || (size_t)n >= buf_size) return BLOB_ERR_INVAL;
    return BLOB_OK;
}

int blob_manifest_tmp_path(const char *dir, const char *upload_id,
                          char *path_buf, size_t buf_size) {
    int n = snprintf(path_buf, buf_size, "%s/.tmp.%s", dir, upload_id);
    if (n < 0 || (size_t)n >= buf_size) return BLOB_ERR_INVAL;
    return BLOB_OK;
}

int blob_manifest_final_path(const char *dir,
                             const uint8_t blob_id[BLOB_BLOB_ID_SIZE],
                             char *path_buf, size_t buf_size) {
    char hex[BLOB_BLOB_ID_SIZE * 2 + 1];
    blob_sha256_to_hex(blob_id, hex);
    int n = snprintf(path_buf, buf_size, "%s/%s.manifest", dir, hex);
    if (n < 0 || (size_t)n >= buf_size) return BLOB_ERR_INVAL;
    return BLOB_OK;
}

/* ========================================================================
 * Header/Payload 校验和计算
 * ======================================================================== */

uint32_t blob_chunk_header_checksum(const blob_chunk_header_t *hdr) {
    /* 覆盖前 48 字节（magic + version + payload_size + chunk_sha256） */
    return blob_chunk_payload_checksum(hdr, offsetof(blob_chunk_header_t, header_checksum));
}

uint32_t blob_manifest_header_checksum(const blob_manifest_header_t *hdr) {
    /* 覆盖前 24 字节 */
    return blob_chunk_payload_checksum(hdr, offsetof(blob_manifest_header_t, manifest_checksum));
}

uint32_t blob_manifest_chunk_checksum(const blob_manifest_chunk_t *chunk) {
    /* 覆盖前 40 字节：chunk_sha256(32) + logical_offset(8) */
    return blob_chunk_payload_checksum(chunk, offsetof(blob_manifest_chunk_t, chunk_checksum));
}

/* ========================================================================
 * Header 序列化/反序列化
 * ======================================================================== */

static void chunk_header_serialize(const blob_chunk_header_t *hdr,
                                   uint8_t buf[BLOB_CHUNK_HEADER_SIZE]) {
    put_le32(buf + 0,  hdr->magic);
    put_le32(buf + 4,  hdr->version);
    put_le64(buf + 8,  hdr->payload_size);
    memcpy(buf + 16, hdr->chunk_sha256, BLOB_CHUNK_ID_SIZE);
    put_le32(buf + 48, hdr->header_checksum);
}

static int chunk_header_deserialize(const uint8_t buf[BLOB_CHUNK_HEADER_SIZE],
                                    blob_chunk_header_t *hdr) {
    hdr->magic          = get_le32(buf + 0);
    hdr->version        = get_le32(buf + 4);
    hdr->payload_size   = get_le64(buf + 8);
    memcpy(hdr->chunk_sha256, buf + 16, BLOB_CHUNK_ID_SIZE);
    hdr->header_checksum = get_le32(buf + 48);
    return 0;
}

static void put_le16(uint8_t *buf, uint16_t val) {
    buf[0] = (uint8_t)(val);
    buf[1] = (uint8_t)(val >> 8);
}

static uint16_t get_le16(const uint8_t *buf) {
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

static void manifest_header_serialize(const blob_manifest_header_t *hdr,
                                      uint8_t buf[BLOB_MANIFEST_HEADER_SIZE]) {
    put_le32(buf + 0,  hdr->magic);
    put_le32(buf + 4,  hdr->version);
    put_le32(buf + 8,  hdr->flags);
    put_le64(buf + 12, hdr->blob_size);
    put_le32(buf + 20, hdr->chunk_size);
    put_le32(buf + 24, hdr->chunk_count);
    put_le16(buf + 28, hdr->content_type_len);
    put_le16(buf + 30, hdr->metadata_len);
    memcpy(buf + 32, hdr->blob_sha256, BLOB_BLOB_ID_SIZE);
    /* manifest_checksum 稍后填充 */
}

static int manifest_header_deserialize(const uint8_t buf[BLOB_MANIFEST_HEADER_SIZE],
                                       blob_manifest_header_t *hdr) {
    hdr->magic           = get_le32(buf + 0);
    hdr->version         = get_le32(buf + 4);
    hdr->flags           = get_le32(buf + 8);
    hdr->blob_size       = get_le64(buf + 12);
    hdr->chunk_size      = get_le32(buf + 20);
    hdr->chunk_count     = get_le32(buf + 24);
    hdr->content_type_len = get_le16(buf + 28);
    hdr->metadata_len    = get_le16(buf + 30);
    memcpy(hdr->blob_sha256, buf + 32, BLOB_BLOB_ID_SIZE);
    hdr->manifest_checksum = get_le32(buf + 60);
    return 0;
}

static void manifest_chunk_serialize(const blob_manifest_chunk_t *chunk,
                                     uint8_t buf[BLOB_MANIFEST_CHUNK_SIZE]) {
    memcpy(buf + 0,  chunk->chunk_sha256, BLOB_CHUNK_ID_SIZE);
    put_le64(buf + 32, chunk->logical_offset);
    put_le32(buf + 40, chunk->chunk_size);
    /* chunk_checksum 稍后填充 */
}

static void manifest_chunk_deserialize(const uint8_t buf[BLOB_MANIFEST_CHUNK_SIZE],
                                       blob_manifest_chunk_t *chunk) {
    memcpy(chunk->chunk_sha256, buf + 0, BLOB_CHUNK_ID_SIZE);
    chunk->logical_offset = get_le64(buf + 32);
    chunk->chunk_size     = get_le32(buf + 40);
    chunk->chunk_checksum = get_le32(buf + 44);
}

/* ========================================================================
 * 目录确保
 * ======================================================================== */

static int ensure_dir(const char *dir) {
    if (mkdir_path(dir) != 0 && errno != EEXIST) {
        return BLOB_ERR_IO;
    }
    return BLOB_OK;
}

/* ========================================================================
 * 文件写入辅助
 * ======================================================================== */

/**
 * @brief 安全写入：检查 fwrite 返回值
 */
static int safe_fwrite(const void *data, size_t size, size_t count, FILE *fp) {
    size_t written = fwrite(data, size, count, fp);
    if (written != count) {
        return BLOB_ERR_IO;
    }
    return 0;
}

/**
 * @brief 跨平台 fsync
 */
static int file_fsync(FILE *fp) {
#ifdef _WIN32
    if (_commit(_fileno(fp)) != 0) {
        return BLOB_ERR_IO;
    }
#else
    if (fsync(fileno(fp)) != 0) {
        return BLOB_ERR_IO;
    }
#endif
    return BLOB_OK;
}

/* ========================================================================
 * blob_chunk_write_tmp: 写入临时文件并原子发布
 * ======================================================================== */

int blob_chunk_write_tmp(const char *dir,
                         const void *data, size_t len,
                         const char *upload_id,
                         uint8_t out_chunk_id[BLOB_CHUNK_ID_SIZE]) {
    if (!dir || !data || len == 0 || !upload_id || !out_chunk_id) {
        return BLOB_ERR_INVAL;
    }

    /* 1. 确保 chunks 目录存在 */
    ensure_dir(dir);

    /* 2. 计算 chunk_id = SHA-256(data) */
    sha256_compute(data, len, out_chunk_id);

    /* 3. 构造临时文件路径 */
    char tmp_path[1024];
    int rc = blob_chunk_tmp_path(dir, upload_id, tmp_path, sizeof(tmp_path));
    if (rc != 0) return rc;

    /* 4. 构造 header */
    blob_chunk_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = BLOB_CHUNK_MAGIC;
    hdr.version = BLOB_CHUNK_VERSION;
    hdr.payload_size = len;
    memcpy(hdr.chunk_sha256, out_chunk_id, BLOB_CHUNK_ID_SIZE);
    hdr.header_checksum = blob_chunk_header_checksum(&hdr);

    /* 5. 计算 payload_checksum */
    uint32_t payload_cksum = blob_chunk_payload_checksum(data, len);

    /* 6. 打开临时文件 */
    FILE *fp = fopen(tmp_path, "wb");
    if (!fp) {
        return BLOB_ERR_IO;
    }

    /* 7. 写入 header */
    uint8_t header_buf[BLOB_CHUNK_HEADER_SIZE];
    chunk_header_serialize(&hdr, header_buf);
    rc = safe_fwrite(header_buf, 1, BLOB_CHUNK_HEADER_SIZE, fp);
    if (rc != 0) {
        fclose(fp);
        return rc;
    }

    /* 8. 写入 payload */
    rc = safe_fwrite(data, 1, len, fp);
    if (rc != 0) {
        fclose(fp);
        return rc;
    }

    /* 9. 写入 payload_checksum */
    rc = safe_fwrite(&payload_cksum, sizeof(payload_cksum), 1, fp);
    if (rc != 0) {
        fclose(fp);
        return rc;
    }

    /* 10. fflush + fsync */
    if (fflush(fp) != 0) {
        fclose(fp);
        return BLOB_ERR_IO;
    }
    if (file_fsync(fp) != 0) {
        fclose(fp);
        return BLOB_ERR_IO;
    }
    fclose(fp);

    /* 11. 原子发布：检查正式文件是否已存在 */
    char final_path[1024];
    rc = blob_chunk_final_path(dir, out_chunk_id, final_path, sizeof(final_path));
    if (rc != 0) {
        return rc;
    }

    /* 正式文件已存在时的处理 */
    struct stat st;
    if (stat(final_path, &st) == 0) {
        /* 正式文件存在，校验内容是否一致 */
        blob_chunk_header_t existing_hdr;
        rc = blob_chunk_read_header(dir, out_chunk_id, &existing_hdr);
        if (rc == BLOB_OK &&
            existing_hdr.magic == hdr.magic &&
            existing_hdr.version == hdr.version &&
            existing_hdr.payload_size == hdr.payload_size &&
            memcmp(existing_hdr.chunk_sha256, hdr.chunk_sha256, BLOB_CHUNK_ID_SIZE) == 0 &&
            existing_hdr.header_checksum == hdr.header_checksum) {
            /* 内容完全一致，删除临时文件，复用正式文件 */
            remove(tmp_path);
            return BLOB_OK;
        }
        /* 正式文件存在但内容不同，不允许覆盖 */
        remove(tmp_path);
        return BLOB_ERR_CONFLICT;
    }

    /* 12. 正式文件不存在，rename 临时文件为正式文件 */
    if (rename(tmp_path, final_path) != 0) {
        remove(tmp_path);
        return BLOB_ERR_IO;
    }

    return BLOB_OK;
}

/* ========================================================================
 * blob_chunk_read_header: 仅读取 header
 * ======================================================================== */

int blob_chunk_read_header(const char *dir,
                           const uint8_t chunk_id[BLOB_CHUNK_ID_SIZE],
                           blob_chunk_header_t *out_header) {
    if (!dir || !chunk_id || !out_header) {
        return BLOB_ERR_INVAL;
    }

    char path[1024];
    int rc = blob_chunk_final_path(dir, chunk_id, path, sizeof(path));
    if (rc != 0) return rc;

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return BLOB_ERR_NOTFOUND;
    }

    uint8_t header_buf[BLOB_CHUNK_HEADER_SIZE];
    size_t nread = fread(header_buf, 1, BLOB_CHUNK_HEADER_SIZE, fp);
    fclose(fp);

    if (nread != BLOB_CHUNK_HEADER_SIZE) {
        return BLOB_ERR_CORRUPT;
    }

    chunk_header_deserialize(header_buf, out_header);

    /* 校验 magic */
    if (out_header->magic != BLOB_CHUNK_MAGIC) {
        return BLOB_ERR_CORRUPT;
    }

    /* 校验 version */
    if (out_header->version != BLOB_CHUNK_VERSION) {
        return BLOB_ERR_CORRUPT;
    }

    /* 校验 header_checksum */
    uint32_t computed_cksum = blob_chunk_header_checksum(out_header);
    if (computed_cksum != out_header->header_checksum) {
        return BLOB_ERR_CORRUPT;
    }

    return BLOB_OK;
}

/* ========================================================================
 * blob_chunk_read_checked: 读取并校验完整 Chunk
 * ======================================================================== */

int blob_chunk_read_checked(const char *dir,
                            const uint8_t chunk_id[BLOB_CHUNK_ID_SIZE],
                            void *out_buf, size_t buf_len,
                            size_t *out_len) {
    if (!dir || !chunk_id || !out_buf || !out_len) {
        return BLOB_ERR_INVAL;
    }

    *out_len = 0;

    /* 1. 读取并校验 header */
    blob_chunk_header_t hdr;
    int rc = blob_chunk_read_header(dir, chunk_id, &hdr);
    if (rc != BLOB_OK) {
        return rc;
    }

    /* 2. 校验 payload_size 合理性 */
    if (hdr.payload_size == 0) {
        return BLOB_ERR_CORRUPT;
    }

    /* 3. 检查输出缓冲区是否足够 */
    if (buf_len < hdr.payload_size) {
        return BLOB_ERR_INVAL;
    }

    /* 4. 打开文件，跳过 header 读取 payload */
    char path[1024];
    rc = blob_chunk_final_path(dir, chunk_id, path, sizeof(path));
    if (rc != 0) return rc;

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return BLOB_ERR_NOTFOUND;
    }

    /* 跳过 header */
    if (fseek(fp, (long)BLOB_CHUNK_HEADER_SIZE, SEEK_SET) != 0) {
        fclose(fp);
        return BLOB_ERR_IO;
    }

    /* 读取 payload */
    size_t nread = fread(out_buf, 1, (size_t)hdr.payload_size, fp);
    if (nread != (size_t)hdr.payload_size) {
        fclose(fp);
        *out_len = 0;
        return BLOB_ERR_CORRUPT;
    }

    /* 5. 读取 payload_checksum */
    uint32_t disk_cksum;
    if (fread(&disk_cksum, sizeof(disk_cksum), 1, fp) != 1) {
        fclose(fp);
        *out_len = 0;
        return BLOB_ERR_CORRUPT;
    }
    fclose(fp);

    /* 6. 校验 payload_checksum */
    uint32_t computed_cksum = blob_chunk_payload_checksum(out_buf, hdr.payload_size);
    if (computed_cksum != disk_cksum) {
        *out_len = 0;
        return BLOB_ERR_CORRUPT;
    }

    /* 7. 校验 SHA-256 */
    uint8_t actual_sha[BLOB_CHUNK_ID_SIZE];
    sha256_compute(out_buf, hdr.payload_size, actual_sha);
    if (memcmp(actual_sha, hdr.chunk_sha256, BLOB_CHUNK_ID_SIZE) != 0) {
        *out_len = 0;
        return BLOB_ERR_CORRUPT;
    }

    *out_len = (size_t)hdr.payload_size;
    return BLOB_OK;
}

/* ========================================================================
 * blob_chunk_exists_checked: 检查 Chunk 存在且可校验
 * ======================================================================== */

int blob_chunk_exists_checked(const char *dir,
                              const uint8_t chunk_id[BLOB_CHUNK_ID_SIZE]) {
    if (!dir || !chunk_id) {
        return BLOB_ERR_INVAL;
    }

    char path[1024];
    int rc = blob_chunk_final_path(dir, chunk_id, path, sizeof(path));
    if (rc != 0) return rc;

    /* 检查文件是否存在 */
    struct stat st;
    if (stat(path, &st) != 0) {
        return BLOB_ERR_NOTFOUND;
    }

    /* 读取并校验 header */
    blob_chunk_header_t hdr;
    rc = blob_chunk_read_header(dir, chunk_id, &hdr);
    if (rc != BLOB_OK) {
        return rc;
    }

    /* 校验文件大小：header + payload + payload_checksum */
    uint64_t expected_size = BLOB_CHUNK_HEADER_SIZE + hdr.payload_size + sizeof(uint32_t);
    if ((uint64_t)st.st_size != expected_size) {
        return BLOB_ERR_CORRUPT;
    }

    return BLOB_OK;
}

/* ========================================================================
 * Manifest 内存结构管理
 * ======================================================================== */

blob_manifest_t *blob_manifest_create(uint32_t chunk_count,
                                      const char *content_type,
                                      const void *metadata, size_t metadata_len) {
    if (chunk_count == 0) {
        return NULL;
    }

    /* 分配 Manifest 结构 */
    blob_manifest_t *manifest = (blob_manifest_t *)calloc(1, sizeof(blob_manifest_t));
    if (!manifest) {
        return NULL;
    }

    /* 分配 Chunk 数组 */
    manifest->chunks = (blob_manifest_chunk_t *)calloc(chunk_count, sizeof(blob_manifest_chunk_t));
    if (!manifest->chunks) {
        free(manifest);
        return NULL;
    }

    manifest->chunk_count = chunk_count;
    manifest->header.chunk_count = chunk_count;
    manifest->header.chunk_size = BLOB_CHUNK_LOGICAL_SIZE;
    manifest->header.flags = 0;

    /* 复制 content_type */
    if (content_type && content_type[0] != '\0') {
        size_t ct_len = strlen(content_type);
        manifest->content_type = (char *)malloc(ct_len + 1);
        if (manifest->content_type) {
            memcpy(manifest->content_type, content_type, ct_len + 1);
            manifest->header.content_type_len = (uint16_t)ct_len;
        } else {
            blob_manifest_free(manifest);
            return NULL;
        }
    } else {
        manifest->content_type = NULL;
        manifest->header.content_type_len = 0;
    }

    /* 复制 metadata */
    if (metadata && metadata_len > 0) {
        manifest->metadata = malloc(metadata_len);
        if (manifest->metadata) {
            memcpy(manifest->metadata, metadata, metadata_len);
            manifest->header.metadata_len = (uint16_t)metadata_len;
        } else {
            blob_manifest_free(manifest);
            return NULL;
        }
    } else {
        manifest->metadata = NULL;
        manifest->header.metadata_len = 0;
    }

    return manifest;
}

void blob_manifest_free(blob_manifest_t *manifest) {
    if (!manifest) {
        return;
    }
    free(manifest->chunks);
    free(manifest->content_type);
    free(manifest->metadata);
    free(manifest);
}

/* ========================================================================
 * blob_manifest_write_atomic: 原子写入 Manifest 文件
 * ======================================================================== */

int blob_manifest_write_atomic(const char *dir,
                               const blob_manifest_t *manifest,
                               const char *upload_id) {
    if (!dir || !manifest || !upload_id) {
        return BLOB_ERR_INVAL;
    }

    /* 1. 确保目录存在 */
    ensure_dir(dir);

    /* 2. 构造临时文件路径 */
    char tmp_path[1024];
    int rc = blob_manifest_tmp_path(dir, upload_id, tmp_path, sizeof(tmp_path));
    if (rc != 0) return rc;

    /* 3. 打开临时文件 */
    FILE *fp = fopen(tmp_path, "wb");
    if (!fp) {
        return BLOB_ERR_IO;
    }

    /* 4. 写入头部（先写入，后填充 checksum） */
    uint8_t header_buf[BLOB_MANIFEST_HEADER_SIZE];
    manifest_header_serialize(&manifest->header, header_buf);

    /* 计算头部校验和 */
    uint32_t hdr_cksum = blob_manifest_header_checksum((blob_manifest_header_t *)header_buf);
    put_le32(header_buf + 60, hdr_cksum);

    rc = safe_fwrite(header_buf, 1, BLOB_MANIFEST_HEADER_SIZE, fp);
    if (rc != 0) {
        fclose(fp);
        return rc;
    }

    /* 5. 写入 content_type */
    if (manifest->header.content_type_len > 0 && manifest->content_type) {
        rc = safe_fwrite(manifest->content_type, 1, manifest->header.content_type_len, fp);
        if (rc != 0) {
            fclose(fp);
            return rc;
        }
    }

    /* 6. 写入 metadata */
    if (manifest->header.metadata_len > 0 && manifest->metadata) {
        rc = safe_fwrite(manifest->metadata, 1, manifest->header.metadata_len, fp);
        if (rc != 0) {
            fclose(fp);
            return rc;
        }
    }

    /* 7. 写入 Chunk 条目数组 */
    for (uint32_t i = 0; i < manifest->chunk_count; i++) {
        uint8_t chunk_buf[BLOB_MANIFEST_CHUNK_SIZE];
        manifest_chunk_serialize(&manifest->chunks[i], chunk_buf);

        /* 计算条目校验和 */
        uint32_t chunk_cksum = blob_manifest_chunk_checksum(&manifest->chunks[i]);
        put_le32(chunk_buf + 44, chunk_cksum);

        rc = safe_fwrite(chunk_buf, 1, BLOB_MANIFEST_CHUNK_SIZE, fp);
        if (rc != 0) {
            fclose(fp);
            return rc;
        }
    }

    /* 8. fflush + fsync */
    if (fflush(fp) != 0) {
        fclose(fp);
        return BLOB_ERR_IO;
    }
    if (file_fsync(fp) != 0) {
        fclose(fp);
        return BLOB_ERR_IO;
    }
    fclose(fp);

    /* 9. 原子 rename 为正式文件 */
    char final_path[1024];
    rc = blob_manifest_final_path(dir, manifest->header.blob_sha256,
                                  final_path, sizeof(final_path));
    if (rc != 0) {
        remove(tmp_path);
        return rc;
    }

    if (rename(tmp_path, final_path) != 0) {
        remove(tmp_path);
        return BLOB_ERR_IO;
    }

    return BLOB_OK;
}

/* ========================================================================
 * blob_manifest_load_checked: 读取并校验 Manifest 文件
 * ======================================================================== */

int blob_manifest_load_checked(const char *dir,
                               const uint8_t blob_id[BLOB_BLOB_ID_SIZE],
                               blob_manifest_t **out_manifest) {
    if (!dir || !blob_id || !out_manifest) {
        return BLOB_ERR_INVAL;
    }

    *out_manifest = NULL;

    /* 1. 打开 Manifest 文件 */
    char path[1024];
    int rc = blob_manifest_final_path(dir, blob_id, path, sizeof(path));
    if (rc != 0) return rc;

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return BLOB_ERR_NOTFOUND;
    }

    /* 2. 检查文件大小 */
    struct stat st;
    if (fstat(fileno(fp), &st) != 0) {
        fclose(fp);
        return BLOB_ERR_IO;
    }

    /* 最小文件大小：header(28) + 至少一个 chunk(44) */
    size_t min_size = BLOB_MANIFEST_HEADER_SIZE + BLOB_MANIFEST_CHUNK_SIZE;
    if ((size_t)st.st_size < min_size) {
        fclose(fp);
        return BLOB_ERR_CORRUPT;
    }

    /* 最大文件大小限制（防止整数溢出）: 100MB */
    const size_t max_size = 100 * 1024 * 1024;
    if ((size_t)st.st_size > max_size) {
        fclose(fp);
        return BLOB_ERR_CORRUPT;
    }

    /* 3. 读取头部 */
    uint8_t header_buf[BLOB_MANIFEST_HEADER_SIZE];
    size_t nread = fread(header_buf, 1, BLOB_MANIFEST_HEADER_SIZE, fp);
    if (nread != BLOB_MANIFEST_HEADER_SIZE) {
        fclose(fp);
        return BLOB_ERR_CORRUPT;
    }

    blob_manifest_header_t hdr;
    manifest_header_deserialize(header_buf, &hdr);

    /* 4. 校验 magic */
    if (hdr.magic != BLOB_MANIFEST_MAGIC) {
        fclose(fp);
        return BLOB_ERR_CORRUPT;
    }

    /* 5. 校验 version */
    if (hdr.version != BLOB_MANIFEST_VERSION) {
        fclose(fp);
        return BLOB_ERR_CORRUPT;
    }

    /* 6. 校验头部校验和 */
    uint32_t computed_cksum = blob_manifest_header_checksum(&hdr);
    if (computed_cksum != hdr.manifest_checksum) {
        fclose(fp);
        return BLOB_ERR_CORRUPT;
    }

    /* 7. 校验 blob_id 一致性 */
    if (memcmp(blob_id, hdr.blob_sha256, BLOB_BLOB_ID_SIZE) != 0) {
        fclose(fp);
        return BLOB_ERR_CORRUPT;
    }

    /* 8. 校验 chunk_count 合理性 */
    if (hdr.chunk_count == 0) {
        fclose(fp);
        return BLOB_ERR_CORRUPT;
    }

    /* 计算期望的文件大小 */
    size_t expected_size = BLOB_MANIFEST_HEADER_SIZE
                         + hdr.content_type_len
                         + hdr.metadata_len
                         + ((size_t)hdr.chunk_count * BLOB_MANIFEST_CHUNK_SIZE);

    if ((size_t)st.st_size != expected_size) {
        fclose(fp);
        return BLOB_ERR_CORRUPT;
    }

    /* 9. 创建 Manifest 内存结构 */
    blob_manifest_t *manifest = blob_manifest_create(
        hdr.chunk_count,
        (hdr.content_type_len > 0) ? "" : NULL,
        NULL, 0);
    if (!manifest) {
        fclose(fp);
        return BLOB_ERR_NOMEM;
    }

    /* 复制头部信息 */
    manifest->header = hdr;

    /* 10. 读取 content_type */
    if (hdr.content_type_len > 0) {
        manifest->content_type = (char *)malloc(hdr.content_type_len + 1);
        if (!manifest->content_type) {
            fclose(fp);
            blob_manifest_free(manifest);
            return BLOB_ERR_NOMEM;
        }
        if (fread(manifest->content_type, 1, hdr.content_type_len, fp) != hdr.content_type_len) {
            fclose(fp);
            blob_manifest_free(manifest);
            return BLOB_ERR_CORRUPT;
        }
        manifest->content_type[hdr.content_type_len] = '\0';
    }

    /* 11. 读取 metadata */
    if (hdr.metadata_len > 0) {
        manifest->metadata = malloc(hdr.metadata_len);
        if (!manifest->metadata) {
            fclose(fp);
            blob_manifest_free(manifest);
            return BLOB_ERR_NOMEM;
        }
        if (fread(manifest->metadata, 1, hdr.metadata_len, fp) != hdr.metadata_len) {
            fclose(fp);
            blob_manifest_free(manifest);
            return BLOB_ERR_CORRUPT;
        }
    }

    /* 12. 读取 Chunk 条目数组 */
    uint64_t prev_offset = 0;
    for (uint32_t i = 0; i < hdr.chunk_count; i++) {
        uint8_t chunk_buf[BLOB_MANIFEST_CHUNK_SIZE];
        if (fread(chunk_buf, 1, BLOB_MANIFEST_CHUNK_SIZE, fp) != BLOB_MANIFEST_CHUNK_SIZE) {
            fclose(fp);
            blob_manifest_free(manifest);
            return BLOB_ERR_CORRUPT;
        }

        manifest_chunk_deserialize(chunk_buf, &manifest->chunks[i]);

        /* 校验条目校验和 */
        uint32_t computed_chunk_cksum = blob_manifest_chunk_checksum(&manifest->chunks[i]);
        if (computed_chunk_cksum != manifest->chunks[i].chunk_checksum) {
            fclose(fp);
            blob_manifest_free(manifest);
            return BLOB_ERR_CORRUPT;
        }

        /* 校验 offset 单调递增 */
        if (i > 0 && manifest->chunks[i].logical_offset <= prev_offset) {
            fclose(fp);
            blob_manifest_free(manifest);
            return BLOB_ERR_CORRUPT;
        }
        prev_offset = manifest->chunks[i].logical_offset;
    }

    fclose(fp);

    /* 13. 计算并校验 blob 整体 SHA-256
     * 由于 blob 内容可能分散在多个 Chunk 中，这里只校验 Manifest 自身完整性
     * 整体 blob_sha256 校验将在 blob_get 时进行 */
    (void)0; /* 预留扩展点 */

    *out_manifest = manifest;
    return BLOB_OK;
}
