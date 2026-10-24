/**
 * @file storage_backend.h
 * @brief 存储后端接口定义 - 多模态数据库索引子系统的统一存储抽象
 *
 * 本接口定义了索引子系统所需的页面级持久化抽象。每种存储后端
 * （纯内存、页面文件、内存映射、Faiss 原生格式）实现统一的操作集，
 * 上层索引模块通过 storage_backend_t 访问底层介质。
 *
 * 设计要点：
 * - 采用面向对象的"接口 + 实现"模式，通过 ops 函数指针分派
 * - 页面抽象沿用 db/page.h 的 page_t，保持与主存储栈兼容
 * - page_id_t 使用 int32_t，INVALID_PAGE_ID = -1
 * - 所有操作失败均返回非零错误码，成功返回 0
 */
#ifndef DB_INDEX_STORAGE_BACKEND_H
#define DB_INDEX_STORAGE_BACKEND_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** 存储后端的最小页面大小（512 字节，与磁盘扇区对齐） */
#define STORAGE_BACKEND_MIN_PAGE_SIZE 512u

/** 存储后端的最大页面大小（64KB） */
#define STORAGE_BACKEND_MAX_PAGE_SIZE (64u * 1024u)

/* ============================================================
 * 类型定义
 * ============================================================ */

/** 存储后端类型 */
typedef enum {
    STORAGE_BACKEND_MEMORY = 0,    /**< 纯内存，不持久化 */
    STORAGE_BACKEND_PAGE_FILE = 1, /**< 页面文件（顺序读写） */
    STORAGE_BACKEND_MMAP = 2,      /**< 内存映射文件 */
    STORAGE_BACKEND_FAISS = 3      /**< Faiss 原生格式（只读） */
} storage_backend_type_t;

/** 存储后端使用的页面 ID 类型 */
typedef int32_t page_id_t;

/** 无效页面 ID */
#define INVALID_PAGE_ID ((page_id_t)-1)

/** 页面结构前向声明
 *
 *  上层一般使用 db/page.h 中的 page_t，但本接口以不透明指针方式引用，
 *  避免对 db/page.h 的硬依赖，便于不同存储语义复用同一接口。
 */
typedef struct page_t page_t;

/**
 * @brief 存储后端操作接口
 *
 * 每个具体的后端实现都需要提供这一组操作。多态分派通过
 * storage_backend_t::ops 完成。ctx 指向后端私有上下文（如文件
 * 句柄、内存池、mmap 句柄等）。
 */
typedef struct storage_backend_ops {
    /**
     * @brief 分配新页面
     * @param ctx 后端私有上下文
     * @return 新页面的 page_id；失败返回 INVALID_PAGE_ID
     */
    page_id_t (*alloc_page)(void *ctx);

    /**
     * @brief 读取页面
     * @param ctx 后端私有上下文
     * @param page_id 页面 ID
     * @param page 输出缓冲区
     * @return 0 成功，非 0 失败
     */
    int (*read_page)(void *ctx, page_id_t page_id, page_t *page);

    /**
     * @brief 写入页面
     * @param ctx 后端私有上下文
     * @param page_id 页面 ID
     * @param page 待写入的页面
     * @return 0 成功，非 0 失败
     */
    int (*write_page)(void *ctx, page_id_t page_id, const page_t *page);

    /**
     * @brief 刷盘指定页面（强制落盘）
     * @param ctx 后端私有上下文
     * @param page_id 页面 ID
     * @return 0 成功，非 0 失败
     */
    int (*flush_page)(void *ctx, page_id_t page_id);

    /**
     * @brief 批量写入多个页面（原子性视后端实现而定）
     * @param ctx 后端私有上下文
     * @param page_ids 页面 ID 数组
     * @param pages 页面指针数组（与 page_ids 一一对应）
     * @param count 批量大小
     * @return 0 成功，非 0 失败
     */
    int (*batch_write)(void *ctx, const page_id_t *page_ids,
                       const page_t **pages, int count);

    /**
     * @brief 同步所有脏页到磁盘
     * @param ctx 后端私有上下文
     * @return 0 成功，非 0 失败
     */
    int (*sync)(void *ctx);

    /**
     * @brief 关闭后端并释放资源
     * @param ctx 后端私有上下文
     * @return 0 成功，非 0 失败
     */
    int (*close)(void *ctx);
} storage_backend_ops_t;

/**
 * @brief 存储后端结构
 *
 * 通过 storage_backend_create 创建，通过 storage_backend_destroy 销毁。
 * type 字段标识后端实现类型；ops 提供统一的操作入口；ctx 是后端私有
 * 数据；page_size 描述该后端的页面粒度（字节）。
 */
typedef struct storage_backend {
    storage_backend_type_t        type;     /**< 后端类型 */
    void                         *ctx;      /**< 后端私有上下文 */
    size_t                        page_size; /**< 页面大小（字节） */
    const storage_backend_ops_t  *ops;      /**< 操作接口 */
} storage_backend_t;

/* ============================================================
 * 公共 API
 * ============================================================ */

/**
 * @brief 创建指定类型的存储后端
 *
 * @param type 后端类型（MEMORY / PAGE_FILE / MMAP / FAISS）
 * @param path 路径（NULL 表示纯内存后端；FAISS 必须提供索引文件路径）
 * @param page_size 页面大小（字节），传 0 表示使用默认值 DEFAULT_PAGE_SIZE
 * @return 新创建的后端指针；失败返回 NULL
 *
 * @note 调用方负责通过 storage_backend_destroy 释放返回的指针。
 */
storage_backend_t *storage_backend_create(storage_backend_type_t type,
                                          const char *path,
                                          size_t page_size);

/**
 * @brief 销毁存储后端
 *
 * 内部会调用 ops->close 关闭后端并释放所有相关资源。
 *
 * @param backend 待销毁的后端指针；允许传入 NULL（空操作）
 */
void storage_backend_destroy(storage_backend_t *backend);

/**
 * @brief 创建纯内存存储后端
 *
 * 分配并初始化一个纯内存后端。所有页面分配在堆上，
 * 不写入磁盘。适用于临时索引和测试场景。
 *
 * @param page_size 页面大小（字节），必须满足最小/最大页面大小约束
 * @return 新创建的后端指针；失败返回 NULL
 *
 * @note 调用方负责通过 storage_backend_destroy 释放返回的指针。
 */
storage_backend_t *storage_memory_create(size_t page_size);

/**
 * @brief 获取后端类型的可读名称（用于日志与错误信息）
 * @param type 后端类型
 * @return 静态字符串描述，永不为 NULL
 */
const char *storage_backend_type_name(storage_backend_type_t type);

/**
 * @brief 创建页面文件存储后端
 *
 * 基于标准 C 文件 I/O 的页面级存储后端。每个页面在文件中按 page_size
 * 对齐顺序存放，通过 fseek 定位读写。适用于中小规模索引的持久化场景。
 *
 * @param path 文件路径（必须非 NULL）
 * @param page_size 页面大小（字节），传 0 使用默认值 8192
 * @return 新创建的后端指针；失败返回 NULL
 *
 * @note 调用方负责通过 storage_backend_destroy 释放返回的指针。
 */
storage_backend_t *storage_page_file_create(const char *path, size_t page_size);

/**
 * @brief 创建 mmap 存储后端
 *
 * 基于内存映射文件（POSIX mmap / Windows MapViewOfFile）的页面级
 * 存储后端。文件被映射到进程虚拟地址空间，页面读写通过内存拷贝
 * 完成，由 OS 负责将脏页回写到磁盘。适合大索引与需要随机访问的场景。
 *
 * @param path 文件路径（必须非 NULL）
 * @param page_size 页面大小（字节），传 0 使用默认值 8192
 * @return 新创建的后端指针；失败返回 NULL
 *
 * @note 调用方负责通过 storage_backend_destroy 释放返回的指针。
 */
storage_backend_t *storage_mmap_create(const char *path, size_t page_size);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_STORAGE_BACKEND_H */