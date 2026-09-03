/**
 * @file vecx_hashagg.c
 * @brief Gap#2 向量化执行引擎 Task5：哈希分组聚合算子（GROUP BY）
 *
 * 语义约定：
 * - 键 null 或度量 null → 整行跳过（该行既不建组也不计数）。
 *   这与 SQL 的 COUNT(m) 语义（度量 null 时该组仍存在、count 不+1）不一致，
 *   选这个更简单的语义是教学级取舍。
 * - 聚合状态（sum/min/max）统一存 double，不按度量类型分叉——
 *   理由：分组状态若按类型分叉会让槽结构复杂一倍，教学级不值得。
 *   测试不要用超过 2^53 的度量值。
 * - MIN/MAX 用首个有效值做初值，不用哨兵，极值数据也能正确返回。
 * - avg 在 emit 时算 sum / (double)count，不存在槽里。
 *
 * null 判定：直接按位读 b->null_bitmap（防御性判 NULL，NULL 视为无 null 行）。
 * 注意：null_bitmap 是行级的（不是每列一个位图），键 null 和度量 null 用同一 bit，
 * 这正是"3.1 语义取舍"能成立的原因（行级 null 无法区分是哪一列 null，
 * 所以简单的"整行跳过"是最自然的教学级实现）。
 */

#include "db/vectorized/vectorized.h"
#include "db/core/columnar_store.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

/* ========================================================================
 * 开放寻址哈希表
 * ======================================================================== */

#define HASHAGG_INITIAL_CAP 64
#define HASHAGG_LOAD_THRESHOLD_NUM 7   /* 分子 */
#define HASHAGG_LOAD_THRESHOLD_DEN 10  /* 分母：7/10 = 0.7 */

/**
 * 哈希槽：键 + 聚合状态。
 *
 * 精度取舍：sum 用 double 累加，与 vecx_agg.c 的标量聚合不同——那里对整型列
 * 用 int64 累加器以保住 2^53 以上的精度。这里统一用 double，因为槽结构和输出
 * 块的 sum 列都是 double；代价是整型度量列超过 2^53 后会丢低位。
 */
typedef struct {
    int64_t key;
    int64_t count;
    double  sum;
    double  min;
    double  max;
    int     used;   /* 0=空槽。不要用 key==0 做空标记（key 完全可能是 0） */
} hashagg_slot_t;

/** splitmix64 finalizer — 混淆器，防止连续键直接取模退化。 */
static inline uint64_t hashagg_hash_i64(int64_t k) {
    uint64_t x = (uint64_t)k;
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27; x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

/** 聚合器结构（不透明） */
struct vecx_hashagg_s {
    int key_col;
    int measure_col;

    hashagg_slot_t *slots;
    int capacity;       /* 2 的幂 */
    int num_groups;     /* 已建组数（used 槽数） */
};

/* ========================================================================
 * 内部辅助
 * ======================================================================== */

/**
 * 判断某行是否为 null（防御性：null_bitmap==NULL 视为无 null 行）。
 * 与 vecx_agg.c 里的 agg_is_null 逐字等价——两处都是 static inline，各自
 * 保留一份而不抽公共头文件，是为了让每个 .c 都能单独读懂。若要改语义
 * （比如换 bitmap 位序），两处必须同步改。
 */
static inline int row_is_null(const VectorBlock *b, int row) {
    if (!b->null_bitmap) return 0;
    return (b->null_bitmap[row / 64] & (1ULL << (row % 64))) != 0;
}

/** 创建聚合器 */
vecx_hashagg_t *vecx_hashagg_create(int key_col, int measure_col) {
    if (key_col < 0 || measure_col < 0) return NULL;

    vecx_hashagg_t *h = (vecx_hashagg_t *)calloc(1, sizeof(vecx_hashagg_t));
    if (!h) return NULL;

    h->key_col = key_col;
    h->measure_col = measure_col;
    h->capacity = HASHAGG_INITIAL_CAP;
    h->num_groups = 0;
    h->slots = (hashagg_slot_t *)calloc((size_t)h->capacity, sizeof(hashagg_slot_t));
    if (!h->slots) {
        free(h);
        return NULL;
    }
    return h;
}

/**
 * 扩容：容量翻倍并重新哈希所有已有槽。
 * 返回 0 成功，-1 OOM（OOM 时旧 slots 不变，保证调用方可继续使用）。
 */
static int hashagg_grow(vecx_hashagg_t *h) {
    int new_cap = h->capacity * 2;
    hashagg_slot_t *new_slots = (hashagg_slot_t *)calloc((size_t)new_cap, sizeof(hashagg_slot_t));
    if (!new_slots) return -1;

    for (int i = 0; i < h->capacity; i++) {
        if (!h->slots[i].used) continue;
        uint64_t hcode = hashagg_hash_i64(h->slots[i].key);
        int pos = (int)(hcode & ((uint64_t)new_cap - 1));
        while (new_slots[pos].used) {
            pos++;
            if (pos >= new_cap) pos = 0;
        }
        new_slots[pos] = h->slots[i];
    }

    free(h->slots);
    h->slots = new_slots;
    h->capacity = new_cap;
    return 0;
}

/**
 * 查找或插入一个键。
 * 返回指向目标槽的指针；若 used==0 表示新槽（caller 负责初始化 min/max）。
 * OOM 时返回 NULL（此时已有状态不变）。
 */
static hashagg_slot_t *hashagg_find_or_insert(vecx_hashagg_t *h, int64_t key) {
    /* 扩容检查 */
    if (h->num_groups * HASHAGG_LOAD_THRESHOLD_DEN >=
        h->capacity * HASHAGG_LOAD_THRESHOLD_NUM) {
        if (hashagg_grow(h) != 0) return NULL;
    }

    uint64_t hcode = hashagg_hash_i64(key);
    int pos = (int)(hcode & ((uint64_t)h->capacity - 1));

    while (1) {
        if (!h->slots[pos].used) {
            h->slots[pos].key = key;
            h->slots[pos].used = 1;
            h->slots[pos].count = 0;
            h->slots[pos].sum = 0.0;
            h->num_groups++;
            return &h->slots[pos];
        }
        if (h->slots[pos].key == key) {
            return &h->slots[pos];
        }
        pos++;
        if (pos >= h->capacity) pos = 0;
    }
}

/* ========================================================================
 * 公开接口
 * ======================================================================== */

/**
 * 把一个块的数据累积进聚合器。
 *
 * 逐行处理：键列为 null 的行整行跳过；度量列为 null 的行也整行跳过。
 * 先校验类型，再进入行循环。
 * 注意：行循环内 hashagg_find_or_insert → hashagg_grow 可能 calloc 失败，
 * 此时本块前 N 行已被计入，返回 -1 不代表"本块完全未计入"。
 * 调用方遇到 -1 应视聚合器状态为不确定并丢弃。
 *
 * @param h 聚合器
 * @param b 输入块（不被修改）
 * @return 0 成功；-1 入参非法 / 列类型不支持 / OOM
 */
int vecx_hashagg_add_block(vecx_hashagg_t *h, const VectorBlock *b) {
    if (!h || !b) return -1;

    const int key_type = vector_block_get_column_type(b, h->key_col);
    const int meas_type = vector_block_get_column_type(b, h->measure_col);

    /* 键列只支持 INT32 / INT64 */
    if (key_type != COLUMN_INT32 && key_type != COLUMN_INT64) return -1;
    /* 度量列支持 INT32 / INT64 / FLOAT / DOUBLE */
    if (meas_type != COLUMN_INT32 && meas_type != COLUMN_INT64 &&
        meas_type != COLUMN_FLOAT && meas_type != COLUMN_DOUBLE) return -1;

    if (!b->columns || !b->columns[h->key_col] || !b->columns[h->measure_col]) {
        return -1;
    }

    const void *key_data = b->columns[h->key_col];
    const void *meas_data = b->columns[h->measure_col];
    const int nrows = b->num_rows;

    for (int row = 0; row < nrows; row++) {
        /* 行级 null 跳过：键 null 或度量 null 都整行跳过 */
        if (row_is_null(b, row)) continue;

        int64_t key;
        if (key_type == COLUMN_INT32) {
            key = (int64_t)((const int32_t *)key_data)[row];
        } else {
            key = ((const int64_t *)key_data)[row];
        }

        double v;
        if (meas_type == COLUMN_INT32) {
            v = (double)((const int32_t *)meas_data)[row];
        } else if (meas_type == COLUMN_INT64) {
            v = (double)((const int64_t *)meas_data)[row];
        } else if (meas_type == COLUMN_FLOAT) {
            v = (double)((const float *)meas_data)[row];
        } else {
            v = ((const double *)meas_data)[row];
        }

        hashagg_slot_t *slot = hashagg_find_or_insert(h, key);
        if (!slot) return -1;  /* OOM */

        if (slot->count == 0) {
            slot->min = v;
            slot->max = v;
        } else {
            if (v < slot->min) slot->min = v;
            if (v > slot->max) slot->max = v;
        }
        slot->sum += v;
        slot->count++;
    }

    return 0;
}

/**
 * 输出聚合结果块。
 *
 * 输出块共 6 列，行数 = distinct 分组数：
 *   列0 key   : int64_t，类型标签 COLUMN_INT64
 *   列1 count : int64_t，类型标签 COLUMN_INT64
 *   列2 sum   : double， 类型标签 COLUMN_DOUBLE
 *   列3 min   : double， 类型标签 COLUMN_DOUBLE
 *   列4 max   : double， 类型标签 COLUMN_DOUBLE
 *   列5 avg   : double， 类型标签 COLUMN_DOUBLE
 *
 * 行顺序不做保证（哈希槽顺序）。emit 不清空内部状态。
 *
 * @param h   聚合器
 * @param out 输出块指针；无分组时写 NULL
 * @return >0 分组数；0 无分组（*out=NULL）；-1 入参非法 / OOM
 */
int vecx_hashagg_emit(vecx_hashagg_t *h, VectorBlock **out) {
    if (!h || !out) return -1;
    *out = NULL;

    if (h->num_groups == 0) return 0;

    VectorBlock *b = vector_block_create(h->num_groups, 6);
    if (!b) return -1;

    /* 分配 6 列缓冲 */
    int64_t *key_buf = (int64_t *)malloc((size_t)h->num_groups * sizeof(int64_t));
    int64_t *cnt_buf = (int64_t *)malloc((size_t)h->num_groups * sizeof(int64_t));
    double  *sum_buf = (double  *)malloc((size_t)h->num_groups * sizeof(double));
    double  *min_buf = (double  *)malloc((size_t)h->num_groups * sizeof(double));
    double  *max_buf = (double  *)malloc((size_t)h->num_groups * sizeof(double));
    double  *avg_buf = (double  *)malloc((size_t)h->num_groups * sizeof(double));

    if (!key_buf || !cnt_buf || !sum_buf || !min_buf || !max_buf || !avg_buf) {
        free(key_buf); free(cnt_buf); free(sum_buf); free(min_buf); free(max_buf); free(avg_buf);
        vector_block_destroy(b);
        return -1;
    }

    int j = 0;
    for (int i = 0; i < h->capacity; i++) {
        if (!h->slots[i].used) continue;
        key_buf[j] = h->slots[i].key;
        cnt_buf[j] = h->slots[i].count;
        sum_buf[j] = h->slots[i].sum;
        min_buf[j] = h->slots[i].min;
        max_buf[j] = h->slots[i].max;
        avg_buf[j] = h->slots[i].sum / (double)h->slots[i].count;
        j++;
    }

    vector_block_set_column(b, 0, key_buf, (int)sizeof(int64_t));
    vector_block_set_column_type(b, 0, COLUMN_INT64);
    vector_block_set_column(b, 1, cnt_buf, (int)sizeof(int64_t));
    vector_block_set_column_type(b, 1, COLUMN_INT64);
    vector_block_set_column(b, 2, sum_buf, (int)sizeof(double));
    vector_block_set_column_type(b, 2, COLUMN_DOUBLE);
    vector_block_set_column(b, 3, min_buf, (int)sizeof(double));
    vector_block_set_column_type(b, 3, COLUMN_DOUBLE);
    vector_block_set_column(b, 4, max_buf, (int)sizeof(double));
    vector_block_set_column_type(b, 4, COLUMN_DOUBLE);
    vector_block_set_column(b, 5, avg_buf, (int)sizeof(double));
    vector_block_set_column_type(b, 5, COLUMN_DOUBLE);
    vector_block_set_num_rows(b, h->num_groups);

    *out = b;
    return h->num_groups;
}

/** 销毁聚合器（NULL 安全） */
void vecx_hashagg_destroy(vecx_hashagg_t *h) {
    if (!h) return;
    free(h->slots);
    free(h);
}
