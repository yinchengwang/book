/**
 * @file vector_segment.c
 * @brief 向量段管理实现
 *
 * 负责向量段的文件级存储和管理，包括：
 * - 向量数据的追加和读取
 * - 页面分配和回收
 * - 索引文件维护
 */
#include <db/storage/vector/vector_persist.h>
#include <db/page.h>
#include <db/log.h>
#include <db/storage/buffer/buf.h>
#include <db/storage/wal/wal.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>

/* ============================================================
 * 向量段结构
 * ============================================================ */

/**
 * @brief 向量段结构
 */
struct vector_segment_s {
    char          path[256];       /**< 段文件目录 */
    uint32_t      segment_id;      /**< 段ID */
    int32_t       dims;            /**< 向量维度 */
    uint32_t      page_size;       /**< 页面大小 */
    uint32_t      vector_size;     /**< 单向量大小 */

    /* 文件句柄 */
    FILE         *data_file;       /**< 数据文件 */
    FILE         *idx_file;        /**< 索引文件 */

    /* 内存中的页面缓存 */
    vector_persist_page_t **pages;         /**< 页面数组 */
    uint32_t      num_pages;       /**< 页面数量 */
    uint32_t      max_pages;       /**< 最大页面数 */

    /* 向量索引 (用于快速定位) */
    vector_index_entry_t *index;   /**< 向量索引数组 */
    int64_t       num_vectors;     /**< 向量数量 */
    int64_t       num_deleted;     /**< 已删除数量 */
    int32_t       next_vector_id;  /**< 下一个向量ID */

    /* 状态 */
    vector_segment_state_t state;  /**< 段状态 */
    bool          is_dirty;        /**< 是否有脏页 */
};

/* ============================================================
 * 工具函数
 * ============================================================ */

/**
 * @brief 生成数据文件路径
 */
static void make_data_path(char *buf, size_t size, const char *dir, uint32_t seg_id) {
    snprintf(buf, size, "%s/segment_%u.dat", dir, seg_id);
}

/**
 * @brief 生成索引文件路径
 */
static void make_idx_path(char *buf, size_t size, const char *dir, uint32_t seg_id) {
    snprintf(buf, size, "%s/segment_%u.idx", dir, seg_id);
}

/**
 * @brief 确保目录存在
 */
static int ensure_dir(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        if (mkdir(path) == -1 && errno != EEXIST) {
            LOG_ERROR("创建目录失败: %s", path);
            return -1;
        }
    }
    return 0;
}

/**
 * @brief 写向量段头部
 */
static int write_segment_header(FILE *fp, vector_segment_t *seg) {
    vector_segment_header_t header;
    memset(&header, 0, sizeof(header));

    header.magic = VECTOR_SEGMENT_MAGIC;
    header.version = VECTOR_SEGMENT_VERSION;
    header.segment_id = seg->segment_id;
    header.dims = seg->dims;
    header.page_size = seg->page_size;
    header.vector_size = seg->vector_size;
    header.num_vectors = seg->num_vectors;
    header.num_pages = seg->num_pages;
    header.num_deleted = seg->num_deleted;
    header.created_at = 0;  /* TODO: 使用实际时间戳 */
    header.modified_at = 0;

    if (fseek(fp, 0, SEEK_SET) != 0) return -1;
    if (fwrite(&header, sizeof(header), 1, fp) != 1) return -1;

    seg->is_dirty = true;
    return 0;
}

/**
 * @brief 读向量段头部
 */
static int read_segment_header(FILE *fp, vector_segment_t *seg) {
    vector_segment_header_t header;

    if (fseek(fp, 0, SEEK_SET) != 0) return -1;
    if (fread(&header, sizeof(header), 1, fp) != 1) return -1;

    if (header.magic != VECTOR_SEGMENT_MAGIC) {
        LOG_ERROR("无效的向量段魔数: 0x%X", header.magic);
        return -1;
    }

    if (header.version != VECTOR_SEGMENT_VERSION) {
        LOG_ERROR("不支持的向量段版本: %u", header.version);
        return -1;
    }

    seg->dims = header.dims;
    seg->page_size = header.page_size;
    seg->vector_size = header.vector_size;
    seg->num_vectors = header.num_vectors;
    seg->num_pages = header.num_pages;
    seg->num_deleted = header.num_deleted;
    seg->next_vector_id = (int32_t)seg->num_vectors;

    return 0;
}

/**
 * @brief 分配新页面
 */
static vector_persist_page_t *allocate_page(vector_segment_t *seg) {
    if (seg->num_pages >= seg->max_pages) {
        /* 扩展页面数组 */
        uint32_t new_max = seg->max_pages == 0 ? 16 : seg->max_pages * 2;
        vector_persist_page_t **new_pages = realloc(seg->pages, new_max * sizeof(vector_persist_page_t *));
        if (!new_pages) {
            LOG_ERROR("扩展页面数组失败");
            return NULL;
        }
        seg->pages = new_pages;
        seg->max_pages = new_max;
    }

    /* 创建新页面 */
    vector_persist_page_t *page = vector_persist_page_create(seg->num_pages, seg->page_size);
    if (!page) {
        return NULL;
    }

    seg->pages[seg->num_pages] = page;
    seg->num_pages++;

    return page;
}

/**
 * @brief 将页面写入文件
 */
static int flush_page(vector_segment_t *seg, uint32_t page_id) {
    if (page_id >= seg->num_pages) return -1;

    vector_persist_page_t *page = seg->pages[page_id];
    if (!page) return -1;

    /* 计算页面在文件中的偏移 */
    off_t offset = VECTOR_SEGMENT_HEADER_SIZE + (off_t)page_id * seg->page_size;

    if (fseek(seg->data_file, offset, SEEK_SET) != 0) {
        LOG_ERROR("定位页面失败: page_id=%u", page_id);
        return -1;
    }

    /* 设置校验和 */
    vector_persist_page_set_checksum(page);

    /* 写入页面头部 */
    if (fwrite(&page->header, sizeof(vector_persist_page_header_t), 1, seg->data_file) != 1) {
        LOG_ERROR("写入页面头部失败");
        return -1;
    }

    /* 写入页面数据 */
    uint32_t data_size = seg->page_size - sizeof(vector_persist_page_header_t);
    if (fwrite(page->data, data_size, 1, seg->data_file) != 1) {
        LOG_ERROR("写入页面数据失败");
        return -1;
    }

    return 0;
}

/* ============================================================
 * 向量段 API 实现
 * ============================================================ */

vector_segment_t *vector_segment_create(const char *path, int32_t dims, uint32_t segment_id) {
    /* 确保目录存在 */
    if (ensure_dir(path) != 0) {
        return NULL;
    }

    vector_segment_t *seg = (vector_segment_t *)calloc(1, sizeof(vector_segment_t));
    if (!seg) {
        LOG_ERROR("分配向量段结构失败");
        return NULL;
    }

    /* 初始化字段 */
    strncpy(seg->path, path, sizeof(seg->path) - 1);
    seg->segment_id = segment_id;
    seg->dims = dims;
    seg->page_size = BUF_PAGE_SIZE;  /* 使用 Buffer Pool 的页面大小 */
    seg->vector_size = (uint32_t)(dims * sizeof(float));
    seg->num_vectors = 0;
    seg->num_deleted = 0;
    seg->next_vector_id = 0;
    seg->num_pages = 0;
    seg->max_pages = 0;
    seg->state = VECTOR_SEGMENT_OPEN;
    seg->is_dirty = false;

    /* 生成文件路径 */
    char data_path[512], idx_path[512];
    make_data_path(data_path, sizeof(data_path), path, segment_id);
    make_idx_path(idx_path, sizeof(idx_path), path, segment_id);

    /* 打开数据文件 */
    seg->data_file = fopen(data_path, "w+b");
    if (!seg->data_file) {
        LOG_ERROR("打开数据文件失败: %s", data_path);
        free(seg);
        return NULL;
    }

    /* 打开索引文件 */
    seg->idx_file = fopen(idx_path, "w+b");
    if (!seg->idx_file) {
        LOG_ERROR("打开索引文件失败: %s", idx_path);
        fclose(seg->data_file);
        free(seg);
        return NULL;
    }

    /* 写入头部 */
    if (write_segment_header(seg->data_file, seg) != 0) {
        LOG_ERROR("写入段头部失败");
        fclose(seg->data_file);
        fclose(seg->idx_file);
        free(seg);
        return NULL;
    }

    LOG_INFO("创建向量段成功: path=%s, id=%u, dims=%d", path, segment_id, dims);
    return seg;
}

vector_segment_t *vector_segment_open(const char *path, uint32_t segment_id) {
    vector_segment_t *seg = (vector_segment_t *)calloc(1, sizeof(vector_segment_t));
    if (!seg) {
        LOG_ERROR("分配向量段结构失败");
        return NULL;
    }

    /* 初始化字段 */
    strncpy(seg->path, path, sizeof(seg->path) - 1);
    seg->segment_id = segment_id;
    seg->state = VECTOR_SEGMENT_OPEN;
    seg->is_dirty = false;

    /* 生成文件路径 */
    char data_path[512], idx_path[512];
    make_data_path(data_path, sizeof(data_path), path, segment_id);
    make_idx_path(idx_path, sizeof(idx_path), path, segment_id);

    /* 打开数据文件 */
    seg->data_file = fopen(data_path, "r+b");
    if (!seg->data_file) {
        LOG_ERROR("打开数据文件失败: %s", data_path);
        free(seg);
        return NULL;
    }

    /* 读取头部 */
    if (read_segment_header(seg->data_file, seg) != 0) {
        LOG_ERROR("读取段头部失败");
        fclose(seg->data_file);
        free(seg);
        return NULL;
    }

    seg->page_size = BUF_PAGE_SIZE;

    /* 打开索引文件 */
    seg->idx_file = fopen(idx_path, "r+b");
    if (!seg->idx_file) {
        LOG_ERROR("打开索引文件失败: %s", idx_path);
        fclose(seg->data_file);
        free(seg);
        return NULL;
    }

    /* 加载索引 */
    seg->num_vectors = 0;  /* 重置，将在加载页面时更新 */
    seg->index = NULL;

    /* 懒加载：读取所有页面 */
    if (seg->num_pages > 0) {
        seg->pages = calloc(seg->num_pages, sizeof(vector_persist_page_t *));
        seg->max_pages = seg->num_pages;

        for (uint32_t i = 0; i < seg->num_pages; i++) {
            /* 读取页面 */
            off_t offset = VECTOR_SEGMENT_HEADER_SIZE + (off_t)i * seg->page_size;
            if (fseek(seg->data_file, offset, SEEK_SET) != 0) {
                LOG_ERROR("定位页面失败: page_id=%u", i);
                continue;
            }

            /* 分配页面内存 */
            uint8_t *page_data = malloc(seg->page_size);
            if (!page_data) {
                LOG_ERROR("分配页面内存失败");
                continue;
            }

            if (fread(page_data, seg->page_size, 1, seg->data_file) != 1) {
                LOG_ERROR("读取页面失败: page_id=%u", i);
                free(page_data);
                continue;
            }

            /* 从数据创建页面对象 */
            seg->pages[i] = vector_persist_page_from_data(page_data, seg->page_size);
            if (seg->pages[i]) {
                /* 验证校验和 */
                if (!vector_persist_page_verify_checksum(seg->pages[i])) {
                    LOG_WARN("页面校验和不匹配: page_id=%u", i);
                }
                seg->num_vectors += seg->pages[i]->header.num_vectors;
            }
        }
    }

    /* 加载索引 */
    if (fseek(seg->idx_file, VECTOR_SEGMENT_HEADER_SIZE, SEEK_SET) == 0) {
        if (seg->num_vectors > 0) {
            seg->index = calloc(seg->num_vectors, sizeof(vector_index_entry_t));
            if (seg->index) {
                size_t n = fread(seg->index, sizeof(vector_index_entry_t), seg->num_vectors, seg->idx_file);
                (void)n;  /* 忽略未使用警告 */
            }
        }
    }

    seg->next_vector_id = (int32_t)seg->num_vectors;

    LOG_INFO("打开向量段成功: path=%s, id=%u, vectors=%ld, pages=%u",
             path, segment_id, (long)seg->num_vectors, seg->num_pages);
    return seg;
}

int vector_segment_close(vector_segment_t *seg) {
    if (!seg) return -1;

    /* 刷新所有脏页 */
    if (seg->is_dirty) {
        vector_segment_flush(seg);
    }

    /* 释放页面 */
    for (uint32_t i = 0; i < seg->num_pages; i++) {
        if (seg->pages[i]) {
            vector_persist_page_free(seg->pages[i]);
            seg->pages[i] = NULL;
        }
    }
    free(seg->pages);
    seg->pages = NULL;

    /* 释放索引 */
    free(seg->index);
    seg->index = NULL;

    /* 关闭文件 */
    if (seg->data_file) {
        fclose(seg->data_file);
        seg->data_file = NULL;
    }
    if (seg->idx_file) {
        fclose(seg->idx_file);
        seg->idx_file = NULL;
    }

    seg->state = VECTOR_SEGMENT_CLOSED;

    LOG_INFO("关闭向量段: id=%u", seg->segment_id);
    free(seg);
    return 0;
}

int vector_segment_drop(const char *path, uint32_t segment_id) {
    char data_path[512], idx_path[512];
    make_data_path(data_path, sizeof(data_path), path, segment_id);
    make_idx_path(idx_path, sizeof(idx_path), path, segment_id);

    int ret = 0;

    if (remove(data_path) != 0 && errno != ENOENT) {
        LOG_ERROR("删除数据文件失败: %s", data_path);
        ret = -1;
    }

    if (remove(idx_path) != 0 && errno != ENOENT) {
        LOG_ERROR("删除索引文件失败: %s", idx_path);
        ret = -1;
    }

    return ret;
}

int32_t vector_segment_append(vector_segment_t *seg, const float *vector) {
    if (!seg || !vector) return -1;

    /* 找到有空间的页面或创建新页面 */
    vector_persist_page_t *page = NULL;
    uint32_t page_id = 0;

    if (seg->num_pages > 0) {
        /* 检查最后一页是否有空间 */
        page = seg->pages[seg->num_pages - 1];
        if (vector_persist_page_free_space(page) < seg->vector_size + sizeof(uint16_t)) {
            page = NULL;  /* 需要新页面 */
        }
    }

    if (!page) {
        /* 分配新页面 */
        page = allocate_page(seg);
        if (!page) {
            LOG_ERROR("分配新页面失败");
            return -1;
        }
        page_id = seg->num_pages - 1;
    } else {
        page_id = seg->num_pages - 1;
    }

    /* 追加向量到页面 */
    uint16_t offset = vector_persist_page_append(page, vector, seg->dims);
    if (offset == UINT16_MAX) {
        LOG_ERROR("追加向量到页面失败");
        return -1;
    }

    /* 分配向量ID */
    int32_t vector_id = seg->next_vector_id++;

    /* 初始化索引（首次追加时） */
    if (!seg->index && seg->num_vectors == 0) {
        seg->index = calloc(16, sizeof(vector_index_entry_t));  /* 预分配 16 个条目 */
        if (seg->index) {
            /* 成功分配 */
        }
    }

    /* 扩展索引（如需要） */
    if (seg->index && vector_id >= (int32_t)(seg->num_vectors + 16)) {
        /* 每 16 个向量扩展一次 */
        uint32_t new_size = (vector_id + 16) * sizeof(vector_index_entry_t);
        vector_index_entry_t *new_index = realloc(seg->index, new_size);
        if (new_index) {
            seg->index = new_index;
            /* 清零新分配的空间 */
            memset(seg->index + seg->num_vectors, 0, 16 * sizeof(vector_index_entry_t));
        }
    }

    if (seg->index) {
        seg->index[vector_id].vector_id = vector_id;
        seg->index[vector_id].page_id = page_id;
        seg->index[vector_id].offset = offset;
    }

    seg->num_vectors++;
    seg->is_dirty = true;

    return vector_id;
}

int32_t vector_segment_append_batch(vector_segment_t *seg, const float *vectors, int32_t n) {
    if (!seg || !vectors || n <= 0) return -1;

    int32_t start_id = seg->next_vector_id;

    for (int32_t i = 0; i < n; i++) {
        int32_t id = vector_segment_append(seg, vectors + (size_t)i * seg->dims);
        if (id < 0) {
            LOG_ERROR("批量追加失败 at index %d", i);
            return -1;
        }
    }

    return start_id;
}

int vector_segment_get(const vector_segment_t *seg, int32_t vector_id, float *out_vector) {
    if (!seg || vector_id < 0 || vector_id >= seg->next_vector_id) {
        return -1;
    }

    /* 通过索引查找 */
    if (seg->index && vector_id < seg->num_vectors) {
        vector_index_entry_t *entry = &seg->index[vector_id];
        if (entry->page_id < seg->num_pages && seg->pages[entry->page_id]) {
            vector_persist_page_read(seg->pages[entry->page_id], entry->offset, seg->dims, out_vector);
            return 0;
        }
    }

    /* 线性搜索（回退方案） */
    for (uint32_t i = 0; i < seg->num_pages; i++) {
        vector_persist_page_t *page = seg->pages[i];
        if (!page) continue;

        /* TODO: 需要页内索引来加速查找 */
        /* 目前简化处理：假设每页只有少量向量，逐个检查 */
    }

    LOG_ERROR("未找到向量: id=%d", vector_id);
    return -1;
}

int32_t vector_segment_get_batch(const vector_segment_t *seg, const int32_t *vector_ids,
                                  int32_t n, float *out_vectors) {
    if (!seg || !vector_ids || !out_vectors || n <= 0) return -1;

    int32_t count = 0;
    for (int32_t i = 0; i < n; i++) {
        if (vector_segment_get(seg, vector_ids[i], out_vectors + (size_t)i * seg->dims) == 0) {
            count++;
        }
    }

    return count;
}

int vector_segment_delete(vector_segment_t *seg, int32_t vector_id) {
    if (!seg || vector_id < 0 || vector_id >= seg->next_vector_id) {
        return -1;
    }

    /* 更新索引：标记为已删除 */
    if (seg->index && vector_id < seg->num_vectors) {
        seg->index[vector_id].vector_id = -1;  /* 标记为已删除 */
    }

    seg->num_deleted++;
    seg->is_dirty = true;

    return 0;
}

int64_t vector_segment_size(const vector_segment_t *seg) {
    return seg ? seg->num_vectors : 0;
}

int32_t vector_segment_dims(const vector_segment_t *seg) {
    return seg ? seg->dims : 0;
}

uint32_t vector_segment_id(const vector_segment_t *seg) {
    return seg ? seg->segment_id : 0;
}

vector_segment_state_t vector_segment_state(const vector_segment_t *seg) {
    return seg ? seg->state : VECTOR_SEGMENT_CLOSED;
}

int vector_segment_flush(vector_segment_t *seg) {
    if (!seg) return -1;

    /* 刷新所有页面 */
    for (uint32_t i = 0; i < seg->num_pages; i++) {
        if (flush_page(seg, i) != 0) {
            LOG_ERROR("刷新页面失败: page_id=%u", i);
            return -1;
        }
    }

    /* 刷新索引文件 */
    if (seg->idx_file && seg->index && seg->num_vectors > 0) {
        if (fseek(seg->idx_file, VECTOR_SEGMENT_HEADER_SIZE, SEEK_SET) == 0) {
            fwrite(seg->index, sizeof(vector_index_entry_t), seg->num_vectors, seg->idx_file);
            fflush(seg->idx_file);
        }
    }

    /* 更新头部 */
    write_segment_header(seg->data_file, seg);
    fflush(seg->data_file);

    seg->is_dirty = false;

    LOG_DEBUG("刷新向量段: id=%u, pages=%u", seg->segment_id, seg->num_pages);
    return 0;
}
