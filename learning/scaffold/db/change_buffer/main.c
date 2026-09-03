/**
 * @file main.c
 * @brief Change Buffer 写缓冲演示
 *
 * Change Buffer 用于缓存对非唯一二级索引的修改，避免频繁磁盘 I/O。
 * 当索引页面不在 Buffer Pool 中时，先将修改写入 Change Buffer；
 * 当页面被读取时，再将 Change Buffer 中的修改 Merge 到主索引。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/* ============================================================
 * 常量与类型定义
 * ============================================================ */

#define IBUF_MAX_ENTRIES 128
#define INDEX_PAGE_SIZE  8192

/** Change Buffer 条目类型 */
typedef enum {
    IBUF_INSERT = 0,   /**< 插入操作 */
    IBUF_DELETE = 1,   /**< 删除操作 */
    IBUF_UPDATE = 2    /**< 更新操作 */
} ibuf_entry_type_t;

/** Change Buffer 条目 */
typedef struct {
    int         index_id;           /**< 索引 ID */
    uint64_t    page_key;           /**< 页面键值 (primary key) */
    ibuf_entry_type_t type;         /**< 操作类型 */
    char        indexed_value[64];  /**< 索引值 */
    bool        valid;              /**< 条目是否有效 */
} ibuf_entry_t;

/** Change Buffer 控制块 */
typedef struct {
    ibuf_entry_t entries[IBUF_MAX_ENTRIES];
    int          count;             /**< 当前条目数 */
    int          merge_count;       /**< 已合并次数 */
} change_buffer_t;

/* ============================================================
 * Change Buffer 全局实例
 * ============================================================ */

static change_buffer_t g_cb;

/* ============================================================
 * Change Buffer 操作
 * ============================================================ */

/**
 * @brief 初始化 Change Buffer
 */
static void ibuf_init(void) {
    memset(&g_cb, 0, sizeof(g_cb));
    printf("[change_buf] Change Buffer 初始化完成，容量: %d\n", IBUF_MAX_ENTRIES);
}

/**
 * @brief 将 DML 操作写入 Change Buffer
 * @param index_id 索引 ID
 * @param page_key 页面键值
 * @param type 操作类型
 * @param value 索引值
 * @return 0 成功，-1 失败
 */
static int ibuf_insert(int index_id, uint64_t page_key,
                       ibuf_entry_type_t type, const char *value) {
    if (g_cb.count >= IBUF_MAX_ENTRIES) {
        printf("[change_buf] Change Buffer 已满，无法写入\n");
        return -1;
    }

    ibuf_entry_t *e = &g_cb.entries[g_cb.count++];
    e->index_id = index_id;
    e->page_key = page_key;
    e->type = type;
    strncpy(e->indexed_value, value, sizeof(e->indexed_value) - 1);
    e->valid = true;

    const char *op_name[] = {"INSERT", "DELETE", "UPDATE"};
    printf("[change_buf] 写入 Change Buffer: idx=%d, key=%u, op=%s, val=%s\n",
           index_id, (unsigned int)page_key, op_name[type], value);
    return 0;
}

/**
 * @brief 将 Change Buffer 中的条目 Merge 到主索引
 * @param index_id 目标索引 ID
 * @param page_key 目标页面键值
 * @return 合并的条目数
 */
static int ibuf_merge_to_index(int index_id, uint64_t page_key) {
    int merged = 0;

    for (int i = 0; i < g_cb.count; i++) {
        ibuf_entry_t *e = &g_cb.entries[i];
        if (!e->valid) continue;

        /* 找到属于该页面的条目 */
        if (e->index_id == index_id) {
            const char *op_name[] = {"INSERT", "DELETE", "UPDATE"};
            printf("[change_buf] Merge 到主索引: idx=%d, key=%u, op=%s, val=%s\n",
                   e->index_id, (unsigned int)e->page_key, op_name[e->type], e->indexed_value);
            e->valid = false;
            merged++;
            g_cb.merge_count++;
        }
    }

    return merged;
}

/**
 * @brief 打印 Change Buffer 状态
 */
static void ibuf_dump(void) {
    printf("[change_buf] Change Buffer 状态: count=%d, merged=%d\n",
           g_cb.count, g_cb.merge_count);
    printf("[change_buf] 条目列表:\n");
    for (int i = 0; i < g_cb.count; i++) {
        ibuf_entry_t *e = &g_cb.entries[i];
        if (!e->valid) continue;
        const char *op_name[] = {"INSERT", "DELETE", "UPDATE"};
        printf("  [%d] idx=%d, key=%u, op=%s, val=%s\n",
               i, e->index_id, (unsigned int)e->page_key, op_name[e->type], e->indexed_value);
    }
}

/* ============================================================
 * 演示场景
 * ============================================================ */

int main(void) {
    printf("=== Change Buffer 演示 ===\n\n");

    /* 1. 初始化 */
    ibuf_init();
    printf("\n");

    /* 2. 模拟对非唯一二级索引的 DML 操作
     *    假设页面 100 不在 Buffer Pool 中，先写入 Change Buffer */
    printf("--- 场景: 批量插入导致页面未命中 ---\n");

    /* 这些操作会写入 Change Buffer，而不是直接修改主索引 */
    ibuf_insert(1, 100, IBUF_INSERT, "zhang_san");   /* idx=1: name 索引 */
    ibuf_insert(1, 101, IBUF_INSERT, "li_si");
    ibuf_insert(1, 102, IBUF_INSERT, "wang_wu");
    ibuf_insert(2, 100, IBUF_INSERT, "2024-01-15");  /* idx=2: create_date 索引 */
    ibuf_insert(2, 101, IBUF_INSERT, "2024-02-20");
    printf("\n");

    /* 3. 查看 Change Buffer 状态 */
    ibuf_dump();
    printf("\n");

    /* 4. 当页面 100 被读入 Buffer Pool 时，Merge Change Buffer */
    printf("--- 场景: 页面 100 被访问，触发 Merge ---\n");
    int merged = ibuf_merge_to_index(1, 100);
    printf("Merge 完成: %d 条目合并到索引 1\n\n", merged);

    /* 5. 最终状态 */
    ibuf_dump();
    printf("\n");

    printf("=== 演示结束 ===\n");
    return 0;
}
