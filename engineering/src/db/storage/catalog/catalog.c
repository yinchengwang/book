/**
 * @file catalog.c
 * @brief Catalog 系统实现
 *
 * ## Catalog 概述
 *
 * Catalog 是数据库的"数据库"，存储所有元数据：
 * - 表定义（pg_class）
 * - 列定义（pg_attribute）
 * - 索引定义（pg_index）
 * - 用户和权限信息
 *
 * ## PostgreSQL Catalog 结构
 *
 * ```
 * pg_class     — 表、视图、序列
 * pg_attribute  — 列属性
 * pg_type      — 数据类型
 * pg_index     — 索引信息
 * pg_namespace — 模式（schema）
 * pg_proc      — 函数/过程
 * ```
 *
 * ## 本实现
 *
 * 采用内存 Hash 表缓存，实现：
 * - O(1) 表/列/索引查找
 * - OID 分配器
 * - 懒加载系统表
 *
 * ## 缓存策略
 *
 * | 数据 | 存储 | 失效策略 |
 * |------|------|----------|
 * | 表信息 | Hash 表 | DDL 时失效 |
 * | 列信息 | Hash 表 | DDL 时失效 |
 * | 索引信息 | Hash 表 | DDL 时失效 |
 */

#include "db/catalog.h"
#include "db/lock.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================
 * 内部数据结构
 * ============================================================ */

/** 哈希表桶 */
#define CATALOG_HASH_SIZE 128

/** 表缓存条目 */
typedef struct catalog_entry_s {
    Oid             oid;            /**< OID */
    table_info_t    info;           /**< 表信息 */
    struct catalog_entry_s *next;   /**< 链表下一项 */
} catalog_entry_t;

/** 列缓存条目 */
typedef struct column_entry_s {
    Oid             table_oid;      /**< 表 OID */
    int             attnum;         /**< 列序号 */
    column_info_t   info;           /**< 列信息 */
    struct column_entry_s *next;    /**< 链表下一项 */
} column_entry_t;

/** 索引缓存条目 */
typedef struct index_entry_s {
    Oid             oid;            /**< 索引 OID */
    index_info_t    info;           /**< 索引信息 */
    struct index_entry_s *next;     /**< 链表下一项 */
} index_entry_t;

/** Catalog 缓存 */
typedef struct catalog_cache_s {
    catalog_entry_t     *tables[CATALOG_HASH_SIZE];  /**< 表缓存 */
    column_entry_t      *columns[CATALOG_HASH_SIZE]; /**< 列缓存 */
    index_entry_t       *indexes[CATALOG_HASH_SIZE]; /**< 索引缓存 */
    Oid                 next_oid;     /**< 下一个可用 OID */

    /* 统计 */
    uint64_t            hits;
    uint64_t            misses;
} catalog_cache_t;

/* ============================================================
 * 全局变量
 * ============================================================ */

static catalog_cache_t *catalog_cache = NULL;
static bool catalog_initialized = false;

/* ============================================================
 * 哈希函数
 * ============================================================ */

static uint32_t oid_hash(Oid oid) {
    return oid % CATALOG_HASH_SIZE;
}

/* ============================================================
 * 初始化
 * ============================================================ */

/**
 * @brief 初始化 Catalog 系统
 *
 * 创建内存缓存，初始化 OID 分配器
 *
 * @return 0 成功，-1 失败
 *
 * ## OID 分配
 *
 * OID (Object Identifier) 是数据库对象的唯一标识：
 * - 从 1 开始分配
 * - 0 表示无效 OID
 * - 顺序递增，简单但可能耗尽
 *
 * ## 生产环境考虑
 *
 * 完整实现应：
 * - 从持久化存储加载 Catalog
 * - 支持 OID 回收（对象删除后）
 * - 定期检查点保存
 */
int catalog_init(void) {
    if (catalog_initialized) {
        return 0;
    }

    catalog_cache = (catalog_cache_t *)calloc(1, sizeof(catalog_cache_t));
    if (!catalog_cache) {
        return -1;
    }

    /* 从 1 开始分配 OID，0 表示无效 */
    catalog_cache->next_oid = 1;

    /* 初始化系统表（pg_class 等） */
    /* 这里使用内存存储，生产环境应从持久化存储加载 */

    catalog_initialized = true;
    return 0;
}

/**
 * @brief 关闭 Catalog 系统
 *
 * 释放所有缓存内存
 */
void catalog_shutdown(void) {
    if (!catalog_initialized) {
        return;
    }

    /* 清理表缓存 */
    for (int i = 0; i < CATALOG_HASH_SIZE; i++) {
        catalog_entry_t *entry = catalog_cache->tables[i];
        while (entry) {
            catalog_entry_t *next = entry->next;
            free(entry);
            entry = next;
        }

        /* 清理列缓存 */
        column_entry_t *col = catalog_cache->columns[i];
        while (col) {
            column_entry_t *next = col->next;
            free(col);
            col = next;
        }

        /* 清理索引缓存 */
        index_entry_t *idx = catalog_cache->indexes[i];
        while (idx) {
            index_entry_t *next = idx->next;
            free(idx->info.key_nums);
            free(idx);
            idx = next;
        }
    }

    free(catalog_cache);
    catalog_cache = NULL;
    catalog_initialized = false;
}

/* ============================================================
 * 表操作
 * ============================================================ */

/**
 * @brief 创建表
 *
 * 在 Catalog 中注册新表
 *
 * @param name      表名
 * @param columns   列定义数组
 * @param ncolumns  列数量
 * @return 新表的 OID，InvalidOid 表示失败
 *
 * ## 表创建流程
 *
 * ```
 * catalog_create_table()
 *      │
 *      ├──→ 检查表名是否已存在
 *      │
 *      ├──→ 分配新 OID
 *      │
 *      ├──→ 创建表缓存条目
 *      │
 *      ├──→ 逐列注册（catalog_add_column）
 *      │
 *      └──→ 返回 OID
 * ```
 *
 * ## 表名唯一性
 *
 * 表名在同一 schema 内必须唯一。
 * 当前实现未检查 schema，使用全局唯一。
 */
Oid catalog_create_table(const char *name, column_def_t *columns, int ncolumns) {
    if (!catalog_initialized || !name) {
        return InvalidOid;
    }

    /* 检查是否已存在同名表 */
    Oid existing = catalog_lookup_table(name);
    if (existing != InvalidOid) {
        return InvalidOid;  /* 表名冲突 */
    }

    /* 分配新 OID */
    Oid table_oid = catalog_cache->next_oid++;

    /* 创建表缓存条目 */
    catalog_entry_t *entry = (catalog_entry_t *)malloc(sizeof(catalog_entry_t));
    if (!entry) {
        return InvalidOid;
    }

    entry->oid = table_oid;
    entry->info.oid = table_oid;
    strncpy(entry->info.name, name, NAMEDATALEN - 1);
    entry->info.name[NAMEDATALEN - 1] = '\0';
    entry->info.namespace_oid = 1;  /* public 命名空间 */
    entry->info.type_oid = table_oid + 10000;  /* 派生类型 OID */
    entry->info.relkind = 'r';  /* OBJECT_TABLE */
    entry->info.natts = ncolumns;
    entry->info.filenode = table_oid;
    entry->info.tablespace = 0;  /* 默认表空间 */
    entry->info.npages = 0;
    entry->info.ntupes = 0;
    entry->info.owner = 1;  /* postgres 用户 */
    entry->info.persistence = 'p';  /* 永久表 */
    entry->info.nchecks = 0;
    entry->info.has_index = false;
    entry->info.has_pkey = false;

    /* 插入哈希表 */
    uint32_t bucket = oid_hash(table_oid);
    entry->next = catalog_cache->tables[bucket];
    catalog_cache->tables[bucket] = entry;

    /* 创建列缓存条目 */
    for (int i = 0; i < ncolumns; i++) {
        column_entry_t *col_entry = (column_entry_t *)malloc(sizeof(column_entry_t));
        if (!col_entry) continue;

        col_entry->table_oid = table_oid;
        col_entry->attnum = i + 1;
        strncpy(col_entry->info.name, columns[i].name, NAMEDATALEN - 1);
        col_entry->info.name[NAMEDATALEN - 1] = '\0';
        col_entry->info.type_oid = columns[i].type_oid;
        col_entry->info.table_oid = table_oid;
        col_entry->info.attnum = i + 1;
        col_entry->info.len = 0;
        col_entry->info.typmod = columns[i].typmod;
        col_entry->info.attnotnull = columns[i].not_null;
        col_entry->info.has_default = columns[i].has_default;
        col_entry->info.is_dropped = false;

        uint32_t col_bucket = oid_hash(table_oid);
        col_entry->next = catalog_cache->columns[col_bucket];
        catalog_cache->columns[col_bucket] = col_entry;
    }

    return table_oid;
}

catalog_result_t catalog_drop_table(Oid table_oid) {
    if (!catalog_initialized || table_oid == InvalidOid) {
        return CATALOG_ERROR;
    }

    uint32_t bucket = oid_hash(table_oid);
    catalog_entry_t **prev = &catalog_cache->tables[bucket];
    catalog_entry_t *entry = *prev;

    while (entry) {
        if (entry->oid == table_oid) {
            *prev = entry->next;
            free(entry);
            return CATALOG_SUCCESS;
        }
        prev = &entry->next;
        entry = entry->next;
    }

    return CATALOG_NOT_FOUND;
}

table_info_t *catalog_get_table(Oid table_oid) {
    if (!catalog_initialized || table_oid == InvalidOid) {
        return NULL;
    }

    uint32_t bucket = oid_hash(table_oid);
    catalog_entry_t *entry = catalog_cache->tables[bucket];

    while (entry) {
        if (entry->oid == table_oid) {
            catalog_cache->hits++;
            return &entry->info;
        }
        entry = entry->next;
    }

    catalog_cache->misses++;
    return NULL;
}

/**
 * @brief 按名称查找表
 *
 * 在 Catalog 中查找指定名称的表
 *
 * @param name 表名
 * @return 表的 OID，未找到返回 InvalidOid
 *
 * ## 查找过程
 *
 * 遍历所有 Hash 桶，逐个比较表名。
 * 时间复杂度：O(n)，n 为表数量。
 *
 * ## 为什么不用 Hash 表按名查找？
 *
 * 当前按 OID 建立 Hash 表（快速 OID→表），
 * 按名查找需要遍历。如果表数量大，应建立 name→OID 的反向索引。
 */
Oid catalog_lookup_table(const char *name) {
    if (!catalog_initialized || !name) {
        return InvalidOid;
    }

    for (int i = 0; i < CATALOG_HASH_SIZE; i++) {
        catalog_entry_t *entry = catalog_cache->tables[i];
        while (entry) {
            if (strcmp(entry->info.name, name) == 0) {
                catalog_cache->hits++;  /* 命中统计 */
                return entry->oid;
            }
            entry = entry->next;
        }
    }

    catalog_cache->misses++;  /* 未命中统计 */
    return InvalidOid;
}

/**
 * @brief 获取所有表
 *
 * 遍历 Catalog，返回所有表的信息
 *
 * @param ntables 输出：表数量
 * @return 表信息数组，需调用方释放
 *
 * ## 实现说明
 *
 * 使用两次遍历：
 * 1. 第一次计数
 * 2. 第二次填充数组
 *
 * 为什么不一次完成？需要先分配正确大小的数组。
 */
table_info_t *catalog_get_all_tables(int *ntables) {
    if (!catalog_initialized || !ntables) {
        return NULL;
    }

    /* 第一次遍历计数 */
    int count = 0;
    for (int i = 0; i < CATALOG_HASH_SIZE; i++) {
        catalog_entry_t *entry = catalog_cache->tables[i];
        while (entry) {
            if (entry->info.relkind == 'r') {  /* OBJECT_TABLE */
                count++;
            }
            entry = entry->next;
        }
    }

    if (count == 0) {
        *ntables = 0;
        return NULL;
    }

    table_info_t *tables = (table_info_t *)malloc(count * sizeof(table_info_t));
    if (!tables) {
        return NULL;
    }

    /* 第二次遍历填充 */
    int idx = 0;
    for (int i = 0; i < CATALOG_HASH_SIZE; i++) {
        catalog_entry_t *entry = catalog_cache->tables[i];
        while (entry) {
            if (entry->info.relkind == 'r') {  /* OBJECT_TABLE */
                tables[idx++] = entry->info;
            }
            entry = entry->next;
        }
    }

    *ntables = count;
    return tables;
}

void catalog_free_table(table_info_t *info) {
    /* 内存由调用者分配，不需要特殊释放 */
    (void)info;
}

/* ============================================================
 * 列操作
 * ============================================================ */

column_info_t *catalog_get_columns(Oid table_oid, int *ncolumns) {
    if (!catalog_initialized || !ncolumns) {
        return NULL;
    }

    /* 第一次遍历计数 */
    int count = 0;
    uint32_t bucket = oid_hash(table_oid);
    column_entry_t *entry = catalog_cache->columns[bucket];

    while (entry) {
        if (entry->table_oid == table_oid && !entry->info.is_dropped) {
            count++;
        }
        entry = entry->next;
    }

    if (count == 0) {
        *ncolumns = 0;
        return NULL;
    }

    column_info_t *columns = (column_info_t *)malloc(count * sizeof(column_info_t));
    if (!columns) {
        return NULL;
    }

    /* 第二次遍历填充 */
    int idx = 0;
    entry = catalog_cache->columns[bucket];
    while (entry) {
        if (entry->table_oid == table_oid && !entry->info.is_dropped) {
            columns[idx++] = entry->info;
        }
        entry = entry->next;
    }

    *ncolumns = count;
    return columns;
}

catalog_result_t catalog_add_column(Oid table_oid, column_def_t *column) {
    if (!catalog_initialized || table_oid == InvalidOid || !column) {
        return CATALOG_ERROR;
    }

    /* 获取当前列数 */
    int ncols = 0;
    catalog_get_columns(table_oid, &ncols);

    /* 创建新列条目 */
    column_entry_t *entry = (column_entry_t *)malloc(sizeof(column_entry_t));
    if (!entry) {
        return CATALOG_ERROR;
    }

    entry->table_oid = table_oid;
    entry->attnum = ncols + 1;
    strncpy(entry->info.name, column->name, NAMEDATALEN - 1);
    entry->info.name[NAMEDATALEN - 1] = '\0';
    entry->info.type_oid = column->type_oid;
    entry->info.table_oid = table_oid;
    entry->info.attnum = ncols + 1;
    entry->info.len = 0;
    entry->info.typmod = column->typmod;
    entry->info.attnotnull = column->not_null;
    entry->info.has_default = column->has_default;
    entry->info.is_dropped = false;

    uint32_t bucket = oid_hash(table_oid);
    entry->next = catalog_cache->columns[bucket];
    catalog_cache->columns[bucket] = entry;

    return CATALOG_SUCCESS;
}

catalog_result_t catalog_drop_column(Oid table_oid, const char *column_name) {
    if (!catalog_initialized || table_oid == InvalidOid || !column_name) {
        return CATALOG_ERROR;
    }

    uint32_t bucket = oid_hash(table_oid);
    column_entry_t **prev = &catalog_cache->columns[bucket];
    column_entry_t *entry = *prev;

    while (entry) {
        if (entry->table_oid == table_oid &&
            strcmp(entry->info.name, column_name) == 0) {
            entry->info.is_dropped = true;
            return CATALOG_SUCCESS;
        }
        prev = &entry->next;
        entry = entry->next;
    }

    return CATALOG_NOT_FOUND;
}

void catalog_free_columns(column_info_t *columns) {
    free(columns);
}

/* ============================================================
 * 索引操作
 * ============================================================ */

/**
 * @brief 创建索引
 *
 * 在 Catalog 中注册新索引
 *
 * @param index_name 索引名
 * @param table_oid  所属表的 OID
 * @param columns    索引列名数组
 * @param ncolumns   列数量
 * @param is_unique  是否唯一索引
 * @return 索引 OID，InvalidOid 表示失败
 *
 * ## 索引创建过程
 *
 * ```
 * catalog_create_index()
 *      │
 *      ├──→ 分配索引 OID
 *      │
 *      ├──→ 创建索引条目
 *      │     - 记录表 OID
 *      │     - 记录索引列
 *      │     - 设置唯一标志
 *      │
 *      └──→ 插入 Hash 表
 * ```
 *
 * ## 索引与表的关系
 *
 * - 索引必须关联一个表
 * - 一个表可以有多个索引
 * - 索引的列必须是表的列
 */
Oid catalog_create_index(const char *index_name, Oid table_oid,
                         const char **columns, int ncolumns, bool is_unique) {
    if (!catalog_initialized || !index_name || table_oid == InvalidOid) {
        return InvalidOid;
    }

    /* 分配新 OID */
    Oid index_oid = catalog_cache->next_oid++;

    /* 创建索引缓存条目 */
    index_entry_t *entry = (index_entry_t *)malloc(sizeof(index_entry_t));
    if (!entry) {
        return InvalidOid;
    }

    entry->oid = index_oid;
    entry->info.oid = index_oid;
    entry->info.table_oid = table_oid;
    strncpy(entry->info.name, index_name, NAMEDATALEN - 1);
    entry->info.name[NAMEDATALEN - 1] = '\0';
    entry->info.nkeys = ncolumns;
    entry->info.key_nums = (int16_t *)malloc(ncolumns * sizeof(int16_t));

    /* 解析列名到列序号 */
    column_info_t *cols = NULL;
    int ncols = 0;
    cols = catalog_get_columns(table_oid, &ncols);

    for (int i = 0; i < ncolumns && i < ncols; i++) {
        for (int j = 0; j < ncols; j++) {
            if (strcmp(cols[j].name, columns[i]) == 0) {
                entry->info.key_nums[i] = cols[j].attnum;
                break;
            }
        }
    }

    if (cols) free(cols);

    entry->info.is_unique = is_unique;
    entry->info.is_primary = false;
    entry->info.is_valid = true;

    /* 插入哈希表 */
    uint32_t bucket = oid_hash(index_oid);
    entry->next = catalog_cache->indexes[bucket];
    catalog_cache->indexes[bucket] = entry;

    /* 更新表的 has_index 标志 */
    table_info_t *table = catalog_get_table(table_oid);
    if (table) {
        table->has_index = true;
    }

    return index_oid;
}

catalog_result_t catalog_drop_index(Oid index_oid) {
    if (!catalog_initialized || index_oid == InvalidOid) {
        return CATALOG_ERROR;
    }

    uint32_t bucket = oid_hash(index_oid);
    index_entry_t **prev = &catalog_cache->indexes[bucket];
    index_entry_t *entry = *prev;

    while (entry) {
        if (entry->oid == index_oid) {
            *prev = entry->next;
            free(entry->info.key_nums);
            free(entry);
            return CATALOG_SUCCESS;
        }
        prev = &entry->next;
        entry = entry->next;
    }

    return CATALOG_NOT_FOUND;
}

/**
 * @brief 获取表的所有索引
 *
 * 查找指定表关联的所有索引
 *
 * @param table_oid  表 OID
 * @param nindexes   输出：索引数量
 * @return 索引信息数组，需调用 catalog_free_index() 逐个释放
 *
 * ## 查找过程
 *
 * 遍历所有 Hash 桶，筛选 table_oid 匹配的索引。
 * 返回的数组中每个元素都包含 key_nums 的深拷贝。
 *
 * ## 内存管理
 *
 * 调用方负责释放返回的数组：
 * ```c
 * index_info_t *indexes = catalog_get_indexes(oid, &n);
 * for (int i = 0; i < n; i++) {
 *     catalog_free_index(&indexes[i]);
 * }
 * free(indexes);
 * ```
 */
index_info_t *catalog_get_indexes(Oid table_oid, int *nindexes) {
    if (!catalog_initialized || !nindexes) {
        return NULL;
    }

    /* 第一次遍历计数 */
    int count = 0;
    for (int i = 0; i < CATALOG_HASH_SIZE; i++) {
        index_entry_t *entry = catalog_cache->indexes[i];
        while (entry) {
            if (entry->info.table_oid == table_oid) {
                count++;
            }
            entry = entry->next;
        }
    }

    if (count == 0) {
        *nindexes = 0;
        return NULL;
    }

    /* 分配结果数组 */
    index_info_t *indexes = (index_info_t *)malloc(count * sizeof(index_info_t));
    if (!indexes) {
        return NULL;
    }

    /* 第二次遍历填充（深拷贝 key_nums） */
    int idx = 0;
    for (int i = 0; i < CATALOG_HASH_SIZE; i++) {
        index_entry_t *entry = catalog_cache->indexes[i];
        while (entry) {
            if (entry->info.table_oid == table_oid) {
                indexes[idx] = entry->info;
                /* 深拷贝 key_nums，避免悬垂指针 */
                indexes[idx].key_nums = (int16_t *)malloc(
                    entry->info.nkeys * sizeof(int16_t));
                memcpy(indexes[idx].key_nums, entry->info.key_nums,
                       entry->info.nkeys * sizeof(int16_t));
                idx++;
            }
            entry = entry->next;
        }
    }

    *nindexes = count;
    return indexes;
}

index_info_t *catalog_get_index(Oid index_oid) {
    if (!catalog_initialized || index_oid == InvalidOid) {
        return NULL;
    }

    uint32_t bucket = oid_hash(index_oid);
    index_entry_t *entry = catalog_cache->indexes[bucket];

    while (entry) {
        if (entry->oid == index_oid) {
            return &entry->info;
        }
        entry = entry->next;
    }

    return NULL;
}

void catalog_free_index(index_info_t *info) {
    if (info) {
        free(info->key_nums);
    }
}

/* ============================================================
 * 缓存管理
 * ============================================================ */

void catalog_invalidate_table(Oid table_oid) {
    if (!catalog_initialized || table_oid == InvalidOid) {
        return;
    }

    /* 使表缓存失效 */
    uint32_t bucket = oid_hash(table_oid);
    catalog_entry_t **prev = &catalog_cache->tables[bucket];
    catalog_entry_t *entry = *prev;

    while (entry) {
        if (entry->oid == table_oid) {
            /* 从缓存中移除 */
            *prev = entry->next;
            free(entry);
            return;
        }
        prev = &entry->next;
        entry = entry->next;
    }
}

void catalog_invalidate_all(void) {
    if (!catalog_initialized) {
        return;
    }

    /* 清理所有缓存 */
    for (int i = 0; i < CATALOG_HASH_SIZE; i++) {
        catalog_entry_t *t = catalog_cache->tables[i];
        while (t) { catalog_entry_t *next = t->next; free(t); t = next; }
        catalog_cache->tables[i] = NULL;

        column_entry_t *c = catalog_cache->columns[i];
        while (c) { column_entry_t *next = c->next; free(c); c = next; }
        catalog_cache->columns[i] = NULL;

        index_entry_t *idx = catalog_cache->indexes[i];
        while (idx) { index_entry_t *next = idx->next; free(idx->info.key_nums); free(idx); idx = next; }
        catalog_cache->indexes[i] = NULL;
    }
}

void catalog_get_cache_stats(uint64_t *hits, uint64_t *misses) {
    if (hits) *hits = catalog_cache ? catalog_cache->hits : 0;
    if (misses) *misses = catalog_cache ? catalog_cache->misses : 0;
}
