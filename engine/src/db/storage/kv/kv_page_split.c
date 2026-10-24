#include "db/kv_page.h"
#include "db/core/log.h"

kv_split_result_t kv_page_split(void *pool, page_id_t full_page_id) {
    (void)pool;
    (void)full_page_id;
    /* C1-3 T4：分裂骨架。完整分裂逻辑（按 key 中位切分 + 父节点上提）
     * 留待后续变更展开。当前返回 NEED_BUT_FAIL 标识已识别此问题。
     */
    LOG_WARN("kv_page_split: 完整分裂逻辑待实现，仅为占位（page=%u）",
             (unsigned)full_page_id);
    return KV_SPLIT_NEED_BUT_FAIL;
}