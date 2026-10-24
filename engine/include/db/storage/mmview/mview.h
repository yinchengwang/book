/**
 * @file mview.h
 * @brief 物化视图头文件
 *
 * 实现物化视图系统：
 * 1. 物化视图依赖图
 * 2. 增量刷新（CDC）
 * 3. 定时刷新调度器
 * 4. 链式依赖和级联刷新
 * 5. 查询改写
 */
#ifndef DB_MVIEW_H
#define DB_MVIEW_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 物化视图状态
 * ======================================================================== */

/** 物化视图刷新状态 */
typedef enum MViewState_e {
    MVIEW_STATE_INVALID = 0,    /**< 无效 */
    MVIEW_STATE_STALE = 1,      /**< 数据过期 */
    MVIEW_STATE_REFRESHING = 2, /**< 正在刷新 */
    MVIEW_STATE_READY = 3       /**< 就绪 */
} MViewState;

/** 刷新类型 */
typedef enum MViewRefreshType_e {
    MVIEW_REFRESH_COMPLETE = 0,  /**< 全量刷新 */
    MVIEW_REFRESH_FAST = 1      /**< 增量刷新 */
} MViewRefreshType;

/* ========================================================================
 * 物化视图元数据
 * ======================================================================== */

/** 物化视图信息 */
typedef struct MViewInfo_s {
    uint32_t mview_id;              /**< 物化视图 ID */
    uint32_t relfilenode;           /**< 存储的文件节点 */
    char name[256];                 /**< 视图名称 */
    char query[4096];              /**< 定义查询 */
    MViewState state;              /**< 当前状态 */
    MViewRefreshType refresh_type;   /**< 刷新类型 */
    uint64_t last_refresh_time;     /**< 上次刷新时间 */
    uint64_t last_refresh_duration;  /**< 上次刷新耗时 */
    uint32_t row_count;            /**< 行数 */
    uint64_t size_bytes;           /**< 大小（字节） */
    uint32_t refcount;             /**< 被引用次数 */
} MViewInfo;

/** 物化视图依赖关系 */
typedef struct MViewDep_s {
    uint32_t mview_id;           /**< 物化视图 ID */
    uint32_t depends_on;         /**< 依赖的物化视图 ID */
    uint32_t depends_on_relid;   /**< 依赖的基础表 ID */
} MViewDep;

/** 物化视图变更记录（用于增量刷新） */
typedef struct MViewChange_s {
    uint32_t relfilenode;        /**< 变更的表 */
    uint32_t mview_id;          /**< 相关的物化视图 */
    uint64_t change_time;        /**< 变更时间 */
    uint8_t change_type;        /**< 变更类型：I/U/D */
    uint64_t change_id;         /**< 变更序列号 */
    uint32_t row_data[32];      /**< 变更的数据（简化） */
} MViewChange;

/* ========================================================================
 * 物化视图依赖图
 * ======================================================================== */

/** 依赖图节点 */
typedef struct MViewGraphNode_s {
    uint32_t mview_id;                    /**< 物化视图 ID */
    struct MViewGraphNode_s **dependencies; /**< 依赖节点数组 */
    int dep_count;                        /**< 依赖数量 */
    struct MViewGraphNode_s **dependents;  /**< 被依赖节点数组 */
    int depent_count;                     /**< 被依赖数量 */
    int refresh_order;                    /**< 刷新顺序（拓扑排序结果） */
} MViewGraphNode;

/**
 * @brief 物化视图依赖图
 */
typedef struct MViewGraph_s {
    MViewGraphNode *nodes;    /**< 节点数组 */
    int node_count;           /**< 节点数量 */
    int max_nodes;            /**< 最大节点数 */
} MViewGraph;

/* ========================================================================
 * 刷新调度器
 * ======================================================================== */

/** 刷新调度策略 */
typedef enum MViewSchedulePolicy_e {
    MVIEW_SCHEDULE_ON_DEMAND = 0,   /**< 按需刷新 */
    MVIEW_SCHEDULE_INTERVAL = 1,     /**< 定时刷新 */
    MVIEW_SCHEDULE_CRON = 2          /**< Cron 表达式刷新 */
} MViewSchedulePolicy;

/** 刷新调度信息 */
typedef struct MViewSchedule_s {
    uint32_t mview_id;                  /**< 物化视图 ID */
    MViewSchedulePolicy policy;          /**< 调度策略 */
    uint64_t interval_ms;               /**< 刷新间隔（毫秒） */
    char cron_expr[64];                 /**< Cron 表达式 */
    uint64_t next_refresh_time;         /**< 下次刷新时间 */
    bool enabled;                       /**< 是否启用 */
} MViewSchedule;

/* ========================================================================
 * 物化视图 API
 * ======================================================================== */

/**
 * @brief 初始化物化视图子系统
 */
void mview_init(void);

/**
 * @brief 关闭物化视图子系统
 */
void mview_shutdown(void);

/**
 * @brief 创建物化视图
 *
 * @param name 视图名称
 * @param query 定义查询
 * @param refresh_type 刷新类型
 * @param mview_id 输出视图 ID
 * @return 0 成功
 */
int mview_create(const char *name, const char *query,
                MViewRefreshType refresh_type, uint32_t *mview_id);

/**
 * @brief 删除物化视图
 *
 * @param mview_id 视图 ID
 * @return 0 成功
 */
int mview_drop(uint32_t mview_id);

/**
 * @brief 获取物化视图信息
 */
const MViewInfo *mview_get_info(uint32_t mview_id);

/**
 * @brief 全量刷新物化视图
 *
 * @param mview_id 视图 ID
 * @return 0 成功
 */
int mview_refresh_complete(uint32_t mview_id);

/**
 * @brief 增量刷新物化视图
 *
 * @param mview_id 视图 ID
 * @return 0 成功
 */
int mview_refresh_fast(uint32_t mview_id);

/**
 * @brief 刷新所有过期的物化视图
 *
 * @param cascade 是否级联刷新依赖的视图
 * @return 刷新的视图数
 */
int mview_refresh_stale(bool cascade);

/* ========================================================================
 * 依赖图管理
 * ======================================================================== */

/**
 * @brief 添加物化视图依赖
 *
 * @param mview_id 物化视图 ID
 * @param depends_on 依赖的视图 ID
 * @return 0 成功
 */
int mview_add_dependency(uint32_t mview_id, uint32_t depends_on);

/**
 * @brief 添加基础表依赖
 *
 * @param mview_id 物化视图 ID
 * @param relfilenode 基础表文件节点
 * @return 0 成功
 */
int mview_add_base_dependency(uint32_t mview_id, uint32_t relfilenode);

/**
 * @brief 获取物化视图的依赖顺序（拓扑排序）
 *
 * @param order 输出顺序数组
 * @param max_count 数组最大长度
 * @return 顺序中的视图数
 */
int mview_get_refresh_order(uint32_t *order, int max_count);

/**
 * @brief 检查是否存在循环依赖
 *
 * @return true 存在循环依赖
 */
bool mview_has_cycle(void);

/**
 * @brief 获取需要刷新的视图列表（按依赖顺序）
 *
 * @param stale_only 只返回过期的
 * @param order 输出顺序数组
 * @param max_count 数组最大长度
 * @return 视图数
 */
int mview_get_refresh_list(bool stale_only, uint32_t *order, int max_count);

/* ========================================================================
 * CDC（变更数据捕获）
 * ======================================================================== */

/**
 * @brief 记录表变更（用于增量刷新）
 *
 * @param relfilenode 表文件节点
 * @param change_type 变更类型
 * @param change_data 变更数据
 * @param data_size 数据大小
 */
void mview_record_change(uint32_t relfilenode, uint8_t change_type,
                        const void *change_data, size_t data_size);

/**
 * @brief 获取表的未处理变更
 *
 * @param relfilenode 表文件节点
 * @param changes 输出变更数组
 * @param max_changes 最大变更数
 * @return 实际变更数
 */
int mview_get_changes(uint32_t relfilenode,
                      MViewChange *changes, int max_changes);

/**
 * @brief 标记变更已处理
 *
 * @param change_id 最后处理的变更 ID
 */
void mview_acknowledge_changes(uint32_t relfilenode, uint64_t change_id);

/**
 * @brief 清除变更记录
 *
 * @param relfilenode 表文件节点
 */
void mview_clear_changes(uint32_t relfilenode);

/* ========================================================================
 * 刷新调度
 * ======================================================================== */

/**
 * @brief 设置刷新调度
 *
 * @param mview_id 视图 ID
 * @param schedule 调度信息
 * @return 0 成功
 */
int mview_set_schedule(uint32_t mview_id, const MViewSchedule *schedule);

/**
 * @brief 获取刷新调度
 */
const MViewSchedule *mview_get_schedule(uint32_t mview_id);

/**
 * @brief 启用刷新调度
 */
int mview_enable_schedule(uint32_t mview_id);

/**
 * @brief 禁用刷新调度
 */
int mview_disable_schedule(uint32_t mview_id);

/**
 * @brief 执行定时刷新
 *
 * @return 刷新的视图数
 */
int mview_process_scheduled(void);

/**
 * @brief 获取下一个需要刷新的视图
 *
 * @param mview_id 输出视图 ID
 * @return true 有待刷新的视图
 */
bool mview_get_next_scheduled(uint32_t *mview_id);

/* ========================================================================
 * 查询改写
 * ======================================================================== */

/**
 * @brief 检查查询是否可以改写为使用物化视图
 *
 * @param query 查询语句
 * @param mview_id 输出可用的物化视图 ID
 * @return true 可以改写
 */
bool mview_can_rewrite(const char *query, uint32_t *mview_id);

/**
 * @brief 获取物化视图的查询替换
 *
 * @param mview_id 物化视图 ID
 * @param original_query 原始查询
 * @param rewritten_query 输出改写后的查询
 * @param max_size 输出缓冲区大小
 * @return 0 成功
 */
int mview_get_rewrite_query(uint32_t mview_id,
                            const char *original_query,
                            char *rewritten_query, size_t max_size);

/**
 * @brief 估算使用物化视图的代价
 *
 * @param mview_id 物化视图 ID
 * @return 代价（越小越好）
 */
double mview_estimate_cost(uint32_t mview_id);

/**
 * @brief 检查物化视图是否足够新鲜
 *
 * @param mview_id 视图 ID
 * @param max_age_ms 最大允许年龄（毫秒）
 * @return true 足够新鲜
 */
bool mview_is_fresh_enough(uint32_t mview_id, uint64_t max_age_ms);

/* ========================================================================
 * 统计和调试
 * ======================================================================== */

/** 物化视图统计 */
typedef struct MViewStats_s {
    uint32_t total_mviews;          /**< 物化视图总数 */
    uint32_t stale_mviews;          /**< 过期视图数 */
    uint32_t refreshing_mviews;     /**< 正在刷新的视图数 */
    uint64_t total_refreshes;      /**< 总刷新次数 */
    uint64_t total_refresh_time;   /**< 总刷新时间 */
    uint32_t dependency_cycles;     /**< 循环依赖数 */
} MViewStats;

/**
 * @brief 获取物化视图统计
 */
void mview_get_stats(MViewStats *stats);

/**
 * @brief 打印物化视图状态
 */
void mview_dump(void);

/**
 * @brief 打印依赖图
 */
void mview_dump_graph(void);

#ifdef __cplusplus
}
#endif

#endif /* DB_MVIEW_H */
