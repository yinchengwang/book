/**
 * @file nodeSeqscan.h
 * @brief SeqScan 执行器节点（新 Volcano 阵营，T8 阵营统一后）
 *
 * SeqScan 节点用于全表扫描，如：
 *   - SELECT * FROM table
 *   - SELECT col1, col2 FROM table
 *
 * T8（SQL 栈全收敛）：ExecInitSeqScan 的签名统一为本阵营的
 * (Plan *, EState *, int)——executor.c 的节点注册表（ExecInitNodeFn）
 * 实际消费的就是这一签名；旧阵营（include/db/sql/nodes/nodeSeqscan.h 的
 * SeqScanPlan/SeqScanExtState 版本）的唯一实现已改写为本阵营，
 * EStatePrefixShim 布局镜像随之删除（真实 EState 直接可用）。
 *
 * 存储访问约定：本头只暴露执行器框架类型；实现文件 nodeSeqscan.c
 * 在使用存储 API 时按 "rel.h 先于 executor 头" 的顺序引入
 * （见 execnodes.h 中 TupleDescData 的 DB_REL_H 说明）。
 */

#ifndef DB_SQL_NODE_SEQSCAN_H
#define DB_SQL_NODE_SEQSCAN_H

#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * SeqScan 计划节点
 * ======================================================================== */

/**
 * @brief SeqScan 计划节点
 *
 * 用于全表扫描。
 *
 * 设计说明：SeqScan 结构体嵌入 Plan 作为第一个字段，使得 SeqScan* 可以安全转换为 Plan*。
 */
typedef struct SeqScan {
    Plan         plan;               /**< 基类：计划节点（必须作为第一个字段） */
    Oid          scanrelid;          /**< 表 OID */
} SeqScan;

/* ========================================================================
 * SeqScanState - SeqScan 执行状态
 * ======================================================================== */

/**
 * @brief SeqScan 执行状态
 *
 * 维护 SeqScan 节点的运行时状态。
 * ss_currentRelation / ss_currentScanDesc 使用前向声明的存储层标签，
 * 与 db/rel.h 的 Relation（struct RelationData）/ TableScanDesc
 * （struct TableScanDescData）一致。
 */
typedef struct SeqScanState {
    PlanState    ps;                 /**< 基类：计划状态 */
    struct RelationData *ss_currentRelation;    /**< 当前扫描的表 */
    struct TableScanDescData *ss_currentScanDesc;   /**< 扫描描述符 */
    TupleTableSlot *ss_ScanTupleSlot;   /**< 扫描结果槽 */

    /* T8：行解码所需的列元数据（初始化时从 Relation 描述符提取，
     * ss_coltypes 在所属 EState 的查询上下文中分配） */
    int          ss_natts;           /**< 列数 */
    Oid         *ss_coltypes;        /**< 各列类型 OID（attnum 顺序） */
} SeqScanState;

/* ========================================================================
 * 公共 API
 * ======================================================================== */

/**
 * @brief 初始化 SeqScan 节点
 *
 * 分配 SeqScanState 并设置 ExecProcNode 函数指针；
 * scanrelid 有效时打开 Relation 并按其描述符准备结果槽。
 *
 * @param plan   计划节点（实际类型为 SeqScan*）
 * @param estate 执行器状态（内存取自 es_query_cxt）
 * @param eflags 执行器标志
 *
 * @return 初始化后的 SeqScanState（作为 PlanState*）；失败返回 NULL
 */
PlanState *ExecInitSeqScan(Plan *plan, EState *estate, int eflags);

/**
 * @brief SeqScan 节点执行函数
 *
 * 每次调用返回下一行（解码进 ss_ScanTupleSlot 的
 * tts_values/tts_isnull），扫描结束返回 NULL。
 *
 * @param pstate PlanState（实际类型为 SeqScanState）
 *
 * @return 结果元组槽；无更多元组时返回 NULL
 */
TupleTableSlot *ExecSeqScan(PlanState *pstate);

/**
 * @brief 结束 SeqScan 节点
 *
 * 结束扫描、关闭 Relation。状态内存由所属 EState 的查询上下文
 * 统一回收（FreeEState），本函数不 free 状态结构本身。
 *
 * @param node SeqScanState（可为 NULL）
 */
void ExecEndSeqScan(SeqScanState *node);

/**
 * @brief 重置 SeqScan 节点（用于重新扫描）
 *
 * 结束当前扫描描述符，下次 ExecSeqScan 重新 beginscan。
 *
 * @param node SeqScanState（可为 NULL）
 */
void ExecReScanSeqScan(SeqScanState *node);

/* ========================================================================
 * 行编解码（驱动层行格式，堆 blob 之上的类型化编码）
 *
 * 存储层（heapam）只保存不透明字节串，本格式是 SQL 驱动与其扫描算子
 * 之间的行编码约定：
 *   [u32 total_len][col0: u8 isnull][payload0][col1: u8 isnull][payload1]...
 * payload 按列类型 OID：
 *   21(int2)   : 2 字节小端
 *   23(int4)   : 4 字节小端
 *   20(int8)   : 8 字节小端
 *   25/1043/1042(text/varchar/char) : u32 字节数 + 原始字节
 * 其余类型：编码返回失败（调用方显式报错，不静默）。
 * ======================================================================== */

/**
 * @brief 计算编码后行长度
 *
 * text 类列的 value Datum 约定为 NUL 结尾的 char*。
 *
 * @return 编码长度；存在不支持的列类型返回 0
 */
size_t sql_row_encoded_size(int ncols, const Oid *coltypes,
                            const Datum *values, const bool *isnull);

/**
 * @brief 编码一行到缓冲区
 *
 * @return 0 成功（*out_len 为写入长度）；-1 失败（缓冲区不足或不支持的类型）
 */
int sql_row_encode(int ncols, const Oid *coltypes,
                   const Datum *values, const bool *isnull,
                   void *buf, size_t cap, size_t *out_len);

/**
 * @brief 解码一行到 Datum 数组
 *
 * text 类列的解码结果在 mcxt 中分配（NUL 结尾）。
 * blob 自带 total_len 前缀，解码严格按该长度做边界检查。
 *
 * @return 0 成功；-1 失败（长度非法/越界/不支持的类型）
 */
int sql_row_decode(int ncols, const Oid *coltypes,
                   const void *blob,
                   Datum *values, bool *isnull, MemoryContext mcxt);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_NODE_SEQSCAN_H */
