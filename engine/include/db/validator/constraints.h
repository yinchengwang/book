/*
 * constraints.h - 数据库约束检查接口
 */

#ifndef DB_CONSTRAINTS_H
#define DB_CONSTRAINTS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─────────────────────────────────────────────────────────────────
 * 约束类型
 * ───────────────────────────────────────────────────────────────── */

typedef enum {
    CONSTRAINT_PRIMARY_KEY,  /* 主键约束 */
    CONSTRAINT_UNIQUE,       /* 唯一约束 */
    CONSTRAINT_NOT_NULL,     /* 非空约束 */
    CONSTRAINT_CHECK,        /* 检查约束 */
    CONSTRAINT_FOREIGN_KEY   /* 外键约束 */
} constraint_type_t;

/* ─────────────────────────────────────────────────────────────────
 * 约束定义
 * ───────────────────────────────────────────────────────────────── */

typedef struct constraint_def {
    constraint_type_t type;     /* 约束类型 */
    char *name;                 /* 约束名 */
    int column_id;              /* 列 ID（单列约束） */
    int *column_ids;            /* 列 ID 数组（多列约束） */
    int num_columns;            /* 列数 */
    int ref_table_id;           /* 引用表 ID（外键） */
    int ref_column_id;          /* 引用列 ID（外键） */
    bool deferrable;            /* 是否可延迟 */
    bool initially_deferred;    /* 初始延迟状态 */
} constraint_def_t;

/* ─────────────────────────────────────────────────────────────────
 * 约束错误
 * ───────────────────────────────────────────────────────────────── */

typedef enum {
    CONSTRAINT_OK = 0,
    CONSTRAINT_ERR_NULL = 1,          /* 违反 NOT NULL */
    CONSTRAINT_ERR_PRIMARY_KEY = 2,   /* 违反主键 */
    CONSTRAINT_ERR_UNIQUE = 3,        /* 违反唯一 */
    CONSTRAINT_ERR_FOREIGN_KEY = 4,   /* 违反外键 */
    CONSTRAINT_ERR_CHECK = 5,         /* 违反检查约束 */
    CONSTRAINT_ERR_NO_REF = 6         /* 外键无引用 */
} constraint_error_t;

/* ─────────────────────────────────────────────────────────────────
 * 约束检查器
 * ───────────────────────────────────────────────────────────────── */

typedef struct constraint_checker constraint_checker_t;

/**
 * @brief 创建约束检查器
 * @param table_id 表 ID
 * @return 检查器句柄
 */
constraint_checker_t *constraint_checker_create(int table_id);

/**
 * @brief 销毁约束检查器
 * @param checker 检查器
 */
void constraint_checker_destroy(constraint_checker_t *checker);

/**
 * @brief 添加约束
 * @param checker 检查器
 * @param constraint 约束定义
 * @return 0 成功
 */
int constraint_checker_add(constraint_checker_t *checker,
                           const constraint_def_t *constraint);

/**
 * @brief 检查 NOT NULL 约束
 * @param checker 检查器
 * @param column_id 列 ID
 * @param value 值
 * @return CONSTRAINT_OK 或错误码
 */
constraint_error_t constraint_check_not_null(constraint_checker_t *checker,
                                              int column_id, const void *value);

/**
 * @brief 检查主键约束
 * @param checker 检查器
 * @param key_value 键值
 * @param key_len 键值长度
 * @param exclude_row_id 排除的行 ID（更新时使用）
 * @return CONSTRAINT_OK 或 CONSTRAINT_ERR_PRIMARY_KEY
 */
constraint_error_t constraint_check_primary_key(constraint_checker_t *checker,
                                                 const void *key_value,
                                                 size_t key_len,
                                                 uint64_t exclude_row_id);

/**
 * @brief 检查唯一约束
 * @param checker 检查器
 * @param column_id 列 ID
 * @param value 值
 * @param exclude_row_id 排除的行 ID
 * @return CONSTRAINT_OK 或 CONSTRAINT_ERR_UNIQUE
 */
constraint_error_t constraint_check_unique(constraint_checker_t *checker,
                                            int column_id,
                                            const void *value,
                                            uint64_t exclude_row_id);

/**
 * @brief 检查外键约束
 * @param checker 检查器
 * @param ref_checker 引用表的检查器
 * @param value 值
 * @return CONSTRAINT_OK 或 CONSTRAINT_ERR_FOREIGN_KEY
 */
constraint_error_t constraint_check_foreign_key(constraint_checker_t *checker,
                                                 constraint_checker_t *ref_checker,
                                                 const void *value);

/**
 * @brief 检查所有约束
 * @param checker 检查器
 * @param values 值数组
 * @param num_values 值数量
 * @param exclude_row_id 排除的行 ID
 * @return CONSTRAINT_OK 或第一个错误
 */
constraint_error_t constraint_check_all(constraint_checker_t *checker,
                                         const void **values,
                                         size_t num_values,
                                         uint64_t exclude_row_id);

/**
 * @brief 获取错误信息
 * @param error 错误码
 * @return 错误描述
 */
const char *constraint_error_msg(constraint_error_t error);

/**
 * @brief 移除主键值（用于 DELETE）
 * @param checker 检查器
 * @param key_value 键值
 * @param key_len 键值长度
 */
void constraint_remove_primary_key(constraint_checker_t *checker,
                                    const void *key_value, size_t key_len);

/* ─────────────────────────────────────────────────────────────────
 * 约束验证 API（简化接口）
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 验证插入/更新操作的约束
 * @param table_id 表 ID
 * @param constraints 约束数组
 * @param num_constraints 约束数量
 * @param values 值数组
 * @param num_values 值数量
 * @param exclude_row_id 排除的行 ID（用于更新）
 * @return CONSTRAINT_OK 或错误码
 */
constraint_error_t validate_constraints(int table_id,
                                        const constraint_def_t *constraints,
                                        size_t num_constraints,
                                        const void **values,
                                        size_t num_values,
                                        uint64_t exclude_row_id);

#ifdef __cplusplus
}
#endif

#endif /* DB_CONSTRAINTS_H */