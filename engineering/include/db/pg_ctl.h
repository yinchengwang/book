/**
 * @file pg_ctl.h
 * @brief pg_ctl 工具 - 数据库服务器控制
 *
 * 用于启动、停止、重启数据库服务器，
 * 以及检查服务器状态。
 */
#ifndef DB_PG_CTL_H
#define DB_PG_CTL_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 操作类型
 * ============================================================ */

typedef enum pg_ctl_cmd_e {
    PG_CTL_START,
    PG_CTL_STOP,
    PG_CTL_RESTART,
    PG_CTL_RELOAD,
    PG_CTL_STATUS,
    PG_CTL_PROMOTE,
    PG_CTL_LOG,
    PG_CTL_INITDB
} pg_ctl_cmd_t;

/* ============================================================
 * 选项定义
 * ============================================================ */

/** pg_ctl 选项 */
typedef struct pg_ctl_options_s {
    pg_ctl_cmd_t  cmd;              /**< 命令 */
    const char    *data_dir;        /**< 数据目录 */
    const char    *log_file;        /**< 日志文件 */
    const char    *options;         /**< 传递给服务器的选项 */
    int           wait_timeout;     /**< 等待超时（秒） */
    bool          wait;             /**< 等待服务器启动/停止 */
    bool          silent;           /**< 静默模式 */
    bool          fast;             /**< 快速关闭 */
    bool          immediate;        /**< 立即关闭 */
    bool          force;            /**< 强制操作 */
} pg_ctl_options_t;

/* ============================================================
 * 函数
 * ============================================================ */

/**
 * @brief 执行 pg_ctl 命令
 * @param opts 选项
 * @return 0 成功，-1 失败
 */
int pg_ctl(const pg_ctl_options_t *opts);

/**
 * @brief 解析命令行参数
 */
int pg_ctl_parse_args(int argc, char *argv[], pg_ctl_options_t *opts);

/**
 * @brief 打印帮助
 */
void pg_ctl_usage(const char *prog);

/**
 * @brief 打印版本
 */
void pg_ctl_version(void);

#ifdef __cplusplus
}
#endif

#endif /* DB_PG_CTL_H */
