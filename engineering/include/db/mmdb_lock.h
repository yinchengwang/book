/**
 * @file mmdb_lock.h
 * @brief 跨平台读写锁原语（数据库公共层）
 *
 * 提供统一的读写锁抽象，所有存储模态（Vector / Timeseries / Document /
 * Graph / Spatial / KV）必须使用本原语进行并发保护，禁止自行实现。
 *
 * 实现选择：
 *   - Windows：SRWLOCK（Win API 内核态原语）
 *   - POSIX  ：pthread_rwlock_t（POSIX 1003.1 标准原语）
 *
 * 原 SDK 路径 `sdk/impl/mmdb_lock.h` 保留为兼容 shim，本文件为 canonical。
 */
#ifndef DB_MMDB_LOCK_H
#define DB_MMDB_LOCK_H

#ifdef _WIN32
#include <windows.h>
typedef SRWLOCK mmdb_rwlock_t;
#else
#include <pthread.h>
typedef pthread_rwlock_t mmdb_rwlock_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

int mmdb_rwlock_init(mmdb_rwlock_t *lock);
int mmdb_rwlock_rdlock(mmdb_rwlock_t *lock);
int mmdb_rwlock_wrlock(mmdb_rwlock_t *lock);
int mmdb_rwlock_unlock(mmdb_rwlock_t *lock, int is_wrlock);
int mmdb_rwlock_destroy(mmdb_rwlock_t *lock);

/* ========================================================================
 * 关闭锁的约定（C0-1）
 *
 * C0-1 后所有存储引擎的 use_lock 默认 true。如需关闭（仅限 benchmark /
 * 单线程调试场景），通过各引擎已有的 *_enable_lock(handle, false) 接口：
 *
 *   vector_engine_enable_lock(db, false);
 *   ts_engine_enable_lock(db, false);
 *   doc_engine_enable_lock(db, false);
 *   graph_engine_enable_lock(db, false);
 *   graph_csr_enable_lock(csr, false);
 *   rtree_enable_lock(tree, false);
 *
 * 本头文件不维护全局关闭标志，避免跨模块耦合；如需统一关闭请遍历引擎
 * 句柄列表统一调用。
 * ======================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* DB_MMDB_LOCK_H */
