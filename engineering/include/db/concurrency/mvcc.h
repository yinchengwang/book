/*
 * mvcc.h - MVCC 多版本并发控制公共接口（旧版，保留接口定义）
 *
 * 注意：此头文件已废弃，功能合并到 db/storage/txn/mvcc.h
 * 新代码应使用 db/storage/txn/mvcc.h
 */

#ifndef DB_CONCURRENCY_MVCC_H
#define DB_CONCURRENCY_MVCC_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─────────────────────────────────────────────────────────────────
 * 类型定义
 * ───────────────────────────────────────────────────────────────── */

/* 事务 ID 类型 */
typedef int64_t mvcc_txn_id_t;

/* 无效事务 ID */
#define MVCC_INVALID_TXN_ID ((mvcc_txn_id_t)0)

/* 事务 ID 最大值（用于检测回绕） */
#define MVCC_MAX_TXN_ID INT64_MAX

/* 行版本指针（指向页面中的槽） */
typedef struct mvcc_ctid {
    uint32_t block_num;   /* 页面号 */
    uint16_t offset;      /* 页面内偏移/槽号 */
} mvcc_ctid_t;

/* ─────────────────────────────────────────────────────────────────
 * 版本链结构
 * ───────────────────────────────────────────────────────────────── */

/**
 * MVCC 行版本结构
 *
 * 字段命名参考 PostgreSQL：
 * - xmin: 创建该版本的事务 ID
 * - xmax: 删除该版本的事务 ID (0 表示未被删除)
 * - ctid: 版本链指针，指向下一个版本
 * - xvac: Undo 记录指针（用于 GC）
 */
typedef struct mvcc_version {
    mvcc_txn_id_t xmin;           /* 创建事务 ID */
    mvcc_txn_id_t xmax;           /* 删除事务 ID (0 = 未删除) */
    mvcc_ctid_t ctid;             /* 指向下一个版本的指针 */
    mvcc_ctid_t xvac;             /* Undo 记录指针 */

    void *data;                   /* 行数据 payload */
    size_t data_size;             /* 数据大小 */

    struct mvcc_version *next;    /* 链表指针（内存中） */
} mvcc_version_t;

/* ─────────────────────────────────────────────────────────────────
 * Undo 日志结构
 * ───────────────────────────────────────────────────────────────── */

/**
 * Undo 日志记录类型
 */
typedef enum {
    MVCC_UNDO_INSERT = 1,  /* 插入操作的 Undo */
    MVCC_UNDO_UPDATE = 2,  /* 更新操作的 Undo */
    MVCC_UNDO_DELETE = 3   /* 删除操作的 Undo */
} mvcc_undo_type_t;

/**
 * Undo 日志记录
 */
typedef struct mvcc_undo_record {
    mvcc_txn_id_t txn_id;         /* 所属事务 ID */
    mvcc_undo_type_t type;        /* 操作类型 */
    void *old_data;               /* 旧数据（更新/删除时） */
    size_t old_data_size;         /* 旧数据大小 */
    mvcc_ctid_t row_ptr;          /* 指向原行的指针 */
    mvcc_ctid_t prev_undo;        /* 前一个 Undo 记录指针 */
} mvcc_undo_record_t;

/* ─────────────────────────────────────────────────────────────────
 * 快照结构
 * ───────────────────────────────────────────────────────────────── */

/**
 * MVCC 快照
 *
 * 快照在事务开始时创建，包含事务开始时的状态：
 * - xmin: 快照创建时的最小活动事务 ID
 * - xmax: 快照创建时的最大已分配事务 ID
 * - xip_list: 快照创建时活跃的事务 ID 列表
 */
typedef struct mvcc_snapshot {
    mvcc_txn_id_t xmin;           /* 最小活动事务 ID */
    mvcc_txn_id_t xmax;           /* 最大已分配事务 ID */

    /* 活跃事务 ID 列表 */
    mvcc_txn_id_t *xip_list;      /* 数组 */
    int xip_count;                /* 列表长度 */
    int xip_capacity;             /* 数组容量 */
} mvcc_snapshot_t;

/* ─────────────────────────────────────────────────────────────────
 * GC 配置
 * ───────────────────────────────────────────────────────────────── */

/**
 * GC 配置参数
 */
typedef struct mvcc_gc_config {
    /* 触发阈值 */
    int vacuum_threshold;         /* 死亡元组数量阈值 */
    double vacuum_scale_factor;   /* 相对于表大小的比例因子 */

    /* 成本控制 */
    int vacuum_cost_delay;        /* GC 延迟（毫秒） */
    int vacuum_cost_limit;        /* 成本限制 */

    /* 后台任务 */
    int autovacuum_naptime;       /* 自动 VACUUM 间隔（秒） */
    bool autovacuum_enabled;      /* 是否启用自动 VACUUM */
} mvcc_gc_config_t;

/* ─────────────────────────────────────────────────────────────────
 * 版本链操作
 * ───────────────────────────────────────────────────────────────── */

/**
 * 创建新版本
 * @param xmin 创建事务 ID
 * @param data 行数据
 * @param data_size 数据大小
 * @return 新版本指针，失败返回 NULL
 */
mvcc_version_t *mvcc_version_new(mvcc_txn_id_t xmin,
                                  const void *data,
                                  size_t data_size);

/**
 * 销毁版本
 * @param version 版本指针
 */
void mvcc_version_destroy(mvcc_version_t *version);

/**
 * 在版本链头部插入新版本
 * @param head 版本链头指针的地址
 * @param new_version 新版本
 */
void mvcc_version_insert(mvcc_version_t **head, mvcc_version_t *new_version);

/**
 * 标记版本为已删除
 * @param version 版本指针
 * @param xmax 删除事务 ID
 */
void mvcc_version_mark_deleted(mvcc_version_t *version, mvcc_txn_id_t xmax);

/**
 * 获取版本链长度
 * @param head 版本链头
 * @return 版本数量
 */
int mvcc_version_chain_length(const mvcc_version_t *head);

/* ─────────────────────────────────────────────────────────────────
 * 快照操作
 * ───────────────────────────────────────────────────────────────── */

/**
 * 创建快照
 * @param xmin 最小活动事务 ID
 * @param xmax 最大已分配事务 ID
 * @param active_txns 活跃事务 ID 数组
 * @param active_count 活跃事务数量
 * @return 快照指针，失败返回 NULL
 */
mvcc_snapshot_t *mvcc_snapshot_create(mvcc_txn_id_t xmin,
                                       mvcc_txn_id_t xmax,
                                       const mvcc_txn_id_t *active_txns,
                                       int active_count);

/**
 * 销毁快照
 * @param snapshot 快照指针
 */
void mvcc_snapshot_destroy(mvcc_snapshot_t *snapshot);

/**
 * 复制快照
 * @param src 源快照
 * @return 新的快照副本
 */
mvcc_snapshot_t *mvcc_snapshot_copy(const mvcc_snapshot_t *src);

/**
 * 快照中是否包含指定事务
 * @param snapshot 快照
 * @param txn_id 事务 ID
 * @return true 如果事务在快照中
 */
bool mvcc_snapshot_contains_txn(const mvcc_snapshot_t *snapshot,
                                 mvcc_txn_id_t txn_id);

/**
 * 获取快照序列化大小
 * @param snapshot 快照
 * @return 序列化所需字节数
 */
size_t mvcc_snapshot_serialize_size(const mvcc_snapshot_t *snapshot);

/**
 * 序列化快照到缓冲区
 * @param snapshot 快照
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 写入的字节数，失败返回 0
 */
size_t mvcc_snapshot_serialize(const mvcc_snapshot_t *snapshot,
                                void *buf,
                                size_t buf_size);

/**
 * 从缓冲区反序列化快照
 * @param buf 输入缓冲区
 * @param buf_size 缓冲区大小
 * @return 新的快照指针，失败返回 NULL
 */
mvcc_snapshot_t *mvcc_snapshot_deserialize(const void *buf,
                                            size_t buf_size);

/* ─────────────────────────────────────────────────────────────────
 * 可见性判断
 * ───────────────────────────────────────────────────────────────── */

/**
 * 判断单版本对快照是否可见
 *
 * 可见性规则：
 * 1. xmin 事务已提交（不在活跃列表且 xmin < snapshot->xmax）
 * 2. 版本未被删除（xmax = 0）或 xmax 事务未提交或 xmax >= snapshot->xmax
 *
 * @param snapshot 快照
 * @param xmin 创建事务 ID
 * @param xmax 删除事务 ID (0 表示未删除)
 * @param cur_txn_id 当前事务 ID（自身创建的版本始终可见）
 * @return true 如果版本可见
 */
bool mvcc_version_visible(const mvcc_snapshot_t *snapshot,
                          mvcc_txn_id_t xmin,
                          mvcc_txn_id_t xmax,
                          mvcc_txn_id_t cur_txn_id);

/**
 * 在版本链中查找对快照可见的版本
 * @param head 版本链头
 * @param snapshot 快照
 * @param cur_txn_id 当前事务 ID
 * @return 可见版本指针，未找到返回 NULL
 */
mvcc_version_t *mvcc_version_find_visible(const mvcc_version_t *head,
                                           const mvcc_snapshot_t *snapshot,
                                           mvcc_txn_id_t cur_txn_id);

/**
 * 检测写冲突
 *
 * 当事务 A 尝试修改行，而事务 B 已经修改了同一行且已提交，
 * 检测到写冲突。
 *
 * @param snapshot 当前事务快照
 * @param row_xmax 行的当前 xmax
 * @param cur_txn_id 当前事务 ID
 * @return true 如果存在冲突
 */
bool mvcc_check_write_conflict(const mvcc_snapshot_t *snapshot,
                               mvcc_txn_id_t row_xmax,
                               mvcc_txn_id_t cur_txn_id);

/* ─────────────────────────────────────────────────────────────────
 * Undo 日志操作
 * ───────────────────────────────────────────────────────────────── */

/**
 * 创建 Undo 记录
 * @param txn_id 事务 ID
 * @param type 操作类型
 * @param old_data 旧数据
 * @param old_data_size 旧数据大小
 * @param row_ptr 行指针
 * @return Undo 记录指针
 */
mvcc_undo_record_t *mvcc_undo_create(mvcc_txn_id_t txn_id,
                                      mvcc_undo_type_t type,
                                      const void *old_data,
                                      size_t old_data_size,
                                      const mvcc_ctid_t *row_ptr);

/**
 * 销毁 Undo 记录
 * @param record Undo 记录
 */
void mvcc_undo_destroy(mvcc_undo_record_t *record);

/**
 * 回滚操作（从 Undo 恢复）
 * @param record Undo 记录
 * @return 0 成功，非 0 失败
 */
int mvcc_undo_apply(mvcc_undo_record_t *record);

/* ─────────────────────────────────────────────────────────────────
 * 垃圾回收
 * ───────────────────────────────────────────────────────────────── */

/**
 * 获取默认 GC 配置
 * @return 默认配置
 */
mvcc_gc_config_t mvcc_gc_default_config(void);

/**
 * 分析表的死亡元组
 * @param table_oid 表 ID
 * @param dead_tuple_count 输出：死亡元组数量
 * @return 0 成功，非 0 失败
 */
int mvcc_gc_analyze(uint32_t table_oid, int *dead_tuple_count);

/**
 * 执行 VACUUM 清理
 * @param table_oid 表 ID
 * @param config GC 配置
 * @return 清理的页面数，失败返回负数
 */
int mvcc_gc_vacuum(uint32_t table_oid, const mvcc_gc_config_t *config);

/**
 * 增量 GC - 每次处理少量页面
 * @param table_oid 表 ID
 * @param config GC 配置
 * @param pages_to_process 这次处理的页面数
 * @return 处理的页面数，0 表示完成，负数表示失败
 */
int mvcc_gc_incremental(uint32_t table_oid,
                        const mvcc_gc_config_t *config,
                        int pages_to_process);

/**
 * 清理 Undo 日志
 * @param oldest_active_xmin 最早的活跃事务 xmin
 * @return 清理的 Undo 记录数
 */
int mvcc_gc_cleanup_undo(mvcc_txn_id_t oldest_active_xmin);

#ifdef __cplusplus
}
#endif

#endif /* DB_CONCURRENCY_MVCC_H */