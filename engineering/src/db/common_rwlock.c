#include <db/common_rwlock.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

common_rwlock_t* common_rwlock_create(const char* name) {
    common_rwlock_t* lock = calloc(1, sizeof(common_rwlock_t));
    if (!lock) return NULL;

    pthread_rwlockattr_t attr;
    pthread_rwlockattr_init(&attr);
#ifdef __linux__
    // 写者优先，避免写者饥饿（仅 Linux 支持）
    pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
#endif

    if (pthread_rwlock_init(&lock->rwlock, &attr) != 0) {
        free(lock);
        return NULL;
    }

    lock->use_lock = true;  // 默认开启锁
    lock->name = name;
    pthread_rwlockattr_destroy(&attr);

    return lock;
}

void common_rwlock_destroy(common_rwlock_t* lock) {
    if (!lock) return;
    pthread_rwlock_destroy(&lock->rwlock);
    free(lock);
}

void common_rwlock_read_lock(common_rwlock_t* lock) {
    if (!lock || !lock->use_lock) return;
    pthread_rwlock_rdlock(&lock->rwlock);
}

void common_rwlock_read_unlock(common_rwlock_t* lock) {
    if (!lock || !lock->use_lock) return;
    pthread_rwlock_unlock(&lock->rwlock);
}

void common_rwlock_write_lock(common_rwlock_t* lock) {
    if (!lock || !lock->use_lock) return;
    pthread_rwlock_wrlock(&lock->rwlock);
}

void common_rwlock_write_unlock(common_rwlock_t* lock) {
    if (!lock || !lock->use_lock) return;
    pthread_rwlock_unlock(&lock->rwlock);
}

bool common_rwlock_try_write_lock(common_rwlock_t* lock, int timeout_ms) {
    if (!lock || !lock->use_lock) return true;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;

    return pthread_rwlock_timedwrlock(&lock->rwlock, &ts) == 0;
}
