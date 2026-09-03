/*
 * diskann_disk_io.c
 *
 * DiskANN 磁盘 I/O 实现。
 *
 * 本模块封装基于 mmap 的内存映射文件访问，支持：
 * - 按需加载：只访问搜索所需的页面
 * - 缓存热点页面：操作系统自动缓存最近访问的页
 * - 跨平台：支持 Windows (MapViewOfFile) 和 POSIX (mmap)
 */

#include "diskann_private.h"

#include <db/index/storage_backend.h>
#include <errno.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#endif

/* ============================================================================
 * 常量定义
 * ============================================================================ */

/** 默认索引文件扩展名 */
#define DISKANN_INDEX_EXTENSION ".diskann"

/** 备用数据文件扩展名 */
#define DISKANN_DATA_EXTENSION ".diskann.data"

/** 图结构文件扩展名 */
#define DISKANN_GRAPH_EXTENSION ".diskann.graph"

/** 向量文件扩展名 */
#define DISKANN_VECTORS_EXTENSION ".diskann.vec"

/** PQ 码本文件扩展名 */
#define DISKANN_CODES_EXTENSION ".diskann.codes"

/** 默认 mmap 页面大小（4KB，对齐 SSD 物理页） */
#define DISKANN_DEFAULT_MMAP_PAGE_SIZE 4096

/** 热点页面缓存大小（默认 256 页） */
#define DISKANN_DEFAULT_CACHE_SIZE 256

/* ============================================================================
 * 内存映射文件句柄
 * ============================================================================ */

/**
 * @brief mmap 文件句柄结构
 */
typedef struct diskann_mmap_handle {
    char *base_path;                    /**< 基础路径（不含扩展名） */
    storage_backend_t *vector_backend;  /**< 向量存储后端 */
    storage_backend_t *graph_backend;   /**< 图结构存储后端 */
    storage_backend_t *code_backend;    /**< PQ 编码存储后端 */
    storage_backend_t *meta_backend;    /**< 元信息存储后端 */
    int32_t page_size;                  /**< 页面大小 */
    int32_t cache_size;                 /**< 缓存大小 */
    bool read_only;                     /**< 是否只读模式 */
} diskann_mmap_handle_t;

/* ============================================================================
 * 内部辅助函数
 * ============================================================================ */

/**
 * @brief 构建带扩展名的文件路径
 */
static char *diskann_build_path(const char *base, const char *ext)
{
    size_t base_len;
    size_t ext_len;
    char *path;

    if (!base || !ext) {
        return NULL;
    }

    base_len = strlen(base);
    ext_len = strlen(ext);
    path = (char *)malloc(base_len + ext_len + 1);
    if (!path) {
        return NULL;
    }

    memcpy(path, base, base_len);
    memcpy(path + base_len, ext, ext_len + 1);
    return path;
}

/**
 * @brief 创建存储后端
 */
static storage_backend_t *diskann_create_backend(const char *path, int32_t page_size, bool mmap)
{
    if (mmap) {
        return storage_mmap_create(path, (size_t)page_size);
    }
    return storage_page_file_create(path, (size_t)page_size);
}

/**
 * @brief 确保目录存在
 */
static int diskann_ensure_dir(const char *path)
{
    struct stat st;

    if (!path) {
        return -1;
    }

    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return 0;
        }
        return -1;
    }

#ifdef _WIN32
    return _mkdir(path);
#else
    return mkdir(path, 0755);
#endif
}

/* ============================================================================
 * 公共 API：内存映射文件操作
 * ============================================================================ */

/**
 * @brief 创建 mmap 文件句柄
 * @param[in] base_path 基础路径（不含扩展名）
 * @param[in] page_size 页面大小
 * @param[in] cache_size 缓存大小
 * @param[in] read_only 是否只读
 * @return 句柄，失败返回 NULL
 */
diskann_mmap_handle_t *diskann_mmap_create(const char *base_path,
                                           int32_t page_size,
                                           int32_t cache_size,
                                           bool read_only)
{
    diskann_mmap_handle_t *handle;
    char *path;
    int32_t effective_page_size;

    if (!base_path || page_size <= 0) {
        return NULL;
    }

    effective_page_size = page_size > 0 ? page_size : DISKANN_DEFAULT_MMAP_PAGE_SIZE;
    if (cache_size <= 0) {
        cache_size = DISKANN_DEFAULT_CACHE_SIZE;
    }

    handle = (diskann_mmap_handle_t *)calloc(1, sizeof(diskann_mmap_handle_t));
    if (!handle) {
        return NULL;
    }

    handle->base_path = (char *)malloc(strlen(base_path) + 1);
    if (!handle->base_path) {
        free(handle);
        return NULL;
    }
    strcpy(handle->base_path, base_path);

    handle->page_size = effective_page_size;
    handle->cache_size = cache_size;
    handle->read_only = read_only;

    /* 创建各类型数据的存储后端 */
    path = diskann_build_path(base_path, DISKANN_VECTORS_EXTENSION);
    if (path) {
        handle->vector_backend = diskann_create_backend(path, effective_page_size, true);
        free(path);
    }

    path = diskann_build_path(base_path, DISKANN_GRAPH_EXTENSION);
    if (path) {
        handle->graph_backend = diskann_create_backend(path, effective_page_size, true);
        free(path);
    }

    path = diskann_build_path(base_path, DISKANN_CODES_EXTENSION);
    if (path) {
        handle->code_backend = diskann_create_backend(path, effective_page_size, true);
        free(path);
    }

    path = diskann_build_path(base_path, DISKANN_DATA_EXTENSION);
    if (path) {
        handle->meta_backend = diskann_create_backend(path, effective_page_size, false);
        free(path);
    }

    return handle;
}

/**
 * @brief 销毁 mmap 文件句柄
 */
void diskann_mmap_destroy(diskann_mmap_handle_t *handle)
{
    if (!handle) {
        return;
    }

    if (handle->vector_backend) {
        storage_backend_destroy(handle->vector_backend);
    }
    if (handle->graph_backend) {
        storage_backend_destroy(handle->graph_backend);
    }
    if (handle->code_backend) {
        storage_backend_destroy(handle->code_backend);
    }
    if (handle->meta_backend) {
        storage_backend_destroy(handle->meta_backend);
    }
    free(handle->base_path);
    free(handle);
}

/**
 * @brief 获取向量存储后端
 */
storage_backend_t *diskann_mmap_get_vector_backend(diskann_mmap_handle_t *handle)
{
    return handle ? handle->vector_backend : NULL;
}

/**
 * @brief 获取图结构存储后端
 */
storage_backend_t *diskann_mmap_get_graph_backend(diskann_mmap_handle_t *handle)
{
    return handle ? handle->graph_backend : NULL;
}

/**
 * @brief 获取 PQ 编码存储后端
 */
storage_backend_t *diskann_mmap_get_code_backend(diskann_mmap_handle_t *handle)
{
    return handle ? handle->code_backend : NULL;
}

/**
 * @brief 获取元信息存储后端
 */
storage_backend_t *diskann_mmap_get_meta_backend(diskann_mmap_handle_t *handle)
{
    return handle ? handle->meta_backend : NULL;
}

/* ============================================================================
 * 公共 API：批量 mmap 加载
 * ============================================================================ */

/**
 * @brief mmap 批量加载向量数据
 * @param[in] handle mmap 句柄
 * @param[in] start_id 起始向量 ID
 * @param[in] count 向量数量
 * @param[out] vectors_out 向量输出缓冲区（需预分配）
 * @param[in] dims 向量维度
 * @return 成功返回加载的向量数，失败返回 -1
 */
int32_t diskann_mmap_load_vectors(diskann_mmap_handle_t *handle,
                                   int32_t start_id,
                                   int32_t count,
                                   float *vectors_out,
                                   int32_t dims)
{
    (void)handle;
    (void)start_id;
    (void)count;
    (void)vectors_out;
    (void)dims;
    /* TODO: 实现批量加载逻辑 */
    return -1;
}

/**
 * @brief mmap 加载向量块
 * @param[in] handle mmap 句柄
 * @param[in] start_id 起始向量 ID
 * @param[in] count 向量数量
 * @param[out] vectors_out 向量输出缓冲区
 * @param[in] dims 向量维度
 * @return 成功返回加载的向量数，失败返回 -1
 */
int32_t diskann_mmap_load_vector_range(diskann_mmap_handle_t *handle,
                                         int32_t start_id,
                                         int32_t count,
                                         float *vectors_out,
                                         int32_t dims)
{
    (void)handle;
    (void)start_id;
    (void)count;
    (void)vectors_out;
    (void)dims;
    /* TODO: 实现范围加载逻辑 */
    return -1;
}

/**
 * @brief mmap 加载图结构
 * @param[in] handle mmap 句柄
 * @param[out] neighbors_out 邻接表输出缓冲区
 * @param[out] neighbor_counts_out 邻居计数数组
 * @param[in] n_total 节点总数
 * @param[in] index_size 目标邻居数
 * @return 成功返回 0，失败返回 -1
 */
int32_t diskann_mmap_load_graph(diskann_mmap_handle_t *handle,
                                 int32_t *neighbors_out,
                                 int32_t *neighbor_counts_out,
                                 int32_t n_total,
                                 int32_t index_size)
{
    (void)handle;
    (void)neighbors_out;
    (void)neighbor_counts_out;
    (void)n_total;
    (void)index_size;
    /* TODO: 实现图结构加载逻辑 */
    return -1;
}

/**
 * @brief mmap 加载 PQ 编码
 * @param[in] handle mmap 句柄
 * @param[out] codes_out 编码输出缓冲区
 * @param[in] n_total 节点总数
 * @param[in] code_size 单向量编码大小
 * @return 成功返回 0，失败返回 -1
 */
int32_t diskann_mmap_load_codes(diskann_mmap_handle_t *handle,
                                 uint8_t *codes_out,
                                 int32_t n_total,
                                 int32_t code_size)
{
    (void)handle;
    (void)codes_out;
    (void)n_total;
    (void)code_size;
    /* TODO: 实现 PQ 编码加载逻辑 */
    return -1;
}

/* ============================================================================
 * 公共 API：直接文件 I/O（备用方案）
 * ============================================================================ */

/**
 * @brief 打开索引文件进行直接读取
 * @param[in] path 文件路径
 * @param[in] page_size 页面大小
 * @return 文件句柄，失败返回 NULL
 */
void *diskann_file_open(const char *path, int32_t page_size)
{
    (void)path;
    (void)page_size;
    /* TODO: 实现文件打开逻辑 */
    return NULL;
}

/**
 * @brief 关闭索引文件
 * @param[in] file 文件句柄
 */
void diskann_file_close(void *file)
{
    (void)file;
    /* TODO: 实现文件关闭逻辑 */
}

/**
 * @brief 读取向量数据
 * @param[in] file 文件句柄
 * @param[in] offset 偏移量
 * @param[out] data 数据缓冲区
 * @param[in] size 读取大小
 * @return 成功返回读取字节数，失败返回 -1
 */
int64_t diskann_file_read(void *file, int64_t offset, void *data, int64_t size)
{
    (void)file;
    (void)offset;
    (void)data;
    (void)size;
    /* TODO: 实现文件读取逻辑 */
    return -1;
}

/**
 * @brief 写入向量数据
 * @param[in] file 文件句柄
 * @param[in] offset 偏移量
 * @param[in] data 数据缓冲区
 * @param[in] size 写入大小
 * @return 成功返回写入字节数，失败返回 -1
 */
int64_t diskann_file_write(void *file, int64_t offset, const void *data, int64_t size)
{
    (void)file;
    (void)offset;
    (void)data;
    (void)size;
    /* TODO: 实现文件写入逻辑 */
    return -1;
}

/**
 * @brief 刷盘
 * @param[in] file 文件句柄
 * @return 成功返回 0，失败返回 -1
 */
int diskann_file_sync(void *file)
{
    (void)file;
    /* TODO: 实现刷盘逻辑 */
    return -1;
}
