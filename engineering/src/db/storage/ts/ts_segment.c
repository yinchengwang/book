/**
 * @file ts_segment.c
 * @brief 时序数据分段索引实现
 *
 * 实现时序数据的分段存储和高效查询。
 */
#include "db/storage/ts/ts_segment.h"
#include "db/storage/ts/ts_compress.h"
#include "db/core/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path) _mkdir(path)
#endif

/* ========================================================================
 * 内部辅助函数
 * ======================================================================== */

/**
 * @brief 创建数据目录
 */
static int ensure_dir(const char *path) {
    if (path == NULL) return -1;
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return 0;
    }
    return mkdir(path) == 0 ? 0 : -1;
}

/**
 * @brief 获取索引文件路径
 */
static void get_index_path(const ts_segment_index_t *index, char *path, size_t size) {
    snprintf(path, size, "%s/segment_index.bin", index->data_dir);
}

/**
 * @brief 获取段数据文件路径
 */
static void get_segment_path(const ts_segment_index_t *index, uint32_t seg_idx,
                             char *path, size_t size) {
    snprintf(path, size, "%s/segments/seg_%04u.tsd", index->data_dir, seg_idx);
}

/**
 * @brief 确保 segments 目录存在
 */
static int ensure_segments_dir(ts_segment_index_t *index) {
    char path[512];
    snprintf(path, sizeof(path), "%s/segments", index->data_dir);
    return ensure_dir(path);
}

/* ========================================================================
 * 工具函数实现
 * ======================================================================== */

int64_t ts_segment_align_timestamp(int64_t timestamp, ts_granularity_t granularity) {
    int64_t ms = ts_segment_granularity_to_ms(granularity);
    return (timestamp / ms) * ms;
}

int64_t ts_segment_granularity_to_ms(ts_granularity_t granularity) {
    switch (granularity) {
        case TS_GRANULARITY_SECOND: return 1000LL;
        case TS_GRANULARITY_MINUTE: return 60000LL;
        case TS_GRANULARITY_HOUR:   return 3600000LL;
        case TS_GRANULARITY_DAY:    return 86400000LL;
        default: return 1000LL;
    }
}

/* ========================================================================
 * 生命周期 API 实现
 * ======================================================================== */

ts_segment_index_t *ts_segment_index_create(const char *data_dir,
                                            uint32_t seg_size,
                                            ts_granularity_t granularity) {
    if (data_dir == NULL) return NULL;
    if (seg_size == 0) seg_size = TS_SEGMENT_DEFAULT_SIZE;

    /* 创建目录 */
    if (ensure_dir(data_dir) != 0) {
        LOG_ERROR("创建数据目录失败: %s", data_dir);
        return NULL;
    }

    ts_segment_index_t *index = (ts_segment_index_t *)calloc(1, sizeof(ts_segment_index_t));
    if (index == NULL) return NULL;

    strncpy(index->data_dir, data_dir, sizeof(index->data_dir) - 1);
    index->seg_size = seg_size;
    index->granularity = granularity;

    /* 分配段元数据数组 */
    index->seg_capacity = 1024;
    index->segments = (ts_segment_meta_t *)calloc(index->seg_capacity,
                                                  sizeof(ts_segment_meta_t));
    if (index->segments == NULL) {
        free(index);
        return NULL;
    }

    /* 确保 segments 目录存在 */
    if (ensure_segments_dir(index) != 0) {
        LOG_WARN("创建 segments 目录失败");
    }

    /* 初始化当前段 */
    memset(&index->current_seg, 0, sizeof(index->current_seg));
    index->current_seg.start_ts = -1;

    LOG_INFO("分段索引创建成功: seg_size=%u, granularity=%d", seg_size, granularity);
    return index;
}

ts_segment_index_t *ts_segment_index_open(const char *data_dir) {
    if (data_dir == NULL) return NULL;

    char index_path[512];
    ts_segment_index_t temp = {0};
    strncpy(temp.data_dir, data_dir, sizeof(temp.data_dir) - 1);
    get_index_path(&temp, index_path, sizeof(index_path));

    FILE *fp = fopen(index_path, "rb");
    if (fp == NULL) {
        LOG_WARN("分段索引文件不存在");
        return NULL;
    }

    /* 读取文件头 */
    uint32_t magic, version;
    uint32_t seg_count, seg_size;
    ts_granularity_t granularity;

    if (fread(&magic, sizeof(magic), 1, fp) != 1 ||
        fread(&version, sizeof(version), 1, fp) != 1 ||
        fread(&seg_count, sizeof(seg_count), 1, fp) != 1 ||
        fread(&seg_size, sizeof(seg_size), 1, fp) != 1 ||
        fread(&granularity, sizeof(granularity), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }

    if (magic != TS_SEGMENT_MAGIC || version != TS_SEGMENT_VERSION) {
        LOG_ERROR("分段索引文件格式无效");
        fclose(fp);
        return NULL;
    }

    fclose(fp);

    /* 创建索引结构 */
    ts_segment_index_t *index = ts_segment_index_create(data_dir, seg_size, granularity);
    if (index == NULL) return NULL;

    index->seg_count = seg_count;

    /* 读取段元数据 */
    fp = fopen(index_path, "rb");
    if (fp != NULL) {
        fseek(fp, sizeof(uint32_t) * 3 + sizeof(ts_granularity_t), SEEK_SET);
        fread(index->segments, sizeof(ts_segment_meta_t), seg_count, fp);
        fclose(fp);
    }

    /* 计算总数据点数 */
    index->total_points = 0;
    for (uint32_t i = 0; i < index->seg_count; i++) {
        index->total_points += index->segments[i].point_count;
    }

    LOG_INFO("分段索引加载成功: seg_count=%u, total_points=%lu",
             seg_count, index->total_points);
    return index;
}

int ts_segment_index_save(ts_segment_index_t *index) {
    if (index == NULL) return -1;

    /* 刷新当前段 */
    if (index->buffer_count > 0) {
        ts_segment_flush_current(index);
    }

    char index_path[512];
    get_index_path(index, index_path, sizeof(index_path));

    FILE *fp = fopen(index_path, "wb");
    if (fp == NULL) {
        LOG_ERROR("无法创建分段索引文件");
        return -1;
    }

    /* 写入文件头 */
    uint32_t magic = TS_SEGMENT_MAGIC;
    uint32_t version = TS_SEGMENT_VERSION;
    fwrite(&magic, sizeof(magic), 1, fp);
    fwrite(&version, sizeof(version), 1, fp);
    fwrite(&index->seg_count, sizeof(index->seg_count), 1, fp);
    fwrite(&index->seg_size, sizeof(index->seg_size), 1, fp);
    fwrite(&index->granularity, sizeof(index->granularity), 1, fp);

    /* 写入段元数据 */
    fwrite(index->segments, sizeof(ts_segment_meta_t), index->seg_count, fp);

    fclose(fp);

    LOG_INFO("分段索引保存成功: seg_count=%u", index->seg_count);
    return 0;
}

void ts_segment_index_destroy(ts_segment_index_t *index) {
    if (index == NULL) return;

    /* 保存数据 */
    ts_segment_index_save(index);

    /* 释放资源 */
    if (index->segments != NULL) free(index->segments);
    if (index->compress_buffer != NULL) free(index->compress_buffer);
    free(index);
}

/* ========================================================================
 * 数据操作 API 实现
 * ======================================================================== */

int ts_segment_append(ts_segment_index_t *index, int64_t timestamp, double value) {
    if (index == NULL) return -1;

    /* 检查是否需要开始新段 */
    if (index->current_seg.start_ts < 0) {
        index->current_seg.start_ts = ts_segment_align_timestamp(
            timestamp, index->granularity);
        index->current_seg.end_ts = index->current_seg.start_ts;
        index->current_seg.min_value = value;
        index->current_seg.max_value = value;
        index->buffer_count = 0;
    }

    /* 更新段统计 */
    if (value < index->current_seg.min_value) index->current_seg.min_value = value;
    if (value > index->current_seg.max_value) index->current_seg.max_value = value;
    index->current_seg.end_ts = timestamp;
    index->buffer_count++;
    index->total_points++;

    /* 检查是否需要切换段 */
    if (index->buffer_count >= index->seg_size) {
        ts_segment_flush_current(index);
    }

    return 0;
}

uint32_t ts_segment_append_batch(ts_segment_index_t *index,
                                 const int64_t *timestamps,
                                 const double *values,
                                 uint32_t count) {
    if (index == NULL || timestamps == NULL || values == NULL) return 0;

    uint32_t appended = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (ts_segment_append(index, timestamps[i], values[i]) == 0) {
            appended++;
        }
    }
    return appended;
}

uint32_t ts_segment_query(const ts_segment_index_t *index,
                          int64_t start_ts, int64_t end_ts,
                          ts_segment_results_t *results,
                          uint32_t max_results) {
    if (index == NULL || results == NULL || max_results == 0) return 0;

    results->capacity = max_results;
    results->count = 0;
    results->results = (ts_segment_result_t *)malloc(
        max_results * sizeof(ts_segment_result_t));
    if (results->results == NULL) return 0;

    /* 查找匹配的段 */
    for (uint32_t i = 0; i < index->seg_count && results->count < max_results; i++) {
        const ts_segment_meta_t *seg = &index->segments[i];

        /* 检查时间范围交集 */
        if (seg->end_ts < start_ts || seg->start_ts > end_ts) {
            continue;
        }

        /* 加载并解压段数据 */
        char seg_path[512];
        get_segment_path(index, i, seg_path, sizeof(seg_path));

        FILE *fp = fopen(seg_path, "rb");
        if (fp == NULL) continue;

        /* 读取压缩数据 */
        uint8_t *compressed = (uint8_t *)malloc(seg->compressed_size);
        if (compressed != NULL) {
            fread(compressed, seg->compressed_size, 1, fp);

            /* 解压 */
            ts_record_t *records = (ts_record_t *)malloc(
                seg->point_count * sizeof(ts_record_t));
            if (records != NULL) {
                int decompressed = ts_decompress(compressed, seg->compressed_size,
                                                 records, seg->point_count);
                if (decompressed > 0) {
                    /* 过滤并复制结果 */
                    for (int j = 0; j < decompressed && results->count < max_results; j++) {
                        if (records[j].timestamp >= start_ts &&
                            records[j].timestamp <= end_ts) {
                            results->results[results->count].timestamp = records[j].timestamp;
                            results->results[results->count].value = records[j].value;
                            results->count++;
                        }
                    }
                }
                free(records);
            }
            free(compressed);
        }
        fclose(fp);
    }

    return results->count;
}

void ts_segment_free_results(ts_segment_results_t *results) {
    if (results == NULL) return;
    if (results->results != NULL) {
        free(results->results);
        results->results = NULL;
    }
    results->count = 0;
    results->capacity = 0;
}

/* ========================================================================
 * 段管理 API 实现
 * ======================================================================== */

uint32_t ts_segment_count(const ts_segment_index_t *index) {
    return index ? index->seg_count : 0;
}

const ts_segment_meta_t *ts_segment_get_meta(const ts_segment_index_t *index,
                                              uint32_t seg_idx) {
    if (index == NULL || seg_idx >= index->seg_count) {
        return NULL;
    }
    return &index->segments[seg_idx];
}

int ts_segment_flush_current(ts_segment_index_t *index) {
    if (index == NULL || index->buffer_count == 0) return 0;

    LOG_DEBUG("刷新段: start_ts=%ld, points=%u",
              index->current_seg.start_ts, index->buffer_count);

    /* 暂时不实现压缩，将数据写入文件 */
    /* TODO: 集成 ts_compress 模块进行实际压缩 */

    /* 确保目录存在 */
    ensure_segments_dir(index);

    /* 获取段文件路径 */
    char seg_path[512];
    uint32_t seg_idx = index->seg_count;
    get_segment_path(index, seg_idx, seg_path, sizeof(seg_path));

    FILE *fp = fopen(seg_path, "ab");
    if (fp == NULL) {
        LOG_ERROR("无法打开段文件: %s", seg_path);
        return -1;
    }

    /* 写入原始数据（简化版本） */
    size_t written = fwrite(&index->current_seg.start_ts, sizeof(int64_t), 1, fp);
    written += fwrite(&index->buffer_count, sizeof(uint32_t), 1, fp);
    /* 这里应该写入压缩数据，暂时写入元数据 */
    (void)written;

    fclose(fp);

    /* 更新段元数据 */
    index->current_seg.point_count = index->buffer_count;
    index->current_seg.compressed_size = (uint32_t)(index->buffer_count * 16);  /* 估算 */

    /* 添加到段数组 */
    if (index->seg_count >= index->seg_capacity) {
        uint32_t new_cap = index->seg_capacity * 2;
        ts_segment_meta_t *new_segs = (ts_segment_meta_t *)realloc(
            index->segments, new_cap * sizeof(ts_segment_meta_t));
        if (new_segs == NULL) {
            LOG_ERROR("扩容段数组失败");
            return -1;
        }
        index->segments = new_segs;
        index->seg_capacity = new_cap;
    }

    index->segments[index->seg_count++] = index->current_seg;

    /* 重置当前段 */
    memset(&index->current_seg, 0, sizeof(index->current_seg));
    index->current_seg.start_ts = -1;
    index->buffer_count = 0;

    return 0;
}

/* ========================================================================
 * TTL 清理 API 实现
 * ======================================================================== */

uint32_t ts_segment_expire(ts_segment_index_t *index, int64_t older_than_ms) {
    if (index == NULL) return 0;

    uint32_t deleted = 0;
    uint64_t kept_points = 0;

    /* 查找需要删除的段 */
    for (uint32_t i = 0; i < index->seg_count; i++) {
        if (index->segments[i].end_ts < older_than_ms) {
            /* 删除段文件 */
            char seg_path[512];
            get_segment_path(index, i, seg_path, sizeof(seg_path));
            remove(seg_path);

            /* 标记为删除（简单起见，后续可以压缩数组） */
            index->segments[i].point_count = 0;
            deleted++;
            LOG_DEBUG("删除过期段: seg=%u, end_ts=%ld", i, index->segments[i].end_ts);
        } else {
            kept_points += index->segments[i].point_count;
        }
    }

    /* 更新总数据点数 */
    index->total_points = kept_points;

    if (deleted > 0) {
        LOG_INFO("TTL 清理完成: 删除 %u 个段", deleted);
    }

    return deleted;
}
