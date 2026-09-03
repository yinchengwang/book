/*
 * gin_internal.h
 *
 * GIN 索引内部结构声明
 * 仅供内部实现使用，不对外暴露
 */

#ifndef DB_INDEX_GIN_INTERNAL_H
#define DB_INDEX_GIN_INTERNAL_H

#include "gin.h"

/* ── 内部函数声明 ── */

/* Entry 操作 */
static int _gin_entry_insert_doc(gin_entry_t *entry, int doc_id);
static int _gin_entry_delete_doc(gin_entry_t *entry, int doc_id);
static void _gin_entry_destroy(gin_entry_t *entry);
static void _gin_entry_traverse(const gin_entry_t *entry, int *results, int *count);

/* Posting List 操作 */
static void _posting_list_append(posting_list_t **head, int doc_id);
static void _posting_list_destroy(posting_list_t *list);

/* 工具函数 */
static int _compare_int(const void *a, const void *b);
static int _binary_search_entries(gin_entry_t *entries, int count, const char *key);

/* JSON 解析 */
static int _gin_parse_json_object(const char *json, const char *prefix,
                                  gin_index_t *idx, int doc_id, int *count);

#endif /* DB_INDEX_GIN_INTERNAL_H */
