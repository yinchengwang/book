/**
 * @file copy.h
 * @brief 数据导入导出工具公共接口
 *
 * 提供 COPY 命令的数据导入导出功能，
 * 参考 PostgreSQL 的 COPY 协议实现。
 */
#ifndef DB_TOOLS_COPY_H
#define DB_TOOLS_COPY_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─────────────────────────────────────────────────────────────────
 * 格式枚举
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief COPY 数据格式
 */
typedef enum {
    COPY_FORMAT_CSV,    /**< CSV 格式 */
    COPY_FORMAT_TEXT,   /**< TEXT 格式 */
    COPY_FORMAT_JSON,   /**< JSON 格式 */
    COPY_FORMAT_BINARY, /**< 二进制格式 */
    COPY_FORMAT_SQL     /**< SQL 格式 */
} CopyFormat;

/**
 * @brief COPY 方向
 */
typedef enum {
    COPY_TO,   /**< 导出 */
    COPY_FROM  /**< 导入 */
} CopyDirection;

/* ─────────────────────────────────────────────────────────────────
 * 选项结构
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief COPY 选项
 */
typedef struct {
    CopyFormat format;      /**< 数据格式 */
    char delimiter;         /**< 字段分隔符 */
    char quote_char;        /**< 引号字符 */
    char escape_char;       /**< 转义字符 */
    const char *null_string; /**< NULL 字符串表示 */
    bool header;            /**< 是否包含表头 */
} CopyOptions;

/* ─────────────────────────────────────────────────────────────────
 * 上下文（不透明类型）
 * ───────────────────────────────────────────────────────────────── */

typedef struct CopyContext CopyContext;

/* ─────────────────────────────────────────────────────────────────
 * 公共 API
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 获取默认选项
 */
CopyOptions copy_default_options(void);

/**
 * @brief 创建 COPY 上下文
 * @param opts 选项（NULL 则使用默认）
 * @return 上下文指针，失败返回 NULL
 */
CopyContext *copy_create(const CopyOptions *opts);

/**
 * @brief 销毁 COPY 上下文
 */
void copy_destroy(CopyContext *ctx);

/**
 * @brief 获取错误信息
 */
const char *copy_get_error(const CopyContext *ctx);

/**
 * @brief 获取已处理行数
 */
int copy_get_row_count(const CopyContext *ctx);

/**
 * @brief 获取错误行数
 */
int copy_get_error_count(const CopyContext *ctx);

/**
 * @brief 解析选项字符串
 * @param options_str 选项字符串（如 "FORMAT csv DELIMITER ','"）
 * @param opts 输出选项
 * @return 0 成功，非 0 失败
 */
int copy_parse_options(const char *options_str, CopyOptions *opts);

/* ─────────────────────────────────────────────────────────────────
 * CSV 格式接口
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 导出 CSV 表头
 */
int csv_write_header(FILE *fp, const char **column_names, int num_columns,
                     char delimiter);

/**
 * @brief 导出 CSV 行
 */
int csv_write_row(FILE *fp, const char **values, int num_columns,
                  char delimiter, char quote, char escape,
                  const char *null_string);

/**
 * @brief 解析一行 CSV 数据
 * @param line 输入行
 * @param delimiter 分隔符
 * @param quote 引号字符
 * @param escape 转义字符
 * @param[out] fields 字段数组（需预先分配）
 * @param max_fields 字段数组大小
 * @param[out] num_fields 解析出的字段数
 * @return 0 成功，非 0 失败
 */
int csv_parse_line(const char *line, char delimiter, char quote, char escape,
                   char **fields, int max_fields, int *num_fields);

/* ─────────────────────────────────────────────────────────────────
 * TEXT 格式接口
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 导出 TEXT 表头
 */
int text_write_header(FILE *fp, const char **column_names, int num_columns);

/**
 * @brief 导出 TEXT 行
 */
int text_write_row(FILE *fp, const char **values, int num_columns,
                   char delimiter, const char *null_string);

/**
 * @brief 解析一行 TEXT 数据
 */
int text_parse_line(const char *line, char delimiter,
                    char **fields, int max_fields, int *num_fields);

/* ─────────────────────────────────────────────────────────────────
 * JSON 格式 API
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 导出 JSON 行
 */
int json_write_row(FILE *fp, const char **column_names, const char **values,
                   int num_columns, const char *null_string);

/**
 * @brief 解析 JSON 行
 */
int json_parse_line(const char *line, char **keys, char **values,
                    int max_pairs, int *num_pairs);

/* ─────────────────────────────────────────────────────────────────
 * 二进制格式 API
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 写入二进制文件头
 * @param fp 输出文件
 * @param num_columns 列数
 * @return 0 成功，非 0 失败
 */
int binary_write_header(FILE *fp, int num_columns);

/**
 * @brief 写入二进制行
 * @param fp 输出文件
 * @param values 值数组
 * @param num_columns 列数
 * @return 0 成功，非 0 失败
 */
int binary_write_row(FILE *fp, const char **values, int num_columns);

/**
 * @brief 读取二进制文件头
 * @param fp 输入文件
 * @param[out] num_columns 列数
 * @return 0 成功，非 0 失败
 */
int binary_read_header(FILE *fp, int *num_columns);

/**
 * @brief 读取二进制字段
 * @param fp 输入文件
 * @param[out] value 字段值（需调用方 free，NULL 表示 NULL 字段）
 * @param[out] len 字段长度
 * @return 0 成功，非 0 失败
 */
int binary_read_field(FILE *fp, char **value, uint32_t *len);

/* ─────────────────────────────────────────────────────────────────
 * SQL 格式 API
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 生成 INSERT 语句
 */
int sql_write_insert(FILE *fp, const char *table_name,
                     const char **column_names, const char **values,
                     int num_columns, const char *null_string);

#ifdef __cplusplus
}
#endif

#endif /* DB_TOOLS_COPY_H */