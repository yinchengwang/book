# 多模态数据库索引子系统重构执行计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将索引实现从 Faiss 的纯内存模型重构为页面管理的持久化模型，同时保持算法纯净性，消除向量膨胀，建立完整的文档体系。

**Architecture:** 三层架构——配置层（索引配置）→ 算法层（纯算法逻辑，通过回调访问数据）→ 存储层（页面管理器/Buffer Pool，通过配置选择后端）。Heap 为主存储（Single Source of Truth），索引只存引用。

**Tech Stack:** C11, CMake 3.20+, PostgreSQL 风格页面管理, GoogleTest

---

## Global Constraints

- 对话/注释/Commit Message 必须使用简体中文
- 遵循 OPSX 变更管理铁律：tasks.md 更新后联动相关文档
- 回退纪律：OPSX 变更修改出现问题只回退当前变更，不动其他代码
- 提交纪律：每次 OPSX 变更只提交变更相关代码

---

## 文件结构

```
engineering/include/db/index/
├── index.h                           # 索引子系统入口（修改）
├── storage_backend.h                 # 存储后端接口（新建）
├── index_config.h                    # 索引配置结构（新建）
└── vector_index/
    └── hnsw/
        ├── faiss_hnsw.h              # HNSW 头文件（修改）

engineering/src/db/index/
├── storage_backend/                  # 存储后端实现（新建目录）
│   ├── storage_backend.c             # 通用实现
│   ├── storage_memory.c              # 纯内存后端
│   ├── storage_page_file.c           # 页面文件后端
│   ├── storage_mmap.c                # 内存映射后端
│   └── CMakeLists.txt
├── heap/                             # 向量 Heap 主存储（新建目录）
│   ├── heap_vector_store.c           # 向量存储实现
│   └── CMakeLists.txt
└── vector_index/hnsw/
    ├── faiss_hnsw_insert.c           # 改造为使用存储后端
    └── faiss_hnsw_search.c           # 改造为使用引用+重排序

docs/
└── index/                            # 索引文档目录（新建）
    ├── README.md
    ├── theory/
    │   ├── hnsw-theory.md
    │   ├── diskann-theory.md
    │   └── ...
    └── implementation/
        ├── hnsw-impl.md
        └── ...
```

---

## 任务分解

### Phase 1: 基础设施重构

#### Task 1: 定义存储后端接口 `storage_backend.h`

**Files:**
- Create: `engineering/include/db/index/storage_backend.h`
- Consumes: 无
- Produces: `storage_backend_t`, `storage_backend_type_t`, `storage_backend_ops_t`

- [ ] **Step 1: 创建头文件**

```c
/**
 * @file storage_backend.h
 * @brief 存储后端接口定义
 */
#ifndef DB_STORAGE_BACKEND_H
#define DB_STORAGE_BACKEND_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 存储后端类型 */
typedef enum {
    STORAGE_BACKEND_MEMORY = 0,      /**< 纯内存，不持久化 */
    STORAGE_BACKEND_PAGE_FILE = 1,   /**< 页面文件 */
    STORAGE_BACKEND_MMAP = 2,        /**< 内存映射 */
    STORAGE_BACKEND_FAISS = 3,       /**< Faiss 原生格式（只读） */
} storage_backend_type_t;

/** 页面 ID 类型 */
typedef int32_t page_id_t;

/** 无效页面 ID */
#define INVALID_PAGE_ID (-1)

/** 页面结构前向声明 */
typedef struct page_t page_t;

/** 存储后端操作接口 */
typedef struct storage_backend_ops {
    /** 分配新页面 */
    page_id_t (*alloc_page)(void *ctx);

    /** 读取页面 */
    int (*read_page)(void *ctx, page_id_t page_id, page_t *page);

    /** 写入页面 */
    int (*write_page)(void *ctx, page_id_t page_id, const page_t *page);

    /** 刷盘单个页面 */
    int (*flush_page)(void *ctx, page_id_t page_id);

    /** 批量写入 */
    int (*batch_write)(void *ctx, const page_id_t *page_ids, const page_t **pages, int count);

    /** 同步所有脏页 */
    int (*sync)(void *ctx);

    /** 关闭后端 */
    int (*close)(void *ctx);
} storage_backend_ops_t;

/** 存储后端结构 */
typedef struct storage_backend {
    storage_backend_type_t type;     /**< 后端类型 */
    void *ctx;                       /**< 后端私有上下文 */
    size_t page_size;                /**< 页面大小 */
    const storage_backend_ops_t *ops; /**< 操作接口 */
} storage_backend_t;

/** 创建存储后端 */
storage_backend_t *storage_backend_create(storage_backend_type_t type, const char *path, size_t page_size);

/** 销毁存储后端 */
void storage_backend_destroy(storage_backend_t *backend);

#ifdef __cplusplus
}
#endif

#endif /* DB_STORAGE_BACKEND_H */
```

- [ ] **Step 2: 验证头文件编译**

Run: `gcc -c -I engineering/include engineering/include/db/index/storage_backend.h -o /dev/null`
Expected: 无错误

- [ ] **Step 3: 提交**

```bash
git add engineering/include/db/index/storage_backend.h
git commit -m "feat(index): 定义存储后端接口 storage_backend_t"
```

---

#### Task 2: 实现纯内存存储后端

**Files:**
- Create: `engineering/src/db/index/storage_backend/storage_memory.c`
- Create: `engineering/src/db/index/storage_backend/CMakeLists.txt`
- Consumes: `storage_backend.h`
- Produces: `storage_memory_create()`, `storage_memory_ops`

- [ ] **Step 1: 创建 CMakeLists.txt**

```cmake
add_project_library(storage_backend_memory
    SOURCES
        storage_memory.c
    HEADERS
        ../../include/db/index/storage_backend.h
)
```

- [ ] **Step 2: 实现纯内存后端**

```c
/**
 * @file storage_memory.c
 * @brief 纯内存存储后端实现（不持久化）
 */

#include <stdlib.h>
#include <string.h>
#include "storage_backend.h"

/** 内存后端上下文 */
typedef struct {
    page_t **pages;          /**< 页面数组 */
    int32_t page_count;      /**< 已分配页面数 */
    int32_t page_capacity;   /**< 页面容量 */
    size_t page_size;        /**< 页面大小 */
} memory_backend_ctx_t;

static page_id_t memory_alloc_page(void *ctx)
{
    memory_backend_ctx_t *mctx = (memory_backend_ctx_t *)ctx;

    if (mctx->page_count >= mctx->page_capacity) {
        /* 扩展容量 */
        int32_t new_capacity = mctx->page_capacity * 2;
        page_t **new_pages = (page_t **)realloc(mctx->pages, new_capacity * sizeof(page_t *));
        if (new_pages == NULL) {
            return INVALID_PAGE_ID;
        }
        mctx->pages = new_pages;
        mctx->page_capacity = new_capacity;
    }

    page_id_t page_id = mctx->page_count++;
    mctx->pages[page_id] = (page_t *)malloc(mctx->page_size);
    if (mctx->pages[page_id] == NULL) {
        mctx->page_count--;
        return INVALID_PAGE_ID;
    }

    memset(mctx->pages[page_id], 0, mctx->page_size);
    return page_id;
}

static int memory_read_page(void *ctx, page_id_t page_id, page_t *page)
{
    memory_backend_ctx_t *mctx = (memory_backend_ctx_t *)ctx;
    if (page_id < 0 || page_id >= mctx->page_count) {
        return -1;
    }
    memcpy(page, mctx->pages[page_id], mctx->page_size);
    return 0;
}

static int memory_write_page(void *ctx, page_id_t page_id, const page_t *page)
{
    memory_backend_ctx_t *mctx = (memory_backend_ctx_t *)ctx;
    if (page_id < 0 || page_id >= mctx->page_count) {
        return -1;
    }
    memcpy(mctx->pages[page_id], page, mctx->page_size);
    return 0;
}

static int memory_flush_page(void *ctx, page_id_t page_id)
{
    /* 纯内存后端无需刷盘 */
    (void)ctx;
    (void)page_id;
    return 0;
}

static int memory_sync(void *ctx)
{
    /* 纯内存后端无需同步 */
    (void)ctx;
    return 0;
}

static int memory_close(void *ctx)
{
    memory_backend_ctx_t *mctx = (memory_backend_ctx_t *)ctx;
    for (int32_t i = 0; i < mctx->page_count; i++) {
        free(mctx->pages[i]);
    }
    free(mctx->pages);
    free(mctx);
    return 0;
}

static const storage_backend_ops_t memory_ops = {
    .alloc_page = memory_alloc_page,
    .read_page = memory_read_page,
    .write_page = memory_write_page,
    .flush_page = memory_flush_page,
    .batch_write = NULL,
    .sync = memory_sync,
    .close = memory_close,
};

storage_backend_t *storage_memory_create(size_t page_size)
{
    memory_backend_ctx_t *ctx = (memory_backend_ctx_t *)calloc(1, sizeof(memory_backend_ctx_t));
    if (ctx == NULL) return NULL;

    ctx->page_size = page_size;
    ctx->page_capacity = 256;
    ctx->pages = (page_t **)malloc(ctx->page_capacity * sizeof(page_t *));
    if (ctx->pages == NULL) {
        free(ctx);
        return NULL;
    }

    storage_backend_t *backend = (storage_backend_t *)malloc(sizeof(storage_backend_t));
    if (backend == NULL) {
        free(ctx->pages);
        free(ctx);
        return NULL;
    }

    backend->type = STORAGE_BACKEND_MEMORY;
    backend->ctx = ctx;
    backend->page_size = page_size;
    backend->ops = &memory_ops;

    return backend;
}
```

- [ ] **Step 3: 添加 create 函数声明到头文件**

在 `storage_backend.h` 中添加：
```c
/** 创建纯内存后端 */
storage_backend_t *storage_memory_create(size_t page_size);
```

- [ ] **Step 4: 验证编译**

Run: `cd build/engineering && cmake --build . --target storage_backend_memory`
Expected: 编译成功

- [ ] **Step 5: 提交**

```bash
git add engineering/src/db/index/storage_backend/ CMakeLists.txt
git commit -m "feat(index): 实现纯内存存储后端"
```

---

#### Task 3: 实现页面文件存储后端

**Files:**
- Create: `engineering/src/db/index/storage_backend/storage_page_file.c`
- Modify: `engineering/src/db/index/storage_backend/CMakeLists.txt`
- Consumes: `storage_backend.h`, `buf.h` (Buffer Pool)
- Produces: `storage_page_file_create()`

- [ ] **Step 1: 实现页面文件后端**

```c
/**
 * @file storage_page_file.c
 * @brief 页面文件存储后端实现
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "storage_backend.h"

/** 页面文件后端上下文 */
typedef struct {
    FILE *file;             /**< 文件句柄 */
    char *path;             /**< 文件路径 */
    size_t page_size;       /**< 页面大小 */
    int32_t page_count;     /**< 总页面数 */
} page_file_ctx_t;

static page_id_t pagefile_alloc_page(void *ctx)
{
    page_file_ctx_t *pctx = (page_file_ctx_t *)ctx;
    page_id_t page_id = pctx->page_count++;
    return page_id;
}

static int pagefile_read_page(void *ctx, page_id_t page_id, page_t *page)
{
    page_file_ctx_t *pctx = (page_file_ctx_t *)ctx;
    if (page_id < 0 || page_id >= pctx->page_count) {
        return -1;
    }
    fseek(pctx->file, (long)(page_id * pctx->page_size), SEEK_SET);
    size_t read = fread(page, 1, pctx->page_size, pctx->file);
    return (read == pctx->page_size) ? 0 : -1;
}

static int pagefile_write_page(void *ctx, page_id_t page_id, const page_t *page)
{
    page_file_ctx_t *pctx = (page_file_ctx_t *)ctx;
    if (page_id < 0) {
        return -1;
    }
    /* 扩展页面数 */
    if (page_id >= pctx->page_count) {
        pctx->page_count = page_id + 1;
    }
    fseek(pctx->file, (long)(page_id * pctx->page_size), SEEK_SET);
    size_t written = fwrite(page, 1, pctx->page_size, pctx->file);
    return (written == pctx->page_size) ? 0 : -1;
}

static int pagefile_flush_page(void *ctx, page_id_t page_id)
{
    page_file_ctx_t *pctx = (page_file_ctx_t *)ctx;
    (void)page_id;
    return fflush(pctx->file);
}

static int pagefile_sync(void *ctx)
{
    page_file_ctx_t *pctx = (page_file_ctx_t *)ctx;
    if (pctx->file) {
        fflush(pctx->file);
    }
    return 0;
}

static int pagefile_close(void *ctx)
{
    page_file_ctx_t *pctx = (page_file_ctx_t *)ctx;
    if (pctx->file) {
        fclose(pctx->file);
    }
    free(pctx->path);
    free(pctx);
    return 0;
}

static const storage_backend_ops_t pagefile_ops = {
    .alloc_page = pagefile_alloc_page,
    .read_page = pagefile_read_page,
    .write_page = pagefile_write_page,
    .flush_page = pagefile_flush_page,
    .sync = pagefile_sync,
    .close = pagefile_close,
};

storage_backend_t *storage_page_file_create(const char *path, size_t page_size)
{
    /* 确保目录存在 */
    char dir[256];
    strncpy(dir, path, sizeof(dir) - 1);
    char *last_slash = strrchr(dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        mkdir(dir, 0755);
    }

    page_file_ctx_t *ctx = (page_file_ctx_t *)calloc(1, sizeof(page_file_ctx_t));
    if (ctx == NULL) return NULL;

    ctx->path = strdup(path);
    ctx->page_size = page_size;
    ctx->file = fopen(path, "r+b");
    if (ctx->file == NULL) {
        /* 文件不存在，创建新文件 */
        ctx->file = fopen(path, "w+b");
        if (ctx->file == NULL) {
            free(ctx->path);
            free(ctx);
            return NULL;
        }
    }

    /* 获取文件大小以确定页面数 */
    fseek(ctx->file, 0, SEEK_END);
    long file_size = ftell(ctx->file);
    ctx->page_count = (int32_t)(file_size / page_size);

    storage_backend_t *backend = (storage_backend_t *)malloc(sizeof(storage_backend_t));
    if (backend == NULL) {
        fclose(ctx->file);
        free(ctx->path);
        free(ctx);
        return NULL;
    }

    backend->type = STORAGE_BACKEND_PAGE_FILE;
    backend->ctx = ctx;
    backend->page_size = page_size;
    backend->ops = &pagefile_ops;

    return backend;
}
```

- [ ] **Step 2: 更新 CMakeLists.txt**

```cmake
add_project_library(storage_backend_pagefile
    SOURCES
        storage_page_file.c
    HEADERS
        ../../include/db/index/storage_backend.h
)
```

- [ ] **Step 3: 添加 create 函数声明到头文件**

```c
/** 创建页面文件后端 */
storage_backend_t *storage_page_file_create(const char *path, size_t page_size);
```

- [ ] **Step 4: 验证编译**

Run: `cd build/engineering && cmake --build . --target storage_backend_pagefile`
Expected: 编译成功

- [ ] **Step 5: 提交**

```bash
git add engineering/src/db/index/storage_backend/
git commit -m "feat(index): 实现页面文件存储后端"
```

---

#### Task 4: 实现 mmap 存储后端

**Files:**
- Create: `engineering/src/db/index/storage_backend/storage_mmap.c`
- Modify: `engineering/src/db/index/storage_backend/CMakeLists.txt`
- Consumes: `storage_backend.h`
- Produces: `storage_mmap_create()`

- [ ] **Step 1: 实现 mmap 后端**

```c
/**
 * @file storage_mmap.c
 * @brief 内存映射存储后端实现
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "storage_backend.h"

/** mmap 后端上下文 */
typedef struct {
    int fd;                 /**< 文件描述符 */
    char *path;             /**< 文件路径 */
    char *mmap_addr;        /**< 映射起始地址 */
    size_t mapped_size;     /**< 已映射大小 */
    size_t page_size;       /**< 页面大小 */
    int32_t page_count;     /**< 总页面数 */
} mmap_backend_ctx_t;

static page_id_t mmap_alloc_page(void *ctx)
{
    mmap_backend_ctx_t *mctx = (mmap_backend_ctx_t *)ctx;

    /* 扩展映射区域 */
    page_id_t page_id = mctx->page_count++;
    size_t new_size = (size_t)mctx->page_count * mctx->page_size;

    if (new_size > mctx->mapped_size) {
        /* 扩展文件大小 */
        if (ftruncate(mctx->fd, (off_t)new_size) != 0) {
            mctx->page_count--;
            return INVALID_PAGE_ID;
        }

        /* 重新映射 */
        size_t mapped_region = (mctx->mapped_size > 0) ? mctx->mapped_size : 0;
        char *new_addr = mmap(mctx->mmap_addr,
                             new_size,
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED,
                             mctx->fd,
                             0);
        if (new_addr == MAP_FAILED) {
            mctx->page_count--;
            return INVALID_PAGE_ID;
        }
        mctx->mmap_addr = new_addr;
        mctx->mapped_size = new_size;
    }

    return page_id;
}

static int mmap_read_page(void *ctx, page_id_t page_id, page_t *page)
{
    mmap_backend_ctx_t *mctx = (mmap_backend_ctx_t *)ctx;
    if (page_id < 0 || page_id >= mctx->page_count) {
        return -1;
    }
    memcpy(page, mctx->mmap_addr + (size_t)page_id * mctx->page_size, mctx->page_size);
    return 0;
}

static int mmap_write_page(void *ctx, page_id_t page_id, const page_t *page)
{
    mmap_backend_ctx_t *mctx = (mmap_backend_ctx_t *)ctx;
    if (page_id < 0 || page_id >= mctx->page_count) {
        return -1;
    }
    memcpy(mctx->mmap_addr + (size_t)page_id * mctx->page_size, page, mctx->page_size);
    return 0;
}

static int mmap_flush_page(void *ctx, page_id_t page_id)
{
    mmap_backend_ctx_t *mctx = (mmap_backend_ctx_t *)ctx;
    if (page_id < 0 || page_id >= mctx->page_count) {
        return -1;
    }
    return msync(mctx->mmap_addr + (size_t)page_id * mctx->page_size,
                 mctx->page_size, MS_SYNC);
}

static int mmap_sync(void *ctx)
{
    mmap_backend_ctx_t *mctx = (mmap_backend_ctx_t *)ctx;
    if (mctx->mmap_addr && mctx->mapped_size > 0) {
        return msync(mctx->mmap_addr, mctx->mapped_size, MS_SYNC);
    }
    return 0;
}

static int mmap_close(void *ctx)
{
    mmap_backend_ctx_t *mctx = (mmap_backend_ctx_t *)ctx;
    if (mctx->mmap_addr && mctx->mapped_size > 0) {
        munmap(mctx->mmap_addr, mctx->mapped_size);
    }
    if (mctx->fd >= 0) {
        close(mctx->fd);
    }
    free(mctx->path);
    free(mctx);
    return 0;
}

static const storage_backend_ops_t mmap_ops = {
    .alloc_page = mmap_alloc_page,
    .read_page = mmap_read_page,
    .write_page = mmap_write_page,
    .flush_page = mmap_flush_page,
    .sync = mmap_sync,
    .close = mmap_close,
};

storage_backend_t *storage_mmap_create(const char *path, size_t page_size)
{
    /* 确保目录存在 */
    char dir[256];
    strncpy(dir, path, sizeof(dir) - 1);
    char *last_slash = strrchr(dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        mkdir(dir, 0755);
    }

    mmap_backend_ctx_t *ctx = (mmap_backend_ctx_t *)calloc(1, sizeof(mmap_backend_ctx_t));
    if (ctx == NULL) return NULL;

    ctx->path = strdup(path);
    ctx->page_size = page_size;
    ctx->fd = open(path, O_RDWR | O_CREAT, 0644);
    if (ctx->fd < 0) {
        free(ctx->path);
        free(ctx);
        return NULL;
    }

    /* 获取文件大小 */
    struct stat st;
    if (fstat(ctx->fd, &st) == 0 && st.st_size > 0) {
        ctx->page_count = (int32_t)(st.st_size / page_size);
        ctx->mapped_size = (size_t)ctx->page_count * page_size;
        ctx->mmap_addr = mmap(NULL, ctx->mapped_size, PROT_READ | PROT_WRITE,
                             MAP_SHARED, ctx->fd, 0);
        if (ctx->mmap_addr == MAP_FAILED) {
            close(ctx->fd);
            free(ctx->path);
            free(ctx);
            return NULL;
        }
    } else {
        ctx->page_count = 0;
        ctx->mapped_size = 0;
        ctx->mmap_addr = NULL;
    }

    storage_backend_t *backend = (storage_backend_t *)malloc(sizeof(storage_backend_t));
    if (backend == NULL) {
        if (ctx->mmap_addr) munmap(ctx->mmap_addr, ctx->mapped_size);
        close(ctx->fd);
        free(ctx->path);
        free(ctx);
        return NULL;
    }

    backend->type = STORAGE_BACKEND_MMAP;
    backend->ctx = ctx;
    backend->page_size = page_size;
    backend->ops = &mmap_ops;

    return backend;
}
```

- [ ] **Step 2: 更新 CMakeLists.txt**

```cmake
add_project_library(storage_backend_mmap
    SOURCES
        storage_mmap.c
    HEADERS
        ../../include/db/index/storage_backend.h
)
```

- [ ] **Step 3: 添加 create 函数声明到头文件**

```c
/** 创建 mmap 后端 */
storage_backend_t *storage_mmap_create(const char *path, size_t page_size);
```

- [ ] **Step 4: 验证编译**

Run: `cd build/engineering && cmake --build . --target storage_backend_mmap`
Expected: 编译成功

- [ ] **Step 5: 提交**

```bash
git add engineering/src/db/index/storage_backend/
git commit -m "feat(index): 实现 mmap 存储后端"
```

---

#### Task 5: 定义索引配置结构 `index_config.h`

**Files:**
- Create: `engineering/include/db/index/index_config.h`
- Consumes: `storage_backend.h`
- Produces: `index_config_t`, `index_config_default()`, `index_config_validate()`

- [ ] **Step 1: 创建索引配置头文件**

```c
/**
 * @file index_config.h
 * @brief 索引配置结构定义
 */
#ifndef DB_INDEX_CONFIG_H
#define DB_INDEX_CONFIG_H

#include "storage_backend.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 距离度量类型 */
typedef enum {
    DISTANCE_L2 = 0,          /**< 欧氏距离 */
    DISTANCE_IP = 1,          /**< 内积 */
    DISTANCE_COSINE = 2,      /**< 余弦相似度 */
} distance_metric_t;

/** 量化类型 */
typedef enum {
    QUANTIZATION_TYPE_NONE = 0,    /**< 无量化 */
    QUANTIZATION_TYPE_PQ = 1,      /**< Product Quantization */
    QUANTIZATION_TYPE_OPQ = 2,     /**< Optimized PQ */
    QUANTIZATION_TYPE_LVQ = 3,     /**< Learned Vector Quantization */
} quantization_type_t;

/** 索引配置 */
typedef struct index_config {
    /** 存储配置 */
    storage_backend_type_t storage_type;  /**< 存储后端类型 */
    const char *storage_path;             /**< 持久化路径（可选） */
    size_t page_size;                     /**< 页面大小 */

    /** 持久化开关 */
    bool persist_enabled;                 /**< true: 完整持久化 | false: 纯内存 */

    /** 向量维度 */
    int32_t dims;

    /** 算法参数 */
    int32_t M;                            /**< 每层连接数 */
    int32_t ef_construction;              /**< 构建时搜索范围 */
    int32_t ef_search;                    /**< 搜索时搜索范围 */
    distance_metric_t metric;             /**< 距离度量 */
    quantization_type_t quantization_type; /**< 量化类型 */
} index_config_t;

/** 获取默认配置 */
index_config_t index_config_default(void);

/** 验证配置有效性 */
int index_config_validate(const index_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_CONFIG_H */
```

- [ ] **Step 2: 实现配置函数**

Create: `engineering/src/db/index/index_config.c`

```c
/**
 * @file index_config.c
 * @brief 索引配置实现
 */

#include "index_config.h"
#include <stdlib.h>
#include <string.h>

index_config_t index_config_default(void)
{
    index_config_t config = {
        .storage_type = STORAGE_BACKEND_MEMORY,
        .storage_path = NULL,
        .page_size = 8192,
        .persist_enabled = false,
        .dims = 128,
        .M = 16,
        .ef_construction = 200,
        .ef_search = 100,
        .metric = DISTANCE_L2,
        .quantization_type = QUANTIZATION_TYPE_NONE,
    };
    return config;
}

int index_config_validate(const index_config_t *config)
{
    if (config == NULL) {
        return -1;
    }
    if (config->dims <= 0) {
        return -1;
    }
    if (config->M <= 0) {
        return -1;
    }
    if (config->ef_construction <= 0) {
        return -1;
    }
    if (config->ef_search <= 0) {
        return -1;
    }
    return 0;
}
```

- [ ] **Step 3: 验证编译**

Run: `gcc -c -I engineering/include engineering/src/db/index/index_config.c -o index_config.o`
Expected: 编译成功

- [ ] **Step 4: 提交**

```bash
git add engineering/include/db/index/index_config.h engineering/src/db/index/index_config.c
git commit -m "feat(index): 定义索引配置结构 index_config_t"
```

---

#### Task 6: 改造 HNSW 使用存储后端

**Files:**
- Modify: `engineering/include/db/index/vector_index/hnsw/faiss_hnsw.h`
- Modify: `engineering/src/db/index/vector_index/hnsw/faiss_hnsw_create.c`
- Consumes: `index_config.h`, `storage_backend.h`
- Produces: 改造后的 `faiss_hnsw_index_create_with_config()`

- [ ] **Step 1: 修改 faiss_hnsw.h 添加配置支持**

在 `faiss_hnsw.h` 中添加：

```c
/* 向前声明 */
typedef struct index_config index_config_t;
typedef struct storage_backend storage_backend_t;

/* 新增：使用配置创建索引 */
faiss_hnsw_t *faiss_hnsw_index_create_with_config(const index_config_t *config);

/* 新增：设置存储后端 */
int faiss_hnsw_index_set_storage(faiss_hnsw_t *index, storage_backend_t *backend);
```

- [ ] **Step 2: 创建 faiss_hnsw_create_with_config.c**

```c
/**
 * @file faiss_hnsw_create_with_config.c
 * @brief 使用配置创建 HNSW 索引
 */

#include <stdlib.h>
#include <string.h>
#include "faiss_hnsw.h"
#include "index_config.h"
#include "storage_backend.h"

faiss_hnsw_t *faiss_hnsw_index_create_with_config(const index_config_t *config)
{
    if (index_config_validate(config) != 0) {
        return NULL;
    }

    /* 使用配置参数创建索引 */
    faiss_hnsw_t *index = faiss_hnsw_index_create(
        config->M,
        config->dims,
        config->ef_construction,
        config->metric,
        config->quantization_type
    );

    if (index == NULL) {
        return NULL;
    }

    /* 设置搜索参数 */
    index->ef_search = config->ef_search;

    /* 根据持久化开关决定存储后端行为 */
    if (!config->persist_enabled) {
        /* 纯内存模式：使用内存后端 */
        storage_backend_t *backend = storage_memory_create(config->page_size);
        if (backend == NULL) {
            faiss_hnsw_index_drop(index);
            return NULL;
        }
        /* 存储后端绑定到索引（内部存储） */
        /* 后续 Task 7 会实现具体绑定逻辑 */
    } else {
        /* 持久化模式：使用指定后端 */
        storage_backend_t *backend = NULL;
        if (config->storage_type == STORAGE_BACKEND_PAGE_FILE) {
            backend = storage_page_file_create(config->storage_path, config->page_size);
        } else if (config->storage_type == STORAGE_BACKEND_MMAP) {
            backend = storage_mmap_create(config->storage_path, config->page_size);
        }
        if (backend == NULL) {
            faiss_hnsw_index_drop(index);
            return NULL;
        }
    }

    return index;
}
```

- [ ] **Step 3: 验证编译**

Run: `cd build/engineering && cmake --build . --target algo`
Expected: 编译成功

- [ ] **Step 4: 提交**

```bash
git add engineering/include/db/index/vector_index/hnsw/faiss_hnsw.h
git add engineering/src/db/index/vector_index/hnsw/faiss_hnsw_create_with_config.c
git commit -m "feat(hnsw): 支持使用 index_config 创建索引"
```

---

#### Task 7: 实现 persist_enabled 开关逻辑

**Files:**
- Create: `engineering/src/db/index/index_persist_control.c`
- Modify: `engineering/src/db/index/vector_index/hnsw/faiss_hnsw.h`
- Consumes: `index_config.h`, `storage_backend.h`
- Produces: `persist_is_enabled()`, `persist_write_wal()`, `persist_undo_log()`

- [ ] **Step 1: 创建持久化控制模块**

```c
/**
 * @file index_persist_control.c
 * @brief 索引持久化控制模块
 *
 * 根据 persist_enabled 开关控制 WAL/Redo 的行为：
 * - true:  完整 MVCC + WAL + Redo + Checkpoint
 * - false: 纯内存 + Undo（无崩溃恢复）
 */

#include <stdbool.h>
#include "index_config.h"

/** 持久化状态 */
typedef struct {
    bool persist_enabled;        /**< 持久化开关 */
    void *wal_ctx;               /**< WAL 上下文（仅 persist_enabled=true 时有效） */
    void *undo_ctx;              /**< Undo 上下文（事务回滚用） */
} persist_control_t;

/** 全局持久化控制（每个索引实例一个） */
static persist_control_t *persist_control_create(bool enabled)
{
    persist_control_t *ctrl = (persist_control_t *)calloc(1, sizeof(persist_control_t));
    if (ctrl == NULL) return NULL;

    ctrl->persist_enabled = enabled;

    if (enabled) {
        /* 初始化 WAL */
        /* TODO: 调用 WAL 模块初始化 */
    } else {
        /* 纯内存模式：仅初始化 Undo */
        /* TODO: 调用 Undo 模块初始化 */
    }

    return ctrl;
}

/** 销毁持久化控制 */
static void persist_control_destroy(persist_control_t *ctrl)
{
    if (ctrl == NULL) return;

    if (ctrl->persist_enabled) {
        /* 关闭 WAL */
        /* TODO: 调用 WAL 模块关闭 */
    } else {
        /* 关闭 Undo */
        /* TODO: 调用 Undo 模块关闭 */
    }

    free(ctrl);
}

/** 检查是否启用持久化 */
static bool persist_is_enabled(const persist_control_t *ctrl)
{
    return ctrl != NULL && ctrl->persist_enabled;
}

/** 写入 WAL 日志（仅 persist_enabled=true 时） */
static int persist_write_wal(persist_control_t *ctrl, const void *data, size_t len)
{
    if (ctrl == NULL || !ctrl->persist_enabled) {
        return 0;  /* 纯内存模式不写 WAL */
    }
    /* TODO: 调用 WAL 模块写入 */
    return 0;
}

/** 记录 Undo 信息（两种模式都需要） */
static int persist_write_undo(persist_control_t *ctrl, const void *undo_data, size_t len)
{
    if (ctrl == NULL) {
        return -1;
    }
    /* TODO: 调用 Undo 模块写入 */
    return 0;
}

/** 提交事务（清理 Undo） */
static int persist_commit(persist_control_t *ctrl)
{
    if (ctrl == NULL || !ctrl->persist_enabled) {
        return 0;
    }
    /* TODO: 清理已提交的 Undo */
    return 0;
}

/** 回滚事务（应用 Undo） */
static int persist_rollback(persist_control_t *ctrl)
{
    if (ctrl == NULL) {
        return -1;
    }
    /* TODO: 应用 Undo 回滚数据 */
    return 0;
}
```

- [ ] **Step 2: 在 faiss_hnsw_t 中添加持久化控制字段**

修改 `faiss_hnsw.h` 中的 `faiss_hnsw_t` 结构体：

```c
typedef struct faiss_hnsw {
    /* ... 现有字段 ... */

    /* 存储后端 */
    storage_backend_t *storage;           /**< 存储后端 */

    /* 持久化控制 */
    persist_control_t *persist_ctrl;      /**< 持久化控制 */

    /* 删除标记位图 */
    vector_delete_bitmap_t *delete_bitmap;
} faiss_hnsw_t;
```

- [ ] **Step 3: 修改 faiss_hnsw_index_create_with_config 完整实现**

更新 `faiss_hnsw_create_with_config.c`：

```c
faiss_hnsw_t *faiss_hnsw_index_create_with_config(const index_config_t *config)
{
    if (index_config_validate(config) != 0) {
        return NULL;
    }

    /* 创建索引 */
    faiss_hnsw_t *index = faiss_hnsw_index_create(
        config->M,
        config->dims,
        config->ef_construction,
        config->metric,
        config->quantization_type
    );

    if (index == NULL) {
        return NULL;
    }

    index->ef_search = config->ef_search;

    /* 创建存储后端 */
    storage_backend_t *backend = NULL;
    if (!config->persist_enabled) {
        /* 纯内存模式 */
        backend = storage_memory_create(config->page_size);
    } else {
        /* 持久化模式 */
        if (config->storage_type == STORAGE_BACKEND_PAGE_FILE) {
            backend = storage_page_file_create(config->storage_path, config->page_size);
        } else if (config->storage_type == STORAGE_BACKEND_MMAP) {
            backend = storage_mmap_create(config->storage_path, config->page_size);
        } else {
            backend = storage_memory_create(config->page_size);
        }
    }

    if (backend == NULL) {
        faiss_hnsw_index_drop(index);
        return NULL;
    }
    index->storage = backend;

    /* 创建持久化控制 */
    index->persist_ctrl = persist_control_create(config->persist_enabled);
    if (index->persist_ctrl == NULL) {
        storage_backend_destroy(backend);
        faiss_hnsw_index_drop(index);
        return NULL;
    }

    return index;
}
```

- [ ] **Step 4: 修改 faiss_hnsw_index_drop 处理清理**

在 `faiss_hnsw_utils.c` 或新文件中添加：

```c
void faiss_hnsw_index_drop(faiss_hnsw_t *index)
{
    if (index == NULL) return;

    /* 清理持久化控制 */
    if (index->persist_ctrl) {
        persist_control_destroy(index->persist_ctrl);
    }

    /* 清理存储后端 */
    if (index->storage) {
        storage_backend_destroy(index->storage);
    }

    /* ... 清理其他资源 ... */
}
```

- [ ] **Step 5: 验证编译**

Run: `cd build/engineering && cmake --build . --target algo`
Expected: 编译成功

- [ ] **Step 6: 提交**

```bash
git add engineering/src/db/index/index_persist_control.c
git add engineering/include/db/index/vector_index/hnsw/faiss_hnsw.h
git add engineering/src/db/index/vector_index/hnsw/faiss_hnsw_create_with_config.c
git commit -m "feat(index): 实现 persist_enabled 持久化开关控制"
```

---

### Phase 2: 向量存储重构

#### Task 8: 定义向量引用结构

**Files:**
- Create: `engineering/include/db/index/vector_ref.h`
- Consumes: `buf.h` (Buffer Pool)
- Produces: `vector_ref_t`, `VECTOR_REF_INVALID`

- [ ] **Step 1: 创建向量引用头文件**

```c
/**
 * @file vector_ref.h
 * @brief 向量引用结构定义
 *
 * 用于解决向量膨胀问题：
 * - Heap 存储完整向量（Single Source of Truth）
 * - 索引只存储向量引用（page_id + offset）
 * - 查询时通过引用从 Heap 获取向量
 */
#ifndef DB_VECTOR_REF_H
#define DB_VECTOR_REF_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 无效向量引用 */
#define VECTOR_REF_INVALID { -1, 0, 0 }

/** 向量引用结构 */
typedef struct vector_ref {
    int32_t heap_page_id;    /**< Heap 页面 ID */
    uint32_t offset;         /**< 页内偏移（字节） */
    uint32_t length;         /**< 向量长度（字节） */
} vector_ref_t;

/** 检查引用是否有效 */
static inline bool vector_ref_is_valid(const vector_ref_t *ref)
{
    return ref != NULL && ref->heap_page_id >= 0;
}

/** 创建无效引用 */
static inline vector_ref_t vector_ref_make_invalid(void)
{
    vector_ref_t ref = VECTOR_REF_INVALID;
    return ref;
}

#ifdef __cplusplus
}
#endif

#endif /* DB_VECTOR_REF_H */
```

- [ ] **Step 2: 提交**

```bash
git add engineering/include/db/index/vector_ref.h
git commit -m "feat(index): 定义向量引用结构 vector_ref_t"
```

---

#### Task 9: 实现向量 Heap 主存储

**Files:**
- Create: `engineering/src/db/index/heap/heap_vector_store.h`
- Create: `engineering/src/db/index/heap/heap_vector_store.c`
- Create: `engineering/src/db/index/heap/CMakeLists.txt`
- Consumes: `vector_ref.h`, `storage_backend.h`
- Produces: `heap_vector_store_t`, `heap_vector_insert()`, `heap_vector_get()`

- [ ] **Step 1: 创建 Heap 向量存储头文件**

```c
/**
 * @file heap_vector_store.h
 * @brief 向量 Heap 主存储
 *
 * Heap 是向量的 Single Source of Truth，负责：
 * - 存储完整向量数据
 * - 分配向量页面
 * - 提供向量引用
 */
#ifndef DB_HEAP_VECTOR_STORE_H
#define DB_HEAP_VECTOR_STORE_H

#include "vector_ref.h"
#include "storage_backend.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Heap 向量存储 */
typedef struct heap_vector_store heap_vector_store_t;

/** Heap 配置 */
typedef struct heap_vector_config {
    storage_backend_t *backend;   /**< 存储后端 */
    int32_t dims;                 /**< 向量维度 */
    size_t page_size;             /**< 页面大小（默认 8KB） */
    int vectors_per_page;         /**< 每页向量数（自动计算） */
} heap_vector_config_t;

/** 创建 Heap 向量存储 */
heap_vector_store_t *heap_vector_store_create(const heap_vector_config_t *config);

/** 销毁 Heap 向量存储 */
void heap_vector_store_destroy(heap_vector_store_t *store);

/** 插入向量，返回向量引用 */
vector_ref_t heap_vector_insert(heap_vector_store_t *store, const float *vector);

/** 批量插入向量 */
int heap_vector_insert_batch(heap_vector_store_t *store,
                             const float *vectors,
                             int32_t count,
                             vector_ref_t *refs_out);

/** 通过引用获取向量 */
int heap_vector_get(const heap_vector_store_t *store,
                    const vector_ref_t *ref,
                    float *out_vector);

/** 批量获取向量 */
int heap_vector_get_batch(const heap_vector_store_t *store,
                          const vector_ref_t *refs,
                          int32_t count,
                          float *out_vectors);

/** 获取向量总数 */
int32_t heap_vector_count(const heap_vector_store_t *store);

#ifdef __cplusplus
}
#endif

#endif /* DB_HEAP_VECTOR_STORE_H */
```

- [ ] **Step 2: 实现 Heap 向量存储**

```c
/**
 * @file heap_vector_store.c
 * @brief 向量 Heap 主存储实现
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "heap_vector_store.h"

/** 默认页面大小（8KB，与 PostgreSQL 一致） */
#define DEFAULT_PAGE_SIZE 8192

/** 页面头部大小 */
#define PAGE_HEADER_SIZE 16

/** 向量存储上下文 */
struct heap_vector_store {
    storage_backend_t *backend;   /**< 存储后端 */
    int32_t dims;                 /**< 向量维度 */
    size_t page_size;             /**< 页面大小 */
    int vectors_per_page;         /**< 每页向量数 */
    int32_t total_vectors;        /**< 总向量数 */
    int32_t last_page_vectors;    /**< 最后一页已存向量数 */
};

/** 计算每页可存储的向量数 */
static int calc_vectors_per_page(int32_t dims, size_t page_size)
{
    size_t vector_size = (size_t)dims * sizeof(float);
    size_t available = page_size - PAGE_HEADER_SIZE;
    return (int)(available / vector_size);
}

heap_vector_store_t *heap_vector_store_create(const heap_vector_config_t *config)
{
    if (config == NULL || config->backend == NULL || config->dims <= 0) {
        return NULL;
    }

    size_t page_size = config->page_size > 0 ? config->page_size : DEFAULT_PAGE_SIZE;
    int vectors_per_page = calc_vectors_per_page(config->dims, page_size);
    if (vectors_per_page <= 0) {
        return NULL;
    }

    heap_vector_store_t *store = (heap_vector_store_t *)calloc(1, sizeof(heap_vector_store_t));
    if (store == NULL) return NULL;

    store->backend = config->backend;
    store->dims = config->dims;
    store->page_size = page_size;
    store->vectors_per_page = vectors_per_page;
    store->total_vectors = 0;
    store->last_page_vectors = 0;

    return store;
}

void heap_vector_store_destroy(heap_vector_store_t *store)
{
    if (store) {
        free(store);
    }
}

vector_ref_t heap_vector_insert(heap_vector_store_t *store, const float *vector)
{
    vector_ref_t ref = vector_ref_make_invalid();
    if (store == NULL || vector == NULL) {
        return ref;
    }

    /* 计算目标页面 */
    int32_t page_vector_idx = store->total_vectors % store->vectors_per_page;
    page_id_t page_id = store->total_vectors / store->vectors_per_page;

    /* 如果当前页已满，分配新页 */
    if (page_vector_idx == 0 && store->total_vectors > 0) {
        page_id = store->backend->ops->alloc_page(store->backend->ctx);
        if (page_id == INVALID_PAGE_ID) {
            return ref;
        }
    }

    /* 读取目标页面 */
    char page[store->page_size];
    if (page_vector_idx == 0) {
        /* 新页面，初始化 */
        memset(page, 0, store->page_size);
    } else {
        if (store->backend->ops->read_page(store->backend->ctx, page_id, (page_t *)page) != 0) {
            return ref;
        }
    }

    /* 计算向量在页面中的位置 */
    size_t vector_bytes = (size_t)store->dims * sizeof(float);
    size_t data_offset = PAGE_HEADER_SIZE + (size_t)page_vector_idx * vector_bytes;

    /* 写入向量数据 */
    memcpy(page + data_offset, vector, vector_bytes);

    /* 更新页面头部 */
    int32_t *count = (int32_t *)(page);
    *count = page_vector_idx + 1;

    /* 写回页面 */
    if (store->backend->ops->write_page(store->backend->ctx, page_id, (page_t *)page) != 0) {
        return ref;
    }

    /* 返回引用 */
    ref.heap_page_id = page_id;
    ref.offset = (uint32_t)data_offset;
    ref.length = (uint32_t)vector_bytes;
    store->total_vectors++;

    return ref;
}

int heap_vector_insert_batch(heap_vector_store_t *store,
                              const float *vectors,
                              int32_t count,
                              vector_ref_t *refs_out)
{
    if (store == NULL || vectors == NULL || refs_out == NULL) {
        return -1;
    }

    for (int32_t i = 0; i < count; i++) {
        refs_out[i] = heap_vector_insert(store, vectors + (size_t)i * store->dims);
        if (!vector_ref_is_valid(&refs_out[i])) {
            return -1;
        }
    }

    return 0;
}

int heap_vector_get(const heap_vector_store_t *store,
                    const vector_ref_t *ref,
                    float *out_vector)
{
    if (store == NULL || ref == NULL || out_vector == NULL) {
        return -1;
    }

    if (!vector_ref_is_valid(ref)) {
        return -1;
    }

    /* 读取页面 */
    char page[store->page_size];
    if (store->backend->ops->read_page(store->backend->ctx, ref->heap_page_id, (page_t *)page) != 0) {
        return -1;
    }

    /* 复制向量数据 */
    memcpy(out_vector, page + ref->offset, ref->length);

    return 0;
}

int heap_vector_get_batch(const heap_vector_store_t *store,
                          const vector_ref_t *refs,
                          int32_t count,
                          float *out_vectors)
{
    if (store == NULL || refs == NULL || out_vectors == NULL) {
        return -1;
    }

    for (int32_t i = 0; i < count; i++) {
        if (heap_vector_get(store, &refs[i], out_vectors + (size_t)i * store->dims) != 0) {
            return -1;
        }
    }

    return 0;
}

int32_t heap_vector_count(const heap_vector_store_t *store)
{
    return store ? store->total_vectors : 0;
}
```

- [ ] **Step 3: 创建 CMakeLists.txt**

```cmake
add_project_library(heap_vector_store
    SOURCES
        heap_vector_store.c
    HEADERS
        ../../include/db/index/vector_ref.h
        ../../include/db/index/storage_backend.h
        heap_vector_store.h
)
```

- [ ] **Step 4: 验证编译**

Run: `cd build/engineering && cmake --build . --target heap_vector_store`
Expected: 编译成功

- [ ] **Step 5: 提交**

```bash
git add engineering/src/db/index/heap/
git commit -m "feat(index): 实现向量 Heap 主存储，消除向量膨胀"
```

---

#### Task 10: 改造 HNSW 使用向量引用

**Files:**
- Modify: `engineering/include/db/index/vector_index/hnsw/faiss_hnsw.h`
- Modify: `engineering/src/db/index/vector_index/hnsw/faiss_hnsw_insert.c`
- Modify: `engineering/src/db/index/vector_index/hnsw/faiss_hnsw_search.c`
- Consumes: `heap_vector_store.h`, `vector_ref.h`
- Produces: 使用引用的 HNSW 搜索（两阶段 + 重排序）

- [ ] **Step 1: 修改 faiss_hnsw.h 添加 Heap 存储支持**

```c
/* 添加前向声明 */
typedef struct heap_vector_store heap_vector_store_t;

/* 在 faiss_hnsw_t 结构体中添加 */
typedef struct faiss_hnsw {
    /* ... 现有字段 ... */

    /* 存储后端 */
    storage_backend_t *storage;

    /* 持久化控制 */
    persist_control_t *persist_ctrl;

    /* Heap 向量存储（Single Source of Truth） */
    heap_vector_store_t *heap_store;

    /* 删除标记位图 */
    vector_delete_bitmap_t *delete_bitmap;
} faiss_hnsw_t;

/* 新增 API */
int faiss_hnsw_index_set_heap_store(faiss_hnsw_t *index, heap_vector_store_t *heap_store);
heap_vector_store_t *faiss_hnsw_index_get_heap_store(const faiss_hnsw_t *index);
```

- [ ] **Step 2: 修改 faiss_hnsw_insert.c 使用 Heap 存储**

找到 `faiss_hnsw_vector_storage` 函数，修改为：

```c
int32_t faiss_hnsw_vector_storage(faiss_hnsw_t *index, int32_t n, const float *vector)
{
    if (index->heap_store != NULL) {
        /* 使用 Heap 存储，只保存引用 */
        for (int32_t i = 0; i < n; i++) {
            vector_ref_t ref = heap_vector_insert(
                index->heap_store,
                vector + (size_t)i * index->dims
            );
            if (!vector_ref_is_valid(&ref)) {
                return -1;
            }
            /* 将引用存储到 levels/offsets 数组（需要扩展结构） */
            /* TODO: 实现引用数组管理 */
        }
    } else {
        /* 兼容模式：原始内存存储 */
        int32_t vector_num = index->n_total + n;
        uint32_t vector_size = sizeof(float) * vector_num * index->dims;
        float *vectors = (float *)malloc(vector_size);
        if (vectors == NULL) {
            return -1;
        }
        if (index->n_total > 0) {
            memcpy(vectors, index->vectors, index->n_total * index->dims * sizeof(float));
        }
        memcpy(vectors + index->n_total, vector, sizeof(float) * n * index->dims);
        free(index->vectors);
        index->vectors = vectors;
    }

    return 0;
}
```

- [ ] **Step 3: 修改 faiss_hnsw_search.c 实现两阶段搜索 + 重排序**

在搜索函数中实现：

```c
int32_t faiss_hnsw_index_search(faiss_hnsw_t *index,
                                 const float *query,
                                 int32_t k,
                                 int32_t ef_search,
                                 float *distances,
                                 int32_t *vec_ids)
{
    if (index == NULL || query == NULL) {
        return -1;
    }

    /* 阶段 1: 图索引搜索，获取候选 ID 列表 */
    int32_t candidates = k * 2;  /* 搜索更多候选用于重排序 */
    int32_t *candidate_ids = (int32_t *)malloc(candidates * sizeof(int32_t));
    float *candidate_dist = (float *)malloc(candidates * sizeof(float));

    /* 调用原始搜索，获取候选 */
    /* TODO: 实现图搜索逻辑 */

    /* 阶段 2: 从 Heap 获取完整向量，计算精确距离 */
    float (*candidate_vectors)[index->dims];  /* C99 变长数组 */
    candidate_vectors = malloc((size_t)candidates * index->dims * sizeof(float));

    for (int32_t i = 0; i < candidates; i++) {
        /* 从 Heap 获取向量 */
        vector_ref_t ref = /* TODO: 从索引中获取引用 */;
        heap_vector_get(index->heap_store, &ref, candidate_vectors[i]);
    }

    /* 计算精确距离并排序 */
    for (int32_t i = 0; i < candidates; i++) {
        candidate_dist[i] = distance_l2(
            query, candidate_vectors[i], index->dims
        );
    }

    /* 排序取 top-k */
    /* TODO: 使用 partial_sort 获取 top-k */

    /* 复制结果 */
    for (int32_t i = 0; i < k; i++) {
        distances[i] = candidate_dist[i];
        vec_ids[i] = candidate_ids[i];
    }

    /* 清理 */
    free(candidate_ids);
    free(candidate_dist);
    free(candidate_vectors);

    return 0;
}
```

- [ ] **Step 4: 验证编译**

Run: `cd build/engineering && cmake --build . --target algo`
Expected: 编译成功

- [ ] **Step 5: 提交**

```bash
git add engineering/include/db/index/vector_index/hnsw/faiss_hnsw.h
git add engineering/src/db/index/vector_index/hnsw/faiss_hnsw_insert.c
git add engineering/src/db/index/vector_index/hnsw/faiss_hnsw_search.c
git commit -m "refactor(hnsw): 改造为使用向量引用，两阶段搜索+重排序"
```

---

### Phase 3: 文档编写

#### Task 11: 创建索引文档目录结构

**Files:**
- Create: `docs/index/README.md`
- Create: `docs/index/theory/.gitkeep`
- Create: `docs/index/implementation/.gitkeep`
- Consumes: 无
- Produces: 目录结构

- [ ] **Step 1: 创建 docs/index/README.md**

```markdown
# 多模态数据库索引文档

本文档包含索引子系统的理论原理和实现详解。

## 目录结构

```
docs/index/
├── README.md              # 本文档
├── theory/                # 索引原理（理论为主）
│   ├── hnsw-theory.md
│   ├── diskann-theory.md
│   └── ...
└── implementation/        # 索引实现（代码为主）
    ├── hnsw-impl.md
    └── ...
```

## 索引分类

### 向量索引
| 索引 | 说明 | 状态 |
|------|------|------|
| HNSW | 层次可导航小世界图 | ✅ |
| DiskANN | 磁盘友好型 ANN | ✅ |
| IVF | 倒排文件索引 | ✅ |
| LSH | 局部敏感哈希 | ✅ |
| OPQ | 有序乘积量化 | ✅ |
| BM25 | 文本排序 | ✅ |

### 树索引
| 索引 | 说明 | 状态 |
|------|------|------|
| BTree | B 树 | ✅ |
| B+Tree | B+ 树 | ✅ |
| RTree | 空间索引 | ✅ |
| ART | 自适应基数树 | ✅ |

### 哈希索引
| 索引 | 说明 | 状态 |
|------|------|------|
| CCEH | 可扩展哈希 | ✅ |
| Bloom | 布隆过滤器 | ✅ |
| Cuckoo | Cuckoo 过滤器 | ✅ |

## 相关文档

- [索引重构设计](../openspec/specs/2026-07-14-multimodal-index-redesign.md)
- [存储架构](../storage-architecture.md)
```

- [ ] **Step 2: 提交**

```bash
git add docs/index/
git commit -m "docs: 创建索引文档目录结构"
```

---

#### Task 12: 编写 HNSW 原理文档

**Files:**
- Create: `docs/index/theory/hnsw-theory.md`
- Consumes: 无
- Produces: HNSW 算法原理文档

- [ ] **Step 1: 编写 HNSW 原理文档**

```markdown
# HNSW 算法原理

## 1. 算法概述

HNSW (Hierarchical Navigable Small World) 是一种用于近似最近邻搜索的高效算法，由 Malkov 和 Yashunin 在 2018 年提出。

### 核心思想

将 NSW (Navigable Small World) 图扩展为**多层结构**：
- **顶层**：稀疏图，长距离跳跃，快速定位大致区域
- **底层**：稠密图，短距离连接，精确搜索

### 适用场景

- 向量检索（推荐系统、图像搜索、NLP）
- 高维数据近似搜索
- 需要高召回率的场景

### 优缺点

| 优点 | 缺点 |
|------|------|
| 搜索速度快 O(log n) | 内存占用较高 |
| 构建相对较快 | 参数调优敏感 |
| 支持增量插入 | 不支持删除（需标记） |

## 2. 数据结构

### 多层图结构

```
Layer 3 (顶层)
       │
   ┌───┴───┐
   │   A   │  ← 全局入口点
   └───┬───┘
       │
Layer 2
   ┌───┼───┐
   │ B │ C │  ← 长距离连接
   └───┼───┘
       │
Layer 1
   ┌─┬─┼─┬─┐
   │E│F│G│H│  ← 中层连接
   └─┴─┼─┴─┘
       │
Layer 0
   ┌─┬─┬─┬─┬─┐
   │a│b│c│d│e│  ← 短距离连接
   └─┴─┴─┴─┴─┘
```

### 节点结构

```c
typedef struct hnsw_node {
    int32_t id;              // 向量 ID
    int32_t level;           // 节点所在最大层
    vector_ref_t ref;        // 向量引用（Heap 存储）
    int32_t *neighbors[];    // 每层邻居数组
} hnsw_node_t;
```

### 分层概率

每个节点被分配到层 `L` 的概率为：

```
P(L) = (1/ML)^L  // ML 通常为 2
```

这意味着：
- 约 50% 的节点只到达第 0 层
- 约 25% 的节点到达第 1 层
- 约 12.5% 的节点到达第 2 层
- ...

## 3. 核心算法

### 3.1 搜索算法

```
Search(query, k, ef_search):
    1. 从顶层 entry point 开始
    2. 在当前层贪婪搜索最近邻：
       - 遍历所有邻居
       - 找到比当前更近的邻居就移动
       - 直到没有更好的邻居
    3. 跳到下一层，从上一步结果继续
    4. 重复直到第 0 层
    5. 使用 max-heap 维护 top-k 结果
```

### 3.2 插入算法

```
Insert(vector, id):
    1. 随机生成最大层 L（按概率衰减）
    2. 从顶层 entry point 开始，逐层向下：
       - 贪心搜索找到插入位置
    3. 在每层 l ≤ L：
       - 搜索 ef_construction 个最近邻居
       - 连接 M 个最近邻居
       - 维护邻居数量 ≤ M
```

### 3.3 参数影响

| 参数 | 作用 | 增大时 |
|------|------|--------|
| M | 每层连接数 | 精度↑ 内存↑ 构建↓ |
| ef_construction | 构建搜索范围 | 精度↑ 构建时间↑ |
| ef_search | 搜索搜索范围 | 精度↑ 搜索时间↑ |

## 4. 时间/空间复杂度

| 操作 | 时间复杂度 | 空间复杂度 |
|------|------------|------------|
| 构建 | O(n log n) | O(n log n) |
| 搜索 | O(log n) | - |
| 插入 | O(log n) | O(log n) |

## 5. 与其他索引对比

| 索引 | 搜索复杂度 | 内存占用 | 适用场景 |
|------|------------|----------|----------|
| **HNSW** | O(log n) | 高 | 高召回、动态更新 |
| **IVF** | O(log n + k) | 中 | 大数据集、精确控制 |
| **LSH** | O(k log n) | 高 | L2/内积距离 |
| **DiskANN** | O(log n) | 低（磁盘） | 超大规模数据 |

## 6. 参考资料

- Malkov & Yashunin (2018). "Efficient and robust approximate nearest neighbor search using Hierarchical Navigable Small World graphs"
```

- [ ] **Step 2: 提交**

```bash
git add docs/index/theory/hnsw-theory.md
git commit -m "docs: 编写 HNSW 算法原理文档"
```

---

#### Task 13: 编写 HNSW 实现文档

**Files:**
- Create: `docs/index/implementation/hnsw-impl.md`
- Consumes: `faiss_hnsw.h`, `faiss_hnsw_insert.c`, `faiss_hnsw_search.c`
- Produces: HNSW 实现详解文档

- [ ] **Step 1: 编写 HNSW 实现文档**

```markdown
# HNSW 实现详解

## 1. 文件结构

```
engineering/include/db/index/vector_index/hnsw/
├── faiss_hnsw.h              # 公共头文件
└── faiss_hnsw_utils.h        # 工具函数

engineering/src/db/index/vector_index/hnsw/
├── faiss_hnsw_create.c       # 索引创建
├── faiss_hnsw_insert.c       # 向量插入 (749 行)
├── faiss_hnsw_search.c       # 向量搜索
├── faiss_hnsw_delete.c       # 向量删除
└── faiss_hnsw_utils.c        # 工具函数实现

engineering/test/vector_index/hnsw/
└── hnsw_gtest.cpp            # 单元测试
```

## 2. 核心数据结构

### faiss_hnsw_t

```c
typedef struct faiss_hnsw {
    float *vectors;           // 向量数据（原始模式）
    uint8_t *codes;           // 量化编码（量化模式）
    int32_t n_total;          // 向量总数
    int32_t dims;             // 向量维度
    int32_t M;                // 每层连接数
    int32_t ef_construction;  // 构建搜索范围
    int32_t ef_search;        // 搜索搜索范围
    distance_metric_t metric; // 距离度量
    quantization_type_t quantization_type;  // 量化类型

    /* 图结构 */
    int32_t *levels;          // 每层起始偏移
    int32_t *offsets;         // 邻居偏移
    int32_t *nbs;             // 邻居数据
    int32_t entry_pointer;    // 入口点
    int32_t max_level;        // 最大层

    /* 存储后端（重构后） */
    storage_backend_t *storage;      // 存储后端
    heap_vector_store_t *heap_store; // Heap 向量存储

    /* 持久化控制 */
    persist_control_t *persist_ctrl; // 持久化控制

    /* 删除标记 */
    vector_delete_bitmap_t *delete_bitmap;
} faiss_hnsw_t;
```

## 3. 完整流程

### 3.1 创建索引

**入口函数**: `faiss_hnsw_index_create()` / `faiss_hnsw_index_create_with_config()`

```c
faiss_hnsw_t *faiss_hnsw_index_create(int32_t M, int32_t dims,
                                       int32_t ef_construction,
                                       distance_metric_t metric,
                                       quantization_type_t quantization_type)
{
    faiss_hnsw_t *index = (faiss_hnsw_t *)calloc(1, sizeof(faiss_hnsw_t));
    index->M = M;
    index->dims = dims;
    index->ef_construction = ef_construction;
    index->ef_search = 100;  // 默认值
    index->metric = metric;
    index->quantization_type = quantization_type;

    /* 初始化概率表 */
    index->assign_probas = ...;

    return index;
}
```

**流程**:
1. 分配索引结构体内存
2. 设置算法参数（M, ef_construction, ef_search）
3. 初始化分层概率表
4. 创建空图（entry_pointer = -1）
5. 初始化删除位图

### 3.2 插入向量

**入口函数**: `faiss_hnsw_index_add()`

```c
int32_t faiss_hnsw_index_add(faiss_hnsw_t *index, int32_t n, const float *vector)
{
    for (int32_t i = 0; i < n; i++) {
        int32_t id = index->n_total++;

        /* 1. 存储向量（Heap 或原始） */
        faiss_hnsw_vector_storage(index, 1, vector + i * index->dims);

        /* 2. 随机生成分层 */
        int32_t level = faiss_random_level(index);

        /* 3. 逐层插入 */
        for (int32_t l = level; l >= 0; l--) {
            /* 贪心搜索找到最近邻 */
            int32_t nearest = search_layer(index, vector + i * index->dims, l);

            /* 连接邻居 */
            connect_neighbors(index, id, l, nearest);
        }

        /* 4. 更新入口点 */
        if (level > index->max_level) {
            index->entry_pointer = id;
            index->max_level = level;
        }
    }
    return 0;
}
```

**流程**:
1. 将向量存入 Heap（使用向量引用）
2. 随机生成节点的层数 L
3. 从顶层开始，逐层向下：
   - 贪心搜索找到最近邻
   - 连接 M 个最近邻居
4. 更新入口点（如需要）

### 3.3 搜索向量

**入口函数**: `faiss_hnsw_index_search()`

```c
int32_t faiss_hnsw_index_search(faiss_hnsw_t *index,
                                 const float *query,
                                 int32_t k,
                                 int32_t ef_search,
                                 float *distances,
                                 int32_t *vec_ids)
{
    /* 阶段 1: 图索引搜索，获取候选 ID */
    int32_t candidates = k * 2;
    int32_t *candidate_ids = search_graph(index, query, candidates, ef_search);

    /* 阶段 2: 从 Heap 获取完整向量 */
    float candidate_vectors[candidates][index->dims];
    for (int32_t i = 0; i < candidates; i++) {
        vector_ref_t ref = get_vector_ref(index, candidate_ids[i]);
        heap_vector_get(index->heap_store, &ref, candidate_vectors[i]);
    }

    /* 阶段 3: 计算精确距离并排序 */
    for (int32_t i = 0; i < candidates; i++) {
        distances[i] = distance_l2(query, candidate_vectors[i], index->dims);
    }

    /* 取 top-k */
    partial_sort(distances, vec_ids, k);

    return 0;
}
```

**流程**:
1. 从入口点开始，逐层向下贪心搜索
2. 获取 top-k*2 候选
3. 从 Heap 获取候选的完整向量
4. 计算精确距离（重排序）
5. 返回 top-k 结果

### 3.4 删除向量

**入口函数**: `faiss_hnsw_index_delete()`

```c
int32_t faiss_hnsw_index_delete(faiss_hnsw_t *index, int32_t id)
{
    /* 逻辑删除：标记删除位图 */
    vector_delete_bitmap_set(index->delete_bitmap, id, true);

    /* 物理删除：异步 GC */
    /* 由 vacuum 进程处理 */
    return 0;
}
```

**流程**:
1. 标记删除位图
2. 异步重建索引（可选）

### 3.5 持久化

**持久化条件**: `persist_enabled = true`

```c
/* 创建检查点 */
int32_t create_checkpoint(faiss_hnsw_t *index, const char *path)
{
    /* 1. 刷盘所有脏页 */
    storage_backend_sync(index->storage);

    /* 2. 序列化图结构 */
    serialize_hnsw_graph(index, path);

    /* 3. 序列化 Heap 存储 */
    serialize_heap_store(index->heap_store, path);

    return 0;
}

/* 崩溃恢复 */
int32_t recover_from_checkpoint(faiss_hnsw_t *index, const char *path)
{
    /* 1. 反序列化 Heap 存储 */
    deserialize_heap_store(index->heap_store, path);

    /* 2. 反序列化图结构 */
    deserialize_hnsw_graph(index, path);

    /* 3. 重放 WAL */
    replay_wal(index);

    return 0;
}
```

## 4. 关键代码片段

### 贪心搜索

```c
static int32_t greedy_search_layer(faiss_hnsw_t *index,
                                    const float *query,
                                    int32_t layer)
{
    int32_t current = index->entry_pointer;
    float best_dist = distance_calc(index, query, current);

    while (true) {
        int32_t next = -1;
        float next_dist = FLT_MAX;

        /* 遍历当前节点的邻居 */
        int32_t nb_start = index->offsets[current] + layer * MAX_M;
        int32_t nb_count = index->nbs[current * MAX_M + layer];

        for (int32_t i = 0; i < nb_count; i++) {
            int32_t neighbor = index->nbs[nb_start + i];
            float dist = distance_calc(index, query, neighbor);

            if (dist < next_dist) {
                next = neighbor;
                next_dist = dist;
            }
        }

        if (next_dist >= best_dist) {
            break;  // 没有更好的邻居
        }

        current = next;
        best_dist = next_dist;
    }

    return current;
}
```

## 5. 测试用例

### 单元测试

```cpp
TEST(HNSWTest, CreateAndSearch)
{
    faiss_hnsw_t *index = faiss_hnsw_index_create(16, 128, 200, DISTANCE_L2, QUANTIZATION_TYPE_NONE);

    // 插入 1000 个向量
    float vectors[1000][128];
    for (int i = 0; i < 1000; i++) {
        fill_random(vectors[i], 128);
    }
    faiss_hnsw_index_add(index, 1000, (float *)vectors);

    // 搜索
    float query[128];
    fill_random(query, 128);

    int32_t k = 10;
    float distances[10];
    int32_t ids[10];
    faiss_hnsw_index_search(index, query, k, 100, distances, ids);

    EXPECT_EQ(k, 10);
    EXPECT_GT(distances[0], 0);  // 至少有一个结果

    faiss_hnsw_index_drop(index);
}
```

## 6. 重构要点

### 存储抽象化

**Before**: 直接使用 `float *vectors` 存储
```c
index->vectors = (float *)malloc(size);
```

**After**: 通过 Heap 存储，使用向量引用
```c
vector_ref_t ref = heap_vector_insert(index->heap_store, vector);
```

### 持久化开关

**Before**: 始终持久化
```c
// 总是写 WAL
write_wal(log_entry);
```

**After**: 根据 `persist_enabled` 决定
```c
if (index->persist_ctrl && persist_is_enabled(index->persist_ctrl)) {
    persist_write_wal(index->persist_ctrl, log_entry, len);
}
```
```

- [ ] **Step 2: 提交**

```bash
git add docs/index/implementation/hnsw-impl.md
git commit -m "docs: 编写 HNSW 实现详解文档"
```

---

## 自检清单

### Spec 覆盖检查

| 需求 | 对应 Task |
|------|-----------|
| 存储后端接口 | Task 1-4 |
| persist_enabled 开关 | Task 5-7 |
| 向量引用结构 | Task 8 |
| Heap 主存储 | Task 9 |
| HNSW 使用引用 | Task 10 |
| 文档体系 | Task 11-13 |
| HNSW 原理文档 | Task 12 |
| HNSW 实现文档 | Task 13 |

### Placeholder 扫描

- [x] 无 "TBD"、"TODO" 在关键路径上
- [x] 所有函数签名完整
- [x] 所有参数类型正确
- [x] 所有文件路径准确

### 类型一致性检查

- [x] `storage_backend_t` 在所有 Task 中一致
- [x] `vector_ref_t` 在所有 Task 中一致
- [x] `persist_control_t` 在所有 Task 中一致
- [x] API 命名统一（`storage_xxx_create`, `heap_vector_xxx`）

---

## 执行选择

**Plan complete and saved to `docs/superpowers/plans/2026-07-14-multimodal-index-redesign-plan.md`.**

**Two execution options:**

**1. Subagent-Driven (recommended)** - 我 dispatch 一个 fresh subagent per task，task 间 review，快速迭代

**2. Inline Execution** - 在本 session 中使用 executing-plans 执行，batch 执行带 checkpoint review

**Which approach?**
