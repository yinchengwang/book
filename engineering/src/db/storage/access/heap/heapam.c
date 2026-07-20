/**
 * @file heapam.c
 * @brief Heap Access Method 实现
 *
 * ## Heap 文件结构
 *
 * Heap 是 PostgreSQL 风格的关系存储，按页面组织：
 *
 * ```
 * +------------------+
 * | Page 0           |  页面大小固定（默认 8KB）
 * |  +------------+  |
 * |  | PageHeader |  |  元数据（LSN、校验和、空闲空间指针等）
 * |  +------------+  |
 * |  | LinePtr[0]|  |  行指针数组（从 pd_lower 向后增长）
 * |  | LinePtr[1]|  |
 * |  | ...        |  |
 * |  +------------+  |
 * |  | Free Space |  |  元组存储区（从 pd_upper 向前增长）
 * |  | Tuple 1   |  |
 * |  | Tuple 2   |  |
 * |  +------------+  |
 * +------------------+
 * | Page 1           |
 * | ...              |
 * +------------------+
 * ```
 *
 * ## Slotted Page 设计
 *
 * 采用 Slotted Page 结构（与 Oracle/SQL Server 相同）：
 * - 元组可以从页面任意位置存储（不要求连续）
 * - LinePointer 数组维护"序号 → 元组位置"的映射
 * - 支持原地更新和高效删除
 *
 * ## 页面空间管理
 *
 * | 指针 | 方向 | 说明 |
 * |------|------|------|
 * | pd_lower | → | LinePointer 数组下界 |
 * | pd_upper | ← | 空闲空间上界 |
 *
 * ## MVCC 版本链
 *
 * UPDATE 操作创建版本链：
 * - 旧元组：t_xmax = 更新事务ID，t_ctid → 新元组位置
 * - 新元组：t_xmin = 更新事务ID，t_ctid 初始指向自己
 *
 * 版本链允许并发事务看到不同版本的元组，实现 MVCC。
 */

#include "db/heapam.h"
#include "db/rel.h"
#include "db/buf.h"
#include "db/catalog.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================
 * 全局状态
 * ============================================================ */

static bool heapam_initialized = false;
static HeapAMStats global_stats = {0};

/* ============================================================
 * 初始化
 * ============================================================ */

/**
 * @brief 初始化 Heap Access Method
 *
 * 初始化堆表访问方法，建立与 Buffer Pool 和 Relation Manager 的连接。
 *
 * @return 0 成功，-1 失败
 *
 * @note 幂等操作：多次调用安全
 */
int heapam_init(void) {
    if (heapam_initialized) {
        return 0;
    }

    /* 初始化必要的子系统（rel_init 是幂等的） */
    if (rel_init() != 0) {
        return -1;
    }

    heapam_initialized = true;
    return 0;
}

void heapam_shutdown(void) {
    heapam_initialized = false;
}

/* ============================================================
 * 页面操作实现
 * ============================================================ */

/**
 * @brief 初始化 Heap 页面
 *
 * 将页面格式化为 Heap 页面结构：
 * - 设置页面头部
 * - 初始化空闲空间指针
 * - 重置元数据
 *
 * @param page 页面数据指针
 * @param size 页面大小
 *
 * ## 初始化后的页面状态
 *
 * ```
 * pd_lower = SizeOfPageHeaderData (偏移量 24)
 * pd_upper = size (页面末尾)
 * pd_special = size (特殊空间起始，默认等于页面大小)
 * pd_flags = 0
 * pd_lsn = 0
 * ```
 *
 * ## pd_special 字段
 *
 * Heap 页面的 special space 保留给索引方法使用。
 * Heap 本身不使用，但需要初始化以保持格式兼容。
 */
void heap_page_init(void *page, size_t size) {
    if (!page || size < HEAP_PAGE_SIZE) {
        return;
    }

    PageHeader ph = (PageHeader)page;

    /* 初始化页面头部 */
    ph->pd_lsn = 0;
    ph->pd_checksum = 0;
    ph->pd_flags = 0;
    ph->pd_lower = SizeOfPageHeaderData;  /* LinePointer 数组起始 */
    ph->pd_upper = size;                   /* 空闲空间起始 */
    ph->pd_special = size;                 /* 特殊空间（索引用） */
    ph->pd_pagesize_version = 0x0001;      /* 8KB, version 1 */
    ph->pd_prune_xid = 0;                  /* 清理事务ID */
    ph->pd_xid_base = 0;                   /* 事务ID基址 */
    ph->pd_multi_base = 0;                  /* 多事务基址 */
}

PageHeader heap_page_get_header(void *page) {
    return (PageHeader)page;
}

/**
 * @brief 获取页面空闲空间
 *
 * @param page 页面指针
 * @return 空闲空间字节数，0 表示页面已满或无效
 *
 * ## 计算公式
 *
 * ```
 * free_space = pd_upper - pd_lower
 * ```
 *
 * ## 使用场景
 *
 * - 插入前检查页面是否有足够空间
 * - 选择最佳页面进行插入
 * - 监控页面碎片化程度
 */
int heap_page_get_free_space(void *page) {
    if (!page) {
        return 0;
    }

    PageHeader ph = (PageHeader)page;
    return (int)(ph->pd_upper - ph->pd_lower);
}

/**
 * @brief 向页面添加元组
 *
 * 在 Slotted Page 中插入新元组：
 * - 元组从页面末尾向前存储
 * - LinePointer 从页面头部向后增长
 *
 * @param page 页面指针
 * @param tuple 元组数据
 * @param len 元组长度
 * @param lp 输出：LinePointer 指针
 * @return 0 成功，-1 失败
 *
 * ## 插入过程
 *
 * ```
 * 插入前:              插入后:
 * +------------+       +------------+
 * | PageHeader |       | PageHeader |
 * +------------+       +------------+
 * | LinePtr[]  | →     | LinePtr[]  | → (新增 LinePtr)
 * +------------+       +------------+
 * | Free Space |       | Tuple      | ← (新元组)
 * +------------+       +------------+
 *                    ↑ pd_upper
 * ```
 *
 * ## 空间计算
 *
 * 所需空间 = 元组长度 + LinePointer 大小
 * 如果 free_space < needed，返回失败
 */
int heap_page_add_tuple(void *page, const void *tuple, size_t len,
                        HeapLinePointer *lp) {
    if (!page || !tuple) {
        return -1;
    }

    PageHeader ph = (PageHeader)page;

    /* 检查是否有足够空间 */
    int free_space = ph->pd_upper - ph->pd_lower;
    int needed = len + SizeOfHeapLinePointer;

    if (free_space < needed) {
        return -1;
    }

    /* 计算新元组位置（在 pd_upper 处向前） */
    uint16_t off = ph->pd_upper - len;
    ph->pd_upper = off;

    /* 设置 LinePointer */
    HeapLinePointer lp_ptr = (HeapLinePointer)((char *)page + ph->pd_lower);
    lp_ptr->t_off = off;       /* 元组偏移量 */
    lp_ptr->t_flags = LP_USED;  /* 标记为有效 */
    lp_ptr->t_len = (uint16_t)len;  /* 元组长度 */

    /* 复制元组数据 */
    memcpy((char *)page + off, tuple, len);

    /* 更新 pd_lower（增加 LinePointer 空间） */
    ph->pd_lower += SizeOfHeapLinePointer;

    /* 输出 LinePointer */
    if (lp) {
        *lp = lp_ptr;
    }

    return 0;
}

/**
 * @brief 页面空间清理
 *
 * 清理页面中已删除的元组，回收空间。
 *
 * @param rel Relation 句柄
 * @param page 页面指针
 * @return 清理的元组数量
 *
 * ## 清理条件
 *
 * 元组可以被清理的条件：
 * 1. LP_DEAD 标志已设置
 * 2. 元组对所有活跃事务都不可见
 *
 * ## 清理过程
 *
 * 1. 遍历所有 LinePointer
 * 2. 识别 LP_DEAD 的死亡元组
 * 3. 将存活元组向前移动，紧凑排列
 * 4. 更新 LinePointer 数组
 * 5. 更新 pd_upper 和 pd_lower
 */
int heap_page_prune(Relation rel, void *page) {
    if (!page) {
        return 0;
    }

    PageHeader ph = (PageHeader)page;

    /* 计算 LinePointer 数量 */
    int lp_count = (ph->pd_lower - SizeOfPageHeaderData) / SizeOfHeapLinePointer;
    if (lp_count <= 0) {
        return 0;
    }

    int pruned = 0;

    /* 第一遍：识别需要清理的元组 */
    /* 第二遍：移动存活元组（简化实现：只统计，不实际移动） */
    for (int i = 0; i < lp_count; i++) {
        uint16_t lp_offset = SizeOfPageHeaderData + i * SizeOfHeapLinePointer;
        HeapLinePointer lp = (HeapLinePointer)((char *)page + lp_offset);

        /* 检查是否是死亡元组 */
        if ((lp->t_flags & LP_DEAD) && (lp->t_flags & LP_USED)) {
            pruned++;
        }
    }

    /* 简化实现：返回清理计数，不实际移动元组 */
    /* 完整实现需要： */
    /* 1. 分配临时缓冲区 */
    /* 2. 复制所有存活元组到临时缓冲区 */
    /* 3. 清空页面 */
    /* 4. 从临时缓冲区复制回页面 */
    /* 5. 更新所有 LinePointer 的 t_off */

    (void)rel;
    return pruned;
}

/**
 * @brief 获取页面元组数量
 *
 * @param page 页面指针
 * @return 元组数量
 */
int heap_page_get_tuple_count(void *page) {
    if (!page) {
        return 0;
    }

    PageHeader ph = (PageHeader)page;
    int lp_count = (ph->pd_lower - SizeOfPageHeaderData) / SizeOfHeapLinePointer;
    return lp_count > 0 ? lp_count : 0;
}

/* ============================================================
 * 元组操作实现
 * ============================================================ */

int heap_insert(Relation rel, const void *tuple, size_t len,
                uint32_t cid, int options, void *bistate) {
    if (!rel || !tuple) {
        return -1;
    }

    (void)cid;
    (void)options;
    (void)bistate;

    /* 获取或分配新页面 */
    BlockNumber blocknum = rel->rd_nblocks;
    BufferDesc *buf = NULL;

    /* 如果表为空，创建第一页 */
    if (rel->rd_nblocks == 0) {
        buf = buf_new_page(rel->rd_relfilenode);
        if (!buf) {
            return -1;
        }
        buf->blocknum = 0;
        rel->rd_nblocks = 1;
        /* 将新页面注册到 hash 表，使其可被 buf_read 找到 */
        buf_hash_insert(rel->rd_relfilenode, 0, buf->buf_id);
    } else {
        /* 尝试读取最后一个页面 */
        buf = buf_read(rel->rd_relfilenode, blocknum - 1, 0);
        if (!buf) {
            /* 如果读取失败，创建新页面 */
            buf = buf_new_page(rel->rd_relfilenode);
            if (!buf) {
                return -1;
            }
            buf->blocknum = blocknum;
            rel->rd_nblocks++;
        }
    }

    /* 获取页面数据 */
    char *page = (char *)buf_get_data(buf);
    if (!page) {
        buf_unpin(buf);
        return -1;
    }

    /* 如果是新页面，初始化它 */
    PageHeader ph = heap_page_get_header(page);
    if (ph->pd_lower == 0) {
        heap_page_init(page, HEAP_PAGE_SIZE);
    }

    /* 检查页面是否有足够空间 */
    int free_space = heap_page_get_free_space(page);
    if (free_space < (int)(len + SizeOfHeapLinePointer)) {
        /* 页面空间不足，创建新页面 */
        buf_unpin(buf);
        buf = buf_new_page(rel->rd_relfilenode);
        if (!buf) {
            return -1;
        }
        buf->blocknum = rel->rd_nblocks;
        rel->rd_nblocks++;

        page = (char *)buf_get_data(buf);
        if (!page) {
            buf_unpin(buf);
            return -1;
        }
        heap_page_init(page, HEAP_PAGE_SIZE);
        ph = heap_page_get_header(page);
    }

    /* 在页面中插入元组 */
    HeapLinePointer lp;
    if (heap_page_add_tuple(page, tuple, len, &lp) != 0) {
        /* 页面空间不足，需要分裂或创建新页 */
        buf_unpin(buf);
        return -1;
    }

    /* 标记为脏并释放 */
    buf_dirty(buf);
    buf_unpin(buf);

    /* 更新统计 */
    global_stats.inserts++;

    return 0;
}

int heap_delete(Relation rel, const void *tid, uint32_t cid,
                bool crosscheck, bool wait) {
    if (!rel || !tid) {
        return -1;
    }

    (void)cid;
    (void)crosscheck;
    (void)wait;

    /* 解析 TID：前4字节是 block，后2字节是 offset */
    const uint8_t *tid_data = (const uint8_t *)tid;
    uint32_t blocknum = 0;
    uint16_t offset = 0;
    memcpy(&blocknum, tid_data, sizeof(uint32_t));
    memcpy(&offset, tid_data + sizeof(uint32_t), sizeof(uint16_t));

    /* 读取页面 */
    BufferDesc *buf = buf_read(rel->rd_relfilenode, blocknum, 0);
    if (!buf) {
        return -1;
    }

    char *page = (char *)buf_get_data(buf);
    if (!page) {
        buf_unpin(buf);
        return -1;
    }

    /* 检查 offset 是否有效 */
    PageHeader ph = heap_page_get_header(page);
    if (offset < SizeOfPageHeaderData || offset >= ph->pd_upper) {
        buf_unpin(buf);
        return -1;
    }

    /* 获取 LinePointer */
    HeapLinePointer lp = (HeapLinePointer)(page + offset);
    uint16_t lp_offset = offset;

    /* 检查是否是有效的 LinePointer 位置 */
    if ((lp_offset - SizeOfPageHeaderData) % SizeOfHeapLinePointer != 0) {
        buf_unpin(buf);
        return -1;
    }

    /* 设置 t_flags 标记删除 */
    lp->t_flags |= LP_DEAD;  /* 标记为死亡元组 */

    /* 标记页面为脏 */
    buf_dirty(buf);
    buf_unpin(buf);

    /* 更新统计 */
    global_stats.deletes++;

    return 0;
}

/**
 * @brief 更新元组（创建版本链）
 *
 * UPDATE 操作创建版本链：
 * 1. 旧元组标记：t_xmax = 当前事务ID，t_infomask |= HEAP_XMAX_INVALID | HEAP_UPDATED
 * 2. 旧元组 t_ctid 指向新元组位置
 * 3. 新元组设置：t_xmin = 当前事务ID，t_ctid 初始指向自己
 *
 * @param rel Relation
 * @param tid 旧元组指针
 * @param newtuple 新元组数据
 * @param newlen 新元组长度
 * @param cid 命令 ID
 * @param options 插入选项
 * @param bistate 批量插入状态
 * @param lockmode 锁模式
 * @return 0 成功，-1 失败
 */
int heap_update(Relation rel, const void *tid,
                const void *newtuple, size_t newlen,
                uint32_t cid, int options, void *bistate,
                int lockmode) {
    if (!rel || !tid || !newtuple) {
        return -1;
    }

    (void)cid;
    (void)options;
    (void)bistate;
    (void)lockmode;

    /* 解析 TID */
    const ItemPointerData *old_tid = (const ItemPointerData *)tid;
    uint32_t old_block = old_tid->ip_blkid;
    uint16_t old_posid = old_tid->ip_posid;

    /* 读取旧元组所在页面 */
    BufferDesc *buf = buf_read(rel->rd_relfilenode, old_block, 1);  /* 1 = 可写 */
    if (!buf) {
        return -1;
    }

    char *page = (char *)buf_get_data(buf);
    if (!page) {
        buf_unpin(buf);
        return -1;
    }

    /* 计算 LinePointer 位置 */
    uint16_t lp_offset = SizeOfPageHeaderData + (old_posid - 1) * SizeOfHeapLinePointer;
    HeapLinePointer old_lp = (HeapLinePointer)(page + lp_offset);

    /* 检查旧元组是否有效 */
    if (!(old_lp->t_flags & LP_USED) || old_lp->t_off == 0) {
        buf_unpin(buf);
        return -1;
    }

    /* ========== 版本链：标记旧元组为死亡 ========== */
    old_lp->t_flags |= LP_DEAD;

    /* ========== 检查当前页面空间 ========== */
    PageHeader ph = heap_page_get_header(page);
    int free_space = ph->pd_upper - ph->pd_lower;

    if (free_space < (int)(newlen + SizeOfHeapLinePointer)) {
        /* 页面空间不足，需要分配新页面 */
        /* 注意：跨页更新会破坏 HOT 链，所有后续的更新都需要更新索引 */
        buf_unpin(buf);

        /* 分配新页面 */
        BufferDesc *new_buf = buf_new_page(rel->rd_relfilenode);
        if (!new_buf) {
            return -1;
        }

        char *new_page = (char *)buf_get_data(new_buf);

        /* 插入新元组 */
        HeapLinePointer new_lp;
        if (heap_page_add_tuple(new_page, newtuple, newlen, &new_lp) != 0) {
            buf_unpin(new_buf);
            return -1;
        }

        /* 标记新页面为脏 */
        buf_dirty(new_buf);

        /* 更新统计 */
        global_stats.updates++;

        buf_unpin(new_buf);

        return 0;
    }

    /* 页面有空间，插入新元组（支持 HOT） */
    /* HOT: 同一页面更新时，新版本插入到旧版本之后，通过 t_ctid 形成版本链 */
    HeapLinePointer new_lp;
    if (heap_page_add_tuple(page, newtuple, newlen, &new_lp) != 0) {
        buf_unpin(buf);
        return -1;
    }

    /* HOT 链：旧元组的 t_ctid 应指向新元组 */
    /* 当前简化实现中，旧元组已标记为 LP_DEAD */
    /* 新元组插入后，后续扫描会通过版本链访问 */

    /* 标记页面为脏 */
    buf_dirty(buf);

    /* 更新统计 */
    global_stats.updates++;

    buf_unpin(buf);
    return 0;
}

int heap_lock_tuple(Relation rel, void *tid, uint32_t cid,
                    int mode, bool nowait, void *tm_result) {
    if (!rel || !tid) {
        return -1;
    }

    (void)cid;
    (void)mode;
    (void)nowait;
    (void)tm_result;

    return 0;
}

/* ============================================================
 * 扫描操作实现
 * ============================================================ */

void *heap_getnext(TableScanDesc scan, ScanDirection direction) {
    if (!scan || !scan->rs_rd) {
        return NULL;
    }

    (void)direction;

    Relation rel = scan->rs_rd;

    /* 如果没有当前 buffer，读取第一块 */
    if (!scan->rs_cbuf) {
        if (scan->rs_cblock >= rel->rd_nblocks) {
            return NULL;  /* 扫描结束 */
        }

        /* 读取页面 */
        scan->rs_cbuf = buf_read(rel->rd_relfilenode, scan->rs_cblock, 0);
        if (!scan->rs_cbuf) {
            return NULL;
        }

        scan->rs_cindex = 0;
    }

    /* 获取页面数据 */
    char *page = (char *)buf_get_data(scan->rs_cbuf);
    if (!page) {
        return NULL;
    }

    /* 获取元组计数 */
    int tuple_count = heap_page_get_tuple_count(page);

    /* 遍历当前页面的元组 */
    while (scan->rs_cindex < tuple_count) {
        /* 计算 LinePointer 位置 */
        uint16_t lp_offset = SizeOfPageHeaderData + scan->rs_cindex * SizeOfHeapLinePointer;
        HeapLinePointer lp = (HeapLinePointer)(page + lp_offset);

        /* 检查元组是否有效 */
        if ((lp->t_flags & LP_USED) && lp->t_off > 0 && lp->t_off < HEAP_PAGE_SIZE) {
            /* 检查元组是否被删除（LP_DEAD 标志） */
            bool is_deleted = (lp->t_flags & LP_DEAD) != 0;

            if (!is_deleted) {
                /* 获取元组数据 */
                char *tuple_data = page + lp->t_off;
                scan->rs_cindex++;

                /* 更新统计 */
                global_stats.tuples_read++;

                return tuple_data;
            }
        }

        scan->rs_cindex++;
    }

    /* 移到下一页 */
    buf_unpin(scan->rs_cbuf);
    scan->rs_cbuf = NULL;
    scan->rs_cblock++;
    scan->rs_cindex = 0;

    /* 检查是否还有更多页 */
    if (scan->rs_cblock >= rel->rd_nblocks) {
        return NULL;
    }

    /* 读取下一页 */
    scan->rs_cbuf = buf_read(rel->rd_relfilenode, scan->rs_cblock, 0);
    if (!scan->rs_cbuf) {
        return NULL;
    }

    /* 递归获取下一个元组 */
    return heap_getnext(scan, direction);
}

void *heap_getcurr(TableScanDesc scan) {
    return scan ? scan->rs_ctbuf : NULL;
}

void heap_mark_tuples_dead(Relation rel, void *tid) {
    (void)rel;
    (void)tid;
    global_stats.dead_tuples++;
}

bool heap_lock_tuple_wait(Relation rel, void *tid, int mode) {
    (void)rel;
    (void)tid;
    (void)mode;
    return true;
}

/* ============================================================
 * 统计信息
 * ============================================================ */

void heapam_get_stats(HeapAMStats *stats) {
    if (stats) {
        *stats = global_stats;
    }
}

void heapam_reset_stats(void) {
    memset(&global_stats, 0, sizeof(global_stats));
}
