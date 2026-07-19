/**
 * @file am_btree.c
 * @brief BTree Index Access Method — 适配 IndexAmRoutine 框架
 *
 * 将 btreeam.h 的 BT* 函数包装为 IndexAmRoutine 回调，
 * 通过 index_am_register 注册为 "btree" AM。
 */
#include "db/access/amapi.h"
#include "db/btreeam.h"
#include "db/rel.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 回调实现
 * ============================================================ */

static int am_btree_build(Relation heap, IndexInfo *indexInfo)
{
    (void)heap;
    (void)indexInfo;
    /* 简化：btbuild 需要元组数组，这里先返回 0 表示构建成功 */
    return 0;
}

static int am_btree_insert(Relation index, Datum *values, bool *isnull)
{
    (void)isnull;
    return btinsert(index, (const void **)values, 1, NULL);
}

static int am_btree_delete(Relation index, Datum *values, bool *isnull)
{
    (void)isnull;
    return btdelete(index, (const void **)values, 1, NULL);
}

static IndexScanDesc am_btree_beginscan(Relation index, int nkeys, ScanKey key)
{
    BTScanDesc bt_scan = bt_beginscan(index, nkeys, key);
    return (IndexScanDesc)bt_scan;
}

static void am_btree_endscan(IndexScanDesc scan)
{
    bt_endscan((BTScanDesc)scan);
}

static bool am_btree_rescan(IndexScanDesc scan, ScanKey key)
{
    bt_rescan((BTScanDesc)scan, key);
    return true;
}

static void *am_btree_getnext(IndexScanDesc scan)
{
    return bt_getnext((BTScanDesc)scan, ForwardScanDirection);
}

/* ============================================================
 * AM 注册
 * ============================================================ */

static const IndexAmRoutine g_btree_am = {
    .am_name = "btree",
    .ambuild = am_btree_build,
    .aminsert = am_btree_insert,
    .amdelete = am_btree_delete,
    .ambeginscan = am_btree_beginscan,
    .amendscan = am_btree_endscan,
    .amrescan = am_btree_rescan,
    .amgetnext = am_btree_getnext,
};

int am_btree_init(void)
{
    return index_am_register("btree", &g_btree_am);
}