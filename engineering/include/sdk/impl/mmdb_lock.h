#ifndef MMDB_LOCK_H
#define MMDB_LOCK_H

#ifdef _WIN32
#include <windows.h>
typedef SRWLOCK mmdb_rwlock_t;
#else
#include <pthread.h>
typedef pthread_rwlock_t mmdb_rwlock_t;
#endif

int mmdb_rwlock_init(mmdb_rwlock_t *lock);
int mmdb_rwlock_rdlock(mmdb_rwlock_t *lock);
int mmdb_rwlock_wrlock(mmdb_rwlock_t *lock);
int mmdb_rwlock_unlock(mmdb_rwlock_t *lock, int is_wrlock);
int mmdb_rwlock_destroy(mmdb_rwlock_t *lock);

#endif /* MMDB_LOCK_H */
