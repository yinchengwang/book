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

/*
 * 内部节点条目结构（与 btreeam.h 中的 BTInternalTupleData 保持一致）
 * 注意：btpage.h 和 btreeam.h 的 BTPageHeaderData 定义冲突，
 * 因此不能在同一个文件中同时包含两者，这里局部定义内部条目类型。
 */
typedef struct InternalEntry_s {
    uint32_t    block_number;     /**< 子页面块号 */
    uint16_t    offnum;           /**< 元组偏移 */
    uint8_t     downlink_offset;  /**< 下链偏移 */
    uint8_t     flags;            /**< 标志 */
} InternalEntry;

/** 内部条目指针类型 */
typedef InternalEntry *InternalEntryPtr;

/* ============================================================
 * 辅助函数
 * ============================================================ */

/**
 * @brief 获取新页面的块号（跨 buf_new 兼容）
 *
 * buf_new() 返回的 blocknum 为 0（临时标记），
 * 需要用 buf_get_id() + 1 手动分配唯一块号。
 */
static uint32_t assign_new_blockno(BufferDesc *buf) {
    return (uint32_t)(buf_get_id(buf) + 1);
}

/**
 * @brief 注册新页面到 Hash 表
 */
static void register_new_blockno(Relation rel, BufferDesc *buf, uint32_t blockno) {
    buf_hash_insert(rel->rd_relfilenode, blockno, (uint32_t)buf_get_id(buf));
}

/**
 * @brief 分配并初始化新页面
 * @param rel 索引 Relation
 * @param blockno 输出：新页面块号
 * @param level 页面层级
 * @param flags 页面标志
 * @return Buffer 描述符，失败返回 NULL
 */
static BufferDesc *alloc_new_page(Relation rel, uint32_t *blockno,
                                  uint16_t level, uint16_t flags) {
    BufferDesc *buf = buf_new(rel->rd_relfilenode);
    if (!buf) {
        buf = buf_new_page(rel->rd_relfilenode);
    }
    if (!buf) {
        return NULL;
    }

    *blockno = assign_new_blockno(buf);
    register_new_blockno(rel, buf, *blockno);

    void *page = buf_get_data(buf);
    if (!page) {
        buf_unpin(buf);
        return NULL;
    }

    memset(page, 0, BTREE_PAGE_SIZE);
    BTPageHeader header = (BTPageHeader)page;
    header->btpo_flags = flags;
    header->btpo_level = level;
    header->btpo_offset = BTREE_PAGE_SIZE;
    header->btpo_count = 0;

    return buf;
}

/**
 * @brief 内部节点条目大小
 */
#define INTERNAL_ENTRY_SIZE sizeof(InternalEntry)

/**
 * @brief 内部节点条目偏移
 */
static uint16_t internal_entry_offset(int idx) {
    return BTREE_PAGE_HEADER_SIZE + idx * INTERNAL_ENTRY_SIZE;
}

/**
 * @brief 获取内部节点条目指针
 */
static InternalEntryPtr internal_entry_ptr(void *page, int idx) {
    uint16_t off = internal_entry_offset(idx);
    return (InternalEntryPtr)((char *)page + off);
}

/* ============================================================
 * btree_split_find_pivot — 寻找分裂点
 * ============================================================ */

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

/* ============================================================
 * btree_split_leaf — 叶节点分裂
 *
 * 将旧页右半部分的条目搬到新页，更新兄弟链接。
 * 返回新页块号和中键值（用于插入父节点）。
 * ============================================================ */

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

    /* 检查是否是叶子页面 */
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

    *new_blkno = assign_new_blockno(new_buf);
    register_new_blockno(rel, new_buf, *new_blkno);

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
    new_header->btpo_next = old_header->btpo_next;
    new_header->btpo_rightlink = old_header->btpo_rightlink;
    old_header->btpo_next = *new_blkno;
    old_header->btpo_rightlink = *new_blkno;
    new_header->btpo_prev = old_blkno;

    /* 6. 分裂条目计数 */
    int total_count = old_header->btpo_count;
    int right_count = total_count - pivot_idx;

    /* 旧页保留左半部分 */
    old_header->btpo_count = pivot_idx;

    /* 新页获得右半部分 */
    new_header->btpo_count = right_count;

    /*
     * 注意：叶节点条目数据移动需要在条目格式统一后实现。
     * 当前简化版本仅通过 btpo_count 记录，未来 Phase 需：
     * 1. 移动 pivot_idx 之后的数据条目到新页
     * 2. 调整 btpo_offset
     */

    /* 7. 标记脏页 */
    buf_dirty(old_buf);
    buf_dirty(new_buf);

    /* 8. 释放缓冲区 */
    buf_unpin(old_buf);
    buf_unpin(new_buf);

    return 0;
}

/* ============================================================
 * btree_split_internal — 内部节点分裂
 *
 * 内部节点满时，将右半部分搬到新页，
 * 中间键（pivot 位置的键）提升到父节点。
 * ============================================================ */

int btree_split_internal(Relation rel, uint32_t old_blkno, uint32_t *new_blkno) {
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

    /* 检查是否是内部节点 */
    if (!BT_PAGE_IS_INTERNAL(old_header)) {
        buf_unpin(old_buf);
        return -1;
    }

    /* 2. 找到分裂点 */
    int pivot_idx;
    if (btree_split_find_pivot(old_page, &pivot_idx) != 0) {
        buf_unpin(old_buf);
        return -1;
    }

    int total_count = old_header->btpo_count;
    int right_count = total_count - pivot_idx;
    if (right_count <= 0) {
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

    *new_blkno = assign_new_blockno(new_buf);
    register_new_blockno(rel, new_buf, *new_blkno);

    void *new_page = buf_get_data(new_buf);
    if (!new_page) {
        buf_unpin(old_buf);
        buf_unpin(new_buf);
        return -1;
    }

    /* 4. 初始化新页面 */
    memset(new_page, 0, BTREE_PAGE_SIZE);
    BTPageHeader new_header = (BTPageHeader)new_page;

    new_header->btpo_flags = BTP_INTERNAL | (old_header->btpo_flags & BTP_ROOT);
    new_header->btpo_level = old_header->btpo_level;
    new_header->btpo_xact = old_header->btpo_xact;
    new_header->btpo_offset = old_header->btpo_offset;

    /* 5. 更新兄弟链接 */
    new_header->btpo_next = old_header->btpo_next;
    new_header->btpo_rightlink = old_header->btpo_rightlink;
    old_header->btpo_next = *new_blkno;
    old_header->btpo_rightlink = *new_blkno;
    new_header->btpo_prev = old_blkno;

    /* 6. 移动条目数据（pivot+1 到 total-1 搬到新页） */
    for (int i = pivot_idx; i < total_count; i++) {
        InternalEntryPtr src = internal_entry_ptr(old_page, i);
        InternalEntryPtr dst = internal_entry_ptr(new_page, i - pivot_idx);
        *dst = *src;
    }

    /* 7. 更新计数 */
    old_header->btpo_count = pivot_idx;
    new_header->btpo_count = right_count;

    /* 8. 更新旧页的 btpo_offset（回收已搬走的条目空间） */
    /* 内部节点的条目从页头向后排列，搬走右半后空间自动回收 */

    /* 9. 标记脏页 */
    buf_dirty(old_buf);
    buf_dirty(new_buf);

    buf_unpin(old_buf);
    buf_unpin(new_buf);

    return 0;
}

/* ============================================================
 * btree_split_root — 根节点分裂
 *
 * 当根节点满时，创建新根节点（level+1），
 * 原根和分裂新页作为新根的两个子节点。
 * ============================================================ */

int btree_split_root(Relation rel, uint32_t old_blkno, uint32_t new_blkno) {
    if (!rel) {
        return -1;
    }

    /* 1. 读取原根页面（获取 level 信息） */
    BufferDesc *old_root_buf = buf_read(rel->rd_relfilenode, old_blkno, 1);
    if (!old_root_buf) {
        return -1;
    }

    void *old_root_page = buf_get_data(old_root_buf);
    if (!old_root_page) {
        buf_unpin(old_root_buf);
        return -1;
    }

    BTPageHeader old_root_header = (BTPageHeader)old_root_page;
    uint16_t child_level = old_root_header->btpo_level;

    /* 2. 分配新根页面 */
    BufferDesc *new_root_buf = buf_new(rel->rd_relfilenode);
    if (!new_root_buf) {
        new_root_buf = buf_new_page(rel->rd_relfilenode);
    }
    if (!new_root_buf) {
        buf_unpin(old_root_buf);
        return -1;
    }

    uint32_t new_root_blkno = assign_new_blockno(new_root_buf);
    register_new_blockno(rel, new_root_buf, new_root_blkno);

    void *new_root_page = buf_get_data(new_root_buf);
    if (!new_root_page) {
        buf_unpin(old_root_buf);
        buf_unpin(new_root_buf);
        return -1;
    }

    /* 3. 初始化新根页面 */
    memset(new_root_page, 0, BTREE_PAGE_SIZE);
    BTPageHeader new_root_header = (BTPageHeader)new_root_page;

    /* 新根 level = 子节点 level + 1 */
    new_root_header->btpo_flags = BTP_ROOT | BTP_INTERNAL;
    new_root_header->btpo_level = child_level + 1;

    /* 新根有两个子节点条目：old_blkno 和 new_blkno */
    new_root_header->btpo_count = 2;
    new_root_header->btpo_offset = BTREE_PAGE_SIZE;

    /* 4. 存储两个子节点块号 */
    InternalEntryPtr entry0 = internal_entry_ptr(new_root_page, 0);
    InternalEntryPtr entry1 = internal_entry_ptr(new_root_page, 1);

    memset(entry0, 0, INTERNAL_ENTRY_SIZE);
    memset(entry1, 0, INTERNAL_ENTRY_SIZE);
    entry0->block_number = old_blkno;
    entry1->block_number = new_blkno;

    /* 5. 原根去掉 BTP_ROOT 标志 */
    old_root_header->btpo_flags &= ~BTP_ROOT;
    /* 如果原根目前只有叶节点标志，添加内部标志 */
    if (!(old_root_header->btpo_flags & BTP_INTERNAL)) {
        old_root_header->btpo_flags |= BTP_INTERNAL;
    }

    buf_dirty(old_root_buf);
    buf_unpin(old_root_buf);

    /* 6. 标记脏并释放新根 */
    buf_dirty(new_root_buf);
    buf_unpin(new_root_buf);

    return 0;
}

/* ============================================================
 * btree_split_insert — 递归分裂插入（供 btinsert 调用）
 *
 * 当页面满时触发分裂，将中键插入父节点。
 * 如果父节点也满，递归向上分裂直到根。
 *
 * 当前简化版本：仅触发一次叶节点或内部节点分裂，
 * 不执行递归向上提升。完整递归实现在后续 Phase。
 * ============================================================ */

int btree_split_insert(Relation rel, uint32_t blkno) {
    if (!rel) {
        return -1;
    }

    /* 读取页面判断类型 */
    BufferDesc *buf = buf_read(rel->rd_relfilenode, blkno, 0);
    if (!buf) {
        return -1;
    }

    void *page = buf_get_data(buf);
    if (!page) {
        buf_unpin(buf);
        return -1;
    }

    BTPageHeader header = (BTPageHeader)page;
    uint16_t flags = header->btpo_flags;
    buf_unpin(buf);

    uint32_t new_blkno = 0;
    int result;

    if (BT_PAGE_IS_LEAF(header)) {
        result = btree_split_leaf(rel, blkno, &new_blkno);
    } else if (BT_PAGE_IS_INTERNAL(header)) {
        result = btree_split_internal(rel, blkno, &new_blkno);
    } else {
        return -1;
    }

    if (result != 0) {
        return -1;
    }

    /* 如果是根节点分裂，创建新根 */
    if ((flags & BTP_ROOT) != 0 && new_blkno > 0) {
        result = btree_split_root(rel, blkno, new_blkno);
    }

    return result;
}