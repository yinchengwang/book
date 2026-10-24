/**
 * @file wal_recover.c
 * @brief 统一 WAL 恢复入口实现（C0-2 T9）
 */
#include "db/storage/wal/wal_recover.h"
#include "db/storage/wal/wal.h"
#include "db/core/log.h"

#include <stdlib.h>
#include <string.h>

#define MAX_RECOVER_REGISTRATIONS 32

typedef struct {
    wal_log_type_t type;
    wal_recover_apply_fn apply;
    void *ctx;
    int active;
} recover_entry_t;

static recover_entry_t g_entries[MAX_RECOVER_REGISTRATIONS];

int wal_recover_register(wal_log_type_t type, wal_recover_apply_fn apply, void *ctx) {
    if (apply == NULL) return -1;
    for (int i = 0; i < MAX_RECOVER_REGISTRATIONS; ++i) {
        if (g_entries[i].active && g_entries[i].type == type) {
            /* 已注册：覆盖 */
            g_entries[i].apply = apply;
            g_entries[i].ctx = ctx;
            return 0;
        }
    }
    for (int i = 0; i < MAX_RECOVER_REGISTRATIONS; ++i) {
        if (!g_entries[i].active) {
            g_entries[i].type = type;
            g_entries[i].apply = apply;
            g_entries[i].ctx = ctx;
            g_entries[i].active = 1;
            return 0;
        }
    }
    LOG_ERROR("WAL recover 注册表已满");
    return -1;
}

static recover_entry_t *find_entry(wal_log_type_t type) {
    for (int i = 0; i < MAX_RECOVER_REGISTRATIONS; ++i) {
        if (g_entries[i].active && g_entries[i].type == type) {
            return &g_entries[i];
        }
    }
    return NULL;
}

/* 内部：复用 wal.c 实现的 replay（如未提供则由 wal_replay 自行遍历） */
extern int wal_replay(wal_t *wal,
                      int (*apply_fn)(void *ctx, wal_log_type_t type,
                                      const void *key, size_t key_len,
                                      const void *value, size_t value_len),
                      void *ctx);

/* 分发器：把 wal_replay 调用的回调路由到 g_entries 表 */
typedef struct {
    int replayed;
    int failed;
} dispatch_ctx_t;

static int dispatch_callback(void *ctx, wal_log_type_t type,
                             const void *key, size_t key_len,
                             const void *value, size_t value_len) {
    dispatch_ctx_t *dctx = (dispatch_ctx_t *)ctx;
    recover_entry_t *e = find_entry(type);
    if (e == NULL || e->apply == NULL) {
        /* 未注册该类型的 apply：跳过（兼容旧 WAL 记录） */
        return 0;
    }
    int rc = e->apply(e->ctx, type, key, key_len, value, value_len);
    if (rc != 0) dctx->failed++;
    dctx->replayed++;
    return 0;  /* 永不因单条记录失败而中止 */
}

int db_startup_recover(const char *wal_path, uint32_t page_size) {
    if (wal_path == NULL || wal_path[0] == '\0') {
        LOG_WARN("db_startup_recover: 空 WAL 路径，跳过");
        return 0;
    }

    wal_t *wal = wal_open(wal_path);
    if (wal == NULL) {
        /* WAL 不存在 = 新库或未启用 WAL，正常 */
        LOG_INFO("db_startup_recover: WAL %s 不存在，按空库启动", wal_path);
        return 0;
    }

    dispatch_ctx_t dctx = { 0, 0 };
    int rc = wal_replay(wal, dispatch_callback, &dctx);
    if (rc < 0) {
        LOG_WARN("db_startup_recover: wal_replay 返回 %d（部分记录可能未重放）", rc);
    }

    LOG_INFO("db_startup_recover: 重放 %d 条记录，失败 %d 条",
             dctx.replayed, dctx.failed);
    wal_close(wal);
    return dctx.replayed;
}
