/**
 * @file memctx.h
 * @brief 内存上下文子系统
 *
 * 实现 AllocSet 内存分配器，支持 palloc/pfree/reset API。
 * 为 SQL 执行引擎提供内存生命周期管理，避免每次小分配都直接调用 malloc/free。
 *
 * AllocSet 设计要点：
 * 1. 按块（Block）批量向系统申请内存，再在块内做小对象分配
 * 2. 父子层级：reset 时只释放子上下文块，保留当前块；delete 时释放全部块
 * 3. 8 字节对齐：所有分配地址按 8 字节对齐
 * 4. pfree 为空操作：释放由 reset/delete 统一完成（典型 PostgreSQL 风格）
 */

#ifndef DB_SQL_MEMCTX_H
#define DB_SQL_MEMCTX_H

#include <stddef.h>
#include <stdbool.h>

/* 复用 parser 层的公共 NodeTag（其中已扩展 T_MemoryContext / T_AllocSetContext），
 * 避免另建独立枚举值域造成契约偏离与潜在冲突。 */
#include "db/parser/sql/parsenodes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 基本类型定义
 * ======================================================================== */

/** 字节大小类型 */
typedef unsigned long Size;

/** 默认块大小（8KB） */
#define ALLOCSET_DEFAULT_BLOCK_SIZE 8192

/** 最小块大小（含 AllocBlock 头） */
#define ALLOCSET_MIN_BLOCK_SIZE 1024

/** 内存对齐基数 */
#define ALLOCSET_ALIGNMENT 8

/** 对齐宏：向上对齐到 8 字节 */
#define ALLOCSET_ALIGN(size) \
    (((size) + (ALLOCSET_ALIGNMENT - 1)) & ~(Size)(ALLOCSET_ALIGNMENT - 1))

/** Size 类型可表示的最大值（用于溢出检查） */
#define ALLOCSET_MAX_SIZE (~(Size)0)

/* ========================================================================
 * 内存上下文方法表
 * ======================================================================== */

/* 前向声明 */
typedef struct MemoryContextData *MemoryContext;

/**
 * @brief 内存上下文方法表
 *
 * 多态分派表：不同的分配器实现可注册自己的方法。
 */
typedef struct MemoryContextMethods {
    void *(*alloc)(MemoryContext ctx, Size size);  /**< 分配内存 */
    void  (*free_p)(MemoryContext ctx, void *ptr); /**< 释放单个指针（可为 noop） */
    void  (*reset)(MemoryContext ctx);             /**< 重置上下文（释放子块） */
    void  (*delete_ctx)(MemoryContext ctx);        /**< 删除上下文（释放全部） */
} MemoryContextMethods;

/* ========================================================================
 * 内存上下文数据结构
 * ======================================================================== */

/**
 * @brief 内存上下文数据结构
 */
typedef struct MemoryContextData {
    NodeTag                  type;          /**< 节点类型（复用公共 NodeTag，值为 T_MemoryContext/T_AllocSetContext） */
    MemoryContext            parent;        /**< 父上下文 */
    MemoryContext            firstchild;    /**< 第一个子上下文 */
    MemoryContext            prevchild;     /**< 前一个兄弟 */
    MemoryContext            nextchild;     /**< 后一个兄弟 */
    const MemoryContextMethods *methods;    /**< 方法表 */
    const char              *name;          /**< 上下文名称（调试用） */
    Size                     mem_allocated; /**< 已分配字节数（应用层累计） */
    bool                     isReset;       /**< 是否已重置 */
} MemoryContextData;

/* ========================================================================
 * AllocSet 块结构
 * ======================================================================== */

/**
 * @brief AllocSet 块
 *
 * 每块一次性向系统申请大块内存（initBlockSize~maxBlockSize），
 * 之后的小分配在块内线性推进。
 */
typedef struct AllocSetBlock {
    Size                size;       /**< 块总大小（含本结构） */
    Size                free;       /**< 剩余空间（字节） */
    char               *start;      /**< 数据起始地址（紧接本结构之后） */
    char               *end;        /**< 块结束地址 */
    struct AllocSetBlock *next;     /**< 下一块 */
} AllocSetBlock;

/**
 * @brief AllocSet 私有数据
 */
typedef struct AllocSetContext {
    MemoryContextData   header;     /**< 公共头 */
    AllocSetBlock      *blocks;     /**< 块链表（首块） */
    Size                initBlockSize; /**< 初始块大小 */
    Size                maxBlockSize;  /**< 最大块大小 */
} AllocSetContext;

/* ========================================================================
 * 公共 API
 * ======================================================================== */

/**
 * @brief 创建 AllocSet 内存上下文
 *
 * @param parent         父上下文（NULL 表示根上下文）
 * @param name           上下文名称（调试用）
 * @param minContextSize 首块最小容量下限（字节，0 表示不设下限）。
 *                       语义：保证首块的数据区（可分配空间）不小于该值；
 *                       若 initBlockSize 推导出的首块数据区偏小，则按此值抬高首块尺寸。
 * @param initBlockSize  初始块大小（0 表示默认 8KB）
 * @param maxBlockSize   最大块大小（0 表示默认 8KB）
 *
 * @return 新创建的 MemoryContext；失败返回 NULL
 */
MemoryContext AllocSetContextCreate(
    MemoryContext parent,
    const char *name,
    Size minContextSize,
    Size initBlockSize,
    Size maxBlockSize);

/**
 * @brief 从上下文中分配内存（自动 8 字节对齐）
 *
 * @param ctx  内存上下文
 * @param size 请求字节数
 *
 * @return 分配的内存指针；失败返回 NULL
 */
void *palloc(MemoryContext ctx, Size size);

/**
 * @brief 从上下文中分配零初始化内存
 *
 * @param ctx  内存上下文
 * @param size 请求字节数
 *
 * @return 分配的内存指针（已清零）；失败返回 NULL
 */
void *palloc0(MemoryContext ctx, Size size);

/**
 * @brief 释放内存
 *
 * AllocSet 实现为空操作；实际释放由 reset/delete 完成。
 * 保留接口以兼容 PostgreSQL 风格 API。
 *
 * @param ctx 内存上下文
 * @param ptr 待释放指针
 */
void pfree(MemoryContext ctx, void *ptr);

/**
 * @brief 重置上下文：释放所有子上下文块，保留当前上下文主块
 *
 * @param ctx 待重置的内存上下文
 */
void reset_memory(MemoryContext ctx);

/**
 * @brief 删除上下文：释放当前上下文所有块及子上下文
 *
 * @param ctx 待删除的内存上下文
 */
void delete_memory(MemoryContext ctx);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_MEMCTX_H */