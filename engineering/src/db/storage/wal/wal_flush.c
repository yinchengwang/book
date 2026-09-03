/**
 * @file wal_flush.c
 * @brief WAL 刷盘策略实现（C0-2）
 */
#include "db/storage/wal/wal_flush.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

/* 全局刷盘策略（默认 FSYNC） */
wal_flush_policy_t g_wal_flush_policy = WAL_FLUSH_FSYNC;

int db_fsync(int fd) {
    if (fd < 0) {
        errno = EINVAL;
        return -1;
    }
#ifdef _WIN32
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        return -1;
    }
    if (!FlushFileBuffers(h)) {
        errno = EINVAL;
        return -1;
    }
    return 0;
#else
    return fsync(fd);
#endif
}

void wal_flush_set_policy(wal_flush_policy_t policy) {
    switch (policy) {
        case WAL_FLUSH_NONE:
        case WAL_FLUSH_OS:
        case WAL_FLUSH_FSYNC:
        case WAL_FLUSH_BATCH:
            g_wal_flush_policy = policy;
            break;
        default:
            /* 非法值保持原状 */
            break;
    }
}

wal_flush_policy_t wal_flush_get_policy(void) {
    return g_wal_flush_policy;
}

wal_flush_policy_t wal_flush_parse(const char *name) {
    if (name == NULL || name[0] == '\0') return WAL_FLUSH_FSYNC;
    if (strcasecmp(name, "none") == 0) return WAL_FLUSH_NONE;
    if (strcasecmp(name, "os") == 0)   return WAL_FLUSH_OS;
    if (strcasecmp(name, "fsync") == 0) return WAL_FLUSH_FSYNC;
    if (strcasecmp(name, "batch") == 0) return WAL_FLUSH_BATCH;
    return WAL_FLUSH_FSYNC;
}
