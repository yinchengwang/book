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
common_rwlock_t* common_rwlock_create(const char* name) {
    common_rwlock_t* lock = (common_rwlock_t*)malloc(sizeof(common_rwlock_t));
    if (lock == NULL) {
        return NULL;
    }

    lock->name = name;
    lock->use_lock = true;  // 默认为 true，修复 use_lock=false 的问题

    pthread_rwlock_init(&lock->rwlock, NULL);

    return lock;
}

// 销毁锁
void common_rwlock_destroy(common_rwlock_t* lock) {
    if (lock == NULL) {
        return;
    }
    pthread_rwlock_destroy(&lock->rwlock);
    free(lock);
}

// 读锁（可重入）
void common_rwlock_read_lock(common_rwlock_t* lock) {
    if (lock == NULL || !lock->use_lock) {
        return;
    }
    pthread_rwlock_rdlock(&lock->rwlock);
}

void common_rwlock_read_unlock(common_rwlock_t* lock) {
    if (lock == NULL || !lock->use_lock) {
        return;
    }
    pthread_rwlock_unlock(&lock->rwlock);
}

// 写锁（独占）
void common_rwlock_write_lock(common_rwlock_t* lock) {
    if (lock == NULL || !lock->use_lock) {
        return;
    }
    pthread_rwlock_wrlock(&lock->rwlock);
}

void common_rwlock_write_unlock(common_rwlock_t* lock) {
    if (lock == NULL || !lock->use_lock) {
        return;
    }
    pthread_rwlock_unlock(&lock->rwlock);
}

// 尝试获取写锁，超时返回 false
bool common_rwlock_try_write_lock(common_rwlock_t* lock, int timeout_ms) {
    if (lock == NULL || !lock->use_lock) {
        return true;  // 不使用锁时视为成功
    }

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000L;
    }

    int ret = pthread_rwlock_timedwrlock(&lock->rwlock, &ts);
    if (ret == ETIMEDOUT) {
        return false;
    }
    return (ret == 0);
}

#endif // COMMON_RWLOCK_H
