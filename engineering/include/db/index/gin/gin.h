/*
 * gin.h
 *
 * GIN (Generalized Inverted Index) 通用倒排索引
 *
 * 功能：
 * - 支持 JSONB、数组、全文搜索等复合数据类型的快速检索
 * - 核心结构：Key → Posting List → Posting Array
 *
 * 使用示例：
 * @code
 *   gin_index_t *idx = gin_create(1024);
 *
 *   // 插入 key-doc_id 映射
 *   gin_insert(idx, "hello", 1);
 *   gin_insert(idx, "world", 1);
 *   gin_insert(idx, "hello", 2);
 *
 *   // 搜索
 *   int results[10];
 *   int count = 0;
 *   gin_search(idx, "hello", results, &count);
 *
 *   gin_destroy(idx);
 * @endcode
 */

#ifndef DB_INDEX_GIN_H
#define DB_INDEX_GIN_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 前向声明 ── */
typedef struct gin_index gin_index_t;
typedef struct posting_list posting_list_t;
typedef struct posting_array posting_array_t;

/* ── Posting List：同一 key 的文档 ID 有序链表（用于小列表） ── */
struct posting_list {
    int doc_id;                 /* 文档 ID */
    struct posting_list *next;  /* 下一个节点 */
};

/* ── Posting Array：有序数组（用于大列表，支持二分查找） ── */
struct posting_array {
    int *doc_ids;               /* 文档 ID 数组（有序） */
    int count;                  /* 当前数量 */
    int capacity;               /* 容量 */
};

/* ── 内部 Entry 结构 ── */
typedef struct gin_entry {
    char *key;                  /* 搜索键 */
    union {
        posting_list_t *list;       /* 链表头指针（小列表） */
        posting_array_t *array;     /* 数组指针（大列表） */
        void *ptr;                  /* 通用指针 */
    } postings;
    int count;                  /* 该 key 关联的文档数 */
    int is_array;               /* 1=数组模式，0=链表模式 */
} gin_entry_t;

/* ── GIN 索引核心结构 ── */
struct gin_index {
    gin_entry_t *entries;              /* 条目数组（按 key 排序） */
    int size;                          /* 索引中的 key 数量 */
    int capacity;                      /* 数组容量 */
    int max_entries;                   /* 最大条目数限制 */
    int total_docs;                    /* 索引的总文档数 */
    int posting_list_threshold;        /* 链表转数组的阈值 */
};

/* ── 核心 API ── */

/**
 * 创建 GIN 索引
 * @param max_entries 预期最大 key 数量
 * @return GIN 索引指针，失败返回 NULL
 */
gin_index_t *gin_create(int max_entries);

/**
 * 插入 key-doc_id 映射
 * @param idx GIN 索引
 * @param key 搜索键
 * @param doc_id 文档 ID
 * @return 0 成功，-1 失败
 */
int gin_insert(gin_index_t *idx, const char *key, int doc_id);

/**
 * 批量插入
 * @param idx GIN 索引
 * @param keys 搜索键数组
 * @param doc_ids 文档 ID 数组
 * @param count 插入数量
 * @return 成功插入数量
 */
int gin_insert_batch(gin_index_t *idx, const char **keys, const int *doc_ids, int count);

/**
 * 搜索包含指定 key 的所有文档
 * @param idx GIN 索引
 * @param query 查询的 key
 * @param results 输出：匹配的文档 ID 数组（需预分配）
 * @param count 输出：匹配数量
 * @return 0 成功，-1 失败
 */
int gin_search(const gin_index_t *idx, const char *query, int *results, int *count);

/**
 * 范围查询
 * @param idx GIN 索引
 * @param start 起始 key（包含）
 * @param end 结束 key（包含）
 * @param results 输出：匹配的文档 ID 数组
 * @param count 输出：匹配数量
 * @return 0 成功，-1 失败
 */
int gin_range_query(const gin_index_t *idx, const char *start, const char *end,
                    int *results, int *count);

/**
 * 删除 key-doc_id 映射
 * @param idx GIN 索引
 * @param key 搜索键
 * @param doc_id 文档 ID
 * @return 0 成功，-1 未找到
 */
int gin_delete(gin_index_t *idx, const char *key, int doc_id);

/**
 * 获取 key 关联的文档数量
 * @param idx GIN 索引
 * @param key 搜索键
 * @return 文档数量，-1 未找到
 */
int gin_count(const gin_index_t *idx, const char *key);

/**
 * 获取索引统计信息
 * @param idx GIN 索引
 * @param out_key_count 输出：key 数量
 * @param out_doc_count 输出：总文档数
 */
void gin_stats(const gin_index_t *idx, int *out_key_count, int *out_doc_count);

/**
 * 销毁 GIN 索引，释放所有资源
 * @param idx GIN 索引
 */
void gin_destroy(gin_index_t *idx);

/* ── JSONB/数组支持 ── */

/**
 * 插入 JSONB，自动展平所有 key-value 对
 * @param idx GIN 索引
 * @param json JSON 字符串
 * @param doc_id 文档 ID
 * @return 0 成功，-1 失败
 */
int gin_insert_json(gin_index_t *idx, const char *json, int doc_id);

/**
 * 插入数组，为每个元素创建 posting
 * @param idx GIN 索引
 * @param values 值数组（字符串形式）
 * @param count 元素数量
 * @param doc_id 文档 ID
 * @return 0 成功，-1 失败
 */
int gin_insert_array(gin_index_t *idx, const char **values, int count, int doc_id);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_GIN_H */
