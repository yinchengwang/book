/**
 * @file materialized_view.h
 * @brief 物化视图接口
 *
 * 实现物化视图（Materialized View）支持：
 * 1. 物化视图创建与管理
 * 2. 完整刷新与增量刷新
 * 3. CONCURRENTLY 无锁刷新
 * 4. 查询重写（Query Rewrite）
 * 5. 时序物化视图（Sliding Window）
 */
#ifndef DB_MATERIALIZED_VIEW_H
#define DB_MATERIALIZED_VIEW_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 物化视图状态
 * ======================================================================== */

typedef enum {
    MV_STATE_VALID = 0,       /**< 有效 */
    MV_STATE_STALE = 1,       /**< 需要刷新 */
    MV_STATE_BUILDING = 2,    /**< 构建中 */
    MV_STATE_ERROR = 3,       /**< 错误状态 */
    MV_STATE_UNUSABLE = 4     /**< 不可用（需要完全重建） */
} MvState;

/* ========================================================================
 * 刷新类型
 * ======================================================================== */

typedef enum {
    REFRESH_FULL = 0,         /**< 完整刷新 */
    REFRESH_INCREMENTAL = 1,  /**< 增量刷新 */
    REFRESH_CONCURRENTLY = 2  /**< 无锁刷新 */
} MvRefreshType;

/* ========================================================================
 * 物化视图定义
 * ======================================================================== */

typedef struct MvColumnDef {
    char *name;               /**< 列名 */
    char *type_name;          /**< 类型名 */
    int type_len;             /**< 类型长度 */
    bool is_not_null;         /**< 是否非空 */
} MvColumnDef;

typedef struct MvDef {
    char *name;               /**< 物化视图名称 */
    char *select_sql;         /**< SELECT 查询语句 */
    MvColumnDef *columns;     /**< 列定义数组 */
    int column_count;         /**< 列数 */
    char **source_tables;     /**< 源表名数组 */
    int source_table_count;   /**< 源表数 */
    char *tablespace;         /**< 表空间 */
    int refresh_interval;     /**< 刷新间隔（秒，0表示手动） */
    bool with_data;           /**< 是否物化数据 */
} MvDef;

/* ========================================================================
 * 物化视图数据存储
 * ======================================================================== */

/** 物化视图存储类型 */
typedef enum {
    MV_STORAGE_HEAP = 0,      /**< 堆表存储 */
    MV_STORAGE_CTREE = 1,     /**< 压缩树存储 */
    MV_STORAGE_COLUMNAR = 2   /**< 列式存储 */
} MvStorageType;

/** 物化视图数据 */
typedef struct MvData {
    void *data;               /**< 数据指针 */
    size_t size;              /**< 数据大小 */
    size_t capacity;          /**< 容量 */
    uint64_t row_count;       /**< 行数 */
    MvStorageType storage;    /**< 存储类型 */
} MvData;

/* ========================================================================
 * 物化视图变更追踪（增量刷新用）
 * ======================================================================== */

typedef struct MvChangeLog {
    char *table_name;         /**< 源表名 */
    uint64_t lsn;             /**< 日志序列号 */
    uint64_t xid;             /**< 事务ID */
    char operation;           /**< 操作：I/U/D */
    char *old_row;            /**< 旧行数据 */
    char *new_row;            /**< 新行数据 */
    struct MvChangeLog *next; /**< 下一个变更 */
} MvChangeLog;

/* ========================================================================
 * 物化视图
 * ======================================================================== */

typedef struct MaterializedView MaterializedView;

/* ========================================================================
 * 物化视图管理器
 * ======================================================================== */

typedef struct MvManager MvManager;

/**
 * @brief 创建物化视图管理器
 * @param data_dir 数据目录
 * @return 管理器句柄
 */
MvManager *mv_manager_create(const char *data_dir);

/**
 * @brief 销毁物化视图管理器
 * @param mgr 管理器
 */
void mv_manager_destroy(MvManager *mgr);

/* ========================================================================
 * 物化视图生命周期
 * ======================================================================== */

/**
 * @brief 创建物化视图
 * @param mgr 管理器
 * @param def 物化视图定义
 * @return 物化视图句柄，失败返回NULL
 */
MaterializedView *mv_create(MvManager *mgr, const MvDef *def);

/**
 * @brief 删除物化视图
 * @param mgr 管理器
 * @param name 视图名
 * @return 0成功，-1失败
 */
int mv_drop(MvManager *mgr, const char *name);

/**
 * @brief 获取物化视图
 * @param mgr 管理器
 * @param name 视图名
 * @return 物化视图句柄
 */
MaterializedView *mv_get(const MvManager *mgr, const char *name);

/**
 * @brief 检查物化视图是否存在
 * @param mgr 管理器
 * @param name 视图名
 * @return true存在
 */
bool mv_exists(const MvManager *mgr, const char *name);

/**
 * @brief 列出所有物化视图
 * @param mgr 管理器
 * @param count 输出：视图数量
 * @return 视图名数组（需调用mv_free_names释放）
 */
char **mv_list(const MvManager *mgr, int *count);

/**
 * @brief 释放视图名数组
 * @param names 视图名数组
 * @param count 数组长度
 */
void mv_free_names(char **names, int count);

/* ========================================================================
 * 物化视图刷新
 * ======================================================================== */

/**
 * @brief 刷新物化视图
 * @param view 物化视图
 * @param type 刷新类型
 * @return 0成功，-1失败
 */
int mv_refresh(MaterializedView *view, MvRefreshType type);

/**
 * @brief 异步刷新物化视图
 * @param view 物化视图
 * @param type 刷新类型
 * @return 0成功，-1失败
 */
int mv_refresh_async(MaterializedView *view, MvRefreshType type);

/**
 * @brief 刷新所有物化视图
 * @param mgr 管理器
 * @param type 刷新类型
 * @return 成功刷新数量
 */
int mv_refresh_all(MvManager *mgr, MvRefreshType type);

/* ========================================================================
 * 物化视图属性
 * ======================================================================== */

/**
 * @brief 获取物化视图状态
 * @param view 物化视图
 * @return 状态
 */
MvState mv_get_state(const MaterializedView *view);

/**
 * @brief 获取最后刷新时间
 * @param view 物化视图
 * @return 时间戳（秒）
 */
uint64_t mv_last_refresh_time(const MaterializedView *view);

/**
 * @brief 获取行数
 * @param view 物化视图
 * @return 行数
 */
uint64_t mv_row_count(const MaterializedView *view);

/**
 * @brief 获取数据大小
 * @param view 物化视图
 * @return 字节数
 */
size_t mv_data_size(const MaterializedView *view);

/**
 * @brief 获取视图名
 * @param view 物化视图
 * @return 视图名
 */
const char *mv_get_name(const MaterializedView *view);

/* ========================================================================
 * 查询重写
 * ======================================================================== */

/** 查询重写结果 */
typedef struct MvRewriteResult {
    bool can_rewrite;                 /**< 是否可以重写 */
    const char *original_query;       /**< 原始查询 */
    char *rewritten_query;            /**< 重写后查询（需释放） */
    const char *matched_mv_name;      /**< 匹配的物化视图名 */
    double estimated_speedup;         /**< 估计加速比 */
} MvRewriteResult;

/**
 * @brief 分析查询是否可以重写为物化视图
 * @param mgr 管理器
 * @param sql SQL查询语句
 * @return 重写结果（需调用mv_rewrite_result_free释放）
 */
MvRewriteResult *mv_analyze_rewrite(const MvManager *mgr, const char *sql);

/**
 * @brief 执行查询重写
 * @param mgr 管理器
 * @param sql SQL查询语句
 * @param out_buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 0成功，-1失败
 */
int mv_rewrite_query(const MvManager *mgr, const char *sql,
                     char *out_buffer, size_t buffer_size);

/**
 * @brief 释放重写结果
 * @param result 重写结果
 */
void mv_rewrite_result_free(MvRewriteResult *result);

/* ========================================================================
 * 时序物化视图（Sliding Window）
 * ======================================================================== */

/** 滑动窗口配置 */
typedef struct MvSlidingWindow {
    int window_size;          /**< 窗口大小 */
    int window_unit;          /**< 窗口单位：1=秒，60=分钟，3600=小时，86400=天 */
    int slide_interval;       /**< 滑动间隔 */
    int slide_unit;           /**< 滑动间隔单位 */
    bool keep_history;        /**< 是否保留历史数据 */
} MvSlidingWindow;

/**
 * @brief 设置滑动窗口
 * @param view 物化视图
 * @param config 滑动窗口配置
 * @return 0成功，-1失败
 */
int mv_set_sliding_window(MaterializedView *view, const MvSlidingWindow *config);

/**
 * @brief 获取滑动窗口配置
 * @param view 物化视图
 * @return 滑动窗口配置（返回后不可修改）
 */
const MvSlidingWindow *mv_get_sliding_window(const MaterializedView *view);

/**
 * @brief 推进滑动窗口
 * @param view 物化视图
 * @return 0成功，-1失败
 */
int mv_advance_sliding_window(MaterializedView *view);

/* ========================================================================
 * 变更追踪（增量刷新用）
 * ======================================================================== */

/**
 * @brief 记录源表变更
 * @param view 物化视图
 * @param table_name 源表名
 * @param lsn 日志序列号
 * @param operation 操作类型
 * @param old_row 旧行
 * @param new_row 新行
 * @return 0成功，-1失败
 */
int mv_record_change(MaterializedView *view, const char *table_name,
                     uint64_t lsn, char operation,
                     const char *old_row, const char *new_row);

/**
 * @brief 获取未处理的变更日志
 * @param view 物化视图
 * @param since_lsn 从指定LSN开始
 * @param out_count 输出：变更数量
 * @return 变更日志链表（需调用mv_free_change_log释放）
 */
MvChangeLog *mv_get_changes(MaterializedView *view, uint64_t since_lsn, int *out_count);

/**
 * @brief 释放变更日志
 * @param log 变更日志链表
 */
void mv_free_change_log(MvChangeLog *log);

/**
 * @brief 清理已处理的变更日志
 * @param view 物化视图
 * @param up_to_lsn 清理到此LSN
 */
void mv_purge_changes(MaterializedView *view, uint64_t up_to_lsn);

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

/**
 * @brief 获取物化视图状态名称
 * @param state 状态
 * @return 状态名称
 */
const char *mv_state_name(MvState state);

/**
 * @brief 获取刷新类型名称
 * @param type 刷新类型
 * @return 类型名称
 */
const char *mv_refresh_type_name(MvRefreshType type);

/**
 * @brief 创建物化视图定义
 * @param name 视图名
 * @param sql SELECT语句
 * @return 物化视图定义
 */
MvDef *mv_def_create(const char *name, const char *sql);

/**
 * @brief 添加列定义
 * @param def 物化视图定义
 * @param col_name 列名
 * @param type_name 类型名
 */
void mv_def_add_column(MvDef *def, const char *col_name, const char *type_name);

/**
 * @brief 添加源表
 * @param def 物化视图定义
 * @param table_name 表名
 */
void mv_def_add_source_table(MvDef *def, const char *table_name);

/**
 * @brief 销毁物化视图定义
 * @param def 物化视图定义
 */
void mv_def_destroy(MvDef *def);

#ifdef __cplusplus
}
#endif

#endif /* DB_MATERIALIZED_VIEW_H */
