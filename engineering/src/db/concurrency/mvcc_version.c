/*
 * mvcc_version.c - MVCC 版本链实现
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <db/concurrency/mvcc.h>

/* ─────────────────────────────────────────────────────────────────
 * 版本链操作
 * ───────────────────────────────────────────────────────────────── */

mvcc_version_t *mvcc_version_new(mvcc_txn_id_t xmin,
                                  const void *data,
                                  size_t data_size)
{
    mvcc_version_t *version = (mvcc_version_t *)malloc(sizeof(mvcc_version_t));
    if (version == NULL) {
        return NULL;
    }

    version->xmin = xmin;
    version->xmax = MVCC_INVALID_TXN_ID;
    version->ctid.block_num = 0;
    version->ctid.offset = 0;
    version->xvac.block_num = 0;
    version->xvac.offset = 0;
    version->next = NULL;

    if (data != NULL && data_size > 0) {
        version->data = malloc(data_size);
        if (version->data == NULL) {
            free(version);
            return NULL;
        }
        memcpy(version->data, data, data_size);
        version->data_size = data_size;
    } else {
        version->data = NULL;
        version->data_size = 0;
    }

    return version;
}

void mvcc_version_destroy(mvcc_version_t *version)
{
    if (version == NULL) {
        return;
    }

    if (version->data != NULL) {
        free(version->data);
        version->data = NULL;
    }

    free(version);
}

void mvcc_version_insert(mvcc_version_t **head, mvcc_version_t *new_version)
{
    if (head == NULL || new_version == NULL) {
        return;
    }

    /* 新版本插入到链表头部 */
    new_version->next = *head;
    *head = new_version;
}

void mvcc_version_mark_deleted(mvcc_version_t *version, mvcc_txn_id_t xmax)
{
    if (version == NULL) {
        return;
    }
    version->xmax = xmax;
}

int mvcc_version_chain_length(const mvcc_version_t *head)
{
    int count = 0;
    const mvcc_version_t *cur = head;
    while (cur != NULL) {
        count++;
        cur = cur->next;
    }
    return count;
}

/* ─────────────────────────────────────────────────────────────────
 * Undo 日志操作
 * ───────────────────────────────────────────────────────────────── */

mvcc_undo_record_t *mvcc_undo_create(mvcc_txn_id_t txn_id,
                                      mvcc_undo_type_t type,
                                      const void *old_data,
                                      size_t old_data_size,
                                      const mvcc_ctid_t *row_ptr)
{
    mvcc_undo_record_t *record = (mvcc_undo_record_t *)malloc(
        sizeof(mvcc_undo_record_t));
    if (record == NULL) {
        return NULL;
    }

    record->txn_id = txn_id;
    record->type = type;
    record->prev_undo.block_num = 0;
    record->prev_undo.offset = 0;

    if (row_ptr != NULL) {
        record->row_ptr = *row_ptr;
    } else {
        record->row_ptr.block_num = 0;
        record->row_ptr.offset = 0;
    }

    if (old_data != NULL && old_data_size > 0) {
        record->old_data = malloc(old_data_size);
        if (record->old_data == NULL) {
            free(record);
            return NULL;
        }
        memcpy(record->old_data, old_data, old_data_size);
        record->old_data_size = old_data_size;
    } else {
        record->old_data = NULL;
        record->old_data_size = 0;
    }

    return record;
}

void mvcc_undo_destroy(mvcc_undo_record_t *record)
{
    if (record == NULL) {
        return;
    }
    if (record->old_data != NULL) {
        free(record->old_data);
        record->old_data = NULL;
    }
    free(record);
}

int mvcc_undo_apply(mvcc_undo_record_t *record)
{
    if (record == NULL) {
        return -1;
    }

    /* Undo 恢复逻辑：
     * - INSERT: 删除插入的行（通过 row_ptr 定位）
     * - UPDATE: 用 old_data 覆盖当前行
     * - DELETE: 恢复已删除的行
     *
     * 这里只是占位实现，实际需要与存储层集成
     */
    switch (record->type) {
        case MVCC_UNDO_INSERT:
            /* 删除插入的行 */
            /* TODO: 调用存储层删除行 */
            break;
        case MVCC_UNDO_UPDATE:
            /* 用旧值覆盖 */
            /* TODO: 调用存储层更新行 */
            break;
        case MVCC_UNDO_DELETE:
            /* 恢复已删除行 */
            /* TODO: 调用存储层插入行 */
            break;
        default:
            return -1;
    }

    return 0;
}