/**
 * @file amapi.h
 * @brief Index Access Method API — 参考 PostgreSQL access/amapi.h
 *
 * 定义 IndexAmRoutine 结构体和索引 AM 注册表接口。
 * 各索引访问方法（BTree、HNSW 等）通过 index_am_register 注册自己的
 * 回调函数，执行器通过 index_am_get 按名称查找 AM。
 */
#ifndef DB_ACCESS_AMAPI_H
#define DB_ACCESS_AMAPI_H

#include "db/rel.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 前向声明
 * ============================================================ */

struct IndexInfo_s;
struct ScanKeyData;
struct IndexScanDescData;

typedef struct IndexInfo_s      IndexInfo;
typedef struct ScanKeyData     *ScanKey;
typedef struct IndexScanDescData *IndexScanDesc;

/* ============================================================
 * Datum 类型（PG 兼容）
 * ============================================================ */

typedef uintptr_t Datum;

/* ============================================================
 * IndexAmRoutine
 * ============================================================ */

/**
 * @brief 索引访问方法回调表
 *
 * 每个索引 AM（btree、hnsw 等）提供一组回调函数，通过 index_am_register
 * 注册到全局 AM 注册表中。
 */
typedef struct IndexAmRoutine_s {
    const char *am_name;       /**< AM 名称（如 "btree"、"hnsw"） */

    /** 构建索引：从 Relation 读取所有元组，构建完整的索引结构 */
    int (*ambuild)(Relation heap, IndexInfo *indexInfo);

    /** 插入一条索引条目 */
    int (*aminsert)(Relation index, Datum *values, bool *isnull);

    /** 删除一条索引条目 */
    int (*amdelete)(Relation index, Datum *values, bool *isnull);

    /** 开始索引扫描 */
    IndexScanDesc (*ambeginscan)(Relation index, int nkeys, ScanKey key);

    /** 结束索引扫描，释放资源 */
    void (*amendscan)(IndexScanDesc scan);

    /** 重置扫描（重用扫描描述符，设置新的搜索条件） */
    bool (*amrescan)(IndexScanDesc scan, ScanKey key);

    /** 获取下一个匹配的元组（返回 void*，NULL 表示结束） */
    void *(*amgetnext)(IndexScanDesc scan);
} IndexAmRoutine;

/* ============================================================
 * AM 注册表
 * ============================================================ */

/**
 * @brief 注册索引访问方法
 *
 * @param am_name  AM 名称（如 "btree"）
 * @param routine  回调表指针（调用方保持生命周期）
 * @return 0 成功，-1 注册表已满
 */
int index_am_register(const char *am_name, const IndexAmRoutine *routine);

/**
 * @brief 按名称获取索引访问方法
 *
 * @param am_name AM 名称
 * @return IndexAmRoutine 指针，未找到返回 NULL
 */
const IndexAmRoutine *index_am_get(const char *am_name);

/**
 * @brief 初始化 AM 注册表
 */
void index_am_init(void);

#ifdef __cplusplus
}
#endif

#endif /* DB_ACCESS_AMAPI_H */