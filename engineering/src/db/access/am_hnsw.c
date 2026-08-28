/**
 * @file am_hnsw.c
 * @brief HNSW Index Access Method — 适配 IndexAmRoutine 框架
 *
 * 将 faiss_hnsw 包装为 IndexAmRoutine 回调，
 * 通过 index_am_register 注册为 "hnsw" AM。
 */
#include "db/access/amapi.h"
#include "db/rel.h"
#include "db/index/vector_index/hnsw/faiss_hnsw.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 回调实现
 * ============================================================ */

static int am_hnsw_build(Relation heap, IndexInfo *indexInfo)
{
    (void)heap;
    (void)indexInfo;
    /* HNSW 构建：创建索引结构。简化实现，后续完善 */
    return 0;
}

static int am_hnsw_insert(Relation index, Datum *values, bool *isnull)
{
    (void)index;
    (void)values;
    (void)isnull;
    return 0;
}

static int am_hnsw_delete(Relation index, Datum *values, bool *isnull)
{
    (void)index;
    (void)values;
    (void)isnull;
    return 0;
}

static IndexScanDesc am_hnsw_beginscan(Relation index, int nkeys, ScanKey key)
{
    (void)index;
    (void)nkeys;
    (void)key;
    return NULL;
}

static void am_hnsw_endscan(IndexScanDesc scan)
{
    (void)scan;
}

static bool am_hnsw_rescan(IndexScanDesc scan, ScanKey key)
{
    (void)scan;
    (void)key;
    return true;
}

static void *am_hnsw_getnext(IndexScanDesc scan)
{
    (void)scan;
    return NULL;
}

/* ============================================================
 * AM 注册
 * ============================================================ */

static const IndexAmRoutine g_hnsw_am = {
    .am_name = "hnsw",
    .ambuild = am_hnsw_build,
    .aminsert = am_hnsw_insert,
    .amdelete = am_hnsw_delete,
    .ambeginscan = am_hnsw_beginscan,
    .amendscan = am_hnsw_endscan,
    .amrescan = am_hnsw_rescan,
    .amgetnext = am_hnsw_getnext,
};

int am_hnsw_init(void)
{
    return index_am_register("hnsw", &g_hnsw_am);
}