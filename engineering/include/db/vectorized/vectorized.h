/**
 * @file vectorized.h
 * @brief Gap#2 向量化执行引擎——列存批处理算子层（db_vectorized）
 *
 * 本库以 db_core 的 VectorBlock 为列存载体，算子以「块进块出」的方式组织：
 * 输入一个或多个 VectorBlock，经过滤/投影/聚合/连接等处理后输出新的 VectorBlock。
 *
 * 设计约定：
 * - 单线程、教学级实现，优先清晰可移植；
 * - 位图按 64 位字组织，第 i 行对应 word (i/64) 的 bit (i%64)；
 * - 不含 JIT / Arrow / 多线程并行（并行执行属 Gap#4）；
 * - SIMD 内核在后续任务（T2+）引入，本任务只落地骨架、类型标签、位图/gather 基础。
 */
#ifndef DB_VECTORIZED_VECTORIZED_H
#define DB_VECTORIZED_VECTORIZED_H

#include <stdint.h>
#include "db/core/vector_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 位图 / 选择向量基础操作
 * 位图按 64 位字组织，第 i 行对应 bit (i%64) of word (i/64)
 * ======================================================================== */

/**
 * @brief 统计位图 [0, nrows) 内置位行数
 * @param bm    位图（至少 ceil(nrows/64) 个字）
 * @param nrows 有效行数；超出 nrows 的置位不计
 */
int vecx_bitmap_count(const uint64_t *bm, int nrows);

/**
 * @brief 位图转选择向量：把置位行号按升序写入 sel_out
 * @param bm      位图
 * @param nrows   有效行数
 * @param sel_out 输出数组，容量须 >= nrows
 * @return 匹配（置位）行数
 */
int vecx_bitmap_to_selection(const uint64_t *bm, int nrows, int *sel_out);

/**
 * @brief 两个位图按字 AND / OR，结果写入 out（out 长度须 >= nwords）
 */
void vecx_bitmap_and(const uint64_t *a, const uint64_t *b, int nwords, uint64_t *out);
void vecx_bitmap_or (const uint64_t *a, const uint64_t *b, int nwords, uint64_t *out);

/* ========================================================================
 * 块压缩 / 收集
 * ======================================================================== */

/**
 * @brief 按选择向量深拷贝压缩出一个新块
 *
 * 新块行数为 nsel；逐列按 column_sizes 记录的元素大小 memcpy 选定行，
 * 复制每列类型标签，并把源块选中行的 null 标记重映射到新块对应位置。
 *
 * 所有权：返回的块拥有各自 malloc 的列缓冲，随 vector_block_destroy 释放；
 * 本函数不获取/释放 src 的任何内存。
 *
 * @param src  源块；为 NULL 返回 NULL
 * @param sel  选择向量（源块行号，升序不强制要求）；为 NULL 返回 NULL
 * @param nsel 选中行数；<= 0 返回 NULL
 * @return 新块（用 vector_block_destroy 释放），失败返回 NULL
 */
VectorBlock *vecx_block_gather(const VectorBlock *src, const int *sel, int nsel);

/* ========================================================================
 * SIMD 能力上报
 * ======================================================================== */

/**
 * @brief 返回本次运行实际分派到的 SIMD 内核名
 *
 * 取值："avx2" / "sse4.2" / "sse2" / "scalar"。
 * 结果与 db_core 的 vector_get_simd_type() 一致，二者同源于
 * simd_get_best_extension() 的运行时 CPU 检测（不是编译期硬编码）。
 *
 * @return 静态字符串常量，永不为 NULL，调用方不得释放
 */
const char *vecx_active_simd(void);

/* ========================================================================
 * 过滤算子（块进块出）
 *
 * 与 db_core 的 vector_filter_execute 的区别：
 * - vector_filter_execute 返回 VectorFilterResult（**原始行号**数组），供既有
 *   ANN 执行链（vector_query.c 的 exec_vector_filter）按行号自行 gather；
 * - 本层的 vecx_filter_* 返回**压缩后的新块**，是算子流水线的标准形态。
 * 两者语义不同，互不替代。
 * ======================================================================== */

/**
 * @brief 谓词：作用在单列上的一个比较条件
 *
 * 比较值按目标列的类型标签从下列字段中取用（调用方负责填对字段）：
 * - COLUMN_INT32  → i64（内部截断为 int32_t）
 * - COLUMN_INT64  → i64
 * - COLUMN_FLOAT  → f64（内部截断为 float）
 * - COLUMN_DOUBLE → f64
 * - COLUMN_STRING → str（以 NUL 结尾的 C 串，仅支持 CMP_EQ / CMP_NE）
 */
typedef struct vecx_pred_s {
    int col;             /**< 列索引 */
    CompareOp op;        /**< 比较操作符 */
    int64_t i64;         /**< 整型比较值 */
    double f64;          /**< 浮点比较值 */
    const char *str;     /**< 字符串比较值 */
} vecx_pred_t;

/**
 * @brief 单条件向量化过滤：位图内核 → 排除 null → 选择向量 → gather 出新块
 *
 * 内核按列类型标签分派到 db_core 的 vector_filter_*_simd（运行时 CPU 检测）。
 * null 行一律不匹配（先算比较位图，再与 ~null_bitmap 逐字 AND）。
 *
 * @param in    输入块（不被修改）
 * @param col   列索引
 * @param op    比较操作符
 * @param value 比较值指针，按列类型解释：
 *              COLUMN_INT32→const int32_t*，COLUMN_INT64→const int64_t*，
 *              COLUMN_FLOAT→const float*，COLUMN_DOUBLE→const double*，
 *              COLUMN_STRING→const char* const*（指向 C 串指针的指针）
 * @param out   输出块指针；无匹配时写入 NULL，有匹配时写入新块
 *              （由调用方用 vector_block_destroy 释放）
 * @return >0 匹配行数；0 无匹配（*out=NULL）；-1 入参非法 / 列类型未知或不支持
 */
int vecx_filter_block(const VectorBlock *in, int col, CompareOp op,
                      const void *value, VectorBlock **out);

/**
 * @brief 多条件向量化过滤：逐条件产位图后按 AND / OR 合并，再压缩出新块
 *
 * 条件可跨列、跨类型；合并在位图层完成，只做一次 gather。
 * nconds==1 时 is_and 取值不影响结果。null 行一律不匹配。
 *
 * @param in     输入块（不被修改）
 * @param conds  条件数组
 * @param nconds 条件数（须 > 0）
 * @param is_and 1=AND（全部条件同时满足）；0=OR（任一条件满足）
 * @param out    输出块指针；无匹配时写入 NULL
 * @return >0 匹配行数；0 无匹配（*out=NULL）；-1 入参非法 / 某条件的列类型不支持
 */
int vecx_filter_multi(const VectorBlock *in, const vecx_pred_t *conds,
                      int nconds, int is_and, VectorBlock **out);

/* ========================================================================
 * 标量聚合算子（整块/选择子集 → 单个标量）
 * ======================================================================== */

/** 聚合种类 */
typedef enum {
    VECX_AGG_COUNT = 0,  /**< 非 null 行数 */
    VECX_AGG_SUM,        /**< 求和 */
    VECX_AGG_MIN,        /**< 最小值 */
    VECX_AGG_MAX,        /**< 最大值 */
    VECX_AGG_AVG         /**< 平均值 = sum / count */
} vecx_agg_kind_t;

/**
 * @brief 在一个块的某数值列上做标量聚合，结果统一以 double 返回
 *
 * 语义（与 SQL 聚合一致）：
 * - 遍历范围：sel==NULL 时为 [0, b->num_rows)（此时忽略 nsel）；
 *   sel!=NULL 时为 sel[0..nsel-1] 指定的行号（可乱序、可不连续、可重复；
 *   聚合**不做去重**，同一行在 sel 里出现两次就被计入两次）。
 *   越界行号静默跳过，不算入参错误。
 * - null 行一律跳过，不参与任何聚合（包括 COUNT）。
 * - COUNT：结果 = 遍历范围内的非 null 行数。始终 *has_result=1，
 *   空集时 *out=0.0（SQL 的 COUNT 对空集返回 0 而不是 NULL）。
 * - SUM / MIN / MAX / AVG：无任何非 null 行时 has_result=0、*out=0.0
 *   （SQL 对空集返回 NULL）。有值时 has_result=1。
 * - MIN / MAX 用**首个有效值**做初值，不用 DBL_MAX/-DBL_MAX 哨兵，
 *   这样极值数据（含 ±inf）也能被正确返回。
 * - 整型列的 SUM / MIN / MAX 在 int64_t 上累加后再转 double，
 *   避免超过 2^53 的整数和被 double 静默截断。
 *
 * 支持的列类型标签（vector_block_set_column_type 设置过的）：
 * COLUMN_INT32 / COLUMN_INT64 / COLUMN_FLOAT / COLUMN_DOUBLE。
 * 未知(-1)、字符串及其它类型返回 -1（这是**入参错误**，不是"空结果"，
 * 不要与 has_result=0 混为一谈）。
 *
 * @param b          输入块（不被修改）
 * @param col        列索引
 * @param kind       聚合种类
 * @param sel        选择向量；NULL 表示整块
 * @param nsel       sel 的长度；sel==NULL 时忽略；sel 非 NULL 且 nsel<=0 是合法空集
 * @param out        结果输出（非 NULL）；has_result=0 时写 0.0
 * @param has_result 是否有结果（非 NULL）：1=有值，0=空集（SQL NULL）
 * @return 0 成功；-1 入参非法 / 列类型不支持
 */
int vecx_agg_scalar(const VectorBlock *b, int col, vecx_agg_kind_t kind,
                    const int *sel, int nsel, double *out, int *has_result);

/* ========================================================================
 * 哈希分组聚合（GROUP BY，单列 int64 键 + 单列数值度量）
 *
 * 教学级实现：开放寻址（线性探测）哈希表，支持跨多个块累积同一分组，
 * 最后一次性 emit 出结果块。不做溢出到磁盘、不做并行分区（Gap#4）。
 * ======================================================================== */

/** 分组聚合器（不透明句柄） */
typedef struct vecx_hashagg_s vecx_hashagg_t;

/**
 * @brief 创建分组聚合器
 *
 * @param key_col     分组键列索引；该列在每个输入块里的类型标签须为
 *                    COLUMN_INT32 或 COLUMN_INT64（内部统一提升为 int64 键）
 * @param measure_col 度量列索引；类型标签须为
 *                    COLUMN_INT32 / COLUMN_INT64 / COLUMN_FLOAT / COLUMN_DOUBLE
 * @return 聚合器；OOM 或列索引为负返回 NULL
 */
vecx_hashagg_t *vecx_hashagg_create(int key_col, int measure_col);

/**
 * @brief 把一个块的数据累积进聚合器（可对多个块反复调用）
 *
 * 逐行处理：键列为 null 的行整行跳过；度量列为 null 的行**也整行跳过**
 * （即该行既不建组也不计数——见下方"null 语义"一节，这是本实现的明确取舍）。
 *
 * @param h 聚合器
 * @param b 输入块（不被修改，也不被持有——本函数只读取，返回后调用方可自由释放）
 * @return 0 成功；-1 入参非法 / 列类型不支持 / OOM
 */
int vecx_hashagg_add_block(vecx_hashagg_t *h, const VectorBlock *b);

/**
 * @brief 输出聚合结果块
 *
 * 输出块共 6 列，行数 = distinct 分组数：
 *   列0 key   : int64_t，类型标签 COLUMN_INT64
 *   列1 count : int64_t，类型标签 COLUMN_INT64
 *   列2 sum   : double， 类型标签 COLUMN_DOUBLE
 *   列3 min   : double， 类型标签 COLUMN_DOUBLE
 *   列4 max   : double， 类型标签 COLUMN_DOUBLE
 *   列5 avg   : double， 类型标签 COLUMN_DOUBLE
 * 行顺序**不做保证**（哈希槽顺序）——测试必须自己按 key 排序或用 map 比对，
 * 不要依赖插入顺序。这一点要写进头注释。
 *
 * 没有任何分组时返回 0 且 *out=NULL（这是正常的空结果，不是错误）。
 * emit 不清空内部状态；重复调用应产出等价结果。
 *
 * @param h   聚合器
 * @param out 输出块指针；无分组时写 NULL，否则写新块（调用方用 vector_block_destroy 释放）
 * @return >0 分组数；0 无分组（*out=NULL）；-1 入参非法 / OOM
 */
int vecx_hashagg_emit(vecx_hashagg_t *h, VectorBlock **out);

/** @brief 销毁聚合器（NULL 安全） */
void vecx_hashagg_destroy(vecx_hashagg_t *h);

/* ========================================================================
 * 向量化 Hash Join（inner join，单列 int 等值连接键）
 *
 * 经典两阶段：先把 build 侧全部块灌进哈希表，再逐块 probe。
 * 教学级：不做 grace/溢出分区、不做 semi/anti/outer、不做多列复合键。
 * ======================================================================== */

/** Hash Join 句柄（不透明） */
typedef struct vecx_hashjoin_s vecx_hashjoin_t;

/**
 * @brief 创建 Hash Join
 * @param build_key_col build 侧连接键列索引
 * @param probe_key_col probe 侧连接键列索引
 *                      两侧键列类型标签须为 COLUMN_INT32 或 COLUMN_INT64
 *                      （内部统一提升为 int64 键，故两侧类型可不同）
 * @return 句柄；OOM 或列索引为负返回 NULL
 */
vecx_hashjoin_t *vecx_hashjoin_create(int build_key_col, int probe_key_col);

/**
 * @brief 灌入一个 build 侧块（可多次调用）
 *
 * 所有权：本函数对块做深拷贝并持有副本，因为 probe 阶段要回读 build 行的所有列，
 * 而调用方可能在 probe 前就释放了原块。副本在 vecx_hashjoin_destroy 时释放。
 * 调用方对传入的 `build` 保留所有权，返回后可自由释放。
 *
 * 键列为 null 的行不入表（SQL 语义：null 不与任何值相等，包括另一个 null）。
 * 非键列为 null 不影响入表——null 标记会随行一起带到输出块。
 *
 * 多个块的 schema（列数、各列 column_sizes、各列类型标签）**必须一致**，
 * 否则返回 -1（第一个块确立 schema）。
 *
 * @return 0 成功；-1 入参非法 / 键列类型不支持 / schema 不一致 / OOM
 */
int vecx_hashjoin_add_build(vecx_hashjoin_t *j, const VectorBlock *build);

/**
 * @brief 用一个 probe 侧块做 inner join，输出匹配结果块
 *
 * 输出块列布局：**build 侧全部列** 依次排在前，**probe 侧全部列** 依次排在后，
 * 即 ncols_out = build_ncols + probe_ncols；各列 column_sizes 与类型标签
 * 从对应侧原样继承。
 *
 * 语义：
 * - inner join：probe 行无匹配则丢弃；
 * - build 侧同键多行 → 该 probe 行与每个匹配的 build 行各产出一行（键内笛卡尔积），
 *   **同键多行的输出顺序不做保证**，测试须按 (build_key, 其它列) 排序或用 multiset 比对；
 * - probe 侧键 null 不参与探测；
 * - 输出行的 null 标记 = build 行 null || probe 行 null（行级位图的必然结果，
 *   即某行只要任一侧原本是 null，输出行整行被标 null）。
 *   **这是行级 null 位图的固有限制，要在头注释里写明**。
 * - 无匹配时返回 0 且 *out=NULL（正常空结果，不是错误）。
 *
 * @param j     句柄（须已 add_build 至少一次；未 build 过则所有行都无匹配，返回 0）
 * @param probe probe 侧块（不被修改、不被持有）
 * @param out   输出块指针；调用方用 vector_block_destroy 释放
 * @return >0 输出行数；0 无匹配（*out=NULL）；-1 入参非法 / 类型不支持 / OOM
 */
int vecx_hashjoin_probe(vecx_hashjoin_t *j, const VectorBlock *probe, VectorBlock **out);

/** @brief 销毁（释放哈希表与所有 build 侧块副本；NULL 安全） */
void vecx_hashjoin_destroy(vecx_hashjoin_t *j);

/* ========================================================================
 * 向量表达式与投影
 *
 * 表达式是二叉树：COL（取列） / CONST（常量） / ADD / SUB / MUL。
 * 求值方式：紧循环逐行 scalar 求值（教学级；编译器负责自动向量化；
 *   真实 JIT Codegen 留作未来优化点，已在头注释标注）。
 * ======================================================================== */

/** 表达式节点操作符 */
typedef enum {
    VEXPR_COL = 0,   /**< 取输入块某列的值（自动提升为 double） */
    VEXPR_CONST,     /**< 常量 double */
    VEXPR_ADD,       /**< 加法（双精度浮点） */
    VEXPR_SUB,       /**< 减法 */
    VEXPR_MUL        /**< 乘法 */
} vecx_expr_op_t;

/** 表达式节点（不透明） */
typedef struct vecx_expr_s vecx_expr_t;

/**
 * @brief 列引用表达式：取输入块第 col 列的值，自动将 int32/int64/float/double 提升为 double
 * @param col 列索引
 * @return 表达式节点（所有权归调用方，须用 vecx_expr_free 释放）
 */
vecx_expr_t *vecx_expr_col(int col);

/**
 * @brief 常量表达式
 * @param v 常量值
 */
vecx_expr_t *vecx_expr_const(double v);

/**
 * @brief 二元算术表达式（接管左右子节点所有权，调用方不需再 free 它们）
 * @param op 操作符
 * @param left  左操作数（可为 NULL 仅对 CONST 型有意义）
 * @param right 右操作数
 * @return 新节点
 */
vecx_expr_t *vecx_expr_bin(vecx_expr_op_t op, vecx_expr_t *left, vecx_expr_t *right);

/**
 * @brief 在输入块上求值表达式，结果追加为一列（输出块的列数 = 输入列数 + 1）
 *
 * 输出块前 b->num_columns 列与输入完全一致（deep copy，包括类型标签与 null）；
 * 最后一列是表达式结果列，类型标签为 COLUMN_DOUBLE（8），column_size 为 sizeof(double)。
 * 第 b->num_columns 列（结果列）的 null 标记 = 输入块该行任一输入列为 null → 结果 null。
 *
 * @param in  输入块（不被修改）
 * @param e   表达式（树的所有权归调用方，求值过程不释放节点）
 * @param out 输出块指针；调用方用 vector_block_destroy 释放
 * @return 0 成功；-1 入参非法 / 列越界 / OOM
 */
int vecx_project(const VectorBlock *in, const vecx_expr_t *e, VectorBlock **out);

/**
 * @brief 释放表达式树（递归释放所有子节点；NULL 安全）
 */
void vecx_expr_free(vecx_expr_t *e);

/* ========================================================================
 * 内存列 Source：把一组列向量切成 VectorBlock 流
 *
 * 用于驱动算子流水线——给 filter / agg / join 提供块输入。
 * ======================================================================== */

/** Source 句柄（不透明） */
typedef struct vecx_source_s vecx_source_t;

/**
 * @brief 从列数组创建 Source（内存数据驱动）
 *
 * col_data[i] 指向第 i 列的基地址，col_elem_size[i] 是每行该列的元素大小（字节）。
 * 若 col_types[i] != -1（已设置类型标签），source 按类型化列处理；
 * 若为 -1，则按 raw 字节数组处理（用于无法确定类型的场景）。
 *
 * source 持有 col_data 指针的**副本**（仅拷贝指针数组，底层的列缓冲不拷贝），
 * 调用方在 destroy 前须保持 col_data 缓冲有效。
 *
 * @param ncols         列数
 * @param col_types     每列类型标签（COLUMN_INT32/INT64/FLOAT/DOUBLE/-1）；可 NULL→全 -1
 * @param col_data      每列数据指针数组
 * @param col_elem_size 每列每行元素大小（字节）
 * @param total_rows    总行数
 * @param batch_size    每个 VectorBlock 的行数上限
 * @return source 句柄；OOM / 参数非法返回 NULL
 */
vecx_source_t *vecx_source_from_columns(int ncols, const int *col_types,
                                        const void **col_data, const int *col_elem_size,
                                        int64_t total_rows, int batch_size);

/**
 * @brief 从行迭代器创建 Source（演示火山模型行→列适配器）
 *
 * row_next 每被调用一次填充 col_values[col_idx]（按 col_elem_size[col_idx] 字节的标量槽）、
 * col_types[col_idx]（类型标签）、isnull[col_idx]（0/1）。
 * 若 hint_rows > 0，source 用它估算所需批次以减少 realloc，但这是 hint 不是 guarantee。
 *
 * source 按 batch_size 切 VectorBlock，每次 next 返回一个块（调用方负责用 vector_block_destroy）。
 * NULL 返回表示数据结束。
 *
 * @param ncols         列数
 * @param col_types     每列类型标签
 * @param col_elem_size 每列每行元素大小（字节）
 * @param row_next      迭代回调；返回 1 继续，0 停止，<0 错误
 * @param state         迭代器状态指针（透传给 row_next）
 * @param batch_size    每个 VectorBlock 的行数上限
 * @param hint_rows     行数提示（0 表示未知）
 * @return source 句柄；OOM 返回 NULL
 */
vecx_source_t *vecx_source_from_rows(int ncols, const int *col_types,
                                     const int *col_elem_size,
                                     int (*row_next)(void *state, int ncols,
                                                     void **col_values, int *col_types, char *isnull),
                                     void *state, int batch_size, int64_t hint_rows);

/**
 * @brief 取下一块（返回 NULL 表示结束；调用方用 vector_block_destroy 释放返回的块）
 */
VectorBlock *vecx_source_next(vecx_source_t *s);

/** @brief 销毁 source（不释放 col_data 底层缓冲；NULL 安全） */
void vecx_source_destroy(vecx_source_t *s);

/* ========================================================================
 * 端到端流水线演示（不重写 nodeSeqscan 等火山节点）
 *
 * 以下是"薄驱动"接口，演示把 source → filter → agg 串成流水线。
 * 不做通用计划解析器、不引入 SQL 层依赖。
 * ======================================================================== */

/**
 * @brief 把 source 的块依次经过 filter 和聚合，产出标量结果
 *
 * source 每产出一个块就调 vecx_filter_block 过滤，然后调 vecx_agg_scalar 累加入 total。
 * 最后返回 total（若有输出行）除以 block_count 得到 avg_rows_per_block。
 * 实际上这只是**一个示范**，展示如何把 source 与既有算子组合；
 * 调用方可以按自己的需要组合 source 与 filter/agg/hashjoin。
 *
 * @param s        source 句柄
 * @param filter_col 过滤列索引（-1 表示不过滤）
 * @param filter_op   过滤比较符
 * @param filter_val  过滤值指针
 * @param agg_col     聚合列索引
 * @param agg_kind    聚合种类
 * @param out         输出标量结果（sum/avg 等）
 * @param has_result  是否有结果
 * @return 0 成功；-1 入参非法 / 算子错误
 */
int vecx_pipeline_filter_agg(vecx_source_t *s, int filter_col, CompareOp filter_op,
                             const void *filter_val, int agg_col, vecx_agg_kind_t agg_kind,
                             double *out, int *has_result);

/* ========================================================================
 * MinMax/Zone-map 块裁剪（函数级谓词下推）
 *
 * MinMaxIndex：对一个块的指定列扫描非 null 行，记录 [min, max]。
 * ZoneMapSkip：根据比较谓词与区间 [lo, hi] 判断"该 granule 必然无匹配 → 可跳过"。
 *
 * 这两个函数供 Zone-map scan 驱动（如 `vecx_prune_source_next`）或手撸的查询引擎调用，
 * 不绑定特定 source 接口。
 * ======================================================================== */

/**
 * @brief 扫描块的非 null 行，返回列的 [min, max]
 *
 * 遍历 col 列的非 null 行，比较取最小/最大值（用 int64_t 做整型比较，
 * double 做浮点比较）。若全列为 null（count=0），返回 -1 且 *lo / *hi 内容未定义。
 *
 * @param b    输入块
 * @param col  列索引
 * @param lo   最小值输出
 * @param hi   最大值输出
 * @return 0 成功；-1 入参非法 / 列类型不支持 / 列为 null
 */
int vecx_block_minmax_i64(const VectorBlock *b, int col, int64_t *lo, int64_t *hi);

/**
 * @brief 浮点版 MinMax
 *
 * 支持 COLUMN_FLOAT / COLUMN_DOUBLE。±inf / NaN 按 C 语义比较
 *（NaN 既不大于也不小于任何值，故含 NaN 的块 lo/hi 会略偏离直观）。
 */
int vecx_block_minmax_f64(const VectorBlock *b, int col, double *lo, double *hi);

/**
 * @brief Zone-map 跳过判断
 *
 * 给定一个 granule 的 [lo, hi] 区间和一个标量谓词 (op, v)，
 * 如果该 granule **必然没有任何行满足**谓词，返回 1（可跳过）；
 * 如果**可能有行满足**，返回 0（必须扫描）。
 *
 * 区间逻辑（充分条件，非必要）：
 * | op  | 跳过条件（可跳过）                         |
 * | EQ  | v < lo || v > hi                          |
 * | NE  | 永远不会跳过（v!=lo || v!=hi 可能等于其他值）|
 * | LT  | lo >= v                                    |
 * | LE  | lo > v                                     |
 * | GT  | hi <= v                                    |
 * | GE  | hi < v                                     |
 *
 * 注意：此函数**不考虑 null**。如果 granule 里含 null，
 * null 值是否满足谓词取决于 SQL 语义（通常 null 不满足任何比较），
 * 调用方应在调用前自行判断，或结合 null 位图做更精细的判断。
 *
 * @return 1 必无匹配可跳过；0 可能有匹配须扫描
 */
int vecx_zonemap_skip_i64(int64_t lo, int64_t hi, CompareOp op, int64_t v);
int vecx_zonemap_skip_f64(double lo, double hi, CompareOp op, double v);

#ifdef __cplusplus
}
#endif

#endif /* DB_VECTORIZED_VECTORIZED_H */
