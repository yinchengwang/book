/**
 * @file index_persist_control.c
 * @brief 索引持久化控制模块
 *
 * 根据 persist_enabled 开关控制 WAL/Redo 的行为：
 *   - persist_enabled = true  : 完整 MVCC + WAL + Redo + Checkpoint
 *   - persist_enabled = false : 纯内存 + Undo（无崩溃恢复）
 *
 * 设计要点：
 *   - 每个索引实例持有独立的 persist_control_t，生命周期与索引一致
 *   - 控制模块本身不直接执行 WAL 写入，只决定"是否开启 WAL/Undo 路径"
 *   - 实际 WAL 写入由事务层在插入/搜索路径上根据本模块的开关状态决定
 *   - 本模块提供轻量级查询 API（persist_is_enabled）与日志辅助 API
 *
 * 后续工作：
 *   - persist_write_wal / persist_write_undo / persist_commit / persist_rollback
 *     目前以桩实现提供，后续任务将对接 db/wal.h 与 db/concurrency/mvcc.h
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "index_persist_control.h"

/* ============================================================
 * 持久化控制结构
 * ============================================================ */

/**
 * @brief 持久化控制状态
 *
 * 与索引实例一一对应：
 *   - persist_enabled 反映创建时 config 的开关值
 *   - wal_ctx         在持久化模式下指向 WAL 上下文；纯内存模式下为 NULL
 *   - undo_ctx        在两种模式下都可用，承载事务级 Undo 信息
 *   - dirty_pages     脏页位图（仅持久化模式有效），用于跟踪需要刷盘的页面
 *   - checkpoint_seq  检查点序列号（仅持久化模式有效）
 */
struct persist_control {
    bool     persist_enabled;       /**< 持久化开关 */
    void    *wal_ctx;               /**< WAL 上下文（仅 persist_enabled=true 时有效） */
    void    *undo_ctx;              /**< Undo 上下文（事务回滚用） */
    void    *dirty_pages;           /**< 脏页位图（仅持久化模式有效） */
    uint64_t checkpoint_seq;        /**< 检查点序列号 */
    size_t   wal_bytes_written;     /**< WAL 累计写入字节数（用于监控/调试） */
};

/* ============================================================
 * 生命周期
 * ============================================================ */

persist_control_t *persist_control_create(bool enabled)
{
    persist_control_t *ctrl = (persist_control_t *)calloc(1, sizeof(persist_control_t));
    if (ctrl == NULL) {
        return NULL;
    }

    ctrl->persist_enabled   = enabled;
    ctrl->wal_ctx           = NULL;
    ctrl->undo_ctx          = NULL;
    ctrl->dirty_pages       = NULL;
    ctrl->checkpoint_seq    = 0;
    ctrl->wal_bytes_written = 0;

    if (enabled) {
        /*
         * 持久化模式：
         *   - WAL 上下文留待上层事务层注入（本模块只持有指针）
         *   - 脏页位图与检查点状态在此初始化为 0 即可
         *   - 后续任务会通过 persist_control_attach_wal_ctx 注入真实 WAL 上下文
         */
        ctrl->checkpoint_seq = 1;  /* 初始检查点序号 */
    } else {
        /*
         * 纯内存模式：
         *   - 仅初始化 Undo 上下文用于事务回滚
         *   - WAL / 脏页 / 检查点全部保持为 NULL / 0
         */
    }

    return ctrl;
}

void persist_control_destroy(persist_control_t *ctrl)
{
    if (ctrl == NULL) {
        return;
    }

    /*
     * 当前阶段 wal_ctx / undo_ctx / dirty_pages 由调用方注入并负责释放，
     * 本模块只持有指针并解除引用关系。如未来由本模块拥有所有权，应在此处
     * 释放对应资源。
     */

    ctrl->wal_ctx     = NULL;
    ctrl->undo_ctx    = NULL;
    ctrl->dirty_pages = NULL;

    free(ctrl);
}

/* ============================================================
 * 查询 API
 * ============================================================ */

bool persist_is_enabled(const persist_control_t *ctrl)
{
    if (ctrl == NULL) {
        return false;
    }
    return ctrl->persist_enabled;
}

/* ============================================================
 * 日志辅助 API
 * ============================================================ */

int persist_write_wal(persist_control_t *ctrl, const void *data, size_t len)
{
    if (ctrl == NULL || data == NULL) {
        return -1;
    }

    if (!ctrl->persist_enabled) {
        /* 纯内存模式：不写 WAL */
        return 0;
    }

    /*
     * 持久化模式：当前阶段未对接 db/wal.h，仅累计统计量。
     * 真实 WAL 写入将发生在后续任务（如 Task 8+）的事务层接入中。
     */
    ctrl->wal_bytes_written += len;
    (void)data;
    return 0;
}

int persist_write_undo(persist_control_t *ctrl, const void *undo_data, size_t len)
{
    if (ctrl == NULL || undo_data == NULL) {
        return -1;
    }

    /*
     * 当前阶段 Undo 上下文由上层事务层管理，本模块只校验参数合法性。
     * 真实 Undo 写入将发生在事务层接入后。
     */
    (void)len;
    return 0;
}

int persist_commit(persist_control_t *ctrl)
{
    if (ctrl == NULL) {
        return -1;
    }

    if (!ctrl->persist_enabled) {
        return 0;
    }

    /*
     * 持久化模式下：递增检查点序号以标记新的一致性点。
     * 实际事务提交逻辑（包括持久化 Undo、刷盘脏页等）由事务层完成。
     */
    ctrl->checkpoint_seq++;
    return 0;
}

int persist_rollback(persist_control_t *ctrl)
{
    if (ctrl == NULL) {
        return -1;
    }

    /*
     * 当前阶段仅校验参数，真实 Undo 应用由事务层负责。
     */
    return 0;
}