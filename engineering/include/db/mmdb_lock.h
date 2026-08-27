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

#ifdef __cplusplus
}
#endif

#endif /* DB_MMDB_LOCK_H */
