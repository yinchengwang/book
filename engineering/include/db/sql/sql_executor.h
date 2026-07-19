/**
 * @file sql_executor.h
 * @brief 查询执行器头文件
 *
 * 实现 Volcano 迭代器模型：
 * 1. 基础算子（SeqScan/IndexScan/Filter）
 * 2. 连接算子（HashJoin/NestedLoop）
 * 3. 聚合算子（HashAgg/SortAgg）
 * 4. 修改算子（Insert/Update/Delete）
 */
#ifndef DB_SQL_EXECUTOR_H
#define DB_SQL_EXECUTOR_H

#include <stdint.h>
#include <stdbool.h>
#include <db/parser/sql/parsenodes.h>  /* 引入真实 List 定义 */

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 执行器类型和常量
 * ======================================================================== */

/** 执行器节点类型 */
typedef enum ExecutorType_e {
    /* 扫描算子 */
    EXEC_SEQ_SCAN,          /**< 顺序扫描 */
    EXEC_INDEX_SCAN,        /**< 索引扫描 */
    EXEC_INDEX_ONLY_SCAN,   /**< 仅索引扫描 */
    EXEC_BITMAP_SCAN,       /**< 位图扫描 */
    EXEC_TID_SCAN,          /**< TID 扫描 */
    EXEC_SUBQUERY_SCAN,     /**< 子查询扫描 */
    EXEC_FUNCTION_SCAN,     /**< 函数扫描 */
    EXEC_VALUES_SCAN,       /**< VALUES 扫描 */
    EXEC_CTE_SCAN,          /**< CTE 扫描 */
    EXEC_MATERIAL_SCAN,     /**< 物化扫描 */
    EXEC_MVIEW_SCAN,        /**< 物化视图扫描 */
    EXEC_VECTOR_SCAN,       /**< 向量扫描 */
    EXEC_HNSW_SCAN,        /**< HNSW 扫描 */
    EXEC_IVF_SCAN,         /**< IVF 扫描 */
    EXEC_BM25_SCAN,        /**< BM25 扫描 */
    EXEC_TIMESERIES_SCAN,  /**< 时序扫描 */
    EXEC_GRAPH_SCAN,        /**< 图扫描 */
    EXEC_DOCUMENT_SCAN,     /**< 文档扫描 */
    EXEC_SPATIAL_SCAN,      /**< 空间扫描 */
    EXEC_DATALOG_SCAN,      /**< Datalog 扫描 */
    EXEC_YANG_SCAN,         /**< Yang 扫描 */
    EXEC_STREAM_SCAN,       /**< 流扫描 */
    EXEC_STREAM_WINDOW,     /**< 流窗口 */
    EXEC_STREAM_JOIN,       /**< 流连接 */
    EXEC_STREAM_AGG,        /**< 流聚合 */

    /* 连接算子 */
    EXEC_NESTLOOP,          /**< 嵌套循环连接 */
    EXEC_MERGEJOIN,         /**< 归并连接 */
    EXEC_HASHJOIN,         /**< Hash 连接 */

    /* 聚合算子 */
    EXEC_SORT,              /**< 排序 */
    EXEC_HASHAGG,          /**< Hash 聚合 */
    EXEC_SORTAGG,          /**< 排序聚合 */
    EXEC_GROUPAGG,          /**< 分组聚合 */
    EXEC_WINDOW,            /**< 窗口函数 */
    EXEC_UNIQUE,            /**< 去重 */
    EXEC_SETOP,             /**< 集合操作 */

    /* 修饰算子 */
    EXEC_LIMIT,             /**< 限制 */
    EXEC_OFFSET,            /**< 偏移 */
    EXEC_PROJECT,           /**< 投影 */
    EXEC_FILTER,            /**< 过滤 */
    EXEC_DISTINCT,          /**< DISTINCT */
    EXEC_DISTINCTFILTER,   /**< DISTINCT 过滤 */
    EXEC_DISTINCTORDER,    /**< DISTINCT 排序 */
    EXEC_MATERIALIZE,       /**< 物化 */
    EXEC_SORTREVERSE,      /**< 逆序排序 */
    EXEC_GROUPSORT,        /**< 分组排序 */

    /* 修改算子 */
    EXEC_INSERT,            /**< 插入 */
    EXEC_UPDATE,            /**< 更新 */
    EXEC_DELETE,            /**< 删除 */
    EXEC_MODIFYTABLE,       /**< 修改表 */
    EXEC_VACUUM,           /**< 清理 */
    EXEC_REFRESH_MVIEW,    /**< 刷新物化视图 */
    EXEC_ANALYZE,           /**< 分析 */

    /* 控制算子 */
    EXEC_RESULT,            /**< 结果 */
    EXEC_RETURN,            /**< 返回 */
    EXEC_FAKE_TUPLES,      /**< 假元组 */
    EXEC_RECURSIVEUNION,   /**< 递归联合 */
    EXEC_WORKTABLE,         /**< 工作表 */
    EXEC_EXPLAIN            /**< EXPLAIN */
} ExecutorType;

/** 聚合策略 */
typedef enum AggStrategy_e {
    AGG_HASHED,            /**< Hash 聚合 */
    AGG_SORTED,            /**< 排序聚合 */
    AGG_MIXED,             /**< 混合聚合 */
    AGG_PLAIN,             /**< 普通聚合 */
    AGG_SORTED_INT,        /**< 有序内含聚合 */
    AGG_HASHED_INT,        /**< 有序内含 Hash 聚合 */
    AGG_HASHED_ALL,        /**< 全 Hash 聚合 */
    AGG_MIXED_ALL          /**< 混合全部聚合 */
} AggStrategy;

/** 连接策略 */
typedef enum JoinStrategy_e {
    JOIN_NESTLOOP,          /**< 嵌套循环 */
    JOIN_MERGE,            /**< 归并连接 */
    JOIN_HASH,             /**< Hash 连接 */
    JOIN_LOOP              /**< 循环连接 */
} JoinStrategy;

/* ========================================================================
 * 元组和槽
 * ======================================================================== */

/** 虚拟元组描述符（使用 ExecTupleDesc 避免与 db/rel.h 的 TupleDesc 冲突） */
typedef struct ExecTupleDesc_s {
    int natts;              /**< 属性数量 */
    struct AttInhData_s {
        int attnum;         /**< 属性编号 */
        const char *name;   /**< 属性名 */
        int type;           /**< 类型 OID */
        int len;            /**< 类型长度 */
        int typmod;         /**< 类型修饰符 */
        bool isnull;        /**< 是否为空 */
    } *attrs;
} ExecTupleDesc;

/** 元组槽 */
typedef struct TupleTableSlot_s {
    ExecTupleDesc *tts_tupleDescriptor; /**< 元组描述符 */
    void **tts_values;         /**< 属性值数组 */
    bool *tts_isnull;         /**< NULL 标志数组 */
    int tts_nvalid;          /**< 有效属性数 */
    struct {
        void *data;          /**< 元组数据 */
        int len;             /**< 数据长度 */
    } tts_tuple;
    enum {
        TTS_VIRTUAL,         /**< 虚拟元组 */
        TTS_PHYSICAL         /**< 物理元组 */
    } tts_type;
} TupleTableSlot;

/* ========================================================================
 * 元组描述符操作
 * ======================================================================== */

/**
 * @brief 创建元组描述符
 */
ExecTupleDesc *exec_make_tuple_desc(int natts);

/**
 * @brief 销毁元组描述符
 */
void exec_drop_tuple_desc(ExecTupleDesc *desc);

/**
 * @brief 创建元组槽
 */
TupleTableSlot *exec_make_tuple_slot(ExecTupleDesc *desc);

/**
 * @brief 销毁元组槽
 */
void exec_drop_tuple_slot(TupleTableSlot *slot);

/**
 * @brief 复制元组槽
 */
TupleTableSlot *exec_copy_tuple_slot(const TupleTableSlot *src);

/**
 * @brief 清空元组槽
 */
void exec_clear_tuple_slot(TupleTableSlot *slot);

/**
 * @brief 获取元组槽中的值
 */
void *slot_get_value(TupleTableSlot *slot, int attnum, int *len);

/**
 * @brief 设置元组槽中的值
 */
void slot_set_value(TupleTableSlot *slot, int attnum, const void *value, int len);

/**
 * @brief 检查元组槽是否为 NULL
 */
bool slot_attisnull(const TupleTableSlot *slot, int attnum);

/**
 * @brief 设置元组槽的 NULL 标志
 */
void slot_set_null(TupleTableSlot *slot, int attnum);

/* ========================================================================
 * 表达式上下文
 * ======================================================================== */

/** 表达式上下文 */
typedef struct ExprContext_s {
    TupleTableSlot **scandesc;   /**< 扫描描述符 */
    int numscans;                 /**< 扫描数量 */
    TupleTableSlot *slot;        /**< 当前元组槽 */
    TupleTableSlot **result;      /**< 结果槽 */
    void *scan_data;             /**< 扫描数据 */
    void **projection_context;   /**< 投影上下文 */
    int projection_depth;         /**< 投影深度 */
} ExprContext;

/**
 * @brief 创建表达式上下文
 */
ExprContext *exec_create_expr_context(void);

/**
 * @brief 销毁表达式上下文
 */
void exec_destroy_expr_context(ExprContext *ctx);

/**
 * @brief 推送新的表达式上下文
 */
void exec_push_expr_context(ExprContext *ctx);

/**
 * @brief 弹出表达式上下文
 */
void exec_pop_expr_context(ExprContext *ctx);

/* ========================================================================
 * 执行器节点
 * ======================================================================== */

/** 执行器节点基类 */
typedef struct PlanState_s {
    ExecutorType type;            /**< 节点类型 */
    struct PlanState_s *left;    /**< 左子节点 */
    struct PlanState_s *right;   /**< 右子节点 */
    ExprContext *expr_context;    /**< 表达式上下文 */
    ExecTupleDesc *ps_TupDesc;       /**< 元组描述符 */
    TupleTableSlot *(*exec_proc) (struct PlanState_s *); /**< 执行函数 */
    void *state;                /**< 内部状态 */
} PlanState;

/** 扫描状态 */
typedef struct ScanState_s {
    PlanState ps;
    void *scanrelid;            /**< 扫描的 Relation ID */
    void *currentScanBuf;       /**< 当前扫描缓冲区 */
    long killed;                 /**< 已杀死的元组数 */
    bool scan;                   /**< 是否正在扫描 */
} ScanState;

/** 基表扫描状态 */
typedef struct SeqScanState_s {
    ScanState ss;
    void *mview;                /**< 物化视图 */
} SeqScanState;

/** 索引扫描状态 */
typedef struct IndexScanState_s {
    ScanState ss;
    void *indexScanDesc;        /**< 索引扫描描述符 */
    List *indexqual;            /**< 索引条件 */
    List *indexorderby;         /**< 索引排序条件 */
    List *indexorderbyops;      /**< 索引排序操作符 */
    List *recheckqual;          /**< 重新检查条件 */
} IndexScanState;

/** 向量扫描状态 */
typedef struct VectorScanState_s {
    ScanState ss;
    float *query_vector;        /**< 查询向量 */
    int top_k;                  /**< Top-K */
    float **results;            /**< 结果向量 */
    float *distances;           /**< 距离 */
    int nresults;               /**< 结果数 */
    void *index;               /**< 索引句柄 */
    int distance_metric;        /**< 距离度量：0=L2, 1=IP, 2=COSINE */
    int ef;                     /**< HNSW ef 搜索参数（复用） */
    int nprobe;                 /**< IVF 搜索探针数（复用） */
} VectorScanState;

/** HNSW 扫描状态 */
typedef struct HnswScanState_s {
    VectorScanState vss;
    int ef;                     /**< 搜索参数 */
    int m;                      /**< HNSW M 连接数 */
} HnswScanState;

/** IVF 扫描状态 */
typedef struct IvfScanState_s {
    VectorScanState vss;
    int nprobe;                 /**< IVF 搜索探针数 */
    int nlist;                  /**< IVF 聚类数 */
} IvfScanState;

/** Datalog 扫描状态（与 executor/datalog/datalog_scan.h 一致） */
typedef struct DatalogScanState_s {
    PlanState ps;
    void *rule_set;
    void *edb;
    void *idb;
    int   max_iter;   /**< 最大迭代次数 */
} DatalogScanState;

/** Yang 扫描状态（与 executor/yang/yang_scan.h 一致） */
typedef struct YangScanState_s {
    PlanState ps;
    void *yang_path;
    void *xpath_filter;
} YangScanState;

/** 流扫描状态 */
typedef struct StreamScanState_s {
    PlanState ps;
    void *stream_oid;
    int64_t watermark;
    int batch_size;
} StreamScanState;

/** 流窗口状态 */
typedef struct StreamWindowState_s {
    PlanState ps;
    int64_t window_size;
    int64_t slide;
    int window_type;
} StreamWindowState;

/** 流连接状态 */
typedef struct StreamJoinState_s {
    PlanState ps;
    int64_t interval;
    int join_type;
} StreamJoinState;

/** 流聚合状态 */
typedef struct StreamAggState_s {
    PlanState ps;
    int agg_type;
    int64_t window_size;
    void *acc_state;
} StreamAggState;

/** 连接状态 */
typedef struct JoinState_s {
    PlanState ps;
    JoinStrategy join_type;      /**< 连接类型 */
    List *joinqual;             /**< 连接条件 */
    TupleTableSlot *jointuple;  /**< 连接元组 */
    bool matched_outer;         /**< 是否匹配外层 */
} JoinState;

/** Hash 连接状态 */
typedef struct HashJoinState_s {
    JoinState js;
    void *hashtable;            /**< Hash 表 */
    List *hashclauses;          /**< Hash 条件 */
    void *hashvalue;            /**< Hash 值 */
    TupleTableSlot *outerbatchtuple;/**< 外批次元组 */
    int nbatch;                  /**< 批次数 */
    int ibatch;                  /**< 当前批次 */
    int ibuckets;               /**< 桶数 */
} HashJoinState;

/** 嵌套循环状态 */
typedef struct NestLoopState_s {
    JoinState js;
    PlanState *louter;          /**< 外循环 */
    PlanState *rinner;          /**< 内循环 */
    bool need_recheck;          /**< 需要重新检查 */
    List *nl_recheck_quals;     /**< 重新检查条件 */
} NestLoopState;

/** 聚合状态 */
typedef struct AggState_s {
    PlanState ps;
    AggStrategy aggstrategy;     /**< 聚合策略 */
    int num_group_cols;          /**< 分组列数 */
    int *groupColIdx;           /**< 分组列索引 */
    int numaggs;                 /**< 聚合函数数 */
    List *aggs;                 /**< 聚合函数 */
    TupleTableSlot *firsttuple; /**< 第一个元组 */
    void *hashcontext;          /**< Hash 上下文 */
    void *sortcontext;          /**< 排序上下文 */
    long input_total;            /**< 输入总数 */
    long input_used;            /**< 已用输入 */
    TupleTableSlot *hashslot;   /**< Hash 槽 */
    TupleTableSlot *hashslot2;  /**< Hash 槽 2 */
} AggState;

/** 投影状态 */
typedef struct ProjectionInfo_s {
    ExprContext *proj_context;   /**< 投影上下文 */
    int numslots;                 /**< 槽数量 */
    TupleTableSlot **slots;       /**< 槽数组 */
    void *slots_Fatron;          /**< 计算槽 */
} ProjectionInfo;

typedef struct ProjectState_s {
    PlanState ps;
    ProjectionInfo *projinfos;   /**< 投影信息数组 */
    int numinfos;                 /**< 投影信息数量 */
    ExprContext *proj_ctx;       /**< 投影上下文 */
} ProjectState;

/** 过滤状态 */
typedef struct FilterState_s {
    PlanState ps;
    List *qual;                 /**< 过滤条件 */
    TupleTableSlot *result;     /**< 结果槽 */
    bool is_ignore;             /**< 是否忽略 */
    bool is_instrument;         /**< 是否已检测 */
} FilterState;

/** 排序状态 */
typedef struct SortState_s {
    PlanState ps;
    void *tuplesort;           /**< 元组排序器 */
    bool randomAccess;          /**< 随机访问 */
    bool bounded;               /**< 有界 */
    bool nulls_first;           /**< NULL 优先 */
    int64_t limit;               /**< 限制 */
    bool sort_done;             /**< 排序完成 */
} SortState;

/** LIMIT 状态 */
typedef struct LimitState_s {
    PlanState ps;
    int64_t offset;               /**< 偏移 */
    int64_t count;               /**< 限制数 */
    int64_t offset_count;        /**< 偏移计数 */
    int64_t count_count;         /**< 限制计数 */
    bool no_count;              /**< 无计数 */
    bool need_to_scan;          /**< 需要扫描 */
    TupleTableSlot *slot;       /**< 当前槽 */
} LimitState;

/** 修改状态 */
typedef struct ModifyTableState_s {
    PlanState ps;
    ExecutorType operation;     /**< 操作类型 */
    int nominalReltable;        /**< 名义关系表 */
    List *resultRelInfo;       /**< 结果关系信息 */
    int n_result_relations;     /**< 结果关系数 */
    int root_result_relations;  /**< 根结果关系 */
    List *plans;               /**< 子计划列表 */
    TupleTableSlot *mt_outertuple;/**< 外元组 */
    bool can_set_tag;          /**< 可以设置标签 */
} ModifyTableState;

/** 结果状态 */
typedef struct ResultState_s {
    PlanState ps;
    List *resconstantqual;     /**< 常量条件 */
    bool return_single_tuple;   /**< 返回单条元组 */
    TupleTableSlot *slot;       /**< 当前槽 */
} ResultState;

/* ========================================================================
 * 执行器
 * ======================================================================== */

/** 执行器状态 */
typedef enum ExecutorRunDirection_e {
    EXEC_RUN_FORWARD,          /**< 前向 */
    EXEC_RUN_BACKWARD,         /**< 后向 */
    EXEC_RUN_MARK,             /**< 标记 */
    EXEC_RUN_RESTORE           /**< 恢复 */
} ExecutorRunDirection;

/** 执行器全局状态 */
typedef struct ExecutorGlobal_s {
    void *estate;              /**< 执行器状态 */
    void *snapshot;           /**< 快照 */
    void *crosscheck_snapshot;/**< 交叉检查快照 */
    bool use_parallel_mode;    /**< 使用并行模式 */
    bool retrieve_tuples;       /**< 检索元组 */
    long count;                /**< 计数 */
    bool forward;              /**< 前向 */
} ExecutorGlobal;

/** 扫描方向（与 rel.h 一致） */
#ifndef ScanDirectionDefined
#define ScanDirectionDefined
typedef enum ScanDirection_e {
    ForwardScanDirection = 0,   /**< 正向扫描 */
    BackwardScanDirection = 1   /**< 反向扫描 */
} ScanDirection;
#endif

/** 执行器结果 */
typedef struct ExecutorResult_s {
    void *tupleStore;          /**< 元组存储 */
    ScanDirection direction;   /**< 扫描方向 */
    long count;                /**< 返回元组数 */
} ExecutorResult;

/**
 * @brief 创建执行器
 */
void *executor_create(void);

/**
 * @brief 销毁执行器
 */
void executor_destroy(void *exec);

/**
 * @brief 初始化执行器
 */
void executor_init(void *exec, void *plan, void *params);

/**
 * @brief 开始执行
 */
void executor_start(void *exec, void *plan, void *params);

/**
 * @brief 执行查询
 *
 * @param exec 执行器
 * @param plan 物理计划
 * @param direction 扫描方向
 * @param count 最大返回元组数
 * @param dest 结果目标
 * @return 返回的元组数
 */
long executor_run(void *exec, void *plan,
                 ScanDirection direction, long count, void *dest);

/**
 * @brief 继续执行
 */
bool executor_continue(void *exec);

/**
 * @brief 结束执行
 */
void executor_finish(void *exec);

/**
 * @brief 取消执行
 */
void executor_cancel(void *exec);

/* ========================================================================
 * 算子执行函数（已迁移到 nodeXxx.c 中真实实现）
 *
 * 原桩函数（exec_seq_scan, exec_vector_scan, exec_hnsw_scan,
 * exec_nestloop, exec_hashjoin, exec_hash_agg, exec_sort_agg,
 * exec_sort, exec_project, exec_filter, exec_limit,
 * exec_insert, exec_update, exec_delete, exec_result）
 * 已删除，参见：
 *   - nodeSeqscan.h / nodeIndexscan.h / nodes/nodeNestloop.h
 *   - nodeHashjoin.h / nodeSort.h / nodeHashagg.h
 *   - nodeLimit.h / nodeModifyTable.h / nodeResult.h
 *   - nodeProjectSet.h
 * ======================================================================== */

/**
 * @brief 执行索引扫描（实现位于 nodeIndexscan.c）
 */
TupleTableSlot *ExecIndexScan(IndexScanState *node);

/* ========================================================================
 * 表达式求值（已迁移到 executor 框架）
 *
 * 原桩函数（eval_expr, eval_const_expr, eval_func, eval_agg,
 * eval_window_func）已删除，框架中表达式求值由 executor.c
 * 通过 PlanState->ExecProcNode 调度。
 * ======================================================================== */

/* 表达式相关辅助函数（保留） */

/**
 * @brief 求值 CASE 表达式
 */
void *eval_case(void *caseexpr, ExprContext *ctx);

/**
 * @brief 求值 IN 表达式
 */
bool eval_in(void *in, void *expr, ExprContext *ctx);

/**
 * @brief 求值 NULL 测试
 */
bool eval_nulltest(void *expr, bool is_not_null, ExprContext *ctx);

/**
 * @brief 求值布尔表达式
 */
bool eval_bool_expr(void *expr, ExprContext *ctx);

/* ========================================================================
 * 结果集管理
 * ======================================================================== */

/** 结果集目标类型 */
typedef enum DestReceiverType_e {
    DEST_TUPLES,               /**< 虚拟元组 */
    DEST_EXTERNAL_FILE,        /**< 外部文件 */
    DEST_DISTRIBUTOR,          /**< 分发器 */
    DEST_DISTMATRIX,           /**< 分发矩阵 */
    DEST_NONE,                 /**< 无 */
    DEST_TODTC,                /**< 到 DTC */
    DEST_TO_FILE_Buf,         /**< 到文件缓冲 */
    DEST_COOKED                /**< 熟数据 */
} DestReceiverType;

/** 结果接收器 */
typedef struct DestReceiver_s {
    DestReceiverType dest;     /**< 目标类型 */
    void *receive_slot;       /**< 接收槽函数 */
    void *rDestroy;           /**< 销毁函数 */
    void *state;              /**< 状态 */
} DestReceiver;

/**
 * @brief 创建虚拟元组结果接收器
 */
DestReceiver *CreateDestReceiver(DestReceiverType type);

/**
 * @brief 接收元组
 */
void ReceiveTuple(DestReceiver *self, TupleTableSlot *slot);

/**
 * @brief 销毁结果接收器
 */
void DestroyDestReceiver(DestReceiver *self);

/**
 * @brief 创建元组存储
 */
void *CreateTupleStore(void);

/**
 * @brief 将元组存入存储
 */
void tuple_store_put(void *store, TupleTableSlot *slot);

/**
 * @brief 从存储获取元组
 */
TupleTableSlot *tuple_store_get(void *store, bool forward);

/**
 * @brief 关闭元组存储
 */
void tuple_store_close(void *store);

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 比较两个元组
 */
int tuple_compare(const void *a, const void *b, void *arg);

/**
 * @brief 复制元组
 */
void *tuple_copy(const void *src, int len);

/**
 * @brief 分配元组
 */
void *tuple_alloc(int len);

/**
 * @brief 释放元组
 */
void tuple_free(void *tuple);

/**
 * @brief 获取元组描述符
 */
ExecTupleDesc *get_cached_rowtype(int type_id, int typlevel);

/**
 * @brief 释放元组描述符缓存
 */
void ReleaseCachedRowtype(ExecTupleDesc *desc);

/* ========================================================================
 * 调试
 * ======================================================================== */

/**
 * @brief 打印执行器状态
 */
void executor_dump_state(PlanState *state);

/**
 * @brief 打印元组
 */
void print_tuple(const TupleTableSlot *slot);

/**
 * @brief 打印元组内容
 */
void print_tuple_data(const ExecTupleDesc *desc, const void *data);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_EXECUTOR_H */
