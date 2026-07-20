/**
 * @file btree_split.c
 * @brief BTree 页面分裂实现
 *
 * 当页面满时，将页面分裂成两个页面。
 * 参考 PostgreSQL 的 _bt_split 实现。
 */

#include "db/access/btree/btree_split.h"
#include "db/access/btree/btpage.h"
#include "db/buf.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

/* ============================================================
 * 辅助函数
 * ============================================================ */

/**
 * @brief 找到叶节点的中间键索引
 * @param page 页面数据指针
 * @param pivot_idx 输出：中间键索引
 * @return 0 成功，-1 失败
 */
int btree_split_find_pivot(void *page, int *pivot_idx) {
    if (!page || !pivot_idx) {
        return -1;
    }

    BTPageHeader header = (BTPageHeader)page;
    int total = header->btpo_count;

    /* 取中间位置作为分裂点 */
    *pivot_idx = total / 2;

    return 0;
}

/**
 * @brief 叶节点分裂
 * @param rel 索引 Relation
 * @param old_blkno 待分裂页面的块号
 * @param new_blkno 输出：新页面的块号
 * @return 0 成功，-1 失败
 */
int btree_split_leaf(Relation rel, uint32_t old_blkno, uint32_t *new_blkno) {
    if (!rel || !new_blkno) {
        return -1;
    }

    /* 1. 读取旧页面 */
    BufferDesc *old_buf = buf_read(rel->rd_relfilenode, old_blkno, 1);
    if (!old_buf) {
        return -1;
    }

    void *old_page = buf_get_data(old_buf);
    if (!old_page) {
        buf_unpin(old_buf);
        return -1;
    }

    BTPageHeader old_header = (BTPageHeader)old_page;

    /* 检查是否是叶子页面
     * 注意：bt_page_init 使用 BTP_LEAF_FLAG（等于 BTP_LEAF = 0x01）
     * 和 BTP_ROOT_FLAG（等于 BTP_ROOT = 0x02）初始化的页面，
     * btpo_flags = BTP_LEAF_FLAG | BTP_ROOT_FLAG = 0x03。
     * BT_PAGE_IS_LEAF 检查 (btpo_flags & BTP_LEAF) != 0，
     * 因此 0x03 & 0x01 = 0x01 ≠ 0，通过。 */
    if (!BT_PAGE_IS_LEAF(old_header)) {
        buf_unpin(old_buf);
        return -1;
    }

    /* 2. 找到分裂点 */
    int pivot_idx;
    if (btree_split_find_pivot(old_page, &pivot_idx) != 0) {
        buf_unpin(old_buf);
        return -1;
    }

    /* 3. 分配新页面 */
    BufferDesc *new_buf = buf_new(rel->rd_relfilenode);
    if (!new_buf) {
        new_buf = buf_new_page(rel->rd_relfilenode);
    }
    if (!new_buf) {
        buf_unpin(old_buf);
        return -1;
    }

    /* 手动设置新页面的块号（与 heapam.c 模式一致） */
    *new_blkno = buf_get_id(new_buf) + 1;
    buf_hash_insert(rel->rd_relfilenode, *new_blkno, (uint32_t)buf_get_id(new_buf));

    void *new_page = buf_get_data(new_buf);
    if (!new_page) {
        buf_unpin(old_buf);
        buf_unpin(new_buf);
        return -1;
    }

    /* 4. 初始化新页面 */
    memset(new_page, 0, BTREE_PAGE_SIZE);
    BTPageHeader new_header = (BTPageHeader)new_page;

    /* 复制页面属性 */
    new_header->btpo_flags = old_header->btpo_flags;  /* 继承叶子标志 */
    new_header->btpo_level = old_header->btpo_level;
    new_header->btpo_cycleid = old_header->btpo_cycleid;
    new_header->btpo_xact = old_header->btpo_xact;
    new_header->btpo_offset = old_header->btpo_offset;

    /* 5. 更新兄弟链接 */
    /* 新页的下一个指向旧页的下一个 */
    new_header->btpo_next = old_header->btpo_next;
    new_header->btpo_rightlink = old_header->btpo_rightlink;

    /* 旧页的下一个指向新页 */
    old_header->btpo_next = *new_blkno;
    old_header->btpo_rightlink = *new_blkno;

    /* 新页的上一个指向旧页 */
    new_header->btpo_prev = old_blkno;

    /* 6. 计算并更新条目计数 */
    int total_count = old_header->btpo_count;
    int right_count = total_count - pivot_idx;

    /* 旧页保留左半部分 */
    old_header->btpo_count = pivot_idx;

    /* 新页获得右半部分 */
    new_header->btpo_count = right_count;

    /*
     * 注意：当前简化实现中，条目数据仅通过 btpo_count 计数，
     * 没有实际移动键值对。完整的实现需要：
     * 1. 移动 pivot_idx 之后的 BTInternalTupleData 条目到新页
     * 2. 更新 btpo_offset 指针
     * 3. 如果有父节点，将中间键插入父节点
     * 4. 如果是根节点，创建新根
     */

    /* 7. 标记脏页 */
    buf_dirty(old_buf);
    buf_dirty(new_buf);

    /* 8. 释放缓冲区 */
    buf_unpin(old_buf);
    buf_unpin(new_buf);

    return 0;
}

/**
 * @brief 根节点分裂（创建新根）
 * @param rel 索引 Relation
 * @param old_blkno 原根页面的块号
 * @param new_blkno 分裂产生的新页面的块号
 * @return 0 成功，-1 失败
 *
 * 当根页面分裂时，创建新的根页面，将原根和 new_blkno 作为新根的两个子节点。
 */
int btree_split_root(Relation rel, uint32_t old_blkno, uint32_t new_blkno) {
    if (!rel) {
        return -1;
    }

    /* 1. 分配新根页面 */
    BufferDesc *new_root_buf = buf_new(rel->rd_relfilenode);
    if (!new_root_buf) {
        new_root_buf = buf_new_page(rel->rd_relfilenode);
    }
    if (!new_root_buf) {
        return -1;
    }

    uint32_t new_root_blkno = buf_get_id(new_root_buf) + 1;
    buf_hash_insert(rel->rd_relfilenode, new_root_blkno,
                    (uint32_t)buf_get_id(new_root_buf));

    void *new_root_page = buf_get_data(new_root_buf);
    if (!new_root_page) {
        buf_unpin(new_root_buf);
        return -1;
    }

    /* 2. 初始化新根页面（level 比子节点高 1） */
    memset(new_root_page, 0, BTREE_PAGE_SIZE);
    BTPageHeader new_root_header = (BTPageHeader)new_root_page;
    new_root_header->btpo_flags = BTP_ROOT | BTP_INTERNAL;
    new_root_header->btpo_level = 1;
    new_root_header->btpo_count = 2;  /* 两个子节点 */
    new_root_header->btpo_offset = BTREE_PAGE_SIZE;

    /* 3. 将原根降级为内部节点 */
    BufferDesc *old_root_buf = buf_read(rel->rd_relfilenode, old_blkno, 1);
    if (old_root_buf) {
        void *old_root_page = buf_get_data(old_root_buf);
        if (old_root_page) {
            BTPageHeader old_root_header = (BTPageHeader)old_root_page;
            old_root_header->btpo_flags &= ~BTP_ROOT;
            if (!(old_root_header->btpo_flags & BTP_LEAF)) {
                old_root_header->btpo_flags |= BTP_INTERNAL;
            }
            buf_dirty(old_root_buf);
        }
        buf_unpin(old_root_buf);
    }

    /* 4. 在新根中存储两个子节点块号（简化实现） */
    uint32_t *child_entries = (uint32_t *)((char *)new_root_page + BTREE_PAGE_HEADER_SIZE);
    child_entries[0] = old_blkno;
    child_entries[1] = new_blkno;

    buf_dirty(new_root_buf);
    buf_unpin(new_root_buf);

    return 0;
}