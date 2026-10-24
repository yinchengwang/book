/**
 * @file storage_backend_common.c
 * @brief 存储后端公共函数实现
 *
 * 本文件提供 storage_backend_destroy、storage_backend_create 和
 * storage_backend_type_name 等公共函数的实现。
 */

#include <stdlib.h>
#include <string.h>
#include "storage_backend.h"

/**
 * @brief 销毁存储后端
 *
 * 内部会调用 ops->close 关闭后端并释放所有相关资源。
 */
void storage_backend_destroy(storage_backend_t *backend)
{
    if (backend == NULL) {
        return;
    }

    if (backend->ops != NULL && backend->ops->close != NULL) {
        backend->ops->close(backend->ctx);
    }

    free(backend);
}

/**
 * @brief 获取后端类型的可读名称
 */
const char *storage_backend_type_name(storage_backend_type_t type)
{
    switch (type) {
        case STORAGE_BACKEND_MEMORY:
            return "MEMORY";
        case STORAGE_BACKEND_PAGE_FILE:
            return "PAGE_FILE";
        case STORAGE_BACKEND_MMAP:
            return "MMAP";
        case STORAGE_BACKEND_FAISS:
            return "FAISS";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief 创建指定类型的存储后端
 */
storage_backend_t *storage_backend_create(storage_backend_type_t type,
                                          const char *path,
                                          size_t page_size)
{
    switch (type) {
        case STORAGE_BACKEND_MEMORY:
            return storage_memory_create(page_size);
        case STORAGE_BACKEND_PAGE_FILE:
            return storage_page_file_create(path, page_size);
        case STORAGE_BACKEND_MMAP:
            return storage_mmap_create(path, page_size);
        case STORAGE_BACKEND_FAISS:
            // FAISS 后端需要特殊处理，暂不实现
            return NULL;
        default:
            return NULL;
    }
}
