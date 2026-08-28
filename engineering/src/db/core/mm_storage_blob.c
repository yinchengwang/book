/**
 * @file mm_storage_blob.c
 * @brief mm_storage 适配层 — Blob 接入（Task 8）
 *
 * 把 mm_record.h 中的 mm_storage_blob_put/get 与 Blob 引擎连接：
 *   - mm_storage_blob_put：登记 blob_id 与 collection 的绑定关系，
 *     持久化引用到 mm_storage 的辅助文件 collection.blob_refs 中。
 *   - mm_storage_blob_get：根据 blob_id 反查 Blob 引擎中保存的对象。
 *
 * 该实现采取最小可行策略：
 *   - collection 名映射到 data_dir 下的子目录；
 *   - Blob 引擎句柄按 collection 名懒加载并缓存到进程级注册表；
 *   - blob_id 引用持久化在 collection/blob_refs/<hex>.ref；
 *   - put/get 只操作引用文件，真实对象读取仍走 Blob Engine。
 */
#include "db/mm_record.h"
#include "db/blob_engine.h"
#include "db/core/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define mm_mkdir(path) _mkdir(path)
#else
#include <unistd.h>
#define mm_mkdir(path) mkdir(path, 0755)
#endif

/* mm_storage 默认根目录（可通过环境变量 MM_STORAGE_DATA_DIR 覆盖） */
#define MM_STORAGE_DEFAULT_ROOT "./data/mm_storage"

/* 简单哈希：用于 collection -> 子目录桶号（避免目录层级过深） */
static unsigned int mm_str_hash(const char *s) {
    unsigned int h = 2166136261u;
    if (!s) return 0;
    while (*s) {
        h ^= (unsigned char)*s++;
        h *= 16777619u;
    }
    return h;
}

/* 懒加载 + 缓存的 Blob 引擎句柄注册表（最多 64 个 collection） */
#define MM_BLOB_ENGINE_CACHE_SIZE 64

typedef struct mm_blob_engine_slot_s {
    char             name[128];     /**< collection 名 */
    char             data_dir[512]; /**< 实际 data_dir */
    blob_engine_t   *engine;        /**< 缓存的引擎句柄 */
} mm_blob_engine_slot_t;

static mm_blob_engine_slot_t g_blob_cache[MM_BLOB_ENGINE_CACHE_SIZE];
static bool                    g_blob_cache_inited = false;

/* ========================================================================
 * 内部辅助
 * ======================================================================== */

static const char *mm_storage_root(void) {
    const char *root = getenv("MM_STORAGE_DATA_DIR");
    return (root && *root) ? root : MM_STORAGE_DEFAULT_ROOT;
}

/**
 * @brief 计算 collection 的实际 data_dir
 *
 * 形如 <root>/blob/<bucket>/<collection>，bucket 是 16 进制哈希前缀。
 */
static int mm_compute_blob_dir(const char *collection,
                               char *out, size_t out_len) {
    if (!collection || !*collection) return -1;
    const char *root = mm_storage_root();
    unsigned int h = mm_str_hash(collection);
    int n = snprintf(out, out_len,
                     "%s/blob/%08x/%s", root, h, collection);
    return (n > 0 && (size_t)n < out_len) ? 0 : -1;
}

static void ensure_dir_recursive(const char *path) {
    char tmp[512];
    size_t len = strlen(path);
    if (len == 0 || len >= sizeof(tmp)) return;
    memcpy(tmp, path, len + 1);

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char sep = *p;
            *p = '\0';
            mm_mkdir(tmp);
            *p = sep;
        }
    }
    mm_mkdir(tmp);
}

/* 获取（或创建并缓存）指定 collection 对应的 Blob 引擎 */
static blob_engine_t *mm_get_or_create_engine(const char *collection) {
    if (!collection || !*collection) return NULL;

    if (!g_blob_cache_inited) {
        memset(g_blob_cache, 0, sizeof(g_blob_cache));
        g_blob_cache_inited = true;
    }

    /* 查找缓存 */
    for (size_t i = 0; i < MM_BLOB_ENGINE_CACHE_SIZE; i++) {
        if (g_blob_cache[i].engine &&
            strcmp(g_blob_cache[i].name, collection) == 0) {
            return g_blob_cache[i].engine;
        }
    }

    /* 计算 data_dir */
    char data_dir[512];
    if (mm_compute_blob_dir(collection, data_dir, sizeof(data_dir)) != 0) {
        return NULL;
    }

    /* 创建目录树 */
    ensure_dir_recursive(data_dir);

    /* 创建 Blob 引擎 */
    blob_engine_t *eng = blob_engine_create(data_dir);
    if (!eng) return NULL;

    /* 写入缓存槽 */
    for (size_t i = 0; i < MM_BLOB_ENGINE_CACHE_SIZE; i++) {
        if (!g_blob_cache[i].engine) {
            strncpy(g_blob_cache[i].name, collection,
                    sizeof(g_blob_cache[i].name) - 1);
            strncpy(g_blob_cache[i].data_dir, data_dir,
                    sizeof(g_blob_cache[i].data_dir) - 1);
            g_blob_cache[i].engine = eng;
            return eng;
        }
    }

    /* 缓存已满：返回新引擎（不缓存，让调用方管理） */
    return eng;
}

/* 将 32 字节 blob_id 转为 64 字符十六进制串 */
static void mm_blob_id_to_hex(const uint8_t blob_id[32], char hex_out[65]) {
    static const char hexc[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        hex_out[2 * i]     = hexc[blob_id[i] >> 4];
        hex_out[2 * i + 1] = hexc[blob_id[i] & 0x0F];
    }
    hex_out[64] = '\0';
}

/* ========================================================================
 * mm_storage_blob 公共 API
 * ======================================================================== */

int mm_storage_blob_put(const char *collection, const uint8_t blob_id[32]) {
    if (!collection || !blob_id) return -1;

    blob_engine_t *eng = mm_get_or_create_engine(collection);
    if (!eng) {
        LOG_WARN("mm_storage_blob_put: 引擎创建失败 collection=%s", collection);
        return -1;
    }

    /* 校验 blob 是否真的存在于 Blob 引擎的 Manifest 中 */
    size_t sz = 0;
    int rc = blob_stat(eng, blob_id, &sz);
    if (rc != 0) {
        LOG_WARN("mm_storage_blob_put: blob 不存在或不可读 collection=%s", collection);
        return -1;
    }

    /* 取得缓存中该引擎对应的真实 data_dir（缓存命中或新建槽） */
    const char *dd = NULL;
    for (size_t i = 0; i < MM_BLOB_ENGINE_CACHE_SIZE; i++) {
        if (g_blob_cache[i].engine == eng) {
            dd = g_blob_cache[i].data_dir;
            break;
        }
    }
    if (!dd) return -1;

    /* 持久化引用文件：<data_dir>/refs/<hex>.ref */
    char refs_dir[600];
    int n = snprintf(refs_dir, sizeof(refs_dir), "%s/refs", dd);
    if (n <= 0 || (size_t)n >= sizeof(refs_dir)) return -1;

    ensure_dir_recursive(refs_dir);

    char hex[65];
    mm_blob_id_to_hex(blob_id, hex);

    char ref_path[700];
    n = snprintf(ref_path, sizeof(ref_path), "%s/%s.ref", refs_dir, hex);
    if (n <= 0 || (size_t)n >= sizeof(ref_path)) return -1;

    /* 写入一个空引用文件（仅存在性即可；真实数据走 Blob 引擎） */
    FILE *f = fopen(ref_path, "w");
    if (!f) {
        LOG_WARN("mm_storage_blob_put: 创建引用文件失败 path=%s", ref_path);
        return -1;
    }
    fclose(f);

    LOG_INFO("mm_storage_blob_put: 登记成功 collection=%s blob=%s", collection, hex);
    return 0;
}

int mm_storage_blob_get(const char *collection, const uint8_t blob_id[32],
                        void *out_buf, size_t buf_len, size_t *out_read) {
    if (!collection || !blob_id || !out_buf || !out_read) return -1;
    *out_read = 0;

    blob_engine_t *eng = mm_get_or_create_engine(collection);
    if (!eng) {
        LOG_WARN("mm_storage_blob_get: 引擎获取失败 collection=%s", collection);
        return -1;
    }

    /* 走 Blob 引擎的完整读取 */
    return blob_get(eng, blob_id, out_buf, buf_len, out_read);
}