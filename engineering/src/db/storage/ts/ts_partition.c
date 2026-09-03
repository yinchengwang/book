/**
 * @file ts_partition.c
 * @brief 时序数据库分区 CRUD 实现
 *
 * 提供分区的创建、插入、查询操作。
 */
#include "db/storage/ts/ts_compress.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * 分区结构定义
 * ======================================================================== */

/** 分区类型（内部使用，区别于外部 header 的 ts_partition_info_t） */
typedef struct ts_partition {
    uint64_t start_time;       /**< 分区起始时间 */
    uint64_t end_time;         /**< 分区结束时间 */
    char filepath[256];        /**< 分区数据文件路径 */
    uint32_t segment_count;    /**< 分区内的数据点数 */
} ts_partition_t;

/* ========================================================================
 * 分区 CRUD 操作
 * ======================================================================== */

/**
 * @brief 创建新的分区
 *
 * @param dir 分区存储目录
 * @param start 分区起始时间
 * @param end 分区结束时间
 * @return 分区指针，失败返回 NULL
 */
ts_partition_t *ts_partition_create(const char *dir, uint64_t start, uint64_t end) {
    if (dir == NULL) return NULL;

    ts_partition_t *part = (ts_partition_t *)calloc(1, sizeof(ts_partition_t));
    if (part == NULL) return NULL;

    part->start_time = start;
    part->end_time = end;
    snprintf(part->filepath, sizeof(part->filepath), "%s/part_%lu.bin", dir, (unsigned long)start);
    part->segment_count = 0;

    return part;
}

/**
 * @brief 向分区插入数据点
 *
 * @param part 分区
 * @param point 数据点
 * @return 0 成功，-1 失败
 */
int ts_partition_insert(ts_partition_t *part, const ts_record_t *point) {
    if (part == NULL || point == NULL) return -1;

    FILE *f = fopen(part->filepath, "ab");
    if (f == NULL) {
        /* 尝试创建文件 */
        f = fopen(part->filepath, "wb");
        if (f == NULL) return -1;
    }

    fwrite(point, sizeof(ts_record_t), 1, f);
    fclose(f);

    part->segment_count++;
    return 0;
}

/**
 * @brief 查询分区内指定时间范围的数据
 *
 * @param part 分区
 * @param start 查询起始时间
 * @param end 查询结束时间
 * @param results 输出结果数组（调用者负责释放）
 * @param count 输出结果数量
 * @return 0 成功，-1 失败
 */
int ts_partition_query(ts_partition_t *part, uint64_t start, uint64_t end,
                       ts_record_t **results, uint32_t *count) {
    if (part == NULL || results == NULL || count == NULL) return -1;

    FILE *f = fopen(part->filepath, "rb");
    if (f == NULL) {
        *count = 0;
        return 0;
    }

    /* 初始容量 64，动态扩容 */
    uint32_t capacity = 64;
    *results = (ts_record_t *)malloc(capacity * sizeof(ts_record_t));
    if (*results == NULL) {
        fclose(f);
        *count = 0;
        return -1;
    }

    *count = 0;
    ts_record_t point;

    while (fread(&point, sizeof(ts_record_t), 1, f) == 1) {
        if (point.timestamp >= (int64_t)start && point.timestamp <= (int64_t)end) {
            if (*count >= capacity) {
                capacity *= 2;
                ts_record_t *new_results = (ts_record_t *)realloc(
                    *results, capacity * sizeof(ts_record_t));
                if (new_results == NULL) {
                    fclose(f);
                    free(*results);
                    *results = NULL;
                    *count = 0;
                    return -1;
                }
                *results = new_results;
            }
            (*results)[(*count)++] = point;
        }
    }

    fclose(f);
    return 0;
}

/**
 * @brief 销毁分区
 *
 * @param part 分区
 */
void ts_partition_destroy(ts_partition_t *part) {
    if (part) free(part);
}

/**
 * @brief 删除分区数据文件
 *
 * @param part 分区
 * @return 0 成功，-1 失败
 */
int ts_partition_remove(ts_partition_t *part) {
    if (part == NULL) return -1;
    return remove(part->filepath);
}
