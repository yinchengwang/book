/**
 * @file vec_page.c
 * @brief 向量页管理模块实现
 *
 * 提供向量数据的分页存储和内存管理。
 */
#include "db/storage/vector/vec_page.h"
#include "db/core/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path) _mkdir(path)
#endif

/* ========================================================================
 * 内部辅助函数
 * ======================================================================== */

/**
 * @brief 计算每页可存储的向量数
 */
static int32_t calc_vectors_per_page(int32_t page_size, int32_t vector_dim) {
    /* 页头大小 + 向量数据 + ID 数组 */
    int32_t header_size = sizeof(vector_page_header_t);
    int32_t vector_size = vector_dim * sizeof(float);
    int32_t id_size = sizeof(int32_t);

    /* 可用空间 = page_size - header_size - alignment */
    int32_t available = page_size - header_size - 64;  /* 64 字节对齐 */

    /* 向量数 = available / (vector_size + id_size) */
    int32_t count = available / (vector_size + id_size);

    return count > 0 ? count : 1;
}

/**
 * @brief 获取或加载页
 */
static vector_page_t *get_or_load_page(vector_page_pool_t *pool, int32_t page_id) {
    if (pool == NULL || page_id < 0) return NULL;

    /* 检查是否已在内存中 */
    for (int32_t i = 0; i < pool->page_count; i++) {
        if (pool->pages[i] && pool->pages[i]->header.page_id == page_id) {
            return pool->pages[i];
        }
    }

    return NULL;
}

/**
 * @brief 分配新页
 */
static vector_page_t *alloc_page(vector_page_pool_t *pool) {
    if (pool == NULL) return NULL;

    vector_page_t *page = (vector_page_t *)calloc(1, sizeof(vector_page_t));
    if (page == NULL) return NULL;

    page->vectors = (float *)calloc(pool->vectors_per_page * pool->vector_dim,
                                     sizeof(float));
    page->vector_ids = (int32_t *)calloc(pool->vectors_per_page, sizeof(int32_t));
    if (page->vectors == NULL || page->vector_ids == NULL) {
        free(page->vectors);
        free(page->vector_ids);
        free(page);
        return NULL;
    }

    page->header.page_id = -1;
    page->header.vector_count = 0;
    page->header.capacity = pool->vectors_per_page;
    page->header.next_page = -1;
    page->header.dirty = false;
    page->header.lsn = 0;
    page->header.magic = VECTOR_PAGE_FILE_MAGIC;
    page->header.checksum = 0;
    page->header.checksum_type = pool->checksum_type;

    return page;
}

/**
 * @brief 释放页
 */
static void free_page(vector_page_t *page) {
    if (page == NULL) return;
    free(page->vectors);
    free(page->vector_ids);
    free(page);
}

/**
 * @brief 获取页文件路径
 */
static int get_page_file_path(const char *data_dir, char *path, size_t size) {
    snprintf(path, size, "%s/vector_pages.dat", data_dir);
    return 0;
}

/**
 * @brief 获取元数据文件路径
 */
static int get_meta_file_path(const char *data_dir, char *path, size_t size) {
    snprintf(path, size, "%s/vector_pages.meta", data_dir);
    return 0;
}

/* ========================================================================
 * 生命周期 API 实现
 * ======================================================================== */

vector_page_pool_t *vector_page_pool_create(const char *data_dir,
                                            int32_t max_pages,
                                            int32_t page_size,
                                            int32_t vector_dim) {
    if (data_dir == NULL || max_pages <= 0 || page_size <= 0 || vector_dim <= 0) {
        LOG_ERROR("向量页池参数无效");
        return NULL;
    }

    vector_page_pool_t *pool = (vector_page_pool_t *)calloc(1, sizeof(vector_page_pool_t));
    if (pool == NULL) {
        LOG_ERROR("分配向量页池失败");
        return NULL;
    }

    /* 初始化字段 */
    strncpy(pool->data_dir, data_dir, sizeof(pool->data_dir) - 1);
    pool->max_pages = max_pages;
    pool->page_size = page_size;
    pool->vector_dim = vector_dim;
    pool->vectors_per_page = calc_vectors_per_page(page_size, vector_dim);
    pool->page_count = 0;
    pool->clock_hand = 0;
    pool->hits = 0;
    pool->misses = 0;
    pool->evictions = 0;
    pool->flushes = 0;

    /* 分配页数组 */
    pool->pages = (vector_page_t **)calloc(max_pages, sizeof(vector_page_t *));
    pool->clock_bits = (int32_t *)calloc(max_pages, sizeof(int32_t));
    if (pool->pages == NULL || pool->clock_bits == NULL) {
        free(pool->pages);
        free(pool->clock_bits);
        free(pool);
        return NULL;
    }

    /* 创建数据目录 */
#ifdef _WIN32
    if (mkdir(data_dir) != 0 && errno != EEXIST) {
#else
    if (mkdir(data_dir, 0755) != 0 && errno != EEXIST) {
#endif
        LOG_WARN("创建向量页目录失败: %s", data_dir);
    }

    LOG_INFO("向量页池创建成功: max_pages=%d, page_size=%d, dim=%d, vectors_per_page=%d",
              max_pages, page_size, vector_dim, pool->vectors_per_page);

    return pool;
}

void vector_page_pool_destroy(vector_page_pool_t *pool) {
    if (pool == NULL) return;

    /* 刷新所有脏页 */
    vector_page_flush_all(pool);

    /* 释放所有页 */
    for (int32_t i = 0; i < pool->page_count; i++) {
        free_page(pool->pages[i]);
    }

    free(pool->pages);
    free(pool->clock_bits);
    free(pool);
}

int vector_page_pool_load(vector_page_pool_t *pool) {
    if (pool == NULL) return -1;

    char meta_path[512];
    get_meta_file_path(pool->data_dir, meta_path, sizeof(meta_path));

    FILE *fp = fopen(meta_path, "rb");
    if (fp == NULL) {
        LOG_INFO("向量页元数据文件不存在，将创建新页池");
        return 0;
    }

    /* 读取魔数 */
    uint32_t magic;
    if (fread(&magic, sizeof(magic), 1, fp) != 1 || magic != VECTOR_PAGE_FILE_MAGIC) {
        LOG_WARN("向量页元数据文件格式无效");
        fclose(fp);
        return -1;
    }

    /* 读取版本 */
    uint32_t version;
    if (fread(&version, sizeof(version), 1, fp) != 1 || version != VECTOR_PAGE_FILE_VERSION) {
        LOG_WARN("向量页元数据版本不匹配");
        fclose(fp);
        return -1;
    }

    /* 读取统计信息 */
    if (fread(&pool->page_count, sizeof(pool->page_count), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }

    /* 暂时不加载页数据，按需加载 */
    fclose(fp);

    LOG_INFO("向量页池状态加载成功: page_count=%d", pool->page_count);
    return 0;
}

int vector_page_pool_save(const vector_page_pool_t *pool) {
    if (pool == NULL) return -1;

    char meta_path[512];
    get_meta_file_path(pool->data_dir, meta_path, sizeof(meta_path));

    FILE *fp = fopen(meta_path, "wb");
    if (fp == NULL) {
        LOG_ERROR("无法创建向量页元数据文件");
        return -1;
    }

    /* 写入魔数 */
    uint32_t magic = VECTOR_PAGE_FILE_MAGIC;
    fwrite(&magic, sizeof(magic), 1, fp);

    /* 写入版本 */
    uint32_t version = VECTOR_PAGE_FILE_VERSION;
    fwrite(&version, sizeof(version), 1, fp);

    /* 写入统计信息 */
    fwrite(&pool->page_count, sizeof(pool->page_count), 1, fp);

    /* 写入配置 */
    fwrite(&pool->max_pages, sizeof(pool->max_pages), 1, fp);
    fwrite(&pool->page_size, sizeof(pool->page_size), 1, fp);
    fwrite(&pool->vector_dim, sizeof(pool->vector_dim), 1, fp);
    fwrite(&pool->vectors_per_page, sizeof(pool->vectors_per_page), 1, fp);

    /* 写入页头信息 */
    for (int32_t i = 0; i < pool->page_count; i++) {
        if (pool->pages[i]) {
            fwrite(&pool->pages[i]->header, sizeof(vector_page_header_t), 1, fp);
        }
    }

    fclose(fp);
    LOG_INFO("向量页池状态保存成功");
    return 0;
}

/* ========================================================================
 * 页操作 API 实现
 * ======================================================================== */

int32_t vector_page_append(vector_page_pool_t *pool,
                           const float *vector,
                           int32_t vector_id) {
    if (pool == NULL || vector == NULL) return -1;

    /* 查找有空间的页，或分配新页 */
    vector_page_t *target_page = NULL;

    for (int32_t i = 0; i < pool->page_count; i++) {
        vector_page_t *page = pool->pages[i];
        if (page && page->header.vector_count < page->header.capacity) {
            target_page = page;
            break;
        }
    }

    /* 需要分配新页 */
    if (target_page == NULL) {
        if (pool->page_count >= pool->max_pages) {
            /* 页池已满，需要驱逐 */
            int32_t evict_page_id = vector_page_evict(pool);
            if (evict_page_id < 0) {
                LOG_ERROR("页池驱逐失败，无法插入向量");
                return -1;
            }
        }

        target_page = alloc_page(pool);
        if (target_page == NULL) {
            LOG_ERROR("分配新页失败");
            return -1;
        }

        /* 分配页 ID */
        int32_t new_page_id = pool->page_count;
        target_page->header.page_id = new_page_id;
        pool->pages[pool->page_count++] = target_page;
    }

    /* 写入向量 */
    int32_t offset = target_page->header.vector_count;
    int32_t id = (vector_id >= 0) ? vector_id : (target_page->header.page_id * pool->vectors_per_page + offset);

    memcpy(&target_page->vectors[offset * pool->vector_dim],
           vector,
           pool->vector_dim * sizeof(float));
    target_page->vector_ids[offset] = id;
    target_page->header.vector_count++;
    target_page->header.dirty = true;

    /* 更新校验和 */
    if (pool->checksum_type != VECTOR_PAGE_CHECKSUM_NONE) {
        target_page->header.checksum = vector_page_compute_checksum(target_page, pool);
    }

    return id;
}

int32_t vector_page_append_batch(vector_page_pool_t *pool,
                                 const float *vectors,
                                 int32_t count,
                                 int32_t start_id) {
    if (pool == NULL || vectors == NULL || count <= 0) return -1;

    int32_t inserted = 0;
    int32_t current_id = start_id;

    for (int32_t i = 0; i < count; i++) {
        int32_t id = vector_page_append(pool, &vectors[i * pool->vector_dim], current_id);
        if (id >= 0) {
            inserted++;
            if (start_id >= 0) current_id++;
        }
    }

    return inserted;
}

vector_page_t *vector_page_get(vector_page_pool_t *pool, int32_t vector_id) {
    if (pool == NULL || vector_id < 0) return NULL;

    int32_t page_id = vector_page_id(vector_id, pool->vectors_per_page);

    /* 检查是否在内存中 */
    for (int32_t i = 0; i < pool->page_count; i++) {
        if (pool->pages[i] && pool->pages[i]->header.page_id == page_id) {
            pool->hits++;
            /* 设置引用位 */
            pool->clock_bits[i] = 1;
            return pool->pages[i];
        }
    }

    pool->misses++;
    return NULL;
}

int vector_page_get_vector(const vector_page_pool_t *pool,
                          int32_t vector_id,
                          float *out_vector) {
    if (pool == NULL || out_vector == NULL || vector_id < 0) return -1;

    vector_page_t *page = (vector_page_t *)vector_page_get((vector_page_pool_t *)pool, vector_id);
    if (page == NULL) {
        /* 需要从磁盘加载 */
        return -1;
    }

    int32_t offset = vector_page_offset(vector_id, pool->vectors_per_page);
    if (offset < 0 || offset >= page->header.vector_count) {
        return -1;
    }

    memcpy(out_vector,
           &page->vectors[offset * pool->vector_dim],
           pool->vector_dim * sizeof(float));

    return 0;
}

int32_t vector_page_evict(vector_page_pool_t *pool) {
    if (pool == NULL || pool->page_count == 0) return -1;

    /* Clock-Sweep 置换算法 */
    int32_t iterations = 0;
    int32_t max_iterations = pool->page_count * 2;

    while (iterations < max_iterations) {
        int32_t idx = pool->clock_hand;

        /* 跳过空闲槽 */
        while (idx < pool->page_count && pool->pages[idx] == NULL) {
            idx++;
        }

        if (idx >= pool->page_count) {
            idx = 0;
        }

        vector_page_t *page = pool->pages[idx];
        if (page == NULL) {
            pool->clock_hand = (pool->clock_hand + 1) % pool->max_pages;
            iterations++;
            continue;
        }

        /* 检查引用位 */
        if (pool->clock_bits[idx] == 0) {
            /* 没有被引用，可以驱逐 */
            if (page->header.dirty) {
                /* 需要刷盘 */
                if (vector_page_flush(pool, page->header.page_id) != 0) {
                    LOG_WARN("驱逐页 %d 前刷盘失败", page->header.page_id);
                }
            }

            /* 从数组中移除 */
            free_page(page);
            pool->pages[idx] = NULL;
            pool->evictions++;
            pool->clock_hand = (pool->clock_hand + 1) % pool->max_pages;

            LOG_DEBUG("驱逐页 %d", page->header.page_id);
            return page->header.page_id;
        } else {
            /* 清除引用位 */
            pool->clock_bits[idx] = 0;
        }

        pool->clock_hand = (pool->clock_hand + 1) % pool->max_pages;
        iterations++;
    }

    LOG_WARN("Clock-Sweep 置换失败，所有页都在使用中");
    return -1;
}

int vector_page_flush(vector_page_pool_t *pool, int32_t page_id) {
    if (pool == NULL || page_id < 0) return -1;

    /* 查找页 */
    vector_page_t *page = NULL;
    for (int32_t i = 0; i < pool->page_count; i++) {
        if (pool->pages[i] && pool->pages[i]->header.page_id == page_id) {
            page = pool->pages[i];
            break;
        }
    }

    if (page == NULL || !page->header.dirty) {
        return 0;
    }

    char file_path[512];
    get_page_file_path(pool->data_dir, file_path, sizeof(file_path));

    FILE *fp = fopen(file_path, "r+b");
    if (fp == NULL) {
        fp = fopen(file_path, "w+b");
        if (fp == NULL) return -1;
    }

    /* 计算偏移 */
    long offset = page_id * pool->page_size;

    /* 定位并写入 */
    if (fseek(fp, offset, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }

    /* 写入页数据 */
    if (fwrite(&page->header, sizeof(vector_page_header_t), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }

    size_t data_size = page->header.vector_count * pool->vector_dim * sizeof(float);
    if (fwrite(page->vectors, data_size, 1, fp) != 1) {
        fclose(fp);
        return -1;
    }

    size_t id_size = page->header.vector_count * sizeof(int32_t);
    if (fwrite(page->vector_ids, id_size, 1, fp) != 1) {
        fclose(fp);
        return -1;
    }

    fclose(fp);

    page->header.dirty = false;
    pool->flushes++;

    LOG_DEBUG("页 %d 已刷盘", page_id);
    return 0;
}

int vector_page_flush_all(vector_page_pool_t *pool) {
    if (pool == NULL) return -1;

    for (int32_t i = 0; i < pool->page_count; i++) {
        if (pool->pages[i] && pool->pages[i]->header.dirty) {
            vector_page_flush(pool, pool->pages[i]->header.page_id);
        }
    }

    /* 保存元数据 */
    vector_page_pool_save(pool);

    return 0;
}

/* ========================================================================
 * 统计 API 实现
 * ======================================================================== */

void vector_page_get_stats(const vector_page_pool_t *pool,
                           vector_page_stats_t *stats) {
    if (pool == NULL || stats == NULL) return;

    stats->page_count = pool->page_count;
    stats->max_pages = pool->max_pages;
    stats->hits = pool->hits;
    stats->misses = pool->misses;
    stats->evictions = pool->evictions;
    stats->flushes = pool->flushes;

    /* 计算总向量数 */
    stats->vectors_total = 0;
    for (int32_t i = 0; i < pool->page_count; i++) {
        if (pool->pages[i]) {
            stats->vectors_total += pool->pages[i]->header.vector_count;
        }
    }

    /* 计算命中率 */
    uint64_t total = pool->hits + pool->misses;
    stats->hit_rate = (total > 0) ? ((double)pool->hits / total * 100.0) : 0.0;
}

void vector_page_reset_stats(vector_page_pool_t *pool) {
    if (pool == NULL) return;
    pool->hits = 0;
    pool->misses = 0;
    pool->evictions = 0;
    pool->flushes = 0;
}

/* ========================================================================
 * 工具函数实现
 * ======================================================================== */

int32_t vector_page_capacity(int32_t page_size, int32_t vector_dim) {
    return calc_vectors_per_page(page_size, vector_dim);
}

int32_t vector_page_id(int32_t vector_id, int32_t vectors_per_page) {
    if (vectors_per_page <= 0) return -1;
    return vector_id / vectors_per_page;
}

int32_t vector_page_offset(int32_t vector_id, int32_t vectors_per_page) {
    if (vectors_per_page <= 0) return -1;
    return vector_id % vectors_per_page;
}

/* ========================================================================
 * mmap 内存映射实现
 * ======================================================================== */

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <fcntl.h>
#endif

int vector_page_enable_mmap(vector_page_pool_t *pool, size_t mmap_size) {
    if (pool == NULL) return -1;

    /* 关闭已存在的 mmap */
    if (pool->mmap_base != NULL) {
        vector_page_disable_mmap(pool);
    }

    char file_path[512];
    get_page_file_path(pool->data_dir, file_path, sizeof(file_path));

#ifdef _WIN32
    HANDLE hFile = CreateFile(file_path, GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        LOG_ERROR("无法打开文件进行 mmap: %s", file_path);
        return -1;
    }

    DWORD file_size_high = 0;
    DWORD file_size_low = GetFileSize(hFile, &file_size_high);
    size_t file_size = ((size_t)file_size_high << 32) | file_size_low;

    if (file_size < mmap_size) {
        /* 扩展文件 */
        SetFilePointer(hFile, mmap_size, NULL, FILE_BEGIN);
        SetEndOfFile(hFile);
    }

    HANDLE hMap = CreateFileMapping(hFile, NULL, PAGE_READWRITE, 0, mmap_size, NULL);
    if (hMap == NULL) {
        CloseHandle(hFile);
        LOG_ERROR("创建文件映射失败");
        return -1;
    }

    pool->mmap_base = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, mmap_size);
    CloseHandle(hMap);
    CloseHandle(hFile);

    if (pool->mmap_base == NULL) {
        LOG_ERROR("mmap 映射失败");
        return -1;
    }
#else
    int fd = open(file_path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        LOG_ERROR("无法打开文件进行 mmap: %s", file_path);
        return -1;
    }

    struct stat st;
    if (fstat(fd, &st) < 0 || st.st_size < mmap_size) {
        /* 扩展文件 */
        ftruncate(fd, mmap_size);
    }

    pool->mmap_base = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE,
                           MAP_SHARED, fd, 0);
    close(fd);

    if (pool->mmap_base == MAP_FAILED) {
        pool->mmap_base = NULL;
        LOG_ERROR("mmap 映射失败");
        return -1;
    }
#endif

    pool->mmap_size = mmap_size;
    pool->use_mmap = true;
    LOG_INFO("mmap 内存映射已启用: size=%zu", mmap_size);
    return 0;
}

void vector_page_disable_mmap(vector_page_pool_t *pool) {
    if (pool == NULL || pool->mmap_base == NULL) return;

#ifdef _WIN32
    UnmapViewOfFile(pool->mmap_base);
#else
    munmap(pool->mmap_base, pool->mmap_size);
#endif

    pool->mmap_base = NULL;
    pool->mmap_size = 0;
    pool->use_mmap = false;
    LOG_INFO("mmap 内存映射已禁用");
}

int vector_page_get_vector_mmap(const vector_page_pool_t *pool,
                                int32_t vector_id,
                                float *out_vector) {
    if (pool == NULL || !pool->use_mmap || pool->mmap_base == NULL ||
        out_vector == NULL || vector_id < 0) {
        return -1;
    }

    int32_t page_id = vector_page_id(vector_id, pool->vectors_per_page);
    int32_t offset = vector_page_offset(vector_id, pool->vectors_per_page);

    /* 计算 mmap 中的偏移 */
    size_t page_offset = page_id * pool->page_size;
    if (page_offset + pool->page_size > pool->mmap_size) {
        return -1;
    }

    /* 从 mmap 读取页头 */
    vector_page_header_t header;
    memcpy(&header, (char *)pool->mmap_base + page_offset, sizeof(header));

    /* 验证魔数 */
    if (header.magic != VECTOR_PAGE_FILE_MAGIC) {
        LOG_WARN("页 %d 魔数校验失败", page_id);
        return -1;
    }

    /* 验证 checksum */
    if (pool->checksum_type != VECTOR_PAGE_CHECKSUM_NONE) {
        uint32_t stored_checksum = header.checksum;
        vector_page_t temp_page = {0};
        temp_page.header = header;
        temp_page.vectors = (float *)((char *)pool->mmap_base + page_offset +
                                       sizeof(header));
        temp_page.vector_ids = (int32_t *)((char *)temp_page.vectors +
                                            header.vector_count * pool->vector_dim * sizeof(float));

        uint32_t computed = vector_page_compute_checksum(&temp_page, pool);
        if (stored_checksum != computed) {
            LOG_WARN("页 %d checksum 校验失败", page_id);
            return -1;
        }
    }

    /* 检查偏移有效性 */
    if (offset < 0 || offset >= header.vector_count) {
        return -1;
    }

    /* 读取向量数据 */
    float *vector_base = (float *)((char *)pool->mmap_base + page_offset +
                                    sizeof(header));
    memcpy(out_vector, &vector_base[offset * pool->vector_dim],
           pool->vector_dim * sizeof(float));

    return 0;
}

/* ========================================================================
 * 页面完整性校验实现
 * ======================================================================== */

/* CRC32 查找表 */
static uint32_t g_crc32_table[256] = {0};
static bool g_crc32_init = false;

static void init_crc32_table(void) {
    if (g_crc32_init) return;

    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
        g_crc32_table[i] = crc;
    }
    g_crc32_init = true;
}

static uint32_t crc32_update(uint32_t crc, const void *data, size_t len) {
    if (!g_crc32_init) init_crc32_table();

    const uint8_t *buf = (const uint8_t *)data;
    for (size_t i = 0; i < len; i++) {
        crc = g_crc32_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc;
}

/* Adler32 实现（简化版） */
static uint32_t adler32_update(uint32_t adler, const void *data, size_t len) {
    const uint8_t *buf = (const uint8_t *)data;
    uint32_t s1 = adler & 0xFFFF;
    uint32_t s2 = (adler >> 16) & 0xFFFF;

    for (size_t i = 0; i < len; i++) {
        s1 = (s1 + buf[i]) % 65521;
        s2 = (s2 + s1) % 65521;
    }

    return (s2 << 16) | s1;
}

uint32_t vector_page_compute_checksum(const vector_page_t *page,
                                       const vector_page_pool_t *pool) {
    if (page == NULL || pool == NULL) return 0;

    uint32_t checksum = 0;

    if (pool->checksum_type == VECTOR_PAGE_CHECKSUM_CRC32) {
        checksum = crc32_update(0, page->vectors,
                                 page->header.vector_count * pool->vector_dim * sizeof(float));
        checksum = crc32_update(checksum, page->vector_ids,
                                page->header.vector_count * sizeof(int32_t));
    } else if (pool->checksum_type == VECTOR_PAGE_CHECKSUM_ADLER32) {
        checksum = adler32_update(1, page->vectors,
                                  page->header.vector_count * pool->vector_dim * sizeof(float));
        checksum = adler32_update(checksum, page->vector_ids,
                                  page->header.vector_count * sizeof(int32_t));
    }

    return checksum;
}

void vector_page_set_checksum(vector_page_pool_t *pool, int checksum_type) {
    if (pool == NULL) return;
    pool->checksum_type = checksum_type;
    LOG_INFO("校验和类型已设置: %d", checksum_type);
}

bool vector_page_verify(const vector_page_pool_t *pool, int32_t page_id) {
    if (pool == NULL || page_id < 0) return false;

    /* 查找页 */
    vector_page_t *page = NULL;
    for (int32_t i = 0; i < pool->page_count; i++) {
        if (pool->pages[i] && pool->pages[i]->header.page_id == page_id) {
            page = pool->pages[i];
            break;
        }
    }

    if (page == NULL) return false;

    /* 验证魔数 */
    if (page->header.magic != VECTOR_PAGE_FILE_MAGIC) {
        LOG_WARN("页 %d 魔数校验失败", page_id);
        return false;
    }

    /* 验证 checksum */
    if (pool->checksum_type != VECTOR_PAGE_CHECKSUM_NONE) {
        uint32_t stored = page->header.checksum;
        uint32_t computed = vector_page_compute_checksum(page, pool);
        if (stored != computed) {
            LOG_WARN("页 %d checksum 校验失败: stored=%u, computed=%u",
                     page_id, stored, computed);
            return false;
        }
    }

    return true;
}
