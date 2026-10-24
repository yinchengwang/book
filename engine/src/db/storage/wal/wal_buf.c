/**
 * @file wal_buf.c
 * @brief WAL 与 Buffer Pool 协调层实现
 */

#include "db/wal_buf.h"
#include "db/wal.h"
#include "db/buf.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* 默认脏页列表容量 */
#define WAL_BUF_DIRTY_INITIAL 64

/* ============================================================
 * 创建与销毁
 * ============================================================ */

wal_buf_t *wal_buf_create(wal_t *wal, void *buffer_pool) {
    if (!wal) {
        return NULL;
    }

    wal_buf_t *wb = (wal_buf_t *)calloc(1, sizeof(wal_buf_t));
    if (!wb) {
        return NULL;
    }

    wb->wal = wal;
    wb->buffer_pool = buffer_pool;
    wb->dirty_count = 0;
    wb->dirty_capacity = WAL_BUF_DIRTY_INITIAL;
    wb->dirty_pages = (uint32_t *)calloc(wb->dirty_capacity, sizeof(uint32_t));
    wb->last_flush_lsn = 0;
    wb->oldest_dirty_lsn = UINT64_MAX;
    wb->log_waits = 0;
    wb->sync_waits = 0;

    return wb;
}

void wal_buf_destroy(wal_buf_t *wb) {
    if (!wb) {
        return;
    }

    /* 先执行检查点刷所有脏页 */
    wal_buf_checkpoint(wb);

    /* 释放脏页列表 */
    if (wb->dirty_pages) {
        free(wb->dirty_pages);
    }

    free(wb);
}

/* ============================================================
 * 脏页管理
 * ============================================================ */

void wal_buf_mark_dirty(wal_buf_t *wb, BufferDesc *buf, uint32_t txn_id) {
    if (!wb || !buf) {
        return;
    }

    /* 标记 Buffer 为脏 */
    buf_dirty(buf);

    /* 写入 WAL 日志 */
    uint64_t lsn = wal_buf_log_dirty(wb, buf, txn_id);
    if (lsn > 0) {
        /* 更新 Buffer 的 LSN */
        buf->last_written = lsn;

        /* 检查是否需要扩展脏页列表 */
        if (wb->dirty_count >= wb->dirty_capacity) {
            uint32_t new_cap = wb->dirty_capacity * 2;
            uint32_t *new_pages = (uint32_t *)realloc(wb->dirty_pages,
                                                      new_cap * sizeof(uint32_t));
            if (new_pages) {
                wb->dirty_pages = new_pages;
                wb->dirty_capacity = new_cap;
            }
        }

        /* 添加到脏页列表（去重） */
        bool found = false;
        for (uint32_t i = 0; i < wb->dirty_count; i++) {
            if (wb->dirty_pages[i] == buf->buf_id) {
                found = true;
                break;
            }
        }

        if (!found) {
            wb->dirty_pages[wb->dirty_count++] = buf->buf_id;
        }

        /* 更新最旧脏 LSN */
        if (lsn < wb->oldest_dirty_lsn) {
            wb->oldest_dirty_lsn = lsn;
        }
    }
}

uint64_t wal_buf_log_dirty(wal_buf_t *wb, BufferDesc *buf, uint32_t txn_id) {
    if (!wb || !buf) {
        return 0;
    }

    /* 获取页面数据 */
    void *data = buf_get_data(buf);
    if (!data) {
        return 0;
    }

    /* 写入 UPDATE 日志 */
    uint64_t lsn = wal_write_update(
        wb->wal, txn_id,
        &buf->relfilenode, sizeof(buf->relfilenode),
        &buf->blocknum, sizeof(buf->blocknum),
        data, BUF_PAGE_SIZE
    );

    return lsn;
}

bool wal_buf_needs_flush(wal_buf_t *wb, BufferDesc *buf) {
    if (!wb || !buf) {
        return false;
    }

    /* 如果 Buffer 的 LSN 小于上次刷盘 LSN，说明已经刷过了 */
    return buf->last_written > wb->last_flush_lsn;
}

/* ============================================================
 * 事务提交
 * ============================================================ */

int wal_buf_commit(wal_buf_t *wb, uint32_t txn_id) {
    if (!wb) {
        return -1;
    }

    /* 写入提交日志 */
    uint64_t commit_lsn = wal_write_commit(wb->wal, txn_id);
    if (commit_lsn == 0) {
        return -1;
    }

    /* 确保日志刷到磁盘 */
    if (wal_flush(wb->wal) != 0) {
        return -1;
    }

    wb->log_waits++;

    return 0;
}

/* ============================================================
 * 检查点
 * ============================================================ */

/* 获取 BufferDesc 的辅助宏（需要外部提供 buffer_pool 指针） */
static BufferDesc *get_buffer_by_id(void *pool, uint32_t buf_id) {
    if (!pool || buf_id >= buf_get_nbuffers()) {
        return NULL;
    }
    /* 简化：假设 buffer_pool 结构体的第一个字段是 descriptors */
    BufferDesc *descs = *(BufferDesc **)pool;
    return descs ? &descs[buf_id] : NULL;
}

int wal_buf_checkpoint(wal_buf_t *wb) {
    if (!wb) {
        return -1;
    }

    if (wb->dirty_count == 0) {
        /* 没有脏页，只写检查点日志 */
        uint64_t lsn = wal_write_checkpoint(wb->wal, NULL, 0);
        if (lsn == 0) {
            return -1;
        }
        wal_flush(wb->wal);
        wb->last_flush_lsn = lsn;
        return 0;
    }

    /* 刷所有脏页 */
    for (uint32_t i = 0; i < wb->dirty_count; i++) {
        uint32_t buf_id = wb->dirty_pages[i];
        BufferDesc *buf = get_buffer_by_id(wb->buffer_pool, buf_id);
        if (buf && (buf->state & BM_DIRTY)) {
            buf_write(buf);
            buf_clean(buf);
        }
    }

    /* 写检查点日志 */
    uint64_t lsn = wal_write_checkpoint(wb->wal, wb->dirty_pages, wb->dirty_count);
    if (lsn == 0) {
        return -1;
    }

    /* 刷 WAL */
    if (wal_flush(wb->wal) != 0) {
        return -1;
    }

    /* 重置脏页列表 */
    wb->dirty_count = 0;
    wb->last_flush_lsn = lsn;
    wb->oldest_dirty_lsn = UINT64_MAX;

    return 0;
}

uint64_t wal_buf_get_recovery_lsn(wal_buf_t *wb) {
    if (!wb) {
        return 0;
    }

    /* 返回上次检查点 LSN */
    return wb->last_flush_lsn;
}

int wal_buf_wait_lsn(wal_buf_t *wb, uint64_t lsn) {
    if (!wb) {
        return -1;
    }

    /* 如果目标 LSN 已经刷过，直接返回 */
    if (lsn <= wb->last_flush_lsn) {
        return 0;
    }

    /* 刷 WAL 到目标 LSN */
    if (wal_flush(wb->wal) != 0) {
        return -1;
    }

    wb->sync_waits++;

    return 0;
}

/* ============================================================
 * 脏页查询
 * ============================================================ */

uint32_t wal_buf_dirty_count(wal_buf_t *wb) {
    return wb ? wb->dirty_count : 0;
}

uint32_t wal_buf_get_dirty_pages(wal_buf_t *wb, uint32_t *pages, uint32_t max_count) {
    if (!wb || !pages) {
        return 0;
    }

    uint32_t count = (wb->dirty_count < max_count) ? wb->dirty_count : max_count;
    memcpy(pages, wb->dirty_pages, count * sizeof(uint32_t));
    return count;
}

/* ============================================================
 * 恢复
 * ============================================================ */

int wal_buf_recovery_init(wal_buf_t *wb) {
    if (!wb) {
        return -1;
    }

    /* 分析 WAL 文件获取恢复信息 */
    wal_recovery_info_t info;
    if (wal_analyze(NULL, &info) != 0) {
        /* 没有 WAL 文件，从头开始 */
        wb->last_flush_lsn = 0;
        return 0;
    }

    wb->last_flush_lsn = info.last_checkpoint_lsn;

    /* 释放分析结果 */
    wal_recovery_info_free(&info);

    return 0;
}

/* 应用日志记录的回调 */
static int apply_log_record(void *ctx, wal_log_type_t type,
                            const void *key, size_t key_len,
                            const void *value, size_t value_len) {
    (void)ctx;
    (void)type;
    (void)key;
    (void)key_len;
    (void)value;
    (void)value_len;

    /* 简化实现：将数据写回 Buffer */
    /* 实际实现需要根据 relfilenode/blocknum 定位 Buffer */

    return 0;
}

int wal_buf_do_redo(wal_buf_t *wb, uint64_t start_lsn) {
    if (!wb) {
        return -1;
    }

    /* 执行 redo */
    return wal_redo(NULL, start_lsn, apply_log_record, wb);
}

int wal_buf_do_undo(wal_buf_t *wb, uint32_t txn_id) {
    if (!wb) {
        return -1;
    }

    /* 执行 undo */
    return wal_undo(NULL, txn_id, 0, apply_log_record, wb);
}

/* ============================================================
 * 统计
 * ============================================================ */

void wal_buf_get_stats(wal_buf_t *wb, wal_buf_stats_t *stats) {
    if (!wb || !stats) {
        return;
    }

    stats->log_waits = wb->log_waits;
    stats->sync_waits = wb->sync_waits;
    stats->dirty_count = wb->dirty_count;
    stats->last_flush_lsn = wb->last_flush_lsn;
    stats->oldest_dirty_lsn = wb->oldest_dirty_lsn;
}
