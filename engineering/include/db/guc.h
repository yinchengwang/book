/**
 * @file guc.h
 * @brief GUC（Grand Unified Configuration）配置系统
 *
 * PostgreSQL 风格的运行时配置系统，支持：
 * - 配置文件加载（postgresql.conf）
 * - 运行时参数设置
 * - 参数验证和类型转换
 * - 单位转换（MB, GB, kB, s, min 等）
 */
#ifndef DB_GUC_H
#define DB_GUC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 类型定义
 * ============================================================ */

/** GUC 参数类型 */
typedef enum guc_type_e {
    GUC_TYPE_BOOL = 0,     /**< 布尔值 */
    GUC_TYPE_INT,           /**< 整数 */
    GUC_TYPE_REAL,          /**< 浮点数 */
    GUC_TYPE_STRING,        /**< 字符串 */
    GUC_TYPE_ENUM           /**< 枚举值 */
} guc_type_t;

/** GUC 标志 */
typedef enum guc_flags_e {
    GUC_FLAG_NONE       = 0,
    GUC_FLAG_EXPLAIN    = 0x0001,  /**< 可在 EXPLAIN 中显示 */
    GUC_FLAG_NO_SHOW    = 0x0002,  /**< 不在 SHOW ALL 中显示 */
    GUC_FLAG_NOT_WATCH  = 0x0004,  /**< 不在 watch 列表中 */
    GUC_FLAG_NO_RESET   = 0x0008,  /**< ALTER ... RESET ALL 不影响 */
    GUC_FLAG_RELOAD     = 0x0010,  /**< 配置文件重新加载时生效 */
    GUC_FLAG_RUNTIME    = 0x0020   /**< 运行时计算，不可 SET */
} guc_flags_t;

/* ============================================================
 * 参数结构
 * ============================================================ */

/** GUC 参数定义 */
typedef struct guc_param_s {
    const char      *name;           /**< 参数名 */
    guc_type_t      type;            /**< 参数类型 */
    void            *value;          /**< 当前值 */
    void            *reset_val;       /**< 重置值 */
    union {
        struct {
            int     min;             /**< 最小值 */
            int     max;             /**< 最大值 */
        } int_v;
        struct {
            double  min;             /**< 最小值 */
            double  max;             /**< 最大值 */
        } real_v;
        struct {
            const char **options;      /**< 枚举选项列表 */
            int         noptions;     /**< 选项数量 */
        } enum_v;
    } bounds;
    const char      *description;    /**< 参数描述 */
    guc_flags_t     flags;           /**< 标志 */
} guc_param_t;

/** GUC 统计信息 */
typedef struct guc_stats_s {
    uint64_t    set_count;       /**< SET 调用次数 */
    uint64_t    reset_count;     /**< RESET 调用次数 */
    uint64_t    load_count;      /**< 配置文件加载次数 */
} guc_stats_t;

/* ============================================================
 * 生命周期
 * ============================================================ */

/**
 * @brief 初始化 GUC 系统
 * @param data_dir 数据目录（用于查找配置文件）
 * @return 0 成功，-1 失败
 */
int guc_init(const char *data_dir);

/**
 * @brief 销毁 GUC 系统
 */
void guc_shutdown(void);

/**
 * @brief 重新加载配置文件
 * @param path 配置文件路径（NULL 使用默认路径）
 * @return 0 成功，-1 失败
 */
int guc_reload(const char *path);

/* ============================================================
 * 参数操作
 * ============================================================ */

/**
 * @brief 设置参数值
 * @param name 参数名
 * @param value 值字符串
 * @return 0 成功，-1 失败
 */
int guc_set(const char *name, const char *value);

/**
 * @brief 获取参数值
 * @param name 参数名
 * @param buf 输出缓冲区
 * @param buflen 缓冲区长度
 * @return 0 成功，-1 失败
 */
int guc_get(const char *name, char *buf, size_t buflen);

/**
 * @brief 获取参数整数值的指针（直接访问）
 * @param name 参数名
 * @return 整数指针，NULL 不存在或类型不匹配
 */
int *guc_get_int(const char *name);

/**
 * @brief 获取参数布尔值的指针（直接访问）
 * @param name 参数名
 * @return 布尔指针，NULL 不存在或类型不匹配
 */
bool *guc_get_bool(const char *name);

/**
 * @brief 获取参数字符串值（直接访问）
 * @param name 参数名
 * @return 字符串指针，NULL 不存在或类型不匹配
 */
const char *guc_get_string(const char *name);

/**
 * @brief 重置参数为默认值
 * @param name 参数名（NULL 表示全部）
 */
void guc_reset(const char *name);

/**
 * @brief 注册一个 GUC 参数到参数表
 * @param param 参数定义（含名称、类型、默认值、范围等）
 * @return 0 成功，-1 失败（参数名重复、表已满或类型不支持）
 */
int guc_register_param(const guc_param_t *param);

/**
 * @brief 获取参数信息
 * @param name 参数名
 * @return 参数信息，NULL 不存在
 */
const guc_param_t *guc_get_param(const char *name);

/* ============================================================
 * 配置文件
 * ============================================================ */

/**
 * @brief 加载配置文件
 * @param path 配置文件路径（NULL 使用默认路径）
 * @return 0 成功，-1 失败
 */
int guc_load_file(const char *path);

/**
 * @brief 保存当前配置到文件
 * @param path 输出路径
 * @return 0 成功
 */
int guc_save_file(const char *path);

/* ============================================================
 * 枚举值
 * ============================================================ */

/**
 * @brief 获取枚举值的索引
 * @param param 参数
 * @param value 值字符串
 * @return 索引，-1 无效
 */
int guc_enum_index(const guc_param_t *param, const char *value);

/**
 * @brief 获取枚举值的字符串
 * @param param 参数
 * @param index 索引
 * @return 字符串，NULL 无效索引
 */
const char *guc_enum_option(const guc_param_t *param, int index);

/* ============================================================
 * 统计
 * ============================================================ */

/**
 * @brief 获取统计信息
 * @param stats 输出统计
 */
void guc_get_stats(guc_stats_t *stats);

/**
 * @brief 重置统计信息
 */
void guc_reset_stats(void);

/* ============================================================
 * SHOW 命令支持
 * ============================================================ */

/**
 * @brief 获取所有参数（SHOW ALL）
 * @param names 输出参数名数组（调用者分配 n * 64 字节）
 * @param values 输出值数组（调用者分配 n * 256 字节）
 * @param n 最大数量
 * @return 实际数量
 */
int guc_show_all(char **names, char **values, int n);

#ifdef __cplusplus
}
#endif

#endif /* DB_GUC_H */
