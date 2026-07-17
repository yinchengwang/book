/**
 * @file rel.c
 * @brief Relation 和 Access Method 实现
 */

#include "db/rel.h"
#include "db/catalog.h"
#include "db/buf.h"
#include "db/heapam.h"
#include "db/lock.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================
 * 全局状态
 * ============================================================ */

static bool rel_initialized = false;

/* ============================================================
 * TupleDesc 实现
 * ============================================================ */

TupleDesc CreateTupleDesc(int natts) {
    if (natts <= 0) {
        return NULL;
    }

    TupleDesc tdesc = (TupleDesc)calloc(1, sizeof(struct TupleDescData));
    if (!tdesc) {
        return NULL;
    }

    tdesc->natts = natts;
    tdesc->tdtypeid = 0;
    tdesc->tdtypmod = -1;
    tdesc->tdenv = 0;
    tdesc->tdhasoid = false;

    tdesc->attrs = (void *)calloc(natts, sizeof(tdesc->attrs[0]));
    if (!tdesc->attrs) {
        free(tdesc);
        return NULL;
    }

    return tdesc;
}

TupleDesc CreateTupleDescCopy(TupleDesc src) {
    if (!src) {
        return NULL;
    }

    TupleDesc copy = CreateTupleDesc(src->natts);
    if (!copy) {
        return NULL;
    }

    copy->tdtypeid = src->tdtypeid;
    copy->tdtypmod = src->tdtypmod;
    copy->tdenv = src->tdenv;
    copy->tdhasoid = src->tdhasoid;

    memcpy(copy->attrs, src->attrs, src->natts * sizeof(src->attrs[0]));

    return copy;
}

void FreeTupleDesc(TupleDesc tdesc) {
    if (!tdesc) {
        return;
    }
    if (tdesc->attrs) {
        free(tdesc->attrs);
    }
    free(tdesc);
}

int TupleDescNatts(TupleDesc tdesc) {
    return tdesc ? tdesc->natts : 0;
}

void *TupleDescAttr(TupleDesc tdesc, int attnum) {
    if (!tdesc || attnum < 1 || attnum > tdesc->natts) {
        return NULL;
    }
    return &tdesc->attrs[attnum - 1];
}

/* ============================================================
 * ScanKey 实现
 * ============================================================ */

void ScanKeyInit(ScanKey key, int attno, ScanKeyOp op, void *argument) {
    ScanKeyInitWithInfo(key, attno, op, 0, argument);
}

void ScanKeyInitWithInfo(ScanKey key, int attno, ScanKeyOp op,
                         size_t arglen, void *argument) {
    if (!key) {
        return;
    }

    key->sk_attno = attno;
    key->sk_op = op;
    key->sk_procedure = 0;
    key->sk_argument = argument;
    key->sk_arglen = arglen;
}

/* ============================================================
 * Relation 操作
 * ============================================================ */

static Relation relation_alloc(void) {
    Relation rel = (Relation)calloc(1, sizeof(struct RelationData));
    if (!rel) {
        return NULL;
    }

    rel->rd_id = InvalidOid;
    rel->rd_relid = InvalidOid;
    rel->rd_relkind = RELKIND_RELATION;
    rel->rd_am = AM_HEAP;
    rel->rd_relfilenode = 0;
    rel->rd_tablespace = 0;
    rel->rd_backend = 0;
    rel->rd_nblocks = 0;
    rel->rd_lockmode = 0;
    rel->rd_islocal = true;
    rel->rd_att = NULL;
    rel->rd_amstate = NULL;
    rel->rd_fd = NULL;
    rel->rd_bufferPool = NULL;
    rel->rd_refcnt = 0;  /* 由调用者设置 */

    return rel;
}

/**
 * @brief 创建表 Relation（内部辅助函数）
 *
 * @param table_info 表信息（来自 Catalog）
 * @param mode 打开模式
 * @return Relation 句柄，失败返回 NULL
 */
static Relation create_table_relation(table_info_t *table_info, int mode) {
    Relation rel = relation_alloc();
    if (!rel) {
        return NULL;
    }

    rel->rd_id = table_info->oid;
    rel->rd_relid = table_info->oid;
    rel->rd_relkind = (RelKind)table_info->relkind;
    rel->rd_relfilenode = table_info->filenode;
    rel->rd_tablespace = table_info->tablespace;
    rel->rd_nblocks = (table_info->npages > 0) ? table_info->npages : 0;

    /* 构建 TupleDesc */
    int ncols = 0;
    column_info_t *columns = catalog_get_columns(table_info->oid, &ncols);
    if (columns) {
        rel->rd_att = CreateTupleDesc(ncols);
        if (rel->rd_att) {
            rel->rd_att->natts = ncols;
            rel->rd_att->tdtypeid = table_info->type_oid;
            for (int i = 0; i < ncols; i++) {
                rel->rd_att->attrs[i].atttypid = columns[i].type_oid;
                rel->rd_att->attrs[i].atttypmod = columns[i].typmod;
                rel->rd_att->attrs[i].attlen = columns[i].len;
                rel->rd_att->attrs[i].attalign = columns[i].align;
                rel->rd_att->attrs[i].attbyval = false;
                rel->rd_att->attrs[i].attstorage = columns[i].storage;
                rel->rd_att->attrs[i].attnotnull = columns[i].attnotnull;
                rel->rd_att->attrs[i].atthasdef = columns[i].has_default;
                strncpy(rel->rd_att->attrs[i].attname, columns[i].name, NAMEDATALEN - 1);
            }
        }
        free(columns);
    }

    /* 设置锁模式 */
    rel->rd_lockmode = (mode & REL_OPEN_READWRITE) ? 1 : 0;

    return rel;
}

/**
 * @brief 创建索引 Relation（内部辅助函数）
 *
 * @param index_info 索引信息（来自 Catalog）
 * @param mode 打开模式
 * @return Relation 句柄，失败返回 NULL
 */
static Relation create_index_relation(index_info_t *index_info, int mode) {
    Relation rel = relation_alloc();
    if (!rel) {
        return NULL;
    }

    rel->rd_id = index_info->oid;
    rel->rd_relid = index_info->table_oid;  /* 指向所属表 */
    rel->rd_relkind = RELKIND_INDEX;
    rel->rd_am = AM_BTREE;  /* 索引默认使用 BTree */
    rel->rd_relfilenode = index_info->oid;  /* 索引使用自己的 OID */
    rel->rd_nblocks = 0;  /* 索引页面数未知 */

    /* 设置锁模式 */
    rel->rd_lockmode = (mode & REL_OPEN_READWRITE) ? 1 : 0;

    return rel;
}

Relation relation_open(Oid relid, int mode) {
    /* 不自动初始化，让调用者确保已初始化 */

    if (relid == InvalidOid) {
        return NULL;
    }

    /* 先尝试作为表查找 */
    table_info_t *table_info = catalog_get_table(relid);
    if (table_info) {
        Relation rel = create_table_relation(table_info, mode);
        if (rel) {
            rel->rd_refcnt = 1;
        }
        return rel;
    }

    /* 如果不是表，尝试作为索引查找 */
    index_info_t *index_info = catalog_get_index(relid);
    if (index_info) {
        Relation rel = create_index_relation(index_info, mode);
        if (rel) {
            rel->rd_refcnt = 1;
        }
        return rel;
    }

    /* 既不是表也不是索引 */
    fprintf(stderr, "[WARN] relation_open: OID %u not found in catalog\n", relid);
    return NULL;
}

Relation relation_opennode(uint32_t relfilenode, int mode) {
    /* 不自动初始化 */

    if (relfilenode == 0) {
        return NULL;
    }

    Relation rel = relation_alloc();
    if (!rel) {
        return NULL;
    }

    rel->rd_relfilenode = relfilenode;
    rel->rd_lockmode = (mode & REL_OPEN_READWRITE) ? 1 : 0;
    rel->rd_refcnt = 1;  /* relation_opennode 创建时设 refcnt 为 1 */

    return rel;
}

void relation_close(Relation rel, int mode) {
    if (!rel) {
        return;
    }

    /* 如果 refcnt 已经是 0，说明已被释放，直接返回 */
    if (rel->rd_refcnt <= 0) {
        return;
    }

    rel->rd_refcnt--;

    /* refcnt 为 0 时释放内存 */
    if (rel->rd_refcnt == 0) {
        /* 更新 Catalog 中的页面数 */
        if (rel->rd_id != InvalidOid && rel->rd_nblocks > 0) {
            /* 通过 catalog 更新页面数（简化：直接修改 table_info） */
            table_info_t *info = catalog_get_table(rel->rd_id);
            if (info) {
                info->npages = rel->rd_nblocks;
                catalog_free_table(info);
            }
        }

        if (rel->rd_att) {
            FreeTupleDesc(rel->rd_att);
            rel->rd_att = NULL;
        }
        if (rel->rd_amstate) {
            free(rel->rd_amstate);
            rel->rd_amstate = NULL;
        }
        free(rel);
    }

    (void)mode;  /* 未使用 */
}

int relation_create(Oid relid, TupleDesc tdesc, RelKind relkind,
                    AccessMethodType amtype) {
    if (relid == InvalidOid || !tdesc) {
        return -1;
    }

    /* 直接返回成功（实际实现会使用 Catalog） */
    (void)tdesc;
    (void)relkind;
    (void)amtype;

    return 0;
}

int relation_drop(Oid relid) {
    if (relid == InvalidOid) {
        return -1;
    }

    /* 从 Catalog 删除表 */
    catalog_drop_table(relid);

    return 0;
}

TupleDesc relation_getdesc(Relation rel) {
    return rel ? rel->rd_att : NULL;
}

int relation_getnatts(Relation rel) {
    return rel && rel->rd_att ? rel->rd_att->natts : 0;
}

BlockNumber relation_getnblocks(Relation rel) {
    return rel ? rel->rd_nblocks : 0;
}

uint32_t relation_getfilenode(Relation rel) {
    return rel ? rel->rd_relfilenode : 0;
}

AccessMethodType relation_getam(Relation rel) {
    return rel ? rel->rd_am : AM_HEAP;
}

/* ============================================================
 * 表扫描实现
 * ============================================================ */

TableScanDesc table_beginscan(Relation rel, int nkeys, ScanKey key) {
    if (!rel) {
        return NULL;
    }

    TableScanDesc scan = (TableScanDesc)calloc(1, sizeof(TableScanDescData));
    if (!scan) {
        return NULL;
    }

    scan->rs_rd = rel;
    scan->rs_startblock = 0;
    scan->rs_numblocks = InvalidBlockNumber;
    scan->rs_cblock = 0;
    scan->rs_cindex = 0;
    scan->rs_cbuf = NULL;
    scan->rs_ctbuf = NULL;
    scan->rs_key = key;
    scan->rs_nkeys = nkeys;
    scan->rs_direction = ForwardScanDirection;

    /* 注意：不增加 Relation 的 refcnt，由调用者管理 */
    (void)rel;

    return scan;
}

TableScanDesc table_beginscan_all(Relation rel) {
    return table_beginscan(rel, 0, NULL);
}

void table_endscan(TableScanDesc scan) {
    if (!scan) {
        return;
    }

    /* 释放当前 buffer */
    if (scan->rs_cbuf) {
        buf_unpin(scan->rs_cbuf);
        scan->rs_cbuf = NULL;
    }

    /* 注意：Relation 的 refcnt 由调用者管理，不在这里减少 */
    scan->rs_rd = NULL;

    free(scan);
}

void *table_getnext(TableScanDesc scan) {
    /* 委托给 Heap AM 的真实实现 */
    return heap_getnext(scan, ForwardScanDirection);
}

void *table_getcurr(TableScanDesc scan) {
    return scan ? scan->rs_ctbuf : NULL;
}

/* ============================================================
 * 堆元组操作（委托给 Heap AM）
 * ============================================================ */

/* heap_insert, heap_update, heap_delete, heap_getnext, heap_getcurr
 * 等函数已在 heapam.c 中实现，通过 heapam.h 暴露 */

#include "db/heapam.h"

/* ============================================================
 * 索引扫描实现
 * ============================================================ */

IndexScanDesc index_beginscan(Relation rel, int nkeys, ScanKey key) {
    if (!rel) {
        return NULL;
    }

    IndexScanDesc scan = (IndexScanDesc)calloc(1, sizeof(struct IndexScanDescData));
    if (!scan) {
        return NULL;
    }

    scan->idx_relation = rel;
    scan->heap_relation = NULL;
    scan->key = key;
    scan->nkeys = nkeys;
    scan->direction = ForwardScanDirection;
    scan->opclasses = NULL;

    /* 注意：不增加 Relation 的 refcnt，由调用者管理 */
    (void)rel;

    return scan;
}

IndexScanDesc index_beginscan_heapscan(Relation indexrel, Relation heaprel,
                                       int nkeys, ScanKey key) {
    IndexScanDesc scan = index_beginscan(indexrel, nkeys, key);
    if (scan) {
        scan->heap_relation = heaprel;
    }
    return scan;
}

void index_endscan(IndexScanDesc scan) {
    if (!scan) {
        return;
    }

    /* 注意：Relation 的 refcnt 由调用者管理，不在这里减少 */
    scan->idx_relation = NULL;
    scan->heap_relation = NULL;

    free(scan);
}

void *index_getnext(IndexScanDesc scan) {
    if (!scan) {
        return NULL;
    }

    /* 简化：返回 NULL（需要 BTree 实现） */
    (void)scan;
    return NULL;
}

/* ============================================================
 * 初始化
 * ============================================================ */

int rel_init(void) {
    if (rel_initialized) {
        return 0;
    }

    /* 初始化 Buffer Pool */
    if (buf_init(0) != 0) {
        return -1;
    }

    /* 初始化 Catalog */
    if (catalog_init() != 0) {
        buf_shutdown();
        return -1;
    }

    rel_initialized = true;
    return 0;
}

void rel_shutdown(void) {
    if (!rel_initialized) {
        return;
    }

    catalog_shutdown();
    buf_shutdown();

    rel_initialized = false;
}

/* ============================================================
 * Relation 锁操作
 * ============================================================ */

/* 全局锁管理器（简化版） */
static lock_manager_t *g_lockmgr = NULL;

int relation_lock(Relation rel, int mode, uint32_t timeout_ms) {
    if (!rel) {
        return LOCK_ERROR;
    }

    /* 获取锁管理器 */
    lock_manager_t *mgr = rel->rd_lockmgr;
    if (!mgr) {
        mgr = g_lockmgr;
    }
    if (!mgr) {
        /* 懒加载：创建全局锁管理器 */
        mgr = lock_mgr_create();
        if (!mgr) {
            return LOCK_ERROR;
        }
        g_lockmgr = mgr;
    }

    /* 转换为锁模式 */
    lock_mode_t lock_mode = (mode == REL_OPEN_READWRITE) ? LOCK_MODE_X : LOCK_MODE_S;

    /* 尝试获取表锁 */
    lock_result_t result = lock_acquire(mgr, NULL, LOCK_TABLE, rel->rd_id,
                                        0, lock_mode, timeout_ms);

    if (result == LOCK_OK) {
        rel->rd_lockmode = mode;
        rel->rd_explicit_lock = true;
    }

    return (int)result;
}

void relation_unlock(Relation rel) {
    if (!rel || !rel->rd_explicit_lock) {
        return;
    }

    lock_manager_t *mgr = rel->rd_lockmgr;
    if (!mgr) {
        mgr = g_lockmgr;
    }
    if (!mgr) {
        return;
    }

    lock_mode_t lock_mode = (rel->rd_lockmode == REL_OPEN_READWRITE) ?
                            LOCK_MODE_X : LOCK_MODE_S;

    lock_release(mgr, NULL, LOCK_TABLE, rel->rd_id, lock_mode);
    rel->rd_lockmode = 0;
    rel->rd_explicit_lock = false;
}

void relation_set_lockmgr(Relation rel, lock_manager_t *mgr) {
    if (rel) {
        rel->rd_lockmgr = mgr;
    }
}

lock_manager_t *relation_get_lockmgr(Relation rel) {
    return rel ? rel->rd_lockmgr : g_lockmgr;
}
