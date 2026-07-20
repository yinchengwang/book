#ifndef TODO_FIELD_H
#define TODO_FIELD_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 字段类型枚举
 * ============================================================ */
typedef enum {
    FIELD_TYPE_TEXT         = 0,
    FIELD_TYPE_NUMBER       = 1,
    FIELD_TYPE_SINGLE_SELECT = 2,
    FIELD_TYPE_MULTI_SELECT  = 3,
    FIELD_TYPE_DATE         = 4,
    FIELD_TYPE_DATETIME     = 5,
    FIELD_TYPE_USER         = 6,
    FIELD_TYPE_ATTACHMENT   = 7,
    FIELD_TYPE_FORMULA      = 8,
    FIELD_TYPE_LINK         = 9
} field_type_t;

/* ============================================================
 * 字段定义结构体
 * ============================================================ */
#define FIELD_NAME_MAX   64
#define FIELD_OPTS_MAX   1024
#define FIELD_REF_MAX    64
#define FIELD_FORMULA_MAX 512

typedef struct {
    int64_t id;
    char    name[FIELD_NAME_MAX];
    field_type_t type;
    char    options[FIELD_OPTS_MAX];     /* JSON 配置 */
    int     built_in;                    /* 1=内置字段，不可删除 */
    char    ref_table[FIELD_REF_MAX];    /* link 类型专用 */
    char    ref_field[FIELD_REF_MAX];    /* link 类型专用 */
    char    formula[FIELD_FORMULA_MAX];  /* formula 类型专用 */
    int     sort_order;
    int64_t created_at;
} field_def_t;

/* ============================================================
 * 字段值结构体
 * ============================================================ */
#define FIELD_VALUE_MAX 4096

typedef struct {
    int64_t todo_id;
    int64_t field_id;
    char    value[FIELD_VALUE_MAX];
} field_value_t;

/* ============================================================
 * 字段定义 CRUD
 * ============================================================ */

/**
 * @brief 创建字段定义
 * @param field 字段数据（id 会被忽略，由系统分配）
 * @param out_id 输出新创建的 ID
 * @return 0 成功，-1 失败
 */
int field_def_create(const field_def_t *field, int64_t *out_id);

/**
 * @brief 根据 ID 获取字段定义
 * @param id 字段 ID
 * @param field 输出数据
 * @return 0 成功，-1 未找到
 */
int field_def_get(int64_t id, field_def_t *field);

/**
 * @brief 更新字段定义
 * @param field 字段数据（必须包含有效 id）
 * @return 0 成功，-1 失败
 */
int field_def_update(const field_def_t *field);

/**
 * @brief 删除字段定义（级联删除 field_values）
 * @param id 字段 ID
 * @return 0 成功，-1 未找到或为内置字段
 */
int field_def_delete(int64_t id);

/**
 * @brief 列出所有字段定义
 * @param fields 输出字段数组（需调用 field_def_list_free 释放）
 * @param count 输出字段数量
 * @return 0 成功
 */
int field_def_list(field_def_t **fields, int *count);

/**
 * @brief 释放字段数组
 */
void field_def_list_free(field_def_t *fields, int count);

/**
 * @brief 调整字段排序
 * @param id 字段 ID
 * @param sort_order 新的排序序号
 * @return 0 成功
 */
int field_def_update_sort(int64_t id, int sort_order);

/* 工具函数：类型名与枚举互转 */
int field_type_from_string(const char *type_str);
const char *field_type_to_string(int type);

/* ============================================================
 * 字段值 EAV 读写
 * ============================================================ */

/**
 * @brief 设置字段值（插入或更新）
 * @param todo_id 待办 ID
 * @param field_id 字段 ID
 * @param value 字段值（字符串形式）
 * @return 0 成功
 */
int field_value_set(int64_t todo_id, int64_t field_id, const char *value);

/**
 * @brief 获取字段值
 * @param todo_id 待办 ID
 * @param field_id 字段 ID
 * @param value 输出值缓冲区
 * @param value_size 缓冲区大小
 * @return 0 成功，-1 未找到
 */
int field_value_get(int64_t todo_id, int64_t field_id, char *value, size_t value_size);

/**
 * @brief 删除字段值
 * @param todo_id 待办 ID
 * @param field_id 字段 ID
 * @return 0 成功
 */
int field_value_delete(int64_t todo_id, int64_t field_id);

/**
 * @brief 获取某个 todo 的所有字段值
 * @param todo_id 待办 ID
 * @param values 输出字段值数组（需调用 field_value_list_free 释放）
 * @param count 输出字段值数量
 * @return 0 成功
 */
int field_value_list_by_todo(int64_t todo_id, field_value_t **values, int *count);

/**
 * @brief 释放字段值数组
 */
void field_value_list_free(field_value_t *values, int count);

/**
 * @brief 批量设置字段值
 * @param todo_id 待办 ID
 * @param field_values 字段值数组，每项为 "field_id:value" 字符串对
 * @param count 字段值数量
 * @return 0 成功
 */
int field_value_set_batch(int64_t todo_id, const field_value_t *field_values, int count);

#ifdef __cplusplus
}
#endif

#endif /* TODO_FIELD_H */
