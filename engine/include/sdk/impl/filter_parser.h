/**
 * @file filter_parser.h
 * @brief Filter JSON → SQL WHERE 片段（内部接口）
 *
 * 支持的语法（最小子集，参照 MongoDB）：
 * {"field": "literal"}                        -> field = ?
 * {"field": 123}                              -> field = ?
 * {"field": {"$eq": v, "$ne": v, "$gt": v,    -> field <op> ?
 *            "$lt": v, "$gte": v, "$lte": v,
 *            "$in": [v,...], "$nin": [v,...]}}
 *
 * 多个顶层 key 之间为 AND 关系。返回 WHERE 片段 + 绑定值数组。
 */
#ifndef SDK_IMPL_FILTER_PARSER_H
#define SDK_IMPL_FILTER_PARSER_H

#include "sdk/mmdb.h"
#include "sdk/impl/mmdb_internal.h"

#include <sqlite3.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* 绑定参数：int（INTEGER）或 text（TEXT/REAL 字符串形式） */
    int*      int_values;       /* 长度 int_count */
    size_t    int_count;
    char**    text_values;      /* 长度 text_count（每个字符串 SQLite_TRANSIENT 复制） */
    size_t    text_count;
} mmdb_filter_params_t;

/* 解析 filter JSON，生成 WHERE 子句。
 * 返回的 sql 字符串需要 free；失败返回 NULL。
 * 生成的 sql 中每个 ? 对应 params 中的一个值（按顺序拼接）。
 */
char* mmdb_filter_compile(const char* json, mmdb_filter_params_t* params);

/* 释放 params 内部分配的内存 */
void mmdb_filter_params_free(mmdb_filter_params_t* params);

/* 将 params 中的值绑定到 stmt（从 start_idx 开始；返回最终索引） */
int mmdb_filter_bind(sqlite3_stmt* stmt, const mmdb_filter_params_t* params,
                     int start_idx);

#ifdef __cplusplus
}
#endif

#endif /* SDK_IMPL_FILTER_PARSER_H */