/**
 * @file kv_page.h
 * @brief KV 页分裂接口（C1-3 T4）
 *
 * 提供页满时半满分裂 + 父节点上提骨架。当前实现为最小版本：
 * kv_page_split 总是返回成功（实际分裂逻辑待后续细化）。
 * 完整实现复用 index/btree 的 split 算法可作为后续工作。
 */
#ifndef DB_KV_PAGE_SPLIT_H
#define DB_KV_PAGE_SPLIT_H

#include "db/page.h"
#include "db/buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum kv_split_result_e {
    KV_SPLIT_OK = 0,           /* 分裂成功 */
    KV_SPLIT_NO_NEED = 1,      /* 无需分裂（页面未满） */
    KV_SPLIT_NEED_BUT_FAIL = 2 /* 满了但分裂失败 */
} kv_split_result_t;

/**
 * @brief 半满分裂 + 父节点上提骨架
 * @param pool 缓冲池
 * @param full_page_id 已满页 id
 * @return KV_SPLIT_OK / KV_SPLIT_NO_NEED / KV_SPLIT_NEED_BUT_FAIL
 *
 * 当前实现：未满返回 KV_SPLIT_NO_NEED；满了返回 KV_SPLIT_NEED_BUT_FAIL
 * 并 LOG_WARN——完整分裂逻辑（按 key 中位分裂、右半迁移、上提）
 * 在后续变更展开。
 */
kv_split_result_t kv_page_split(void *pool, page_id_t full_page_id);

#ifdef __cplusplus
}
#endif

#endif /* DB_KV_PAGE_SPLIT_H */