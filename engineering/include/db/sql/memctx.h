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

/* 前向声明 */
typedef struct MemoryContextData *MemoryContext;

/* ========================================================================
 * 内存分配头结构
 * ======================================================================== */

/** 分配头魔数 */
#define MEMORY_ALLOCATION_HEADER_MAGIC  0x4D454D435458ULL  /* "MEMCTX" */

/** 分配头标志：已释放 */
#define MEMORY_ALLOCATION_FLAG_FREED    0x1

/**
 * @brief 内存分配头
 *
 * 每次用户分配前插入隐藏头部，用于校验和调试。
 */
typedef struct MemoryAllocationHeader {
    uint64_t           magic;           /**< 魔数校验 */
    Size               requested_size;  /**< 用户请求大小 */
    Size               allocated_size;  /**< 实际分配大小（含对齐） */
    MemoryContext      owner;           /**< 所属上下文 */
    uint64_t           generation;      /**< 分配时的 generation */
    uint32_t           flags;           /**< 标志位 */
} MemoryAllocationHeader;

/* ========================================================================
 * 内存上下文统计结构
 * ======================================================================== */

/**
 * @brief 内存上下文统计信息
 */
typedef struct MemoryContextStats {
    Size               current_bytes;       /**< 当前已分配字节数 */
    Size               peak_bytes;          /**< 历史峰值分配字节数 */
    Size               total_allocated;     /**< 累计分配字节数 */
    Size               total_freed;         /**< 累计释放字节数 */
    Size               allocation_count;    /**< 累计分配次数 */
    Size               free_count;          /**< 累计释放次数 */
    Size               reset_count;         /**< reset 次数 */
    Size               oom_count;           /**< OOM 次数 */
    Size               invalid_free_count;  /**< 无效释放次数 */
    Size               double_free_count;   /**< 双重释放次数 */
    Size               resource_count;      /**< 资源析构回调数量 */
    Size               child_count;         /**< 子上下文数量 */
} MemoryContextStats;

/* ========================================================================
 * 内存上下文错误码
 * ======================================================================== */

/**
 * @brief 内存上下文错误码
 */
typedef enum MemoryContextError {
    MMDB_MEMCTX_OK = 0,                  /**< 成功 */
    MMDB_MEMCTX_INVALID_CONTEXT,         /**< 无效上下文 */
    MMDB_MEMCTX_INVALID_POINTER,         /**< 无效指针 */
    MMDB_MEMCTX_CROSS_CONTEXT_FREE,      /**< 跨上下文释放 */
    MMDB_MEMCTX_DOUBLE_FREE,             /**< 双重释放 */
    MMDB_MEMCTX_LIMIT_EXCEEDED,          /**< 超出限额 */
    MMDB_MEMCTX_OVERFLOW,                /**< 溢出 */
    MMDB_MEMCTX_OOM,                     /**< 内存不足 */
    MMDB_MEMCTX_WRONG_THREAD,            /**< 线程不匹配 */
    MMDB_MEMCTX_ALREADY_DELETED          /**< 已删除 */
} MemoryContextError;

/* ========================================================================
 * 资源析构节点
 * ======================================================================== */

/**
 * @brief 资源析构回调节点
 *
 * 用于注册需要在上下文销毁时释放的外部资源。
 */
typedef struct MemoryResource {
    void            *resource;       /**< 资源指针 */
    void (*destructor)(void *resource, void *arg); /**< 析构回调 */
    void            *arg;            /**< 析构回调参数 */
    const char      *name;           /**< 资源名称（调试用） */
    struct MemoryResource *next;     /**< 下一个节点 */
} MemoryResource;

/* ========================================================================
 * 内存上下文方法表
 * ======================================================================== */

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

    /* 统计字段 */
    Size                     current_bytes;     /**< 当前已分配字节数 */
    Size                     peak_bytes;        /**< 历史峰值分配字节数 */
    Size                     total_allocated;   /**< 累计分配字节数 */
    Size                     total_freed;       /**< 累计释放字节数 */
    Size                     allocation_count;  /**< 累计分配次数 */
    Size                     free_count;        /**< 累计释放次数 */
    Size                     reset_count;       /**< reset 次数 */
    Size                     oom_count;         /**< OOM 次数 */
    Size                     invalid_free_count;/**< 无效释放次数 */
    Size                     double_free_count; /**< 双重释放次数 */

    /* 限额 */
    Size                     max_bytes;         /**< 内存限额（0 表示无限制） */

    /* 校验与生命周期 */
    uint64_t                 generation;        /**< 代次计数器，用于校验 */
    uint32_t                 flags;             /**< 标志位 */

    /* 资源析构 */
    struct MemoryResource   *resources;         /**< 资源析构回调链表 */

    /* OOM 策略 */
    void (*on_oom)(MemoryContext context, Size requested, void *arg); /**< OOM 回调 */
    void                    *on_oom_arg;        /**< OOM 回调参数 */

    /* 状态 */
    bool                     is_reset;          /**< 是否已重置 */
    bool                     is_deleted;        /**< 是否已删除 */

    /* 线程归属 */
    bool                     is_thread_owner;   /**< 是否为线程拥有者 */
    uint64_t                 owner_thread_id;   /**< 拥有者线程 ID */
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

/* ========================================================================
 * CurrentMemoryContext 与 SwitchTo API
 * ======================================================================== */

/* 当前线程的当前上下文（线程局部存储） */
extern __thread MemoryContext CurrentMemoryContext;

/* 获取当前上下文 */
MemoryContext MemoryContextCurrent(void);

/* 切换当前上下文，返回旧上下文 */
MemoryContext MemoryContextSwitchTo(MemoryContext context);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_MEMCTX_H */