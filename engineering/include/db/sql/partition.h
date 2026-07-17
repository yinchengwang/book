/**
 * @file partition.h
 * @brief 分区表路由机制
 *
 * 实现范围分区（RANGE）、列表分区（LIST）和哈希分区（HASH）
 * 的分区键路由功能。
 *
 * 路由核心逻辑：
 *   - PartitionRouting: 根据分区键值确定目标分区
 *   - PartitionRangeRouting: 范围分区边界比较
 *   - PartitionListRouting: 列表分区枚举匹配
 *   - PartitionHashRouting: 哈希分区取模计算
 */

#ifndef DB_SQL_PARTITION_H
#define DB_SQL_PARTITION_H

#include <stdint.h>
#include <stdbool.h>

/* 从 nodetags.h 获取 Datum, Oid, Size, NodeTag 等基础类型 */
#include "db/sql/nodes/nodetags.h"

/* 从 execnodes.h 获取 EState, ResultRelInfo 前向声明 */
#include "db/sql/nodes/execnodes.h"

/* 从 memctx.h 获取 MemoryContext, palloc 等 */
#include "db/sql/memctx.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 分区类型枚举
 * ======================================================================== */

/**
 * @brief 分区类型枚举
 */
typedef enum PartitionType {
    PARTITION_TYPE_RANGE,    /**< 范围分区：按数值/日期范围划分 */
    PARTITION_TYPE_LIST,    /**< 列表分区：按枚举值列表划分 */
    PARTITION_TYPE_HASH,     /**< 哈希分区：按哈希取模划分 */
} PartitionType;

/* ========================================================================
 * 分区边界信息
 * ======================================================================== */

/**
 * @brief 分区边界信息
 *
 * 存储分区的边界值，用于范围分区和列表分区。
 * 对于范围分区：datums 存储上界值
 * 对于列表分区：datums 存储枚举值
 */
typedef struct PartitionBoundInfo {
    PartitionType    strategy;        /**< 分区策略 */
    int              ndatums;         /**< 边界值数量 */
    Datum           *datums;          /**< 边界值数组 */
    int             *indexes;         /**< 分区索引映射（datums 对应的分区号） */
    bool            *is_inclusive;    /**< 是否包含边界值（仅范围分区） */
} PartitionBoundInfo;

/**
 * @brief 创建分区边界信息
 *
 * @param strategy    分区策略
 * @param ndatums     边界值数量
 * @param datums      边界值数组
 * @param indexes     分区索引映射（可为 NULL）
 *
 * @return 新创建的 PartitionBoundInfo；失败返回 NULL
 */
PartitionBoundInfo *CreatePartitionBoundInfo(
    PartitionType strategy,
    int ndatums,
    Datum *datums,
    int *indexes);

/**
 * @brief 销毁分区边界信息
 *
 * @param boundinfo 待销毁的 PartitionBoundInfo（可为 NULL）
 */
void DestroyPartitionBoundInfo(PartitionBoundInfo *boundinfo);

/* ========================================================================
 * 分区描述符
 * ======================================================================== */

/**
 * @brief 分区描述符
 *
 * 描述一个分区表的分区结构信息。
 */
typedef struct PartitionDesc {
    NodeTag          type;            /**< T_PartitionDesc */
    PartitionType     parttype;       /**< 分区类型 */
    Oid              relid;           /**< 父表 OID */
    int              nparts;          /**< 分区数量 */
    int16_t          partnatts;       /**< 分区键列数 */
    int16_t         *partattrs;       /**< 分区键属性号数组 */
    Oid             *partopfamily;   /**< 操作符族数组 */
    Oid             *partopcintype;  /**< 操作符类输入类型数组 */
    PartitionBoundInfo *boundinfo;   /**< 分区边界信息 */
} PartitionDesc;

/**
 * @brief 创建分区描述符
 *
 * @param parttype   分区类型
 * @param relid      父表 OID
 * @param nparts     分区数量
 *
 * @return 新创建的 PartitionDesc；失败返回 NULL
 */
PartitionDesc *CreatePartitionDesc(
    PartitionType parttype,
    Oid relid,
    int nparts);

/**
 * @brief 销毁分区描述符
 *
 * @param partdesc 待销毁的 PartitionDesc（可为 NULL）
 */
void DestroyPartitionDesc(PartitionDesc *partdesc);

/**
 * @brief 获取分区描述符的分类型
 *
 * @param partdesc 分区描述符
 *
 * @return 分区类型
 */
PartitionType GetPartitionType(const PartitionDesc *partdesc);

/**
 * @brief 获取分区数量
 *
 * @param partdesc 分区描述符
 *
 * @return 分区数量
 */
int GetPartitionCount(const PartitionDesc *partdesc);

/* ========================================================================
 * 分区路由上下文
 * ======================================================================== */

/**
 * @brief 分区路由上下文
 *
 * 用于 DML 操作时路由到正确的分区。
 */
typedef struct PartitionRoutingCtx {
    NodeTag          type;            /**< T_PartitionRoutingCtx */
    PartitionDesc    *partdesc;       /**< 分区描述符 */
    EState           *estate;         /**< 执行状态 */
    ResultRelInfo    *root_rrinfo;   /**< 父表结果关系信息 */
    ResultRelInfo    **part_rrinfos; /**< 分区结果关系信息数组 */
    int              nparts;          /**< 分区数量 */
} PartitionRoutingCtx;

/**
 * @brief 创建分区路由上下文
 *
 * @param partdesc 分区描述符
 * @param estate   执行状态
 *
 * @return 新创建的 PartitionRoutingCtx；失败返回 NULL
 */
PartitionRoutingCtx *CreatePartitionRoutingCtx(
    PartitionDesc *partdesc,
    EState *estate);

/**
 * @brief 销毁分区路由上下文
 *
 * @param prctx 待销毁的 PartitionRoutingCtx（可为 NULL）
 */
void DestroyPartitionRoutingCtx(PartitionRoutingCtx *prctx);

/* ========================================================================
 * 分区路由核心函数
 * ======================================================================== */

/**
 * @brief 路由分区键值到目标分区
 *
 * 根据分区键值和分区描述符，确定目标分区号。
 *
 * @param partdesc 分区描述符
 * @param partkey  分区键值数组
 * @param nkeys    分区键数量
 *
 * @return 目标分区号（0-based）；-1 表示未找到匹配分区
 */
int PartitionRouting(
    const PartitionDesc *partdesc,
    Datum *partkey,
    int nkeys);

/**
 * @brief 范围分区路由
 *
 * 通过二分查找确定分区键值所属的分区。
 *
 * @param boundinfo 分区边界信息
 * @param partkey   分区键值
 *
 * @return 目标分区号；-1 表示值超出所有范围
 */
int PartitionRangeRouting(
    const PartitionBoundInfo *boundinfo,
    Datum partkey);

/**
 * @brief 列表分区路由
 *
 * 线性搜索匹配分区键值所在的枚举分区。
 *
 * @param boundinfo 分区边界信息
 * @param partkey   分区键值
 *
 * @return 目标分区号；-1 表示值不在任何列表中
 */
int PartitionListRouting(
    const PartitionBoundInfo *boundinfo,
    Datum partkey);

/**
 * @brief 哈希分区路由
 *
 * 通过取模运算确定目标分区。
 *
 * @param nparts  分区数量
 * @param partkey 分区键值
 *
 * @return 目标分区号（0 ~ nparts-1）
 */
int PartitionHashRouting(
    int nparts,
    Datum partkey);

/* ========================================================================
 * 分区边界比较
 * ======================================================================== */

/**
 * @brief 比较分区边界值
 *
 * 比较分区键值与边界值的关系。
 *
 * @param key    分区键值
 * @param bound  边界值
 * @param is_int 是否为整数类型
 *
 * @return  -1: key < bound
 *           0: key == bound
 *           1: key > bound
 *          -2: 未知类型
 */
int PartitionBoundCmp(Datum key, Datum bound, bool is_int);

/**
 * @brief 检查值是否为 NULL 分区的候选
 *
 * NULL 值在某些分区策略中有特殊处理。
 *
 * @param partdesc 分区描述符
 * @param partkey  分区键值数组
 *
 * @return true 表示值可能属于 NULL 分区
 */
bool IsNullPartition(const PartitionDesc *partdesc, Datum *partkey);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_PARTITION_H */
