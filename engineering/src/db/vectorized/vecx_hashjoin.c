/**
 * @file vecx_hashjoin.c
 * @brief Gap#2 向量化执行引擎 Task6：向量化 Hash Join（inner join）
 *
 * 语义约定：
 * - inner join：probe 行无匹配则丢弃；
 * - build 侧同键多行 → 该 probe 行与每个匹配的 build 行各产出一行（键内笛卡尔积）；
 * - 键列为 null 的行不入表（SQL 语义：null 不与任何值相等，包括另一个 null）；
 * - probe 侧键 null 不参与探测；
 * - 输出行的 null 标记 = build 行 null || probe 行 null（行级位图的固有限制）。
 *
 * null 判定：直接按位读 b->null_bitmap（防御性判 NULL）。
 * 注意：null_bitmap 是行级的（不是每列一个位图），键 null 和非键列 null 用同一 bit，
 * "键列为 null 的行不入表"实际等价于"整行被标 null 的行不入表"。
 *
 * 哈希表设计（参考 vecx_hashagg.c 的 splitmix64 混淆器）：
 * - 开放寻址（线性探测），2 的幂容量，0.7 装载因子扩容；
 * - 用 `used` 字段做空槽哨兵（不能用 key==0 或 head==NULL）；
 * - 一键多行：同键多行用链表串起来（hj_row_t 节点）。
 *
 * 深拷贝：add_build 对 build 块做深拷贝，probe 阶段可回读所有列。
 * 实现直接复用 vecx_block_gather（全行选择向量），已含逐列 memcpy +
 * 类型标签复制 + null 重映射。
 *
 * 两遍扫描：probe 时先数匹配数再分配精确大小的输出块，清晰优先；
 * 生产实现会用一遍+分块输出。
 */

#include "db/vectorized/vectorized.h"
#include "db/core/columnar_store.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * splitmix64 哈希混淆器（参考 vecx_hashagg.c 的 hashagg_hash_i64）
 * ======================================================================== */
static inline uint64_t hj_hash_i64(int64_t k) {
    uint64_t x = (uint64_t)k;
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27; x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

/* ========================================================================
 * 辅助类型
 * ======================================================================== */

/* 同键多行链表节点 */
typedef struct hj_row_s {
    int              block_idx;  /* build 块副本数组下标 */
    int              row_idx;    /* 块内行号 */
    struct hj_row_s *next;       /* 同键的下一行 */
    struct hj_row_s *pool_next;  /* 全局分配池链表（destroy 时统一释放） */
} hj_row_t;

/* 哈希槽 */
typedef struct {
    int64_t   key;
    hj_row_t *head;   /* 该键的链表头；无匹配时为 NULL */
    int       used;   /* 0=空槽（不要用 key==0 或 head==NULL 做哨兵） */
} hj_slot_t;

/* ========================================================================
 * Hash Join 句柄
 * ======================================================================== */

struct vecx_hashjoin_s {
    int build_key_col;
    int probe_key_col;

    /* build 侧块副本数组（深拷贝持有） */
    VectorBlock **build_blocks;
    int build_block_count;
    int build_block_capacity;

    /* build schema（第一个非空 build 块确立） */
    int build_ncols;
    int *build_column_sizes;
    int *build_column_types;

    /* 哈希表 */
    int capacity;          /* 必须是 2 的幂 */
    int num_entries;       /* 已插入的 entry 数（不含 used=0 槽） */
    int resize_threshold;  /* 触发扩容的 entry 数上限（capacity * 7/10） */
    hj_slot_t *slots;

    /* 全局 hj_row_t 分配池链表头 */
    hj_row_t *row_pool;
};

/* ========================================================================
 * 哈希表扩容（只搬 slot 数组，hj_row_t 节点原地不动）
 * ======================================================================== */
static int hj_resize(vecx_hashjoin_t *j) {
    int new_cap = j->capacity * 2;
    if (new_cap <= 0) return -1;

    hj_slot_t *new_slots = (hj_slot_t *)calloc((size_t)new_cap, sizeof(hj_slot_t));
    if (!new_slots) return -1;

    /* 重新插入所有 entry（只搬 key + head 指针，不搬节点） */
    for (int i = 0; i < j->capacity; i++) {
        if (!j->slots[i].used) continue;

        int64_t key = j->slots[i].key;
        hj_row_t *head = j->slots[i].head;

        uint64_t hash = hj_hash_i64(key);
        int idx = (int)(hash & (uint64_t)(new_cap - 1));
        while (new_slots[idx].used) {
            idx = (idx + 1) & (new_cap - 1);
        }
        new_slots[idx].key = key;
        new_slots[idx].head = head;
        new_slots[idx].used = 1;
    }

    free(j->slots);
    j->slots = new_slots;
    j->capacity = new_cap;
    j->resize_threshold = (int)((uint64_t)new_cap * 7ULL / 10ULL);
    return 0;
}

/* ========================================================================
 * 探测哈希表：找到 key 对应的槽，或返回 NULL
 * ======================================================================== */
static hj_slot_t *hj_find_slot(vecx_hashjoin_t *j, int64_t key) {
    if (!j || !j->slots || j->capacity <= 0) return NULL;

    uint64_t hash = hj_hash_i64(key);
    int idx = (int)(hash & (uint64_t)(j->capacity - 1));

    while (j->slots[idx].used) {
        if (j->slots[idx].key == key) {
            return &j->slots[idx];
        }
        idx = (idx + 1) & (j->capacity - 1);
    }
    return NULL;
}

/* ========================================================================
 * 插入键到哈希表：返回槽指针（若已存在则直接返回已有槽），OOM 返回 NULL
 * ======================================================================== */
static hj_slot_t *hj_insert_key(vecx_hashjoin_t *j, int64_t key) {
    if (!j || !j->slots) return NULL;

    /* 检查是否需要扩容 */
    if (j->num_entries >= j->resize_threshold) {
        if (hj_resize(j) != 0) return NULL;
    }

    uint64_t hash = hj_hash_i64(key);
    int idx = (int)(hash & (uint64_t)(j->capacity - 1));

    while (j->slots[idx].used) {
        if (j->slots[idx].key == key) {
            return &j->slots[idx];  /* 已存在 */
        }
        idx = (idx + 1) & (j->capacity - 1);
    }

    /* 找到空槽 */
    j->slots[idx].key = key;
    j->slots[idx].head = NULL;
    j->slots[idx].used = 1;
    j->num_entries++;
    return &j->slots[idx];
}

/* ========================================================================
 * 读取键值，统一提升为 int64
 * ======================================================================== */
static int64_t hj_read_key(const VectorBlock *b, int col, int row, int key_type) {
    if (key_type == COLUMN_INT32) {
        const int32_t *arr = (const int32_t *)b->columns[col];
        return (int64_t)arr[row];
    }
    const int64_t *arr = (const int64_t *)b->columns[col];
    return arr[row];
}

/* ========================================================================
 * 公开接口
 * ======================================================================== */

#define HJ_INITIAL_CAP 64

vecx_hashjoin_t *vecx_hashjoin_create(int build_key_col, int probe_key_col) {
    if (build_key_col < 0 || probe_key_col < 0) return NULL;

    vecx_hashjoin_t *j = (vecx_hashjoin_t *)calloc(1, sizeof(vecx_hashjoin_t));
    if (!j) return NULL;

    j->build_key_col = build_key_col;
    j->probe_key_col = probe_key_col;

    j->capacity = HJ_INITIAL_CAP;
    j->resize_threshold = (int)((uint64_t)HJ_INITIAL_CAP * 7ULL / 10ULL);
    j->num_entries = 0;

    j->slots = (hj_slot_t *)calloc((size_t)HJ_INITIAL_CAP, sizeof(hj_slot_t));
    if (!j->slots) {
        free(j);
        return NULL;
    }

    j->build_blocks = NULL;
    j->build_block_count = 0;
    j->build_block_capacity = 0;
    j->build_ncols = 0;
    j->build_column_sizes = NULL;
    j->build_column_types = NULL;
    j->row_pool = NULL;

    return j;
}

int vecx_hashjoin_add_build(vecx_hashjoin_t *j, const VectorBlock *build) {
    if (!j || !build) return -1;

    int nrows = build->num_rows;
    if (nrows <= 0) return 0;

    int key_col = j->build_key_col;
    if (key_col < 0 || key_col >= build->num_columns) return -1;

    int key_type = vector_block_get_column_type(build, key_col);
    if (key_type != COLUMN_INT32 && key_type != COLUMN_INT64) return -1;

    /* 防御性：部分初始化的块（column_type 已设但 columns[col]==NULL） */
    if (!build->columns || !build->columns[key_col]) return -1;

    /* ======== schema 校验或确立（先校验，再修改状态） ======== */
    if (j->build_block_count == 0) {
        /* 第一个非空块：确立 schema */
        int *new_sizes = (int *)malloc(sizeof(int) * (size_t)build->num_columns);
        int *new_types = (int *)malloc(sizeof(int) * (size_t)build->num_columns);
        if (!new_sizes || !new_types) {
            free(new_sizes);
            free(new_types);
            return -1;
        }
        for (int c = 0; c < build->num_columns; c++) {
            new_sizes[c] = build->column_sizes[c];
            new_types[c] = vector_block_get_column_type(build, c);
        }
        j->build_ncols = build->num_columns;
        j->build_column_sizes = new_sizes;
        j->build_column_types = new_types;
    } else {
        /* 校验一致性 */
        if (build->num_columns != j->build_ncols) return -1;
        for (int c = 0; c < build->num_columns; c++) {
            if (build->column_sizes[c] != j->build_column_sizes[c]) return -1;
            if (vector_block_get_column_type(build, c) != j->build_column_types[c]) return -1;
        }
    }

    /* ======== 深拷贝 build 块（复用 vecx_block_gather） ======== */
    int *sel = (int *)malloc(sizeof(int) * (size_t)nrows);
    if (!sel) {
        /* M5: 第一个块建立的 schema 未提交到任何块，须释放 */
        if (j->build_block_count == 0) {
            free(j->build_column_sizes);
            free(j->build_column_types);
            j->build_column_sizes = NULL;
            j->build_column_types = NULL;
        }
        return -1;
    }
    for (int i = 0; i < nrows; i++) sel[i] = i;

    VectorBlock *copy = vecx_block_gather(build, sel, nrows);
    free(sel);
    if (!copy) {
        if (j->build_block_count == 0) {
            free(j->build_column_sizes);
            free(j->build_column_types);
            j->build_column_sizes = NULL;
            j->build_column_types = NULL;
        }
        return -1;
    }

    /* ======== 存入 build_blocks 数组 ======== */
    if (j->build_block_count >= j->build_block_capacity) {
        int new_cap = j->build_block_capacity == 0 ? 8 : j->build_block_capacity * 2;
        VectorBlock **new_arr = (VectorBlock **)realloc(
            j->build_blocks, sizeof(VectorBlock *) * (size_t)new_cap);
        if (!new_arr) {
            vector_block_destroy(copy);
            if (j->build_block_count == 0) {
                free(j->build_column_sizes);
                free(j->build_column_types);
                j->build_column_sizes = NULL;
                j->build_column_types = NULL;
            }
            return -1;
        }
        j->build_blocks = new_arr;
        j->build_block_capacity = new_cap;
    }
    int block_idx = j->build_block_count;
    j->build_blocks[block_idx] = copy;
    j->build_block_count++;

    /* ======== 逐行插入哈希表 ======== */
    for (int r = 0; r < nrows; r++) {
        if (vector_block_is_null((VectorBlock *)build, r)) continue;

        int64_t key = hj_read_key(build, key_col, r, key_type);

        hj_slot_t *slot = hj_insert_key(j, key);
        if (!slot) return -1;

        /* 创建 hj_row_t 节点，头插到链 */
        hj_row_t *rn = (hj_row_t *)malloc(sizeof(hj_row_t));
        if (!rn) return -1;
        rn->block_idx = block_idx;
        rn->row_idx = r;
        rn->next = slot->head;
        rn->pool_next = j->row_pool;
        slot->head = rn;
        j->row_pool = rn;
    }

    return 0;
}

int vecx_hashjoin_probe(vecx_hashjoin_t *j, const VectorBlock *probe, VectorBlock **out) {
    if (!j || !probe || !out) return -1;

    int nrows = probe->num_rows;
    if (nrows <= 0 || j->build_block_count <= 0) {
        *out = NULL;
        return 0;
    }

    int pk_col = j->probe_key_col;
    if (pk_col < 0 || pk_col >= probe->num_columns) return -1;

    int pk_type = vector_block_get_column_type(probe, pk_col);
    if (pk_type != COLUMN_INT32 && pk_type != COLUMN_INT64) return -1;

    /* 防御性：部分初始化的块（column_type 已设但 columns[col]==NULL） */
    if (!probe->columns || !probe->columns[pk_col]) return -1;

    int build_ncols = j->build_ncols;
    int probe_ncols = probe->num_columns;
    int out_ncols = build_ncols + probe_ncols;

    /* ========== 第一遍：数匹配数 ========== */
    int total_matches = 0;
    int *probe_match_counts = (int *)calloc((size_t)nrows, sizeof(int));
    if (!probe_match_counts) return -1;

    for (int r = 0; r < nrows; r++) {
        if (vector_block_is_null((VectorBlock *)probe, r)) {
            probe_match_counts[r] = 0;
            continue;
        }

        int64_t key = hj_read_key(probe, pk_col, r, pk_type);
        hj_slot_t *slot = hj_find_slot(j, key);

        if (!slot) {
            probe_match_counts[r] = 0;
            continue;
        }

        int count = 0;
        for (hj_row_t *rn = slot->head; rn; rn = rn->next) {
            count++;
        }
        probe_match_counts[r] = count;
        total_matches += count;
    }

    if (total_matches <= 0) {
        free(probe_match_counts);
        *out = NULL;
        return 0;
    }

    /* ========== 分配输出块 ========== */
    VectorBlock *result = vector_block_create(total_matches, out_ncols);
    if (!result) {
        free(probe_match_counts);
        return -1;
    }

    /* 记录每列元素大小，用于回滚 */
    int *out_elem_sizes = (int *)malloc(sizeof(int) * (size_t)out_ncols);
    if (!out_elem_sizes) {
        vector_block_destroy(result);
        free(probe_match_counts);
        return -1;
    }

    /* build 侧列 */
    for (int c = 0; c < build_ncols; c++) {
        int elem = j->build_column_sizes[c];
        out_elem_sizes[c] = elem;
        if (elem > 0) {
            char *buf = (char *)malloc((size_t)total_matches * (size_t)elem);
            if (!buf) {
                for (int c2 = 0; c2 < c; c2++) {
                    if (out_elem_sizes[c2] > 0 && result->columns[c2]) {
                        free(result->columns[c2]);
                        result->columns[c2] = NULL;
                    }
                }
                vector_block_destroy(result);
                free(out_elem_sizes);
                free(probe_match_counts);
                return -1;
            }
            vector_block_set_column(result, c, buf, elem);
        }
        vector_block_set_column_type(result, c, j->build_column_types[c]);
    }

    /* probe 侧列 */
    for (int c = 0; c < probe_ncols; c++) {
        int out_c = build_ncols + c;
        int elem = probe->column_sizes[c];
        out_elem_sizes[out_c] = elem;
        if (elem > 0) {
            char *buf = (char *)malloc((size_t)total_matches * (size_t)elem);
            if (!buf) {
                for (int c2 = 0; c2 < out_c; c2++) {
                    if (out_elem_sizes[c2] > 0 && result->columns[c2]) {
                        free(result->columns[c2]);
                        result->columns[c2] = NULL;
                    }
                }
                vector_block_destroy(result);
                free(out_elem_sizes);
                free(probe_match_counts);
                return -1;
            }
            vector_block_set_column(result, out_c, buf, elem);
        }
        vector_block_set_column_type(result, out_c,
            vector_block_get_column_type(probe, c));
    }

    /* ========== 第二遍：填充输出 ========== */
    int out_row = 0;
    for (int r = 0; r < nrows; r++) {
        if (probe_match_counts[r] == 0) continue;

        int64_t key = hj_read_key(probe, pk_col, r, pk_type);
        hj_slot_t *slot = hj_find_slot(j, key);
        if (!slot) continue;

        for (hj_row_t *rn = slot->head; rn; rn = rn->next) {
            VectorBlock *build_block = j->build_blocks[rn->block_idx];
            int build_row = rn->row_idx;

            /* 复制 build 侧列 */
            for (int c = 0; c < build_ncols; c++) {
                int elem = out_elem_sizes[c];
                if (elem <= 0) continue;
                const char *src = (const char *)build_block->columns[c];
                char *dst = (char *)result->columns[c];
                if (src && dst) {
                    memcpy(dst + (size_t)out_row * (size_t)elem,
                           src + (size_t)build_row * (size_t)elem,
                           (size_t)elem);
                }
            }

            /* 复制 probe 侧列 */
            for (int c = 0; c < probe_ncols; c++) {
                int out_c = build_ncols + c;
                int elem = out_elem_sizes[out_c];
                if (elem <= 0) continue;
                const char *src = (const char *)probe->columns[c];
                char *dst = (char *)result->columns[out_c];
                if (src && dst) {
                    memcpy(dst + (size_t)out_row * (size_t)elem,
                           src + (size_t)r * (size_t)elem,
                           (size_t)elem);
                }
            }

            /* null 标记：build 行 null || probe 行 null */
            bool bnull = vector_block_is_null((VectorBlock *)build_block, build_row);
            bool pnull = vector_block_is_null((VectorBlock *)probe, r);
            // 行级位图下，两侧 null 行在入表/探测时均已被排除，bnull/pnull 恒为 0。
            // 此表达式在将来列级位图时保留语义正确性。
            vector_block_set_null(result, out_row, bnull || pnull);

            out_row++;
        }
    }

    vector_block_set_num_rows(result, total_matches);

    free(out_elem_sizes);
    free(probe_match_counts);

    *out = result;
    return total_matches;
}

void vecx_hashjoin_destroy(vecx_hashjoin_t *j) {
    if (!j) return;

    /* 释放所有 build 块副本 */
    for (int i = 0; i < j->build_block_count; i++) {
        vector_block_destroy(j->build_blocks[i]);
    }
    free(j->build_blocks);

    /* 释放 schema */
    free(j->build_column_sizes);
    free(j->build_column_types);

    /* 释放哈希表 */
    free(j->slots);

    /* 释放所有 hj_row_t 节点 */
    {
        hj_row_t *rn = j->row_pool;
        while (rn) {
            hj_row_t *next = rn->pool_next;
            free(rn);
            rn = next;
        }
    }

    free(j);
}