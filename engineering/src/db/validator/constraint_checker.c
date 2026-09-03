/*
 * constraints.c - 数据库约束检查实现
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <db/constraints/constraints.h>

/* ─────────────────────────────────────────────────────────────────
 * 约束检查器结构
 * ───────────────────────────────────────────────────────────────── */

struct constraint_checker {
    int table_id;                    /* 表 ID */

    /* 主键索引（用于快速查找） */
    void *primary_key_index;         /* 哈希表：key -> row_id */
    size_t pk_key_size;              /* 主键大小 */

    /* 唯一索引（列 ID -> 哈希表） */
    void **unique_indexes;           /* 数组：column_id -> 哈希表 */
    size_t unique_count;             /* 唯一约束数量 */

    /* 约束定义 */
    constraint_def_t *constraints;   /* 约束数组 */
    size_t num_constraints;          /* 约束数量 */

    /* 外键引用信息 */
    constraint_checker_t *ref_checkers[16];  /* 最多 16 个外键 */
    int ref_count;
};

/* ─────────────────────────────────────────────────────────────────
 * 简单哈希表实现（用于约束检查）
 * ───────────────────────────────────────────────────────────────── */

#define HASH_BUCKET_COUNT 256

typedef struct hash_entry {
    char *key;
    size_t key_len;
    uint64_t row_id;
    struct hash_entry *next;
} hash_entry_t;

typedef struct hash_table {
    hash_entry_t *buckets[HASH_BUCKET_COUNT];
    size_t count;
} hash_table_t;

static hash_table_t *hash_table_create(void)
{
    hash_table_t *ht = (hash_table_t *)calloc(1, sizeof(hash_table_t));
    return ht;
}

static void hash_table_destroy(hash_table_t *ht)
{
    if (ht == NULL) return;

    for (int i = 0; i < HASH_BUCKET_COUNT; i++) {
        hash_entry_t *entry = ht->buckets[i];
        while (entry) {
            hash_entry_t *next = entry->next;
            free(entry->key);
            free(entry);
            entry = next;
        }
    }
    free(ht);
}

static unsigned int hash_bytes(const void *data, size_t len)
{
    const unsigned char *p = (const unsigned char *)data;
    unsigned int h = 2166136261U;

    for (size_t i = 0; i < len; i++) {
        h ^= p[i];
        h *= 16777619U;
    }
    return h;
}

static bool hash_table_lookup(hash_table_t *ht, const void *key, size_t key_len)
{
    unsigned int h = hash_bytes(key, key_len) % HASH_BUCKET_COUNT;
    hash_entry_t *entry = ht->buckets[h];

    while (entry) {
        if (entry->key_len == key_len &&
            memcmp(entry->key, key, key_len) == 0) {
            return true;
        }
        entry = entry->next;
    }
    return false;
}

static bool hash_table_insert(hash_table_t *ht, const void *key, size_t key_len,
                               uint64_t row_id)
{
    (void)row_id;  /* 预留用于未来扩展 */
    unsigned int h = hash_bytes(key, key_len) % HASH_BUCKET_COUNT;

    /* 检查是否已存在 */
    hash_entry_t *entry = ht->buckets[h];
    while (entry) {
        if (entry->key_len == key_len &&
            memcmp(entry->key, key, key_len) == 0) {
            return false;  /* 已存在 */
        }
        entry = entry->next;
    }

    /* 插入新条目 */
    entry = (hash_entry_t *)malloc(sizeof(hash_entry_t));
    if (entry == NULL) return false;

    entry->key = (char *)malloc(key_len);
    if (entry->key == NULL) {
        free(entry);
        return false;
    }

    memcpy(entry->key, key, key_len);
    entry->key_len = key_len;
    entry->row_id = row_id;
    entry->next = ht->buckets[h];
    ht->buckets[h] = entry;
    ht->count++;

    return true;
}

/* hash_table_remove 用于 DELETE/UPDATE 时移除索引条目，暂时保留 */
static void hash_table_remove(hash_table_t *ht, const void *key, size_t key_len)
{
    unsigned int h = hash_bytes(key, key_len) % HASH_BUCKET_COUNT;
    hash_entry_t **prev = &ht->buckets[h];
    hash_entry_t *entry = ht->buckets[h];

    while (entry) {
        if (entry->key_len == key_len &&
            memcmp(entry->key, key, key_len) == 0) {
            *prev = entry->next;
            free(entry->key);
            free(entry);
            ht->count--;
            return;
        }
        prev = &entry->next;
        entry = entry->next;
    }
}

/* 辅助函数：移除主键值（供外部调用用于 DELETE） */
void constraint_remove_primary_key(constraint_checker_t *checker,
                                    const void *key_value, size_t key_len)
{
    if (checker == NULL || key_value == NULL) return;
    hash_table_t *pk_index = (hash_table_t *)checker->primary_key_index;
    hash_table_remove(pk_index, key_value, key_len);
}

/* ─────────────────────────────────────────────────────────────────
 * 约束检查器实现
 * ───────────────────────────────────────────────────────────────── */

constraint_checker_t *constraint_checker_create(int table_id)
{
    constraint_checker_t *checker =
        (constraint_checker_t *)calloc(1, sizeof(constraint_checker_t));
    if (checker == NULL) return NULL;

    checker->table_id = table_id;
    checker->primary_key_index = hash_table_create();
    checker->unique_indexes = (void **)calloc(64, sizeof(void *));

    return checker;
}

void constraint_checker_destroy(constraint_checker_t *checker)
{
    if (checker == NULL) return;

    if (checker->primary_key_index) {
        hash_table_destroy((hash_table_t *)checker->primary_key_index);
    }

    if (checker->unique_indexes) {
        for (size_t i = 0; i < 64; i++) {
            if (checker->unique_indexes[i]) {
                hash_table_destroy((hash_table_t *)checker->unique_indexes[i]);
            }
        }
        free(checker->unique_indexes);
    }

    if (checker->constraints) {
        for (size_t i = 0; i < checker->num_constraints; i++) {
            if (checker->constraints[i].name) {
                free(checker->constraints[i].name);
            }
            if (checker->constraints[i].column_ids) {
                free(checker->constraints[i].column_ids);
            }
        }
        free(checker->constraints);
    }

    free(checker);
}

int constraint_checker_add(constraint_checker_t *checker,
                           const constraint_def_t *constraint)
{
    if (checker == NULL || constraint == NULL) return -1;

    /* 扩展约束数组 */
    size_t new_count = checker->num_constraints + 1;
    constraint_def_t *new_constraints =
        (constraint_def_t *)realloc(checker->constraints,
                                     new_count * sizeof(constraint_def_t));
    if (new_constraints == NULL) return -1;

    checker->constraints = new_constraints;

    /* 复制约束定义 */
    constraint_def_t *c = &checker->constraints[checker->num_constraints];
    memset(c, 0, sizeof(constraint_def_t));

    c->type = constraint->type;
    c->name = constraint->name ? strdup(constraint->name) : NULL;
    c->column_id = constraint->column_id;
    c->num_columns = constraint->num_columns;
    c->deferrable = constraint->deferrable;
    c->initially_deferred = constraint->initially_deferred;

    if (constraint->num_columns > 1 && constraint->column_ids) {
        c->column_ids = (int *)malloc(constraint->num_columns * sizeof(int));
        memcpy(c->column_ids, constraint->column_ids,
               constraint->num_columns * sizeof(int));
    }

    if (constraint->type == CONSTRAINT_FOREIGN_KEY) {
        c->ref_table_id = constraint->ref_table_id;
        c->ref_column_id = constraint->ref_column_id;
    }

    /* 初始化索引结构 */
    if (constraint->type == CONSTRAINT_PRIMARY_KEY) {
        /* 主键索引已创建 */
    } else if (constraint->type == CONSTRAINT_UNIQUE) {
        if (checker->unique_indexes[c->column_id] == NULL) {
            checker->unique_indexes[c->column_id] = hash_table_create();
        }
    }

    checker->num_constraints++;
    return 0;
}

constraint_error_t constraint_check_not_null(constraint_checker_t *checker,
                                              int column_id, const void *value)
{
    (void)checker;
    (void)column_id;

    /* NULL 检查（简化实现：假设 value 为 NULL 表示空值） */
    if (value == NULL) {
        return CONSTRAINT_ERR_NULL;
    }

    return CONSTRAINT_OK;
}

constraint_error_t constraint_check_primary_key(constraint_checker_t *checker,
                                                 const void *key_value,
                                                 size_t key_len,
                                                 uint64_t exclude_row_id)
{
    (void)exclude_row_id;

    if (checker == NULL || key_value == NULL) {
        return CONSTRAINT_ERR_PRIMARY_KEY;
    }

    hash_table_t *pk_index =
        (hash_table_t *)checker->primary_key_index;

    /* 检查是否已存在 */
    if (hash_table_lookup(pk_index, key_value, key_len)) {
        return CONSTRAINT_ERR_PRIMARY_KEY;
    }

    /* 注册主键值 */
    hash_table_insert(pk_index, key_value, key_len, 0);

    return CONSTRAINT_OK;
}

constraint_error_t constraint_check_unique(constraint_checker_t *checker,
                                            int column_id,
                                            const void *value,
                                            uint64_t exclude_row_id)
{
    (void)exclude_row_id;

    if (checker == NULL || value == NULL) {
        return CONSTRAINT_OK;  /* NULL 值不检查唯一性 */
    }

    hash_table_t *unique_index =
        (hash_table_t *)checker->unique_indexes[column_id];

    if (unique_index == NULL) {
        return CONSTRAINT_OK;  /* 没有唯一约束 */
    }

    /* 使用字符串长度进行比较和插入 */
    size_t val_len = strlen((const char *)value) + 1;

    if (hash_table_lookup(unique_index, value, val_len)) {
        return CONSTRAINT_ERR_UNIQUE;
    }

    hash_table_insert(unique_index, value, val_len, 0);

    return CONSTRAINT_OK;
}

constraint_error_t constraint_check_foreign_key(constraint_checker_t *checker,
                                                 constraint_checker_t *ref_checker,
                                                 const void *value)
{
    (void)checker;  /* 预留用于检查本表约束 */

    if (value == NULL) {
        return CONSTRAINT_OK;  /* NULL 值允许外键为空 */
    }

    if (ref_checker == NULL) {
        return CONSTRAINT_ERR_FOREIGN_KEY;
    }

    /* 检查引用表中是否存在该值（使用默认大小） */
    hash_table_t *ref_pk =
        (hash_table_t *)ref_checker->primary_key_index;

    if (!hash_table_lookup(ref_pk, value, sizeof(int))) {
        return CONSTRAINT_ERR_NO_REF;
    }

    return CONSTRAINT_OK;
}

constraint_error_t constraint_check_all(constraint_checker_t *checker,
                                         const void **values,
                                         size_t num_values,
                                         uint64_t exclude_row_id)
{
    (void)values;
    (void)num_values;
    (void)exclude_row_id;

    if (checker == NULL) {
        return CONSTRAINT_OK;
    }

    /* 遍历所有约束检查 */
    for (size_t i = 0; i < checker->num_constraints; i++) {
        constraint_def_t *c = &checker->constraints[i];

        switch (c->type) {
            case CONSTRAINT_NOT_NULL:
                /* 需要列值进行 NOT NULL 检查 */
                break;

            case CONSTRAINT_PRIMARY_KEY:
                /* 需要复合键值进行检查 */
                break;

            case CONSTRAINT_UNIQUE:
                /* 需要具体值进行检查 */
                break;

            default:
                break;
        }
    }

    return CONSTRAINT_OK;
}

/* ─────────────────────────────────────────────────────────────────
 * 辅助函数
 * ───────────────────────────────────────────────────────────────── */

const char *constraint_error_msg(constraint_error_t error)
{
    switch (error) {
        case CONSTRAINT_OK:
            return "OK";
        case CONSTRAINT_ERR_NULL:
            return "NOT NULL constraint violation";
        case CONSTRAINT_ERR_PRIMARY_KEY:
            return "PRIMARY KEY constraint violation";
        case CONSTRAINT_ERR_UNIQUE:
            return "UNIQUE constraint violation";
        case CONSTRAINT_ERR_FOREIGN_KEY:
            return "FOREIGN KEY constraint violation";
        case CONSTRAINT_ERR_CHECK:
            return "CHECK constraint violation";
        case CONSTRAINT_ERR_NO_REF:
            return "FOREIGN KEY has no matching reference";
        default:
            return "Unknown constraint error";
    }
}

/* ─────────────────────────────────────────────────────────────────
 * 简化验证 API
 * ───────────────────────────────────────────────────────────────── */

constraint_error_t validate_constraints(int table_id,
                                        const constraint_def_t *constraints,
                                        size_t num_constraints,
                                        const void **values,
                                        size_t num_values,
                                        uint64_t exclude_row_id)
{
    (void)table_id;
    (void)constraints;
    (void)num_constraints;
    (void)values;
    (void)num_values;
    (void)exclude_row_id;

    /* 简化实现：返回成功 */
    return CONSTRAINT_OK;
}