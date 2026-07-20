/**
 * @file clog.c
 * @brief CLOG（Commit Log）持久化实现
 *
 * SLRU 缓存 + 段文件持久化。
 * 参考 PostgreSQL: src/backend/access/transam/slru.c
 */

#include "db/storage/txn/clog.h"
#include "db/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

#ifdef _WIN32
#include <sys/stat.h>
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

/* ============================================================
 * 文件路径格式
 * ============================================================ */

/** CLOG 段文件名格式：0000, 0001, ... */
#define CLOG_SEG_FILENAME_FORMAT "%04d"

/** 段文件名最大长度 */
#define CLOG_MAX_FILENAME_LEN 16

/* ============================================================
 * 全局状态
 * ============================================================ */

/** 是否已初始化 */
static bool g_initialized = false;

/** CLOG 数据目录路径 */
static char g_clog_dir[512] = {0};

/** SLRU 缓存条目数组 */
static ClogCacheEntry g_cache[CLOG_CACHE_PAGES];

/** LRU 链表头尾 */
static int g_lru_head = -1;
static int g_lru_tail = -1;

/** 统计信息 */
static atomic_uint_fast64_t g_cache_hits = 0;
static atomic_uint_fast64_t g_cache_misses = 0;
static atomic_uint_fast64_t g_disk_reads = 0;
static atomic_uint_fast64_t g_disk_writes = 0;
static atomic_uint_fast64_t g_status_sets = 0;
static atomic_uint_fast64_t g_status_gets = 0;

/* ============================================================
 * LRU 链表操作
 * ============================================================ */

static void lru_move_to_head(int idx) {
    if (idx == g_lru_head) return;
    int prev = g_cache[idx].lru_prev;
    int next = g_cache[idx].lru_next;
    if (prev >= 0) g_cache[prev].lru_next = next;
    if (next >= 0) g_cache[next].lru_prev = prev;
    if (idx == g_lru_tail) g_lru_tail = prev;
    g_cache[idx].lru_prev = -1;
    g_cache[idx].lru_next = g_lru_head;
    if (g_lru_head >= 0) g_cache[g_lru_head].lru_prev = idx;
    g_lru_head = idx;
    if (g_lru_tail < 0) g_lru_tail = idx;
}

static int lru_get_victim(void) {
    return (g_lru_tail >= 0) ? g_lru_tail : 0;
}

static void lru_init(void) {
    for (int i = 0; i < CLOG_CACHE_PAGES; i++) {
        g_cache[i].valid = false;
        g_cache[i].dirty = false;
        g_cache[i].lru_prev = i - 1;
        g_cache[i].lru_next = (i + 1 < CLOG_CACHE_PAGES) ? i + 1 : -1;
    }
    g_lru_head = 0;
    g_lru_tail = CLOG_CACHE_PAGES - 1;
}

/* ============================================================
 * 段文件操作（使用标准 C I/O）
 * ============================================================ */

/**
 * @brief 构建段文件路径
 */
static int make_clog_path(int segno, char *path_out, size_t path_size) {
    char filename[CLOG_MAX_FILENAME_LEN];
    snprintf(filename, sizeof(filename), CLOG_SEG_FILENAME_FORMAT, segno);
    snprintf(path_out, path_size, "%s/%s", g_clog_dir, filename);
    return 0;
}

/**
 * @brief 读取段文件指定页（8192 字节）
 */
static int read_segment_page(int segno, int page_idx, unsigned char *page_data) {
    char path[1024];
    make_clog_path(segno, path, sizeof(path));

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        /* 文件不存在，返回全零（视为未使用页面） */
        memset(page_data, 0, BLCKSZ);
        return 0;
    }

    if (fseek(fp, (long)page_idx * BLCKSZ, SEEK_SET) != 0) {
        fclose(fp);
        memset(page_data, 0, BLCKSZ);
        return 0;
    }

    size_t n = fread(page_data, 1, BLCKSZ, fp);
    if (n < BLCKSZ) {
        memset(page_data + n, 0, BLCKSZ - n);
    }
    fclose(fp);
    atomic_fetch_add(&g_disk_reads, 1);
    return 0;
}

/**
 * @brief 写入段文件指定页（8192 字节）
 */
static int write_segment_page(int segno, int page_idx, const unsigned char *page_data) {
    char path[1024];
    make_clog_path(segno, path, sizeof(path));

    FILE *fp = fopen(path, "r+b");
    if (!fp) {
        /* 文件不存在，创建并写入 */
        fp = fopen(path, "w+b");
    }
    if (!fp) {
        LOG_WARN("无法打开 CLOG 段文件 %s", path);
        return -1;
    }

    if (fseek(fp, (long)page_idx * BLCKSZ, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }

    size_t n = fwrite(page_data, 1, BLCKSZ, fp);
    fflush(fp);  /* 确保写入磁盘 */
    fclose(fp);

    if (n != BLCKSZ) {
        LOG_WARN("CLOG 页面写入不完整: segno=%d page=%d", segno, page_idx);
        return -1;
    }

    atomic_fetch_add(&g_disk_writes, 1);
    return 0;
}

/* ============================================================
 * SLRU 缓存操作
 * ============================================================ */

static ClogCacheEntry *clog_get_cache_entry(uint32_t xid) {
    int segno = clog_get_segno(xid);
    size_t xid_in_seg = (size_t)(xid % CLOG_XACTS_PER_SEG);
    int page_idx = (int)(xid_in_seg / CLOG_XACTS_PER_PAGE);
    uint32_t base_xid = (uint32_t)(segno * CLOG_XACTS_PER_SEG + page_idx * CLOG_XACTS_PER_PAGE);

    /* 查找缓存 */
    for (int i = 0; i < CLOG_CACHE_PAGES; i++) {
        if (g_cache[i].valid && g_cache[i].segno == segno && g_cache[i].base_xid == base_xid) {
            atomic_fetch_add(&g_cache_hits, 1);
            lru_move_to_head(i);
            return &g_cache[i];
        }
    }

    /* 缓存未命中 */
    atomic_fetch_add(&g_cache_misses, 1);
    int victim = lru_get_victim();

    /* 淘汰脏页先刷盘 */
    if (g_cache[victim].valid && g_cache[victim].dirty) {
        int old_page_idx = (int)((g_cache[victim].base_xid % CLOG_XACTS_PER_SEG) / CLOG_XACTS_PER_PAGE);
        write_segment_page(g_cache[victim].segno, old_page_idx, g_cache[victim].data);
    }

    g_cache[victim].valid = false;
    g_cache[victim].dirty = false;
    g_cache[victim].segno = segno;
    g_cache[victim].base_xid = base_xid;

    read_segment_page(segno, page_idx, g_cache[victim].data);
    g_cache[victim].valid = true;
    lru_move_to_head(victim);

    return &g_cache[victim];
}

/* ============================================================
 * 公共 API
 * ============================================================ */

int clog_init(const char *data_dir) {
    /* 强制重置：允许每次测试重新初始化 */
    g_initialized = false;
    for (int i = 0; i < CLOG_CACHE_PAGES; i++) {
        g_cache[i].valid = false;
        g_cache[i].dirty = false;
    }
    lru_init();
    atomic_store(&g_cache_hits, 0);
    atomic_store(&g_cache_misses, 0);
    atomic_store(&g_disk_reads, 0);
    atomic_store(&g_disk_writes, 0);
    atomic_store(&g_status_sets, 0);
    atomic_store(&g_status_gets, 0);

    if (data_dir && strlen(data_dir) > 0) {
        snprintf(g_clog_dir, sizeof(g_clog_dir), "%s/" CLOG_DIR_NAME, data_dir);
    } else {
        snprintf(g_clog_dir, sizeof(g_clog_dir), "./" CLOG_DIR_NAME);
    }

    /* 递归创建目录（Windows/Linux 兼容） */
    char tmp[1024];
    char *p = tmp;
    snprintf(tmp, sizeof(tmp), "%s", g_clog_dir);
    while (*p) {
        char *sep = strchr(p, '/');
        if (sep) {
            *sep = '\0';
#ifdef _WIN32
            mkdir(tmp);
#else
            mkdir(tmp, 0755);
#endif
            *sep = '/';
            p = sep + 1;
        } else {
#ifdef _WIN32
            mkdir(tmp);
#else
            mkdir(tmp, 0755);
#endif
            break;
        }
    }

    lru_init();
    g_initialized = true;
    LOG_INFO("CLOG 子系统初始化完成，数据目录: %s", g_clog_dir);
    return 0;
}

void clog_shutdown(void) {
    if (!g_initialized) return;
    clog_flush();
    g_initialized = false;
    LOG_INFO("CLOG 子系统已关闭");
}

bool clog_initialized(void) {
    return g_initialized;
}

int clog_set_status(uint32_t xid, uint8_t status) {
    if (!g_initialized) return -1;

    ClogCacheEntry *entry = clog_get_cache_entry(xid);
    if (!entry) return -1;

    size_t xid_in_seg = (size_t)(xid % CLOG_XACTS_PER_SEG);
    size_t byte_offset = xid_in_seg / CLOG_XACTS_PER_BYTE;
    int bit_offset = (int)((xid_in_seg % CLOG_XACTS_PER_BYTE) * CLOG_BITS_PER_XACT);

    if (byte_offset >= BLCKSZ) {
        LOG_ERROR("clog_set_status: byte_offset %zu >= BLCKSZ %d for xid=%u", byte_offset, BLCKSZ, xid);
        return -1;
    }

    unsigned char mask = (unsigned char)(0x03 << bit_offset);
    entry->data[byte_offset] = (unsigned char)((entry->data[byte_offset] & ~mask) | ((status & 0x03) << bit_offset));
    entry->dirty = true;

    atomic_fetch_add(&g_status_sets, 1);
    return 0;
}

uint8_t clog_get_status(uint32_t xid) {
    if (!g_initialized) return CLOG_STATUS_IN_PROGRESS;

    ClogCacheEntry *entry = clog_get_cache_entry(xid);
    if (!entry) return CLOG_STATUS_IN_PROGRESS;

    size_t xid_in_seg = (size_t)(xid % CLOG_XACTS_PER_SEG);
    size_t byte_offset = xid_in_seg / CLOG_XACTS_PER_BYTE;
    int bit_offset = (int)((xid_in_seg % CLOG_XACTS_PER_BYTE) * CLOG_BITS_PER_XACT);

    if (byte_offset >= BLCKSZ) {
        LOG_ERROR("clog_get_status: byte_offset %zu >= BLCKSZ %d for xid=%u", byte_offset, BLCKSZ, xid);
        return CLOG_STATUS_IN_PROGRESS;
    }

    uint8_t result = (uint8_t)((entry->data[byte_offset] >> bit_offset) & 0x03);
    atomic_fetch_add(&g_status_gets, 1);
    return result;
}

int clog_flush_segment(int segno) {
    for (int i = 0; i < CLOG_CACHE_PAGES; i++) {
        if (g_cache[i].valid && g_cache[i].dirty && g_cache[i].segno == segno) {
            int page_idx = (int)((g_cache[i].base_xid % CLOG_XACTS_PER_SEG) / CLOG_XACTS_PER_PAGE);
            write_segment_page(segno, page_idx, g_cache[i].data);
            g_cache[i].dirty = false;
        }
    }
    return 0;
}

int clog_flush(void) {
    for (int i = 0; i < CLOG_CACHE_PAGES; i++) {
        if (g_cache[i].valid && g_cache[i].dirty) {
            int page_idx = (int)((g_cache[i].base_xid % CLOG_XACTS_PER_SEG) / CLOG_XACTS_PER_PAGE);
            write_segment_page(g_cache[i].segno, page_idx, g_cache[i].data);
            g_cache[i].dirty = false;
        }
    }
    return 0;
}

void clog_get_stats(ClogStats *stats) {
    if (stats) {
        stats->cache_hits = atomic_load(&g_cache_hits);
        stats->cache_misses = atomic_load(&g_cache_misses);
        stats->disk_reads = atomic_load(&g_disk_reads);
        stats->disk_writes = atomic_load(&g_disk_writes);
        stats->status_sets = atomic_load(&g_status_sets);
        stats->status_gets = atomic_load(&g_status_gets);
    }
}

void clog_reset_stats(void) {
    atomic_store(&g_cache_hits, 0);
    atomic_store(&g_cache_misses, 0);
    atomic_store(&g_disk_reads, 0);
    atomic_store(&g_disk_writes, 0);
    atomic_store(&g_status_sets, 0);
    atomic_store(&g_status_gets, 0);
}
