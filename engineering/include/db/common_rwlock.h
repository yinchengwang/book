/*
 * common_rwlock.h - 公共读写锁封装
 *
 * 提供跨模态复用的并发控制原语，封装 pthread_rwlock，
 * 支持读锁（可重入）和写锁（独占），写者优先避免写者饥饿。
 */

#ifndef COMMON_RWLOCK_H
#define COMMON_RWLOCK_H

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

typedef struct {
    pthread_rwlock_t rwlock;
    bool            use_lock;
    const char*     name;
} common_rwlock_t;

// 创建锁，name 用于调试输出
extern common_rwlock_t* common_rwlock_create(const char* name);

// 销毁锁
extern void common_rwlock_destroy(common_rwlock_t* lock);

// 读锁（可重入）
extern void common_rwlock_read_lock(common_rwlock_t* lock);
extern void common_rwlock_read_unlock(common_rwlock_t* lock);

// 写锁（独占）
extern void common_rwlock_write_lock(common_rwlock_t* lock);
extern void common_rwlock_write_unlock(common_rwlock_t* lock);

// 尝试获取写锁，超时返回 false
extern bool common_rwlock_try_write_lock(common_rwlock_t* lock, int timeout_ms);

#endif // COMMON_RWLOCK_H
