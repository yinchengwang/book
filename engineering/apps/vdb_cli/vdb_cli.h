/**
 * @file vdb_cli.h
 * @brief MiniVecDB CLI 工具接口定义
 *
 * 提供向量数据库的命令行工具接口，支持：
 * - 集合管理: create, drop, list
 * - 向量操作: insert, query, delete
 * - 批量操作: import, export, benchmark
 * - 交互模式: shell (REPL)
 *
 * 使用流程：
 *   vdb_cli <command> [options] [args]
 *
 * 示例：
 *   vdb_cli create --collection test --dim 128
 *   vdb_cli insert --collection test --vector 1.0,2.0,3.0
 *   vdb_cli query --collection test --vector 1.0,2.0,3.0 --k 10
 *   vdb_cli shell
 */
#ifndef VDB_CLI_H
#define VDB_CLI_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * CLI 配置
 * ======================================================================== */

/** CLI 配置 */
typedef struct vdb_cli_config_s {
    const char *data_dir;        /**< 数据目录路径 */
    const char *prompt;          /**< 命令提示符 */
    size_t history_size;         /**< 历史记录条数 */
    bool echo;                   /**< 回显命令 */
    bool show_timing;            /**< 显示执行时间 */
    bool json_output;            /**< JSON 输出模式 */
    bool color_output;           /**< 彩色输出 */
} vdb_cli_config_t;

/** 默认配置 */
#define VDB_CLI_DEFAULT_CONFIG { \
    .data_dir = "./vdb_data",   \
    .prompt = "vdb> ",          \
    .history_size = 100,        \
    .echo = true,               \
    .show_timing = true,        \
    .json_output = false,       \
    .color_output = true        \
}

/* ========================================================================
 * CLI 上下文
 * ======================================================================== */

typedef struct vdb_cli_s vdb_cli_t;

/* ========================================================================
 * CLI 创建/销毁
 * ======================================================================== */

/**
 * @brief 创建 CLI 实例
 * @param config 配置（NULL 使用默认配置）
 * @return CLI 句柄，失败返回 NULL
 */
vdb_cli_t *vdb_cli_create(const vdb_cli_config_t *config);

/**
 * @brief 销毁 CLI 实例
 * @param cli CLI
 */
void vdb_cli_destroy(vdb_cli_t *cli);

/* ========================================================================
 * 命令执行
 * ======================================================================== */

/**
 * @brief 执行单条命令
 * @param cli CLI
 * @param command 命令名
 * @param argc 参数个数
 * @param argv 参数数组
 * @return 0 成功，非 0 失败
 */
int vdb_cli_exec(vdb_cli_t *cli, const char *command, int argc, char *argv[]);

/**
 * @brief 打印欢迎信息
 */
void vdb_cli_print_welcome(void);

/**
 * @brief 打印帮助信息
 * @param command 命令名（NULL 打印总体帮助）
 */
void vdb_cli_print_help(const char *command);

/**
 * @brief 打印错误信息
 * @param msg 错误信息
 */
void vdb_cli_print_error(const char *msg);

/**
 * @brief 打印版本信息
 */
void vdb_cli_print_version(void);

/* ========================================================================
 * 输出格式化
 * ======================================================================== */

/** 输出格式 */
typedef enum {
    VDB_OUTPUT_TABLE,   /**< 表格输出 */
    VDB_OUTPUT_JSON,    /**< JSON 输出 */
    VDB_OUTPUT_SILENT   /**< 静默模式 */
} vdb_output_format_t;

/**
 * @brief 设置输出格式
 * @param cli CLI
 * @param format 输出格式
 */
void vdb_cli_set_output_format(vdb_cli_t *cli, vdb_output_format_t format);

/**
 * @brief 获取输出格式
 * @param cli CLI
 * @return 输出格式
 */
vdb_output_format_t vdb_cli_get_output_format(vdb_cli_t *cli);

#ifdef __cplusplus
}
#endif

#endif /* VDB_CLI_H */
