/**
 * @file initdb.h
 * @brief initdb 工具 - 数据库集群初始化
 *
 * 用于初始化 PostgreSQL 风格的数据目录结构，
 * 包括系统表、WAL 目录、配置文件等。
 */
#ifndef DB_INITDB_H
#define DB_INITDB_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 选项定义
 * ============================================================ */

/** initdb 选项 */
typedef struct initdb_options_s {
    const char  *data_dir;          /**< 数据目录 */
    const char  *username;          /**< 数据库超级用户 */
    const char  *encoding;          /**< 编码（默认 UTF8） */
    const char  *locale;            /**< 区域设置 */
    bool        no_locale;          /**< 禁用区域设置 */
    bool        auth_local;         /**< 使用本地认证 */
    const char  *auth_method;       /**< 认证方法 */
    bool        pw_filename;        /**< 从文件读取密码 */
    bool        skip_checksums;     /**< 跳过校验和验证 */
    bool        dry_run;            /**< 干运行模式 */
} initdb_options_t;

/* ============================================================
 * 初始化函数
 * ============================================================ */

/**
 * @brief 执行 initdb
 * @param opts initdb 选项
 * @return 0 成功，-1 失败
 */
int initdb(const initdb_options_t *opts);

/**
 * @brief 获取默认 initdb 选项
 * @return 默认选项（静态分配）
 */
initdb_options_t *initdb_default_options(void);

/**
 * @brief 解析命令行选项
 * @param argc 参数个数
 * @param argv 参数数组
 * @param opts 输出选项
 * @return 0 成功，-1 失败
 */
int initdb_parse_args(int argc, char *argv[], initdb_options_t *opts);

/**
 * @brief 打印帮助信息
 * @param prog 程序名
 */
void initdb_usage(const char *prog);

/**
 * @brief 检查数据目录是否已初始化
 * @param data_dir 数据目录
 * @return true 已初始化，false 未初始化
 */
bool initdb_is_initialized(const char *data_dir);

/* ============================================================
 * 子步骤（可单独调用）
 * ============================================================ */

/**
 * @brief 创建目录结构
 * @param data_dir 数据目录
 * @return 0 成功，-1 失败
 */
int initdb_create_directories(const char *data_dir);

/**
 * @brief 创建系统表
 * @param data_dir 数据目录
 * @return 0 成功，-1 失败
 */
int initdb_create_system_tables(const char *data_dir);

/**
 * @brief 创建模板数据库
 * @param data_dir 数据目录
 * @return 0 成功，-1 失败
 */
int initdb_create_template_db(const char *data_dir);

/**
 * @brief 生成 postgresql.conf
 * @param data_dir 数据目录
 * @return 0 成功，-1 失败
 */
int initdb_generate_config(const char *data_dir);

/**
 * @brief 生成 pg_hba.conf
 * @param data_dir 数据目录
 * @return 0 成功，-1 失败
 */
int initdb_generate_hba(const char *data_dir);

/**
 * @brief 初始化 WAL
 * @param data_dir 数据目录
 * @return 0 成功，-1 失败
 */
int initdb_init_wal(const char *data_dir);

#ifdef __cplusplus
}
#endif

#endif /* DB_INITDB_H */
