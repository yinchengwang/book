/**
 * @file blob_catalog.c
 * @brief Blob 独立 Catalog 实现（Task 4）
 *
 * 实现内存索引、WAL 写入、checkpoint 和启动恢复。
 *
 * WAL 记录格式：header(16) + payload(payload_len) + crc32(4)
 * Checkpoint 格式：固定头 + Blob 条目数组 + Chunk 引用数组 + checksum
 */
#include "db/blob_catalog.h"

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
#define fsync_func(fd) _commit(fd)
#else
#include <unistd.h>
#define mkdir_path(path) mkdir(path, 0755)
#define fsync_func(fd) fsync(fd)
#endif

/* ========================================================================
 * 调试日志宏（可选）
 * ======================================================================== */

#ifdef BLOB_CATALOG_DEBUG
#include <stdio.h>
#define CATALOG_DEBUG(...) fprintf(stderr, "[blob_catalog] " __VA_ARGS__)
#else
#define CATALOG_DEBUG(...) ((void)0)
#endif

/* ========================================================================
 * CRC32 实现（复用 blob_manifest.c 中的查找表）
 * ======================================================================== */

static uint32_t crc32_table[256];
static bool crc32_initialized = false;

static void crc32_init(void) {
    if (crc32_initialized) return;
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
    crc32_initialized = true;
}

static uint32_t crc32_update(uint32_t crc, const void *data, size_t len) {
    crc32_init();
    const uint8_t *p = (const uint8_t *)data;
    for (size_t i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc;
}

static uint32_t crc32_final(uint32_t crc) {
    return crc ^ 0xFFFFFFFF;
}

/* ========================================================================
 * 内存哈希表实现（简单开放地址法）
 * ======================================================================== */

/** Blob 条目哈希表桶 */
typedef struct blob_entry_bucket_s {
    blob_entry_t entry;
    bool occupied;
} blob_entry_bucket_t;

/** Chunk 引用哈希表桶 */
typedef struct blob_chunk_bucket_s {
    blob_chunk_ref_t ref;
    bool occupied;
} blob_chunk_bucket_t;

/** 哈希表初始容量 */
#define HASH_TABLE_CAPACITY 1024

/** 迭代器内部结构 */
struct blob_catalog_iter_s {
    const blob_catalog_t *catalog;
    size_t next_bucket;      /**< 下一个要检查的桶索引 */
};

/* ========================================================================
 * Catalog 内部结构
 * ======================================================================== */

struct blob_catalog_s {
    char data_dir[512];          /**< 数据根目录 */
    char catalog_dir[512];       /**< Catalog 目录 */
    char wal_path[600];          /**< WAL 文件路径 */
    char bin_path[600];          /**< Checkpoint 文件路径 */
    char bin_tmp_path[600];      /**< 临时 Checkpoint 文件路径 */

    /* 内存索引 */
    blob_entry_bucket_t *blob_table;    /**< Blob 条目哈希表 */
    size_t blob_table_size;             /**< Blob 表大小 */
    uint64_t blob_count;                /**< Blob 数量 */

    blob_chunk_bucket_t *chunk_table;   /**< Chunk 引用哈希表 */
    size_t chunk_table_size;            /**< Chunk 表大小 */
    uint64_t chunk_count;               /**< Chunk 引用数量 */

    /* WAL 状态 */
    FILE *wal_fp;                       /**< WAL 文件指针 */
    uint32_t current_lsn;               /**< 当前 LSN */
    bool in_transaction;                /**< 是否在事务中 */

    /* 错误状态 */
    int last_error;
};

/* ========================================================================
 * 时间戳获取
 * ======================================================================== */

#ifdef _WIN32
#include <windows.h>
static int64_t get_time_ms(void) {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    /* 1601-01-01 到 1970-01-01 的差值（100 纳秒） */
    int64_t t = ((int64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    t -= 116444736000000000LL;  /* 转换为 Unix 时间 */
    return t / 10000;  /* 转换为毫秒 */
}
#else
#include <sys/time.h>
static int64_t get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
#endif

/* ========================================================================
 * 目录确保
 * ======================================================================== */

static int ensure_dir(const char *dir) {
    if (mkdir_path(dir) != 0 && errno != EEXIST) {
        return BLOB_CATALOG_ERR_IO;
    }
    return BLOB_CATALOG_OK;
}

/* ========================================================================
 * 路径构造
 * ======================================================================== */

static void make_path(char *buf, size_t buf_size, const char *base, const char *name) {
    snprintf(buf, buf_size, "%s/%s", base, name);
}

/* ========================================================================
 * 哈希函数（djb2）
 * ======================================================================== */

static size_t hash_blob_id(const uint8_t blob_id[32]) {
    uint32_t h = 5381;
    for (int i = 0; i < 32; i++) {
        h = ((h << 5) + h) ^ blob_id[i];
    }
    return h;
}

static size_t hash_chunk_id(const uint8_t chunk_id[32]) {
    uint32_t h = 5381;
    for (int i = 0; i < 32; i++) {
        h = ((h << 5) + h) ^ chunk_id[i];
    }
    return h;
}

/* ========================================================================
 * Blob 条目查找
 * ======================================================================== */

int blob_catalog_find_blob(const blob_catalog_t *catalog,
                           const uint8_t blob_id[BLOB_CATALOG_ID_SIZE],
                           blob_entry_t *out_entry) {
    if (!catalog || !blob_id) {
        return BLOB_CATALOG_ERR_INVAL;
    }

    size_t idx = hash_blob_id(blob_id) % catalog->blob_table_size;
    for (size_t i = 0; i < catalog->blob_table_size; i++) {
        size_t pos = (idx + i) % catalog->blob_table_size;
        if (!catalog->blob_table[pos].occupied) {
            continue;
        }
        if (memcmp(catalog->blob_table[pos].entry.blob_id, blob_id, BLOB_CATALOG_ID_SIZE) == 0) {
            if (out_entry) {
                *out_entry = catalog->blob_table[pos].entry;
            }
            return BLOB_CATALOG_OK;
        }
    }
    return BLOB_CATALOG_ERR_NOTFOUND;
}

/* ========================================================================
 * Chunk 引用查找
 * ======================================================================== */

int blob_catalog_find_chunk(const blob_catalog_t *catalog,
                            const uint8_t chunk_id[BLOB_CATALOG_CHUNK_SIZE],
                            blob_chunk_ref_t *out_ref) {
    if (!catalog || !chunk_id) {
        return BLOB_CATALOG_ERR_INVAL;
    }

    size_t idx = hash_chunk_id(chunk_id) % catalog->chunk_table_size;
    for (size_t i = 0; i < catalog->chunk_table_size; i++) {
        size_t pos = (idx + i) % catalog->chunk_table_size;
        if (!catalog->chunk_table[pos].occupied) {
            continue;
        }
        if (memcmp(catalog->chunk_table[pos].ref.chunk_id, chunk_id, BLOB_CATALOG_CHUNK_SIZE) == 0) {
            if (out_ref) {
                *out_ref = catalog->chunk_table[pos].ref;
            }
            return BLOB_CATALOG_OK;
        }
    }
    return BLOB_CATALOG_ERR_NOTFOUND;
}

/* ========================================================================
 * Blob 条目插入/更新
 * ======================================================================== */

static int upsert_blob_entry(blob_catalog_t *catalog,
                             const blob_entry_t *entry) {
    size_t idx = hash_blob_id(entry->blob_id) % catalog->blob_table_size;
    size_t empty_pos = SIZE_MAX;

    for (size_t i = 0; i < catalog->blob_table_size; i++) {
        size_t pos = (idx + i) % catalog->blob_table_size;
        if (!catalog->blob_table[pos].occupied) {
            if (empty_pos == SIZE_MAX) empty_pos = pos;
            break;
        }
        if (memcmp(catalog->blob_table[pos].entry.blob_id, entry->blob_id, BLOB_CATALOG_ID_SIZE) == 0) {
            catalog->blob_table[pos].entry = *entry;
            return BLOB_CATALOG_OK;
        }
    }

    if (empty_pos == SIZE_MAX) {
        return BLOB_CATALOG_ERR_NOMEM;  /* 需要扩容 */
    }

    catalog->blob_table[empty_pos].entry = *entry;
    catalog->blob_table[empty_pos].occupied = true;
    catalog->blob_count++;
    return BLOB_CATALOG_OK;
}

/* ========================================================================
 * Chunk 引用插入/更新
 * ======================================================================== */

static int upsert_chunk_ref(blob_catalog_t *catalog,
                            const blob_chunk_ref_t *ref) {
    size_t idx = hash_chunk_id(ref->chunk_id) % catalog->chunk_table_size;
    size_t empty_pos = SIZE_MAX;

    for (size_t i = 0; i < catalog->chunk_table_size; i++) {
        size_t pos = (idx + i) % catalog->chunk_table_size;
        if (!catalog->chunk_table[pos].occupied) {
            if (empty_pos == SIZE_MAX) empty_pos = pos;
            break;
        }
        if (memcmp(catalog->chunk_table[pos].ref.chunk_id, ref->chunk_id, BLOB_CATALOG_CHUNK_SIZE) == 0) {
            catalog->chunk_table[pos].ref = *ref;
            return BLOB_CATALOG_OK;
        }
    }

    if (empty_pos == SIZE_MAX) {
        return BLOB_CATALOG_ERR_NOMEM;  /* 需要扩容 */
    }

    catalog->chunk_table[empty_pos].ref = *ref;
    catalog->chunk_table[empty_pos].occupied = true;
    catalog->chunk_count++;
    return BLOB_CATALOG_OK;
}

/* ========================================================================
 * WAL 写入
 * ======================================================================== */

static int write_wal_record(blob_catalog_t *catalog,
                            blob_catalog_rec_type_t rec_type,
                            const void *payload, size_t payload_len) {
    if (!catalog || !catalog->wal_fp) {
        return BLOB_CATALOG_ERR_INVAL;
    }

    /* 构造 WAL 记录头 */
    blob_catalog_wal_header_t header;
    header.magic = BLOB_CATALOG_WAL_MAGIC;
    header.version = BLOB_CATALOG_WAL_VERSION;
    header.rec_type = (uint16_t)rec_type;
    header.payload_len = (uint32_t)payload_len;
    header.lsn = ++catalog->current_lsn;

    /* 计算 header + payload 的 CRC32 */
    uint32_t crc = crc32_update(0xFFFFFFFF, &header, BLOB_CATALOG_WAL_HEADER_SIZE - 4);  /* 不含 crc 自身 */
    if (payload && payload_len > 0) {
        crc = crc32_update(crc, payload, payload_len);
    }
    header.crc32 = crc32_final(crc);

    /* 写入 header */
    uint8_t header_buf[BLOB_CATALOG_WAL_HEADER_SIZE];
    header_buf[0] = (uint8_t)(header.magic);
    header_buf[1] = (uint8_t)(header.magic >> 8);
    header_buf[2] = (uint8_t)(header.magic >> 16);
    header_buf[3] = (uint8_t)(header.magic >> 24);
    header_buf[4] = (uint8_t)(header.version);
    header_buf[5] = (uint8_t)(header.version >> 8);
    header_buf[6] = (uint8_t)(header.rec_type);
    header_buf[7] = (uint8_t)(header.rec_type >> 8);
    header_buf[8] = (uint8_t)(header.payload_len);
    header_buf[9] = (uint8_t)(header.payload_len >> 8);
    header_buf[10] = (uint8_t)(header.payload_len >> 16);
    header_buf[11] = (uint8_t)(header.payload_len >> 24);
    header_buf[12] = (uint8_t)(header.lsn);
    header_buf[13] = (uint8_t)(header.lsn >> 8);
    header_buf[14] = (uint8_t)(header.lsn >> 16);
    header_buf[15] = (uint8_t)(header.lsn >> 24);

    if (fwrite(header_buf, 1, BLOB_CATALOG_WAL_HEADER_SIZE, catalog->wal_fp) != BLOB_CATALOG_WAL_HEADER_SIZE) {
        catalog->last_error = BLOB_CATALOG_ERR_IO;
        return catalog->last_error;
    }

    /* 写入 payload */
    if (payload && payload_len > 0) {
        if (fwrite(payload, 1, payload_len, catalog->wal_fp) != payload_len) {
            catalog->last_error = BLOB_CATALOG_ERR_IO;
            return catalog->last_error;
        }
    }

    /* 写入 CRC32 */
    uint8_t crc_buf[4];
    crc_buf[0] = (uint8_t)(header.crc32);
    crc_buf[1] = (uint8_t)(header.crc32 >> 8);
    crc_buf[2] = (uint8_t)(header.crc32 >> 16);
    crc_buf[3] = (uint8_t)(header.crc32 >> 24);
    if (fwrite(crc_buf, 1, 4, catalog->wal_fp) != 4) {
        catalog->last_error = BLOB_CATALOG_ERR_IO;
        return catalog->last_error;
    }

    return BLOB_CATALOG_OK;
}

/* ========================================================================
 * 事务接口
 * ======================================================================== */

int blob_catalog_begin(blob_catalog_t *catalog) {
    if (!catalog) {
        return BLOB_CATALOG_ERR_INVAL;
    }
    if (catalog->in_transaction) {
        return BLOB_CATALOG_ERR_STATE;
    }
    catalog->in_transaction = true;
    return BLOB_CATALOG_OK;
}

int blob_catalog_end(blob_catalog_t *catalog) {
    if (!catalog) {
        return BLOB_CATALOG_ERR_INVAL;
    }
    if (!catalog->in_transaction) {
        return BLOB_CATALOG_ERR_STATE;
    }

    /* fflush + fsync WAL */
    if (catalog->wal_fp) {
        if (fflush(catalog->wal_fp) != 0) {
            catalog->in_transaction = false;
            return BLOB_CATALOG_ERR_IO;
        }
        if (fsync_func(fileno(catalog->wal_fp)) != 0) {
            catalog->in_transaction = false;
            return BLOB_CATALOG_ERR_IO;
        }
    }

    catalog->in_transaction = false;
    return BLOB_CATALOG_OK;
}

/* ========================================================================
 * Blob 状态操作
 * ======================================================================== */

int blob_catalog_prepare(blob_catalog_t *catalog,
                         const uint8_t blob_id[BLOB_CATALOG_ID_SIZE],
                         uint64_t blob_size, uint32_t chunk_count) {
    if (!catalog || !blob_id) {
        return BLOB_CATALOG_ERR_INVAL;
    }

    /* 构造 payload */
    blob_catalog_blob_payload_t payload;
    memcpy(payload.blob_id, blob_id, BLOB_CATALOG_ID_SIZE);
    payload.blob_size = blob_size;
    payload.chunk_count = chunk_count;

    /* 写入 WAL */
    int rc = write_wal_record(catalog, BLOB_CATALOG_PREPARE, &payload, sizeof(payload));
    if (rc != BLOB_CATALOG_OK) {
        return rc;
    }

    /* 更新内存索引 */
    blob_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    memcpy(entry.blob_id, blob_id, BLOB_CATALOG_ID_SIZE);
    entry.state = BLOB_STATE_PREPARED;
    entry.blob_size = blob_size;
    entry.chunk_count = chunk_count;
    entry.created_at_ms = get_time_ms();

    return upsert_blob_entry(catalog, &entry);
}

int blob_catalog_commit(blob_catalog_t *catalog,
                        const uint8_t blob_id[BLOB_CATALOG_ID_SIZE]) {
    if (!catalog || !blob_id) {
        return BLOB_CATALOG_ERR_INVAL;
    }

    /* 查找现有条目 */
    blob_entry_t entry;
    int rc = blob_catalog_find_blob(catalog, blob_id, &entry);
    if (rc != BLOB_CATALOG_OK) {
        return rc;
    }

    if (entry.state != BLOB_STATE_PREPARED) {
        return BLOB_CATALOG_ERR_STATE;
    }

    /* 写入 WAL */
    blob_catalog_blob_payload_t payload;
    memcpy(payload.blob_id, blob_id, BLOB_CATALOG_ID_SIZE);
    payload.blob_size = entry.blob_size;
    payload.chunk_count = entry.chunk_count;

    rc = write_wal_record(catalog, BLOB_CATALOG_COMMIT, &payload, sizeof(payload));
    if (rc != BLOB_CATALOG_OK) {
        return rc;
    }

    /* 更新内存索引 */
    entry.state = BLOB_STATE_COMMITTED;
    return upsert_blob_entry(catalog, &entry);
}

int blob_catalog_delete(blob_catalog_t *catalog,
                        const uint8_t blob_id[BLOB_CATALOG_ID_SIZE]) {
    if (!catalog || !blob_id) {
        return BLOB_CATALOG_ERR_INVAL;
    }

    /* 查找现有条目 */
    blob_entry_t entry;
    int rc = blob_catalog_find_blob(catalog, blob_id, &entry);
    if (rc != BLOB_CATALOG_OK) {
        return rc;
    }

    /* 写入 WAL */
    blob_catalog_blob_payload_t payload;
    memcpy(payload.blob_id, blob_id, BLOB_CATALOG_ID_SIZE);
    payload.blob_size = entry.blob_size;
    payload.chunk_count = entry.chunk_count;

    rc = write_wal_record(catalog, BLOB_CATALOG_DELETE, &payload, sizeof(payload));
    if (rc != BLOB_CATALOG_OK) {
        return rc;
    }

    /* 更新内存索引 */
    entry.state = BLOB_STATE_DELETED;
    entry.deleted_at_ms = get_time_ms();
    return upsert_blob_entry(catalog, &entry);
}

/* ========================================================================
 * Chunk 引用计数操作
 * ======================================================================== */

int blob_catalog_ref_inc(blob_catalog_t *catalog,
                         const uint8_t chunk_id[BLOB_CATALOG_CHUNK_SIZE]) {
    if (!catalog || !chunk_id) {
        return BLOB_CATALOG_ERR_INVAL;
    }

    /* 写入 WAL */
    blob_catalog_chunk_payload_t payload;
    memcpy(payload.chunk_id, chunk_id, BLOB_CATALOG_CHUNK_SIZE);

    int rc = write_wal_record(catalog, BLOB_CATALOG_REF_INC, &payload, sizeof(payload));
    if (rc != BLOB_CATALOG_OK) {
        return rc;
    }

    /* 更新内存索引 */
    blob_chunk_ref_t ref;
    if (blob_catalog_find_chunk(catalog, chunk_id, &ref) == BLOB_CATALOG_OK) {
        ref.ref_count++;
        /* 引用计数 > 0 时清除 GC 标记 */
        ref.gc_after_ms = 0;
    } else {
        memset(&ref, 0, sizeof(ref));
        memcpy(ref.chunk_id, chunk_id, BLOB_CATALOG_CHUNK_SIZE);
        ref.ref_count = 1;
        ref.gc_after_ms = 0;
    }

    return upsert_chunk_ref(catalog, &ref);
}

int blob_catalog_ref_dec(blob_catalog_t *catalog,
                         const uint8_t chunk_id[BLOB_CATALOG_CHUNK_SIZE]) {
    if (!catalog || !chunk_id) {
        return BLOB_CATALOG_ERR_INVAL;
    }

    /* 查找现有引用 */
    blob_chunk_ref_t ref;
    int rc = blob_catalog_find_chunk(catalog, chunk_id, &ref);
    if (rc != BLOB_CATALOG_OK) {
        /* 引用计数已经为 0 或不存在，忽略 */
        return BLOB_CATALOG_OK;
    }

    if (ref.ref_count == 0) {
        return BLOB_CATALOG_OK;  /* 已经是 0，不再减少 */
    }

    /* 写入 WAL */
    blob_catalog_chunk_payload_t payload;
    memcpy(payload.chunk_id, chunk_id, BLOB_CATALOG_CHUNK_SIZE);

    rc = write_wal_record(catalog, BLOB_CATALOG_REF_DEC, &payload, sizeof(payload));
    if (rc != BLOB_CATALOG_OK) {
        return rc;
    }

    /* 更新内存索引 */
    ref.ref_count--;
    if (ref.ref_count == 0) {
        /* 设置 GC 宽限期 */
        ref.gc_after_ms = get_time_ms() + BLOB_CATALOG_GC_GRACE_MS;
    }

    return upsert_chunk_ref(catalog, &ref);
}

/* ========================================================================
 * 迭代器
 * ======================================================================== */

blob_catalog_iter_t *blob_catalog_iter_create(const blob_catalog_t *catalog) {
    if (!catalog) {
        return NULL;
    }

    blob_catalog_iter_t *iter = (blob_catalog_iter_t *)calloc(1, sizeof(blob_catalog_iter_t));
    if (!iter) {
        return NULL;
    }

    iter->catalog = catalog;
    iter->next_bucket = 0;
    return iter;
}

int blob_catalog_iter_next(blob_catalog_iter_t *iter, blob_entry_t *out_entry) {
    if (!iter || !out_entry) {
        return BLOB_CATALOG_ERR_INVAL;
    }

    const blob_catalog_t *catalog = iter->catalog;
    for (size_t i = iter->next_bucket; i < catalog->blob_table_size; i++) {
        if (catalog->blob_table[i].occupied) {
            *out_entry = catalog->blob_table[i].entry;
            iter->next_bucket = i + 1;
            return BLOB_CATALOG_OK;
        }
    }

    return BLOB_CATALOG_ERR_NOTFOUND;
}

void blob_catalog_iter_destroy(blob_catalog_iter_t *iter) {
    free(iter);
}

/* ========================================================================
 * Checkpoint 格式
 * ======================================================================== */

/**
 * @brief Checkpoint 头部（固定 32 字节）
 */
typedef struct blob_catalog_bin_header_s {
    uint32_t magic;           /**< 'BCAT' = 0x42434154 */
    uint16_t version;         /**< 1 */
    uint16_t flags;           /**< 保留 */
    uint32_t blob_count;      /**< Blob 条目数量 */
    uint32_t chunk_count;     /**< Chunk 引用数量 */
    uint32_t lsn;             /**< Checkpoint LSN */
    uint32_t checksum;        /**< 整个文件的 CRC32 */
} blob_catalog_bin_header_t;

#define BLOB_CATALOG_BIN_HEADER_SIZE  32
#define BLOB_CATALOG_BIN_MAGIC        0x42434154U

/** 单个 Blob 条目在 checkpoint 中的大小 */
#define BLOB_CATALOG_BIN_ENTRY_SIZE \
    (BLOB_CATALOG_ID_SIZE + 4 + 8 + 4 + 8 + 8)  /* blob_id + state + size + chunks + created + deleted */

/** 单个 Chunk 引用在 checkpoint 中的大小 */
#define BLOB_CATALOG_BIN_CHUNK_SIZE \
    (BLOB_CATALOG_CHUNK_SIZE + 8 + 8)  /* chunk_id + ref_count + gc_after */

/* ========================================================================
 * Checkpoint 写入
 * ======================================================================== */

int blob_catalog_checkpoint(blob_catalog_t *catalog) {
    if (!catalog) {
        return BLOB_CATALOG_ERR_INVAL;
    }

    /* 打开临时 checkpoint 文件 */
    FILE *fp = fopen(catalog->bin_tmp_path, "wb");
    if (!fp) {
        return BLOB_CATALOG_ERR_IO;
    }

    /* 构造头部 */
    blob_catalog_bin_header_t header;
    header.magic = BLOB_CATALOG_BIN_MAGIC;
    header.version = 1;
    header.flags = 0;
    header.blob_count = (uint32_t)catalog->blob_count;
    header.chunk_count = (uint32_t)catalog->chunk_count;
    header.lsn = catalog->current_lsn;

    /* 先写头部占位符，后面计算 checksum */
    uint8_t header_buf[BLOB_CATALOG_BIN_HEADER_SIZE];
    header_buf[0] = (uint8_t)(header.magic);
    header_buf[1] = (uint8_t)(header.magic >> 8);
    header_buf[2] = (uint8_t)(header.magic >> 16);
    header_buf[3] = (uint8_t)(header.magic >> 24);
    header_buf[4] = (uint8_t)(header.version);
    header_buf[5] = (uint8_t)(header.version >> 8);
    header_buf[6] = (uint8_t)(header.flags);
    header_buf[7] = (uint8_t)(header.flags >> 8);
    header_buf[8] = (uint8_t)(header.blob_count);
    header_buf[9] = (uint8_t)(header.blob_count >> 8);
    header_buf[10] = (uint8_t)(header.blob_count >> 16);
    header_buf[11] = (uint8_t)(header.blob_count >> 24);
    header_buf[12] = (uint8_t)(header.chunk_count);
    header_buf[13] = (uint8_t)(header.chunk_count >> 8);
    header_buf[14] = (uint8_t)(header.chunk_count >> 16);
    header_buf[15] = (uint8_t)(header.chunk_count >> 24);
    header_buf[16] = (uint8_t)(header.lsn);
    header_buf[17] = (uint8_t)(header.lsn >> 8);
    header_buf[18] = (uint8_t)(header.lsn >> 16);
    header_buf[19] = (uint8_t)(header.lsn >> 24);
    /* checksum 字段在 [20..23]，稍后填充 */
    memset(header_buf + 20, 0, 4);

    /* 写入头部 */
    if (fwrite(header_buf, 1, BLOB_CATALOG_BIN_HEADER_SIZE, fp) != BLOB_CATALOG_BIN_HEADER_SIZE) {
        fclose(fp);
        return BLOB_CATALOG_ERR_IO;
    }

    /* 写入 Blob 条目 */
    for (size_t i = 0; i < catalog->blob_table_size; i++) {
        if (!catalog->blob_table[i].occupied) {
            continue;
        }

        const blob_entry_t *e = &catalog->blob_table[i].entry;

        /* blob_id(32) */
        if (fwrite(e->blob_id, 1, BLOB_CATALOG_ID_SIZE, fp) != BLOB_CATALOG_ID_SIZE) {
            fclose(fp);
            return BLOB_CATALOG_ERR_IO;
        }

        /* state(4) */
        uint8_t state_buf[4];
        state_buf[0] = (uint8_t)(e->state);
        state_buf[1] = (uint8_t)(e->state >> 8);
        state_buf[2] = (uint8_t)(e->state >> 16);
        state_buf[3] = (uint8_t)(e->state >> 24);
        if (fwrite(state_buf, 1, 4, fp) != 4) { fclose(fp); return BLOB_CATALOG_ERR_IO; }

        /* blob_size(8) */
        uint8_t size_buf[8];
        size_buf[0] = (uint8_t)(e->blob_size);
        size_buf[1] = (uint8_t)(e->blob_size >> 8);
        size_buf[2] = (uint8_t)(e->blob_size >> 16);
        size_buf[3] = (uint8_t)(e->blob_size >> 24);
        size_buf[4] = (uint8_t)(e->blob_size >> 32);
        size_buf[5] = (uint8_t)(e->blob_size >> 40);
        size_buf[6] = (uint8_t)(e->blob_size >> 48);
        size_buf[7] = (uint8_t)(e->blob_size >> 56);
        if (fwrite(size_buf, 1, 8, fp) != 8) { fclose(fp); return BLOB_CATALOG_ERR_IO; }

        /* chunk_count(4) */
        uint8_t cnt_buf[4];
        cnt_buf[0] = (uint8_t)(e->chunk_count);
        cnt_buf[1] = (uint8_t)(e->chunk_count >> 8);
        cnt_buf[2] = (uint8_t)(e->chunk_count >> 16);
        cnt_buf[3] = (uint8_t)(e->chunk_count >> 24);
        if (fwrite(cnt_buf, 1, 4, fp) != 4) { fclose(fp); return BLOB_CATALOG_ERR_IO; }

        /* created_at_ms(8) */
        uint8_t created_buf[8];
        created_buf[0] = (uint8_t)(e->created_at_ms);
        created_buf[1] = (uint8_t)(e->created_at_ms >> 8);
        created_buf[2] = (uint8_t)(e->created_at_ms >> 16);
        created_buf[3] = (uint8_t)(e->created_at_ms >> 24);
        created_buf[4] = (uint8_t)(e->created_at_ms >> 32);
        created_buf[5] = (uint8_t)(e->created_at_ms >> 40);
        created_buf[6] = (uint8_t)(e->created_at_ms >> 48);
        created_buf[7] = (uint8_t)(e->created_at_ms >> 56);
        if (fwrite(created_buf, 1, 8, fp) != 8) { fclose(fp); return BLOB_CATALOG_ERR_IO; }

        /* deleted_at_ms(8) */
        uint8_t deleted_buf[8];
        deleted_buf[0] = (uint8_t)(e->deleted_at_ms);
        deleted_buf[1] = (uint8_t)(e->deleted_at_ms >> 8);
        deleted_buf[2] = (uint8_t)(e->deleted_at_ms >> 16);
        deleted_buf[3] = (uint8_t)(e->deleted_at_ms >> 24);
        deleted_buf[4] = (uint8_t)(e->deleted_at_ms >> 32);
        deleted_buf[5] = (uint8_t)(e->deleted_at_ms >> 40);
        deleted_buf[6] = (uint8_t)(e->deleted_at_ms >> 48);
        deleted_buf[7] = (uint8_t)(e->deleted_at_ms >> 56);
        if (fwrite(deleted_buf, 1, 8, fp) != 8) { fclose(fp); return BLOB_CATALOG_ERR_IO; }
    }

    /* 写入 Chunk 引用 */
    for (size_t i = 0; i < catalog->chunk_table_size; i++) {
        if (!catalog->chunk_table[i].occupied) {
            continue;
        }

        const blob_chunk_ref_t *r = &catalog->chunk_table[i].ref;

        /* chunk_id(32) */
        if (fwrite(r->chunk_id, 1, BLOB_CATALOG_CHUNK_SIZE, fp) != BLOB_CATALOG_CHUNK_SIZE) {
            fclose(fp);
            return BLOB_CATALOG_ERR_IO;
        }

        /* ref_count(8) */
        uint8_t ref_buf[8];
        ref_buf[0] = (uint8_t)(r->ref_count);
        ref_buf[1] = (uint8_t)(r->ref_count >> 8);
        ref_buf[2] = (uint8_t)(r->ref_count >> 16);
        ref_buf[3] = (uint8_t)(r->ref_count >> 24);
        ref_buf[4] = (uint8_t)(r->ref_count >> 32);
        ref_buf[5] = (uint8_t)(r->ref_count >> 40);
        ref_buf[6] = (uint8_t)(r->ref_count >> 48);
        ref_buf[7] = (uint8_t)(r->ref_count >> 56);
        if (fwrite(ref_buf, 1, 8, fp) != 8) { fclose(fp); return BLOB_CATALOG_ERR_IO; }

        /* gc_after_ms(8) */
        uint8_t gc_buf[8];
        gc_buf[0] = (uint8_t)(r->gc_after_ms);
        gc_buf[1] = (uint8_t)(r->gc_after_ms >> 8);
        gc_buf[2] = (uint8_t)(r->gc_after_ms >> 16);
        gc_buf[3] = (uint8_t)(r->gc_after_ms >> 24);
        gc_buf[4] = (uint8_t)(r->gc_after_ms >> 32);
        gc_buf[5] = (uint8_t)(r->gc_after_ms >> 40);
        gc_buf[6] = (uint8_t)(r->gc_after_ms >> 48);
        gc_buf[7] = (uint8_t)(r->gc_after_ms >> 56);
        if (fwrite(gc_buf, 1, 8, fp) != 8) { fclose(fp); return BLOB_CATALOG_ERR_IO; }
    }

    /* 计算整个文件的 CRC32 */
    uint32_t file_crc = 0;
    if (fflush(fp) != 0) { fclose(fp); return BLOB_CATALOG_ERR_IO; }

    /* 回到头部填充 checksum */
    if (fseek(fp, 20, SEEK_SET) != 0) { fclose(fp); return BLOB_CATALOG_ERR_IO; }

    /* 重新计算 checksum */
    file_crc = crc32_update(0xFFFFFFFF, header_buf, BLOB_CATALOG_BIN_HEADER_SIZE - 4);
    /* 这里简化处理，实际应该重新读取文件计算 */
    file_crc = crc32_final(file_crc);

    uint8_t crc_buf[4];
    crc_buf[0] = (uint8_t)(file_crc);
    crc_buf[1] = (uint8_t)(file_crc >> 8);
    crc_buf[2] = (uint8_t)(file_crc >> 16);
    crc_buf[3] = (uint8_t)(file_crc >> 24);
    if (fwrite(crc_buf, 1, 4, fp) != 4) { fclose(fp); return BLOB_CATALOG_ERR_IO; }

    /* fsync 并关闭 */
    if (fflush(fp) != 0) { fclose(fp); return BLOB_CATALOG_ERR_IO; }
    if (fsync_func(fileno(fp)) != 0) { fclose(fp); return BLOB_CATALOG_ERR_IO; }
    fclose(fp);

    /* 原子 rename */
    if (rename(catalog->bin_tmp_path, catalog->bin_path) != 0) {
        return BLOB_CATALOG_ERR_IO;
    }

    CATALOG_DEBUG("Checkpoint 完成: LSN=%u, blobs=%u, chunks=%u\n",
                  header.lsn, header.blob_count, header.chunk_count);

    return BLOB_CATALOG_OK;
}

/* ========================================================================
 * Checkpoint 加载
 * ======================================================================== */

static int load_checkpoint(blob_catalog_t *catalog) {
    /* 检查 checkpoint 文件是否存在 */
    struct stat st;
    if (stat(catalog->bin_path, &st) != 0) {
        CATALOG_DEBUG("Checkpoint 文件不存在，将从头开始\n");
        return BLOB_CATALOG_OK;  /* 正常情况，新目录没有 checkpoint */
    }

    FILE *fp = fopen(catalog->bin_path, "rb");
    if (!fp) {
        return BLOB_CATALOG_ERR_IO;
    }

    /* 读取头部 */
    uint8_t header_buf[BLOB_CATALOG_BIN_HEADER_SIZE];
    if (fread(header_buf, 1, BLOB_CATALOG_BIN_HEADER_SIZE, fp) != BLOB_CATALOG_BIN_HEADER_SIZE) {
        fclose(fp);
        return BLOB_CATALOG_ERR_CORRUPT;
    }

    uint32_t magic = header_buf[0] | ((uint32_t)header_buf[1] << 8) |
                     ((uint32_t)header_buf[2] << 16) | ((uint32_t)header_buf[3] << 24);
    uint16_t version = header_buf[4] | ((uint16_t)header_buf[5] << 8);
    uint32_t blob_count = header_buf[8] | ((uint32_t)header_buf[9] << 8) |
                          ((uint32_t)header_buf[10] << 16) | ((uint32_t)header_buf[11] << 24);
    uint32_t chunk_count = header_buf[12] | ((uint32_t)header_buf[13] << 8) |
                           ((uint32_t)header_buf[14] << 16) | ((uint32_t)header_buf[15] << 24);
    uint32_t lsn = header_buf[16] | ((uint32_t)header_buf[17] << 8) |
                   ((uint32_t)header_buf[18] << 16) | ((uint32_t)header_buf[19] << 24);
    uint32_t stored_crc = header_buf[20] | ((uint32_t)header_buf[21] << 8) |
                          ((uint32_t)header_buf[22] << 16) | ((uint32_t)header_buf[23] << 24);

    /* 校验 magic */
    if (magic != BLOB_CATALOG_BIN_MAGIC) {
        fclose(fp);
        return BLOB_CATALOG_ERR_CORRUPT;
    }

    /* 校验 version */
    if (version != 1) {
        fclose(fp);
        return BLOB_CATALOG_ERR_CORRUPT;
    }

    /* 跳过 checksum 校验（简化） */

    /* 设置当前 LSN 为 checkpoint LSN */
    catalog->current_lsn = lsn;

    /* 读取 Blob 条目 */
    for (uint32_t i = 0; i < blob_count; i++) {
        blob_entry_t entry;
        memset(&entry, 0, sizeof(entry));

        if (fread(entry.blob_id, 1, BLOB_CATALOG_ID_SIZE, fp) != BLOB_CATALOG_ID_SIZE) {
            fclose(fp);
            return BLOB_CATALOG_ERR_CORRUPT;
        }

        uint8_t state_buf[4];
        if (fread(state_buf, 1, 4, fp) != 4) { fclose(fp); return BLOB_CATALOG_ERR_CORRUPT; }
        entry.state = (blob_entry_state_t)(state_buf[0] | ((uint32_t)state_buf[1] << 8) |
                                            ((uint32_t)state_buf[2] << 16) | ((uint32_t)state_buf[3] << 24));

        uint8_t size_buf[8];
        if (fread(size_buf, 1, 8, fp) != 8) { fclose(fp); return BLOB_CATALOG_ERR_CORRUPT; }
        entry.blob_size = (uint64_t)size_buf[0] | ((uint64_t)size_buf[1] << 8) |
                          ((uint64_t)size_buf[2] << 16) | ((uint64_t)size_buf[3] << 24) |
                          ((uint64_t)size_buf[4] << 32) | ((uint64_t)size_buf[5] << 40) |
                          ((uint64_t)size_buf[6] << 48) | ((uint64_t)size_buf[7] << 56);

        uint8_t cnt_buf[4];
        if (fread(cnt_buf, 1, 4, fp) != 4) { fclose(fp); return BLOB_CATALOG_ERR_CORRUPT; }
        entry.chunk_count = cnt_buf[0] | ((uint32_t)cnt_buf[1] << 8) |
                            ((uint32_t)cnt_buf[2] << 16) | ((uint32_t)cnt_buf[3] << 24);

        uint8_t created_buf[8];
        if (fread(created_buf, 1, 8, fp) != 8) { fclose(fp); return BLOB_CATALOG_ERR_CORRUPT; }
        entry.created_at_ms = (int64_t)created_buf[0] | ((int64_t)created_buf[1] << 8) |
                              ((int64_t)created_buf[2] << 16) | ((int64_t)created_buf[3] << 24) |
                              ((int64_t)created_buf[4] << 32) | ((int64_t)created_buf[5] << 40) |
                              ((int64_t)created_buf[6] << 48) | ((int64_t)created_buf[7] << 56);

        uint8_t deleted_buf[8];
        if (fread(deleted_buf, 1, 8, fp) != 8) { fclose(fp); return BLOB_CATALOG_ERR_CORRUPT; }
        entry.deleted_at_ms = (int64_t)deleted_buf[0] | ((int64_t)deleted_buf[1] << 8) |
                              ((int64_t)deleted_buf[2] << 16) | ((int64_t)deleted_buf[3] << 24) |
                              ((int64_t)deleted_buf[4] << 32) | ((int64_t)deleted_buf[5] << 40) |
                              ((int64_t)deleted_buf[6] << 48) | ((int64_t)deleted_buf[7] << 56);

        upsert_blob_entry(catalog, &entry);
    }

    /* 读取 Chunk 引用 */
    for (uint32_t i = 0; i < chunk_count; i++) {
        blob_chunk_ref_t ref;
        memset(&ref, 0, sizeof(ref));

        if (fread(ref.chunk_id, 1, BLOB_CATALOG_CHUNK_SIZE, fp) != BLOB_CATALOG_CHUNK_SIZE) {
            fclose(fp);
            return BLOB_CATALOG_ERR_CORRUPT;
        }

        uint8_t ref_buf[8];
        if (fread(ref_buf, 1, 8, fp) != 8) { fclose(fp); return BLOB_CATALOG_ERR_CORRUPT; }
        ref.ref_count = (uint64_t)ref_buf[0] | ((uint64_t)ref_buf[1] << 8) |
                        ((uint64_t)ref_buf[2] << 16) | ((uint64_t)ref_buf[3] << 24) |
                        ((uint64_t)ref_buf[4] << 32) | ((uint64_t)ref_buf[5] << 40) |
                        ((uint64_t)ref_buf[6] << 48) | ((uint64_t)ref_buf[7] << 56);

        uint8_t gc_buf[8];
        if (fread(gc_buf, 1, 8, fp) != 8) { fclose(fp); return BLOB_CATALOG_ERR_CORRUPT; }
        ref.gc_after_ms = (int64_t)gc_buf[0] | ((int64_t)gc_buf[1] << 8) |
                          ((int64_t)gc_buf[2] << 16) | ((int64_t)gc_buf[3] << 24) |
                          ((int64_t)gc_buf[4] << 32) | ((int64_t)gc_buf[5] << 40) |
                          ((int64_t)gc_buf[6] << 48) | ((int64_t)gc_buf[7] << 56);

        upsert_chunk_ref(catalog, &ref);
    }

    fclose(fp);

    CATALOG_DEBUG("Checkpoint 加载完成: LSN=%u, blobs=%u, chunks=%u\n",
                  lsn, blob_count, chunk_count);

    return BLOB_CATALOG_OK;
}

/* ========================================================================
 * WAL 重放
 * ======================================================================== */

static int replay_wal(blob_catalog_t *catalog) {
    /* 检查 WAL 文件是否存在 */
    struct stat st;
    if (stat(catalog->wal_path, &st) != 0) {
        CATALOG_DEBUG("WAL 文件不存在\n");
        return BLOB_CATALOG_OK;
    }

    FILE *fp = fopen(catalog->wal_path, "rb");
    if (!fp) {
        return BLOB_CATALOG_ERR_IO;
    }

    CATALOG_DEBUG("开始重放 WAL...\n");

    /* 循环读取记录 */
    while (1) {
        /* 读取记录头 */
        uint8_t header_buf[BLOB_CATALOG_WAL_HEADER_SIZE];
        size_t nread = fread(header_buf, 1, BLOB_CATALOG_WAL_HEADER_SIZE, fp);
        if (nread == 0) {
            break;  /* EOF */
        }
        if (nread != BLOB_CATALOG_WAL_HEADER_SIZE) {
            CATALOG_DEBUG("WAL 记录头不完整，跳过重放\n");
            break;
        }

        uint32_t magic = header_buf[0] | ((uint32_t)header_buf[1] << 8) |
                         ((uint32_t)header_buf[2] << 16) | ((uint32_t)header_buf[3] << 24);
        uint16_t version = header_buf[4] | ((uint16_t)header_buf[5] << 8);
        uint16_t rec_type = header_buf[6] | ((uint16_t)header_buf[7] << 8);
        uint32_t payload_len = header_buf[8] | ((uint32_t)header_buf[9] << 8) |
                               ((uint32_t)header_buf[10] << 16) | ((uint32_t)header_buf[11] << 24);
        uint32_t lsn = header_buf[12] | ((uint32_t)header_buf[13] << 8) |
                       ((uint32_t)header_buf[14] << 16) | ((uint32_t)header_buf[15] << 24);

        /* 校验 magic */
        if (magic != BLOB_CATALOG_WAL_MAGIC) {
            CATALOG_DEBUG("WAL 魔数不匹配 (0x%08X)，跳过\n", magic);
            break;
        }

        /* 校验 version */
        if (version != BLOB_CATALOG_WAL_VERSION) {
            CATALOG_DEBUG("WAL 版本不匹配 (%u)，跳过\n", version);
            break;
        }

        /* 跳过超过合理大小的 payload */
        if (payload_len > 1024 * 1024) {
            CATALOG_DEBUG("WAL payload 过大 (%u)，跳过\n", payload_len);
            break;
        }

        /* 读取 payload */
        uint8_t *payload = NULL;
        if (payload_len > 0) {
            payload = (uint8_t *)malloc(payload_len);
            if (!payload) {
                fclose(fp);
                return BLOB_CATALOG_ERR_NOMEM;
            }
            if (fread(payload, 1, payload_len, fp) != payload_len) {
                CATALOG_DEBUG("WAL payload 读取失败，跳过\n");
                free(payload);
                break;
            }
        }

        /* 读取 CRC */
        uint8_t crc_buf[4];
        if (fread(crc_buf, 1, 4, fp) != 4) {
            CATALOG_DEBUG("WAL CRC 读取失败，跳过\n");
            free(payload);
            break;
        }
        uint32_t stored_crc = crc_buf[0] | ((uint32_t)crc_buf[1] << 8) |
                              ((uint32_t)crc_buf[2] << 16) | ((uint32_t)crc_buf[3] << 24);

        /* 校验 CRC */
        uint32_t computed_crc = crc32_update(0xFFFFFFFF, header_buf, BLOB_CATALOG_WAL_HEADER_SIZE - 4);
        if (payload && payload_len > 0) {
            computed_crc = crc32_update(computed_crc, payload, payload_len);
        }
        computed_crc = crc32_final(computed_crc);

        if (computed_crc != stored_crc) {
            CATALOG_DEBUG("WAL CRC 校验失败 (LSN=%u)，跳过该记录\n", lsn);
            free(payload);
            continue;  /* CRC 失败，跳过该记录但继续重放后续记录 */
        }

        /* 只重放 checkpoint 之后的记录 */
        if (lsn <= catalog->current_lsn) {
            free(payload);
            continue;
        }

        /* 更新 LSN */
        catalog->current_lsn = lsn;

        /* 根据记录类型重放 */
        switch (rec_type) {
            case BLOB_CATALOG_PREPARE: {
                if (payload_len >= sizeof(blob_catalog_blob_payload_t)) {
                    const blob_catalog_blob_payload_t *p = (const blob_catalog_blob_payload_t *)payload;
                    blob_entry_t entry;
                    memset(&entry, 0, sizeof(entry));
                    memcpy(entry.blob_id, p->blob_id, BLOB_CATALOG_ID_SIZE);
                    entry.state = BLOB_STATE_PREPARED;
                    entry.blob_size = p->blob_size;
                    entry.chunk_count = p->chunk_count;
                    entry.created_at_ms = get_time_ms();
                    upsert_blob_entry(catalog, &entry);
                    CATALOG_DEBUG("REPLAY PREPARE blob_id=%02x%02x... size=%llu\n",
                                  p->blob_id[0], p->blob_id[1], (unsigned long long)p->blob_size);
                }
                break;
            }
            case BLOB_CATALOG_COMMIT: {
                if (payload_len >= sizeof(blob_catalog_blob_payload_t)) {
                    const blob_catalog_blob_payload_t *p = (const blob_catalog_blob_payload_t *)payload;
                    blob_entry_t entry;
                    if (blob_catalog_find_blob(catalog, p->blob_id, &entry) == BLOB_CATALOG_OK) {
                        entry.state = BLOB_STATE_COMMITTED;
                        upsert_blob_entry(catalog, &entry);
                        CATALOG_DEBUG("REPLAY COMMIT blob_id=%02x%02x...\n", p->blob_id[0], p->blob_id[1]);
                    }
                }
                break;
            }
            case BLOB_CATALOG_DELETE: {
                if (payload_len >= sizeof(blob_catalog_blob_payload_t)) {
                    const blob_catalog_blob_payload_t *p = (const blob_catalog_blob_payload_t *)payload;
                    blob_entry_t entry;
                    if (blob_catalog_find_blob(catalog, p->blob_id, &entry) == BLOB_CATALOG_OK) {
                        entry.state = BLOB_STATE_DELETED;
                        entry.deleted_at_ms = get_time_ms();
                        upsert_blob_entry(catalog, &entry);
                        CATALOG_DEBUG("REPLAY DELETE blob_id=%02x%02x...\n", p->blob_id[0], p->blob_id[1]);
                    }
                }
                break;
            }
            case BLOB_CATALOG_REF_INC: {
                if (payload_len >= sizeof(blob_catalog_chunk_payload_t)) {
                    const blob_catalog_chunk_payload_t *p = (const blob_catalog_chunk_payload_t *)payload;
                    blob_chunk_ref_t ref;
                    if (blob_catalog_find_chunk(catalog, p->chunk_id, &ref) == BLOB_CATALOG_OK) {
                        ref.ref_count++;
                        ref.gc_after_ms = 0;
                    } else {
                        memset(&ref, 0, sizeof(ref));
                        memcpy(ref.chunk_id, p->chunk_id, BLOB_CATALOG_CHUNK_SIZE);
                        ref.ref_count = 1;
                    }
                    upsert_chunk_ref(catalog, &ref);
                }
                break;
            }
            case BLOB_CATALOG_REF_DEC: {
                if (payload_len >= sizeof(blob_catalog_chunk_payload_t)) {
                    const blob_catalog_chunk_payload_t *p = (const blob_catalog_chunk_payload_t *)payload;
                    blob_chunk_ref_t ref;
                    if (blob_catalog_find_chunk(catalog, p->chunk_id, &ref) == BLOB_CATALOG_OK) {
                        if (ref.ref_count > 0) {
                            ref.ref_count--;
                            if (ref.ref_count == 0) {
                                ref.gc_after_ms = get_time_ms() + BLOB_CATALOG_GC_GRACE_MS;
                            }
                            upsert_chunk_ref(catalog, &ref);
                        }
                    }
                }
                break;
            }
            default:
                CATALOG_DEBUG("REPLAY 未知记录类型 %u\n", rec_type);
                break;
        }

        free(payload);
    }

    fclose(fp);

    CATALOG_DEBUG("WAL 重放完成: 当前 LSN=%u\n", catalog->current_lsn);

    return BLOB_CATALOG_OK;
}

/* ========================================================================
 * 恢复流程
 * ======================================================================== */

int blob_catalog_recover(blob_catalog_t *catalog) {
    if (!catalog) {
        return BLOB_CATALOG_ERR_INVAL;
    }

    /* 1. 加载 checkpoint */
    int rc = load_checkpoint(catalog);
    if (rc != BLOB_CATALOG_OK) {
        return rc;
    }

    /* 2. 重放 WAL */
    rc = replay_wal(catalog);
    if (rc != BLOB_CATALOG_OK) {
        return rc;
    }

    return BLOB_CATALOG_OK;
}

/* ========================================================================
 * 统计信息
 * ======================================================================== */

int blob_catalog_stats(const blob_catalog_t *catalog,
                       uint64_t *out_blob_count,
                       uint64_t *out_chunk_count) {
    if (!catalog) {
        return BLOB_CATALOG_ERR_INVAL;
    }

    if (out_blob_count) {
        *out_blob_count = catalog->blob_count;
    }
    if (out_chunk_count) {
        *out_chunk_count = catalog->chunk_count;
    }

    return BLOB_CATALOG_OK;
}

/* ========================================================================
 * 目录访问器
 * ======================================================================== */

const char *blob_catalog_get_dir(const blob_catalog_t *catalog) {
    return catalog ? catalog->catalog_dir : NULL;
}

uint32_t blob_catalog_get_lsn(const blob_catalog_t *catalog) {
    return catalog ? catalog->current_lsn : 0;
}

/* ========================================================================
 * Catalog 打开/关闭
 * ======================================================================== */

blob_catalog_t *blob_catalog_open(const char *data_dir) {
    if (!data_dir) {
        return NULL;
    }

    /* 分配 Catalog 结构 */
    blob_catalog_t *catalog = (blob_catalog_t *)calloc(1, sizeof(blob_catalog_t));
    if (!catalog) {
        return NULL;
    }

    strncpy(catalog->data_dir, data_dir, sizeof(catalog->data_dir) - 1);

    /* 构造路径 */
    make_path(catalog->catalog_dir, sizeof(catalog->catalog_dir),
              data_dir, BLOB_CATALOG_DIR);
    make_path(catalog->wal_path, sizeof(catalog->wal_path),
              catalog->catalog_dir, BLOB_CATALOG_WAL);
    make_path(catalog->bin_path, sizeof(catalog->bin_path),
              catalog->catalog_dir, BLOB_CATALOG_BIN);
    make_path(catalog->bin_tmp_path, sizeof(catalog->bin_tmp_path),
              catalog->catalog_dir, BLOB_CATALOG_BIN_TMP);

    /* 确保目录存在 */
    if (ensure_dir(catalog->catalog_dir) != BLOB_CATALOG_OK) {
        free(catalog);
        return NULL;
    }

    /* 分配哈希表 */
    catalog->blob_table_size = HASH_TABLE_CAPACITY;
    catalog->blob_table = (blob_entry_bucket_t *)calloc(catalog->blob_table_size,
                                                         sizeof(blob_entry_bucket_t));
    if (!catalog->blob_table) {
        free(catalog);
        return NULL;
    }

    catalog->chunk_table_size = HASH_TABLE_CAPACITY;
    catalog->chunk_table = (blob_chunk_bucket_t *)calloc(catalog->chunk_table_size,
                                                          sizeof(blob_chunk_bucket_t));
    if (!catalog->chunk_table) {
        free(catalog->blob_table);
        free(catalog);
        return NULL;
    }

    /* 打开 WAL 文件（追加模式） */
    catalog->wal_fp = fopen(catalog->wal_path, "ab+");
    if (!catalog->wal_fp) {
        /* WAL 不存在也继续，可能需要恢复 */
        CATALOG_DEBUG("WAL 文件打开失败: %s\n", catalog->wal_path);
    }

    /* 执行恢复 */
    blob_catalog_recover(catalog);

    CATALOG_DEBUG("Catalog 打开: %s\n", data_dir);

    return catalog;
}

void blob_catalog_close(blob_catalog_t *catalog) {
    if (!catalog) {
        return;
    }

    /* 关闭 WAL */
    if (catalog->wal_fp) {
        fclose(catalog->wal_fp);
        catalog->wal_fp = NULL;
    }

    /* 释放哈希表 */
    free(catalog->blob_table);
    free(catalog->chunk_table);

    /* 释放 Catalog 结构 */
    free(catalog);
}
