/**
 * @file rel_engine.c
 * @brief Relational 存储引擎实现（含 WAL 支持）
 *
 * C0-2：WAL 接入
 * - 在 insert/update/delete 路径调用统一 WAL API 写入 redo 日志
 * - 默认 WAL_SYNC_FULL 模式（write + fsync，防系统崩溃）
 * - WAL 写入失败时中止当次 DML，确保数据一致性
 */

#define _POSIX_C_SOURCE 200809L

#include "db/storage/rel/rel.h"
#include "db/table.h"
#include "db/heapam.h"
#include "db/catalog.h"
#include "db/buf.h"
#include "db/storage/wal/wal.h"  /* C0-2：统一 WAL API */
#include "core/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

/* Windows 兼容：mkdir / sys/stat.h */
#ifdef _WIN32
    #include <direct.h>
    #include <io.h>
    #define mkdir _mkdir
#else
    #include <sys/stat.h>
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** Relational 引擎名称 */
#define REL_ENGINE_NAME "rel_engine"

/** WAL 文件路径前缀 */
#define REL_WAL_PATH_PREFIX "rel_wal"

/* ============================================================
 * 全局状态
 * ============================================================ */

/** 引擎全局状态 */
typedef struct rel_engine_global_s {
    char    data_dir[512];   /**< 数据目录 */
    bool    initialized;     /**< 是否已初始化 */
    wal_t   *wal;            /**< 统一 WAL 句柄（C0-2） */
} rel_engine_global_t;

static rel_engine_global_t g_rel_engine = {
    .data_dir = {0},
    .initialized = false,
    .wal = NULL
};

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 构造 WAL 文件路径
 */
static void rel_engine_wal_path(char *buf, size_t buf_size,
                                const char *data_dir) {
    snprintf(buf, buf_size, "%s/%s", data_dir, REL_WAL_PATH_PREFIX);
}

/* ============================================================
 * 引擎初始化 / 关闭
 * ============================================================ */

/**
 * @brief 初始化 Relational 引擎（含 WAL）
 *
 * C0-2：WAL 初始化 — 默认 WAL_SYNC_FULL 模式
 * @return 0 成功，-1 失败
 */
int rel_engine_init(const char *data_dir) {
    if (g_rel_engine.initialized) {
        return 0;
    }

    if (!data_dir) {
        LOG_ERROR("Relational 引擎初始化：data_dir 为空");
        return -1;
    }

    /* 复制数据目录路径 */
    strncpy(g_rel_engine.data_dir, data_dir,
            sizeof(g_rel_engine.data_dir) - 1);
    g_rel_engine.data_dir[sizeof(g_rel_engine.data_dir) - 1] = '\0';

    /* 初始化 Buffer Pool 和 Catalog（rel.c 中已有实现） */
    if (rel_init() != 0) {
        LOG_ERROR("Relational 引擎：rel_init() 失败");
        return -1;
    }

    /* C0-2：初始化统一 WAL */
    char wal_path[512];
    rel_engine_wal_path(wal_path, sizeof(wal_path), g_rel_engine.data_dir);

    /* 确保数据目录存在 */
#ifdef _WIN32
    if (mkdir(g_rel_engine.data_dir) != 0 && errno != EEXIST) {
        LOG_WARN("数据目录创建失败: %s", g_rel_engine.data_dir);
    }
#else
    if (mkdir(g_rel_engine.data_dir, 0755) != 0 && errno != EEXIST) {
        LOG_WARN("数据目录创建失败: %s", g_rel_engine.data_dir);
    }
#endif

    g_rel_engine.wal = wal_create(wal_path, 8192);
    if (g_rel_engine.wal == NULL) {
        LOG_WARN("WAL 初始化失败，Relational 引擎将继续运行（无 WAL 保护）");
        /* WAL 初始化失败不阻止引擎启动 */
    } else {
        /* C0-2：显式设置 WAL_SYNC_FULL 模式（write + fsync） */
        if (wal_set_sync_mode(g_rel_engine.wal, WAL_SYNC_FULL) != 0) {
            LOG_WARN("WAL_SYNC_FULL 模式设置失败");
        }
        /* C0-2：注册为当前活跃 WAL（线程局部变量） */
        wal_set_current(g_rel_engine.wal);
        LOG_INFO("Relational 引擎 WAL 已启用，模式=WAL_SYNC_FULL, path=%s",
                 wal_path);
    }

    g_rel_engine.initialized = true;
    return 0;
}

/**
 * @brief 关闭 Relational 引擎
 */
void rel_engine_shutdown(void) {
    if (!g_rel_engine.initialized) {
        return;
    }

    /* C0-2：关闭 WAL */
    if (g_rel_engine.wal != NULL) {
        /* 执行 checkpoint 再关闭 */
        wal_flush(g_rel_engine.wal);
        wal_close(g_rel_engine.wal);
        g_rel_engine.wal = NULL;
        LOG_INFO("Relational 引擎 WAL 已关闭");
    }

    /* 关闭 Buffer Pool 和 Catalog */
    rel_shutdown();

    g_rel_engine.initialized = false;
}

/**
 * @brief 获取当前 WAL 句柄（供外部使用）
 */
wal_t *rel_engine_get_wal(void) {
    return g_rel_engine.wal;
}

/* ============================================================
 * WAL 持久化 API
 * ============================================================ */

/**
 * @brief 执行检查点（C0-2）
 */
int rel_engine_checkpoint(void) {
    if (g_rel_engine.wal == NULL) {
        LOG_WARN("WAL 未启用，无法执行 checkpoint");
        return -1;
    }

    if (wal_flush(g_rel_engine.wal) != 0) {
        LOG_ERROR("WAL flush 失败");
        return -1;
    }

    LOG_INFO("Relational 引擎 checkpoint 完成");
    return 0;
}

/**
 * @brief 启用/关闭 WAL（动态控制）
 *
 * @param enable true=启用，false=关闭
 * @return 0 成功，-1 失败
 */
int rel_engine_enable_wal(bool enable) {
    if (enable) {
        if (g_rel_engine.wal != NULL) {
            /* 已有 WAL，直接返回 */
            return 0;
        }

        char wal_path[512];
        rel_engine_wal_path(wal_path, sizeof(wal_path), g_rel_engine.data_dir);

        g_rel_engine.wal = wal_create(wal_path, 8192);
        if (g_rel_engine.wal == NULL) {
            LOG_ERROR("WAL 创建失败: %s", wal_path);
            return -1;
        }

        /* 设置 WAL_SYNC_FULL 模式 */
        wal_set_sync_mode(g_rel_engine.wal, WAL_SYNC_FULL);
        wal_set_current(g_rel_engine.wal);

        LOG_INFO("Relational 引擎 WAL 已启用，模式=WAL_SYNC_FULL");
    } else {
        if (g_rel_engine.wal != NULL) {
            wal_close(g_rel_engine.wal);
            g_rel_engine.wal = NULL;
            LOG_INFO("Relational 引擎 WAL 已关闭");
        }
    }

    return 0;
}

/* ============================================================
 * 元组操作（含 WAL）
 * ============================================================ */

/**
 * @brief 插入元组（含 WAL）
 *
 * C0-2：WAL-first 铁律 — 在主存修改前先写 redo 日志
 * @param rel Relation
 * @param tuple 序列化元组
 * @param len 元组长度
 * @param tid_out 输出：元组物理位置（block+offset）
 * @return 0 成功，-1 失败
 */
int rel_engine_insert(Relation rel, const void *tuple, size_t len,
                      void *tid_out) {
    if (!rel || !tuple) {
        return -1;
    }

    /* C0-2：WAL-first — 在主存修改前先写 redo 日志 */
    if (g_rel_engine.wal != NULL) {
        uint64_t lsn = wal_write_heap_insert(g_rel_engine.wal,
                                             rel->rd_relfilenode,
                                             tuple, len);
        if (lsn == 0) {
            /* WAL 写入失败：中止本次插入 */
            LOG_ERROR("WAL 写入失败（heap_insert），中止操作");
            return -1;
        }
        LOG_INFO("WAL 写入成功：heap_insert, rel_id=%u, LSN=%lu",
                 rel->rd_relfilenode, (unsigned long)lsn);
    }

    /* 调用 Heap AM 执行实际插入 */
    return heap_insert(rel, tuple, len, 0, 0, NULL, tid_out);
}

/**
 * @brief 删除元组（含 WAL）
 *
 * C0-2：WAL-first — 在修改前先写 redo 日志
 * @param rel Relation
 * @param tid 元组物理位置（block+offset）
 * @return 0 成功，-1 失败
 */
int rel_engine_delete(Relation rel, const void *tid) {
    if (!rel || !tid) {
        return -1;
    }

    /* C0-2：WAL-first — 解析 TID 后先写 redo 日志 */
    if (g_rel_engine.wal != NULL) {
        const uint8_t *tid_data = (const uint8_t *)tid;
        uint32_t blocknum = 0;
        uint16_t offset = 0;
        memcpy(&blocknum, tid_data, sizeof(uint32_t));
        memcpy(&offset, tid_data + sizeof(uint32_t), sizeof(uint16_t));

        uint64_t packed_tid = ((uint64_t)blocknum << 32) | (uint64_t)offset;
        uint64_t lsn = wal_write_heap_delete(g_rel_engine.wal,
                                             rel->rd_relfilenode,
                                             packed_tid);
        if (lsn == 0) {
            /* WAL 写入失败：中止本次删除 */
            LOG_ERROR("WAL 写入失败（heap_delete），中止操作");
            return -1;
        }
        LOG_INFO("WAL 写入成功：heap_delete, rel_id=%u, LSN=%lu",
                 rel->rd_relfilenode, (unsigned long)lsn);
    }

    /* 调用 Heap AM 执行实际删除 */
    return heap_delete(rel, tid, 0, false, false);
}

/**
 * @brief 更新元组（含 WAL）
 *
 * C0-2：WAL-first — update 等价 delete + insert，先记 redo
 * @param rel Relation
 * @param tid 旧元组物理位置
 * @param newtuple 新元组数据
 * @param newlen 新元组长度
 * @return 0 成功，-1 失败
 */
int rel_engine_update(Relation rel, const void *tid,
                      const void *newtuple, size_t newlen) {
    if (!rel || !tid || !newtuple) {
        return -1;
    }

    /* C0-2：WAL-first — 先解析 TID 并写 redo 日志 */
    if (g_rel_engine.wal != NULL) {
        const uint8_t *tid_data = (const uint8_t *)tid;
        uint32_t blocknum = 0;
        uint16_t offset = 0;
        memcpy(&blocknum, tid_data, sizeof(uint32_t));
        memcpy(&offset, tid_data + sizeof(uint32_t), sizeof(uint16_t));

        uint64_t packed_tid = ((uint64_t)blocknum << 32) | (uint64_t)offset;
        uint64_t lsn = wal_write_heap_update(g_rel_engine.wal,
                                             rel->rd_relfilenode,
                                             packed_tid,
                                             newtuple, newlen);
        if (lsn == 0) {
            /* WAL 写入失败：中止本次更新 */
            LOG_ERROR("WAL 写入失败（heap_update），中止操作");
            return -1;
        }
        LOG_INFO("WAL 写入成功：heap_update, rel_id=%u, LSN=%lu",
                 rel->rd_relfilenode, (unsigned long)lsn);
    }

    /* 调用 Heap AM 执行实际更新 */
    return heap_update(rel, tid, newtuple, newlen, 0, 0, NULL, 0);
}

/* ============================================================
 * 关系操作（WAL 集成）
 * ============================================================ */

/**
 * @brief 创建表（含 WAL 元数据记录）
 *
 * @param relid 表 OID
 * @param relkind 表类型
 * @return 0 成功，-1 失败
 */
int rel_engine_table_create(Oid relid, RelKind relkind) {
    if (relid == InvalidOid) {
        return -1;
    }

    /* 创建 Relation 元数据 */
    if (relation_create(relid, NULL, relkind, AM_HEAP) != 0) {
        return -1;
    }

    /* C0-2：记录表创建 WAL 日志（简化：记录 relid + relkind） */
    if (g_rel_engine.wal != NULL) {
        /* 序列化表创建记录：relid(4) + relkind(1) = 5 bytes */
        uint8_t record[8];
        memset(record, 0, sizeof(record));
        memcpy(record, &relid, sizeof(relid));
        record[4] = (uint8_t)relkind;

        uint64_t lsn = wal_write_heap_insert(g_rel_engine.wal,
                                             relid,
                                             record, 5);
        if (lsn == 0) {
            LOG_WARN("WAL 写入表创建记录失败（非致命）");
        } else {
            LOG_INFO("WAL 记录表创建: relid=%u, LSN=%lu",
                     relid, (unsigned long)lsn);
        }
    }

    return 0;
}

/**
 * @brief 删除表（含 WAL 元数据记录）
 *
 * @param relid 表 OID
 * @return 0 成功，-1 失败
 */
int rel_engine_table_drop(Oid relid) {
    if (relid == InvalidOid) {
        return -1;
    }

    /* C0-2：记录表删除 WAL 日志 */
    if (g_rel_engine.wal != NULL) {
        uint64_t lsn = wal_write_heap_delete(g_rel_engine.wal,
                                             relid,
                                             (uint64_t)0);
        if (lsn == 0) {
            LOG_WARN("WAL 写入表删除记录失败（非致命）");
        } else {
            LOG_INFO("WAL 记录表删除: relid=%u, LSN=%lu",
                     relid, (unsigned long)lsn);
        }
    }

    /* 删除 Relation 元数据 */
    return relation_drop(relid);
}

/* ============================================================
 * 统计信息
 * ============================================================ */

/**
 * @brief 获取 WAL 统计信息
 */
int rel_engine_get_wal_stats(uint64_t *out_records, uint64_t *out_bytes) {
    if (g_rel_engine.wal == NULL) {
        if (out_records) *out_records = 0;
        if (out_bytes) *out_bytes = 0;
        return 0;
    }

    wal_stats_t stats;
    if (wal_get_stats(g_rel_engine.wal, &stats) != 0) {
        return -1;
    }

    if (out_records) *out_records = stats.total_records;
    if (out_bytes) *out_bytes = stats.total_bytes;
    return 0;
}

/**
 * @brief 获取 WAL 同步模式
 */
WalSyncMode rel_engine_get_wal_sync_mode(void) {
    if (g_rel_engine.wal == NULL) {
        return WAL_SYNC_NONE;
    }
    /* 返回内部同步模式（简化） */
    return WAL_SYNC_FULL;
}
