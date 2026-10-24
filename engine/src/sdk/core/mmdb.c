/**
 * @file mmdb.c
 * @brief mmdb 生命周期：open / close / last_error / version
 */
#include "sdk/mmdb.h"
#include "sdk/mmdb_error.h"
#include "sdk/mmdb_version.h"
#include "sdk/impl/mmdb_internal.h"
#include "sdk/impl/mmdb_memctx.h"  /* Task 8：SDK 兼容层内存上下文 */
#include "sdk/impl/sqlite_backend.h"
#include "sdk/impl/collection.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* 内部辅助                                                            */
/* ------------------------------------------------------------------ */

/*
 * Task 11：在指定 MemoryContext 上 strdup，失败返回 NULL。
 * 主导出形式：Core 与 Vectors 等新代码应当用此 API 进入 db 的内存上下文层级。
 */
char* mmdb_strdup_in_ctx(MemoryContext ctx, const char* s) {
    if (!s) return NULL;
    if (!ctx) {
        /* 兼容降级：ctx 为空时回退到 malloc（仅用于初始化前场景） */
        size_t n = strlen(s);
        char* p = (char*)malloc(n + 1);
        if (!p) return NULL;
        memcpy(p, s, n + 1);
        return p;
    }
    return mmdb_mem_strdup(ctx, s);
}

/*
 * 旧的 strdup 入口，Task 11 之后保留作为兼容层：仅用于 graph.c / filter_parser.c
 * 等未迁移模块。Core 与 Vectors 等迁移完成的代码请改用 mmdb_strdup_in_ctx()。
 */
char* mmdb_strdup_internal(const char* s) {
    /* 直接退回 malloc，调用方负责 free。
     * Task 11 备注：graph.c / filter_parser.c 中尚未迁移的 13 处调用仍依赖此行为。 */
    if (!s) return NULL;
    size_t n = strlen(s);
    char* p = (char*)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

/**
 * @brief 初始化数据库三层内存上下文
 *
 * 创建 DatabaseContext / ConnectionContext / CacheContext 三层结构。
 * ConnectionContext 和 CacheContext 都是 DatabaseContext 的子节点。
 * 创建顺序为根先建，子节点后建；任一子节点创建失败即触发根节点统一回滚。
 *
 * @param db 数据库句柄（不可为 NULL）
 * @return MMDB_OK 成功；MMDB_ERR_INVALID 入参无效；MMDB_ERR_NOMEM 分配失败
 */
static int mmdb_init_contexts(mmdb_t* db) {
    if (!db) return MMDB_ERR_INVALID;

    /* DatabaseContext — 根上下文 */
    db->memory_context = mmdb_memctx_create(NULL, "DatabaseContext", 0);
    if (!db->memory_context) return MMDB_ERR_NOMEM;

    /* ConnectionContext — 数据库连接级上下文 */
    db->connection_context = mmdb_memctx_create(db->memory_context,
                                                 "ConnectionContext", 0);
    if (!db->connection_context) {
        mmdb_memctx_delete(db->memory_context);
        db->memory_context = NULL;
        return MMDB_ERR_NOMEM;
    }

    /* CacheContext — 缓存级上下文（与 ConnectionContext 同级） */
    db->cache_context = mmdb_memctx_create(db->memory_context,
                                            "CacheContext", 0);
    if (!db->cache_context) {
        mmdb_memctx_delete(db->connection_context);
        db->connection_context = NULL;
        mmdb_memctx_delete(db->memory_context);
        db->memory_context = NULL;
        return MMDB_ERR_NOMEM;
    }

    return MMDB_OK;
}

/**
 * @brief 销毁数据库三层内存上下文
 *
 * 通过删除根上下文（DatabaseContext）触发 LIFO 递归销毁所有子上下文。
 * 所有在上下文中分配的内存（包括 path / last_err_msg / collections 等）
 * 统一回收，调用方无需手动释放。
 *
 * @param db 数据库句柄（NULL 安全）
 */
static void mmdb_destroy_contexts(mmdb_t* db) {
    if (!db) return;
    if (db->memory_context) {
        MemoryContextDelete(db->memory_context);
        db->memory_context = NULL;
    }
    db->connection_context = NULL;
    db->cache_context = NULL;
}

/**
 * @brief SQLite 句柄析构器（注册到 connection_context）
 *
 * 通过 mmdb_mem_register_resource 注册后，在 context delete 时由 LIFO 顺序
 * 自动调用，实现 SQLite 句柄随 memory_context 生命周期统一回收。
 *
 * @param resource 资源指针（实际为 sqlite3*）
 * @param arg      附加参数（未使用）
 */
static void mmdb_destroy_sqlite_handle(void* resource, void* arg) {
    (void)arg;
    if (resource) {
        sqlite3_close((sqlite3*)resource);
    }
}

void mmdb_set_error(mmdb_t* db, int code, const char* msg) {
    if (!db) return;
    db->last_err = code;

    /*
     * 释放旧错误信息：path / last_err_msg 由 memory_context 统一管理，
     * 不再单独 free；将指针置 NULL 让后续覆盖分配时 ctx 自然覆盖旧块。
     */
    db->last_err_msg = NULL;

    if (!msg || !db->memory_context) return;

    size_t n = strlen(msg);
    if (n == 0) return;
    /* 长度裁剪到 MMDB_ERR_MSG_MAX-1 保留末尾 \0 空间 */
    if (n >= MMDB_ERR_MSG_MAX) n = MMDB_ERR_MSG_MAX - 1;

    char* buf = (char*)mmdb_mem_alloc(db->memory_context, n + 1);
    if (buf) {
        memcpy(buf, msg, n);
        buf[n] = '\0';
        db->last_err_msg = buf;
    }
}

/* ------------------------------------------------------------------ */
/* mmdb_open / mmdb_close                                              */
/* ------------------------------------------------------------------ */

mmdb_t* mmdb_open(const char* path, const mmdb_options_t* opts) {
    if (!path) return NULL;

    mmdb_options_t effective = opts ? *opts : (mmdb_options_t)MMDB_OPTIONS_DEFAULT;

    mmdb_t* db = (mmdb_t*)calloc(1, sizeof(mmdb_t));
    if (!db) return NULL;

    /* 1. 优先创建三层内存上下文（Task 8 集成） */
    int ctx_rc = mmdb_init_contexts(db);
    if (ctx_rc != MMDB_OK) {
        free(db);
        return NULL;
    }

    /* 2. path 由 memory_context 统一管理（末尾 \0 含在 strdup 中） */
    db->path = mmdb_mem_strdup(db->memory_context, path);
    if (!db->path) {
        mmdb_destroy_contexts(db);
        free(db);
        return NULL;
    }
    db->options = effective;
    db->last_err = MMDB_OK;

    if (mmdb_rwlock_init(&db->lock) != 0) {
        mmdb_destroy_contexts(db);
        free(db);
        return NULL;
    }

    char err_buf[MMDB_ERR_MSG_MAX] = {0};
    int rc = mmdb_sqlite_open(path, &effective, &db->db, err_buf, sizeof(err_buf));
    if (rc != MMDB_OK) {
        mmdb_set_error(db, rc, err_buf[0] ? err_buf : "open failed");
        mmdb_rwlock_destroy(&db->lock);
        mmdb_destroy_contexts(db);
        free(db);
        return NULL;
    }

    /* 注册 SQLite 句柄到 connection_context，由 context delete 自动回收 */
    mmdb_mem_register_resource(db->connection_context, db->db,
                               mmdb_destroy_sqlite_handle, NULL, "sqlite3");

    rc = mmdb_sqlite_bootstrap(db->db);
    if (rc != MMDB_OK) {
        mmdb_set_error(db, rc, "bootstrap failed");
        mmdb_destroy_contexts(db);
        free(db);
        return NULL;
    }

    rc = mmdb_collection_init(db);
    if (rc != MMDB_OK) {
        mmdb_set_error(db, rc, "collection init failed");
        mmdb_destroy_contexts(db);
        free(db);
        return NULL;
    }

    rc = mmdb_collection_load_all(db);
    if (rc != MMDB_OK) {
        mmdb_set_error(db, rc, "load collections failed");
        mmdb_collection_dispose(db);
        mmdb_destroy_contexts(db);
        free(db);
        return NULL;
    }

    return db;
}

void mmdb_close(mmdb_t* db) {
    if (!db) return;

    /*
     * 关闭顺序：
     * 1. 释放所有 collection（collection 自身持有 HNSW 等资源）
     * 2. 销毁内存上下文（递归释放 path / last_err_msg /
     *    collections 数组 / SQLite 句柄 注册资源等所有由 ctx 分配的内存）
     * 3. 销毁读写锁（必须在所有读操作结束后释放）
     *
     * 关键变化：
     * - 不再单独 free(db->path) 与 free(db->last_err_msg)，由 memory_context 回收
     * - 不再单独调用 sqlite3_close(db->db)，由 connection_context 资源析构回收
     * - 不再单独 free(db->collections) 与 free(col)，由 memory_context 回收
     */
    mmdb_collection_dispose(db);
    mmdb_destroy_contexts(db);
    mmdb_rwlock_destroy(&db->lock);
    free(db);
}

int mmdb_last_error_code(mmdb_t* db) {
    if (!db) return MMDB_ERR_INVALID;
    return db->last_err;
}

const char* mmdb_last_error_message(mmdb_t* db) {
    if (!db) return mmdb_strerror(MMDB_ERR_INVALID);
    if (db->last_err_msg && db->last_err_msg[0]) return db->last_err_msg;
    return mmdb_strerror(db->last_err);
}

/* ------------------------------------------------------------------ */
/* 版本查询                                                            */
/* ------------------------------------------------------------------ */

void mmdb_version(int* major, int* minor, int* patch) {
    if (major) *major = MMDB_VERSION_MAJOR;
    if (minor) *minor = MMDB_VERSION_MINOR;
    if (patch) *patch = MMDB_VERSION_PATCH;
}

/* ========================================================================
 * 请求级内存上下文作用域（Task 9）
 * ======================================================================== */

/**
 * @brief 开始请求级内存作用域
 *
 * 在 connection_context 下创建请求上下文并切换为当前上下文。
 * 后续所有 palloc 操作将在 scope->context 中进行。
 *
 * 失败语义：
 * - db/scope 为 NULL → MMDB_ERR_INVALID
 * - db->connection_context 未初始化 → MMDB_ERR_INVALID
 * - AllocSetContextCreate 失败 → MMDB_ERR_NOMEM
 */
int mmdb_request_begin(mmdb_t* db, const char* name, mmdb_request_scope_t* scope) {
    if (!db || !scope) {
        return MMDB_ERR_INVALID;
    }
    if (!db->connection_context) {
        return MMDB_ERR_INVALID;
    }

    scope->db = db;
    scope->previous = MemoryContextCurrent();
    scope->context = AllocSetContextCreate(
        db->connection_context,   /* parent */
        name,                     /* name */
        0,                        /* minContextSize */
        8192,                     /* initBlockSize */
        1024 * 1024,              /* maxBlockSize */
        ALLOCSET_PRESET_DEFAULT   /* preset */
    );
    if (!scope->context) {
        return MMDB_ERR_NOMEM;
    }

    scope->active = 1;
    MemoryContextSwitchTo(scope->context);
    return MMDB_OK;
}

/**
 * @brief 结束请求级内存作用域
 *
 * 恢复之前的内存上下文，销毁请求上下文（自动释放所有分配）。
 * 对非活跃或 NULL 作用域安全（no-op）。
 */
void mmdb_request_end(mmdb_request_scope_t* scope) {
    if (!scope || !scope->active) {
        return;
    }

    /* 恢复之前的上下文 */
    MemoryContextSwitchTo(scope->previous);

    /* 销毁请求上下文及其所有子上下文（资源析构 + 块释放） */
    MemoryContextDelete(scope->context);

    scope->context = NULL;
    scope->active = 0;
}