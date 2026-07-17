/**
 * @file main.c
 * @brief Catalog 系统演示程序
 *
 * 演示 OID 分配算法、系统表管理、表/列/索引元数据 CRUD 操作。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* 简化版类型定义（与 engineering/include/db/catalog.h 对应） */
typedef unsigned int Oid;
#define InvalidOid ((Oid)0)
#define NAMEDATALEN 64
#define CATALOG_HASH_SIZE 128

typedef enum {
    OBJECT_TABLE = 'r',
    OBJECT_INDEX = 'i',
    OBJECT_VIEW = 'v',
    OBJECT_COLUMN = 'c'
} ObjectType;

typedef struct {
    Oid         oid;
    char        name[NAMEDATALEN];
    Oid         namespace_oid;
    char        relkind;
    int16_t     natts;
    Oid         filenode;
    bool        has_index;
    bool        has_pkey;
} table_info_t;

typedef struct {
    Oid         table_oid;
    char        name[NAMEDATALEN];
    Oid         type_oid;
    int16_t     attnum;
    bool        attnotnull;
    bool        has_default;
} column_info_t;

typedef struct {
    Oid         oid;
    Oid         table_oid;
    char        name[NAMEDATALEN];
    int16_t     nkeys;
    int16_t     *key_nums;
    bool        is_unique;
    bool        is_primary;
} index_info_t;

typedef struct {
    char        name[NAMEDATALEN];
    Oid         type_oid;
    bool        not_null;
    bool        has_default;
} column_def_t;

/* 简化版 OID 分配器 */
static Oid next_oid = 1;
static Oid alloc_oid(void) {
    return next_oid++;
}

/* 简化版表缓存 */
static table_info_t *tables[CATALOG_HASH_SIZE];
static int table_count = 0;

static column_info_t *columns[CATALOG_HASH_SIZE];
static int column_count = 0;

static index_info_t *indexes[CATALOG_HASH_SIZE];
static int index_count = 0;

/* 创建表 */
Oid create_table(const char *name, column_def_t *cols, int ncols) {
    Oid oid = alloc_oid();
    table_info_t *t = (table_info_t *)malloc(sizeof(table_info_t));
    t->oid = oid;
    strncpy(t->name, name, NAMEDATALEN - 1);
    t->name[NAMEDATALEN - 1] = '\0';
    t->namespace_oid = 1;
    t->relkind = OBJECT_TABLE;
    t->natts = ncols;
    t->filenode = oid;
    t->has_index = false;
    t->has_pkey = false;
    tables[table_count++] = t;

    /* 创建列 */
    for (int i = 0; i < ncols; i++) {
        column_info_t *c = (column_info_t *)malloc(sizeof(column_info_t));
        c->table_oid = oid;
        strncpy(c->name, cols[i].name, NAMEDATALEN - 1);
        c->name[NAMEDATALEN - 1] = '\0';
        c->type_oid = cols[i].type_oid;
        c->attnum = i + 1;
        c->attnotnull = cols[i].not_null;
        c->has_default = cols[i].has_default;
        columns[column_count++] = c;
    }
    return oid;
}

/* 创建索引 */
Oid create_index(const char *name, Oid table_oid, int *col_nums, int nkeys, bool unique) {
    Oid oid = alloc_oid();
    index_info_t *idx = (index_info_t *)malloc(sizeof(index_info_t));
    idx->oid = oid;
    idx->table_oid = table_oid;
    strncpy(idx->name, name, NAMEDATALEN - 1);
    idx->name[NAMEDATALEN - 1] = '\0';
    idx->nkeys = nkeys;
    idx->key_nums = (int16_t *)malloc(nkeys * sizeof(int16_t));
    for (int i = 0; i < nkeys; i++) idx->key_nums[i] = col_nums[i];
    idx->is_unique = unique;
    idx->is_primary = false;
    indexes[index_count++] = idx;

    /* 更新表的 has_index */
    for (int i = 0; i < table_count; i++) {
        if (tables[i]->oid == table_oid) {
            tables[i]->has_index = true;
            break;
        }
    }
    return oid;
}

/* 获取表的列 */
column_info_t* get_columns(Oid table_oid, int *ncols) {
    *ncols = 0;
    for (int i = 0; i < column_count; i++) {
        if (columns[i]->table_oid == table_oid) (*ncols)++;
    }
    column_info_t *result = (column_info_t *)malloc((*ncols) * sizeof(column_info_t));
    int j = 0;
    for (int i = 0; i < column_count; i++) {
        if (columns[i]->table_oid == table_oid) {
            result[j++] = *columns[i];
        }
    }
    return result;
}

int main(void) {
    printf("\n[Catalog] 初始化 Catalog 系统\n");
    printf("[Catalog] OID 分配器就绪，从 OID=1 开始\n\n");

    /* 定义列 */
    column_def_t user_cols[] = {
        {"id", 1, true, false},
        {"name", 2, false, false},
        {"email", 2, false, false}
    };
    column_def_t order_cols[] = {
        {"id", 1, true, false},
        {"user_id", 1, true, false},
        {"amount", 3, false, false}
    };

    /* 创建表 users */
    printf("[catalog] 创建表: users (3 列)\n");
    Oid users_oid = create_table("users", user_cols, 3);
    printf("[catalog]   -> OID=%u, 文件节点=%u\n\n", users_oid, users_oid);

    /* 创建表 orders */
    printf("[catalog] 创建表: orders (3 列)\n");
    Oid orders_oid = create_table("orders", order_cols, 3);
    printf("[catalog]   -> OID=%u, 文件节点=%u\n\n", orders_oid, orders_oid);

    /* 创建索引 */
    printf("[catalog] 创建索引: users_pkey (主键, 列 id)\n");
    int id_col[] = {1};
    Oid idx1 = create_index("users_pkey", users_oid, id_col, 1, true);
    printf("[catalog]   -> OID=%u, 唯一=%d\n\n", idx1, true);

    printf("[catalog] 创建索引: orders_user_idx (列 user_id)\n");
    int user_col[] = {2};
    Oid idx2 = create_index("orders_user_idx", orders_oid, user_col, 1, false);
    printf("[catalog]   -> OID=%u, 唯一=%d\n\n", idx2, false);

    /* 查询表信息 */
    printf("[catalog] 查询表: users\n");
    for (int i = 0; i < table_count; i++) {
        if (tables[i]->oid == users_oid) {
            printf("[catalog]   OID=%u, name=%s, 列数=%d, 有索引=%d\n",
                   tables[i]->oid, tables[i]->name,
                   tables[i]->natts, tables[i]->has_index);
            break;
        }
    }
    printf("\n");

    /* 查询列信息 */
    printf("[catalog] 查询表 users 的列\n");
    int ncols = 0;
    column_info_t *cols = get_columns(users_oid, &ncols);
    for (int i = 0; i < ncols; i++) {
        printf("[catalog]   列 #%d: %s, 类型OID=%u, NOT_NULL=%d\n",
               cols[i].attnum, cols[i].name, cols[i].type_oid, cols[i].attnotnull);
    }
    free(cols);
    printf("\n");

    /* 清理 */
    for (int i = 0; i < table_count; i++) free(tables[i]);
    for (int i = 0; i < column_count; i++) free(columns[i]);
    for (int i = 0; i < index_count; i++) {
        free(indexes[i]->key_nums);
        free(indexes[i]);
    }

    printf("[Catalog] 演示完成\n\n");
    return 0;
}
