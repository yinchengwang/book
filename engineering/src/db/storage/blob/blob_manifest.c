/**
 * @file blob_manifest.c
 * @brief Blob Chunk 固定格式与原子发布实现（Task 2）
 *
 * 实现 Chunk 文件的写入、校验和原子发布逻辑。
 * 文件格式：header(56) + payload(payload_size) + payload_checksum(4)
 * 所有多字节字段按 little-endian 写入。
 */
#include "db/blob_manifest.h"
#include "db/sha256.h"
#include "db/core/log.h"
#include "db/storage/wal/wal_flush.h"

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

static uint32_t crc32_compute(const void *data, size_t len) {
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

static void sha256_to_hex(const uint8_t digest[BLOB_CHUNK_ID_SIZE],
                          char hex[BLOB_CHUNK_ID_SIZE * 2 + 1]) {
    static const char hexc[] = "0123456789abcdef";
    for (size_t i = 0; i < BLOB_CHUNK_ID_SIZE; i++) {
        hex[2 * i]     = hexc[digest[i] >> 4];
        hex[2 * i + 1] = hexc[digest[i] & 0x0F];
    }
    hex[BLOB_CHUNK_ID_SIZE * 2] = '\0';
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
    sha256_to_hex(chunk_id, hex);
    int n = snprintf(path_buf, buf_size, "%s/%s.chunk", dir, hex);
    if (n < 0 || (size_t)n >= buf_size) return BLOB_ERR_INVAL;
    return BLOB_OK;
}

/* ========================================================================
 * Header/Payload 校验和计算
 * ======================================================================== */

uint32_t blob_chunk_header_checksum(const blob_chunk_header_t *hdr) {
    /* 覆盖前 48 字节（magic + version + payload_size + chunk_sha256） */
    return crc32_compute(hdr, offsetof(blob_chunk_header_t, header_checksum));
}

uint32_t blob_chunk_payload_checksum(const void *data, size_t len) {
    return crc32_compute(data, len);
}

/* ========================================================================
 * Header 序列化/反序列化
 * ======================================================================== */

static void header_serialize(const blob_chunk_header_t *hdr, uint8_t buf[BLOB_CHUNK_HEADER_SIZE]) {
    put_le32(buf + 0,  hdr->magic);
    put_le32(buf + 4,  hdr->version);
    put_le64(buf + 8,  hdr->payload_size);
    memcpy(buf + 16, hdr->chunk_sha256, BLOB_CHUNK_ID_SIZE);
    put_le32(buf + 48, hdr->header_checksum);
}

static int header_deserialize(const uint8_t buf[BLOB_CHUNK_HEADER_SIZE],
                              blob_chunk_header_t *hdr) {
    hdr->magic          = get_le32(buf + 0);
    hdr->version        = get_le32(buf + 4);
    hdr->payload_size   = get_le64(buf + 8);
    memcpy(hdr->chunk_sha256, buf + 16, BLOB_CHUNK_ID_SIZE);
    hdr->header_checksum = get_le32(buf + 48);
    return 0;
}

/* ========================================================================
 * 目录确保
 * ======================================================================== */

static int ensure_chunks_dir(const char *dir) {
    mkdir_path(dir);
    return 0;
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
    int fd = fileno(fp);
    return db_fsync(fd);
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
    ensure_chunks_dir(dir);

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
        LOG_ERROR("Blob Chunk: 无法创建临时文件 %s", tmp_path);
        return BLOB_ERR_IO;
    }

    /* 7. 写入 header */
    uint8_t header_buf[BLOB_CHUNK_HEADER_SIZE];
    header_serialize(&hdr, header_buf);
    rc = safe_fwrite(header_buf, 1, BLOB_CHUNK_HEADER_SIZE, fp);
    if (rc != 0) {
        LOG_ERROR("Blob Chunk: 写入 header 失败 %s", tmp_path);
        fclose(fp);
        return rc;
    }

    /* 8. 写入 payload */
    rc = safe_fwrite(data, 1, len, fp);
    if (rc != 0) {
        LOG_ERROR("Blob Chunk: 写入 payload 失败 %s", tmp_path);
        fclose(fp);
        return rc;
    }

    /* 9. 写入 payload_checksum */
    rc = safe_fwrite(&payload_cksum, sizeof(payload_cksum), 1, fp);
    if (rc != 0) {
        LOG_ERROR("Blob Chunk: 写入 payload_checksum 失败 %s", tmp_path);
        fclose(fp);
        return rc;
    }

    /* 10. fflush + fsync */
    if (fflush(fp) != 0) {
        LOG_ERROR("Blob Chunk: fflush 失败 %s", tmp_path);
        fclose(fp);
        return BLOB_ERR_IO;
    }
    if (file_fsync(fp) != 0) {
        LOG_ERROR("Blob Chunk: fsync 失败 %s", tmp_path);
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
            LOG_DEBUG("Blob Chunk: 正式文件已存在且一致，复用 %s", final_path);
            return BLOB_OK;
        }
        /* 正式文件存在但内容不同，不允许覆盖 */
        remove(tmp_path);
        LOG_WARN("Blob Chunk: 正式文件已存在但内容不同，拒绝覆盖 %s", final_path);
        return BLOB_ERR_CONFLICT;
    }

    /* 12. 正式文件不存在，rename 临时文件为正式文件 */
    if (rename(tmp_path, final_path) != 0) {
        LOG_ERROR("Blob Chunk: rename 失败 %s -> %s", tmp_path, final_path);
        remove(tmp_path);
        return BLOB_ERR_IO;
    }

    LOG_DEBUG("Blob Chunk: 发布成功 %s", final_path);
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

    header_deserialize(header_buf, out_header);

    /* 校验 magic */
    if (out_header->magic != BLOB_CHUNK_MAGIC) {
        LOG_WARN("Blob Chunk: magic 不匹配 (期望 0x%08X, 实际 0x%08X)",
                 BLOB_CHUNK_MAGIC, out_header->magic);
        return BLOB_ERR_CORRUPT;
    }

    /* 校验 version */
    if (out_header->version != BLOB_CHUNK_VERSION) {
        LOG_WARN("Blob Chunk: version 不匹配 (期望 %u, 实际 %u)",
                 BLOB_CHUNK_VERSION, out_header->version);
        return BLOB_ERR_CORRUPT;
    }

    /* 校验 header_checksum */
    uint32_t computed_cksum = blob_chunk_header_checksum(out_header);
    if (computed_cksum != out_header->header_checksum) {
        LOG_WARN("Blob Chunk: header_checksum 不匹配");
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
        LOG_WARN("Blob Chunk: payload_size 为零");
        return BLOB_ERR_CORRUPT;
    }

    /* 3. 检查输出缓冲区是否足够 */
    if (buf_len < hdr.payload_size) {
        LOG_WARN("Blob Chunk: 输出缓冲区不足 (需 %lu, 有 %lu)",
                 (unsigned long)hdr.payload_size, (unsigned long)buf_len);
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
        LOG_WARN("Blob Chunk: payload 读取长度不匹配 (需 %lu, 实际 %lu)",
                 (unsigned long)hdr.payload_size, (unsigned long)nread);
        fclose(fp);
        *out_len = 0;
        return BLOB_ERR_CORRUPT;
    }

    /* 5. 读取 payload_checksum */
    uint32_t disk_cksum;
    if (fread(&disk_cksum, sizeof(disk_cksum), 1, fp) != 1) {
        LOG_WARN("Blob Chunk: 读取 payload_checksum 失败");
        fclose(fp);
        *out_len = 0;
        return BLOB_ERR_CORRUPT;
    }
    fclose(fp);

    /* 6. 校验 payload_checksum */
    uint32_t computed_cksum = blob_chunk_payload_checksum(out_buf, hdr.payload_size);
    if (computed_cksum != disk_cksum) {
        LOG_WARN("Blob Chunk: payload_checksum 不匹配 (期望 0x%08X, 实际 0x%08X)",
                 disk_cksum, computed_cksum);
        *out_len = 0;
        return BLOB_ERR_CORRUPT;
    }

    /* 7. 校验 SHA-256 */
    uint8_t actual_sha[BLOB_CHUNK_ID_SIZE];
    sha256_compute(out_buf, hdr.payload_size, actual_sha);
    if (memcmp(actual_sha, hdr.chunk_sha256, BLOB_CHUNK_ID_SIZE) != 0) {
        LOG_WARN("Blob Chunk: SHA-256 不匹配");
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
        LOG_WARN("Blob Chunk: 文件大小不匹配 (期望 %lu, 实际 %lu)",
                 (unsigned long)expected_size, (unsigned long)st.st_size);
        return BLOB_ERR_CORRUPT;
    }

    return BLOB_OK;
}
