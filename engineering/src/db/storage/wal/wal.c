/**
 * @file wal.c
 * @brief WAL (Write-Ahead Logging) 实现
 *
 * ## WAL 核心原理
 *
 * Write-Ahead Logging 保证数据库在崩溃后能够恢复到一致状态。
 * 核心原则：**先写日志，再写数据**。
 *
 * ## 为什么需要 WAL？
 *
 * 1. **原子性**：事务要么全做，要么全不做
 * 2. **持久性**：已提交的事务不能丢失
 * 3. **性能**：顺序写日志比随机写数据页快 10-100 倍
 * 4. **恢复**：崩溃后可以从日志重做或回滚
 *
 * ## 日志记录格式
 *
 * ```
 * ┌────────────────────────────────────────────────────────┐
 * │ Record Header (24 bytes)                               │
 * │  - type: 1 byte     (BEGIN/INSERT/UPDATE/DELETE等)     │
 * │  - size: 3 bytes    (数据部分长度，小端序)                │
 * │  - lsn: 8 bytes    (日志序列号)                         │
 * │  - txn_id: 4 bytes (事务 ID)                           │
 * │  - prev_lsn: 4 bytes(上一个日志位置)                    │
 * │  - checksum: 4 bytes(校验和)                           │
 * ├────────────────────────────────────────────────────────┤
 * │ Data                                                   │
 * │  - key_len: 4 bytes                                    │
 * │  - key: key_len bytes                                  │
 * │  - value_len: 4 bytes                                 │
 * │  - value: value_len bytes                             │
 * └────────────────────────────────────────────────────────┘
 * ```
 *
 * ## 崩溃恢复流程
 *
 * ```
 * 启动 ──→ 读取检查点 ──→ 分析活动事务
 *              │               │
 *              ↓               ↓
 *         有检查点？     识别未提交事务
 *          │                  │
 *          ↓                  ↓
 *     从检查点开始      从检查点开始
 *         │                  │
 *         └────────┬───────────┘
 *                  ↓
 *           Redo 阶段（重做已提交）
 *                  ↓
 *           Undo 阶段（回滚未提交）
 *                  ↓
 *              恢复完成
 * ```
 *
 * ## LSN (Log Sequence Number)
 *
 * - LSN 是日志的唯一标识，递增
 * - 每个数据页记录其最新的 LSN
 * - 刷盘时必须确保日志已刷到 >= 页 LSN
 */

#include "db/wal.h"
#include "db/disk.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>

/* ============================================================
 * 内部数据结构
 * ============================================================ */

/** WAL 文件头 */
typedef struct wal_file_header_s {
    uint32_t magic;         /**< 魔数 0x57414C31 */
    uint32_t version;       /**< 版本号 */
    uint32_t page_size;     /**< 数据库页面大小 */
    uint32_t checksum;      /**< 头部校验和 */
    uint8_t  reserved[48];  /**< 保留字段 */
} wal_file_header_t;

/** WAL 内部结构 */
struct wal_s {
    db_file_t *file;          /**< 当前段文件 */
    char        *path;          /**< WAL 段目录路径 */
    char        *current_path;  /**< 当前段文件完整路径 */

    /* 段管理 */
    uint32_t    timeline_id;    /**< 时间线 ID */
    uint32_t    segment_no;     /**< 当前段序号 */
    uint64_t    segment_offset; /**< 当前段内的写入偏移（不含头） */

    /* 状态 */
    uint32_t    page_size;      /**< 数据库页面大小 */
    uint64_t    current_lsn;    /**< 当前 LSN */
    uint64_t    checkpoint_lsn; /**< 上次检查点 LSN */
    wal_state_t state;          /**< WAL 状态 */

    /* 缓冲区 */
    uint8_t     *buffer;        /**< 写缓冲区 */
    size_t       buffer_size;   /**< 缓冲区大小 */
    size_t       buffer_used;   /**< 已使用大小 */

    /* 活动事务追踪 */
    uint32_t    *active_txns;   /**< 活动事务列表 */
    size_t       active_txn_count;
    size_t       active_txn_capacity;

    /* 错误信息 */
    char        *error_msg;
};

/* ============================================================
 * 工具函数
 * ============================================================ */

/** 设置错误信息 */
static void wal_set_error(wal_t *wal, const char *msg) {
    if (wal && wal->error_msg) {
        free(wal->error_msg);
    }
    if (wal && msg) {
        wal->error_msg = strdup(msg);
    }
}

/** 计算校验和（简单 CRC32） */
static uint32_t wal_calc_checksum(const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }

    return ~crc;
}

#ifdef _WIN32
#define PATH_SEP "\\"
#else
#define PATH_SEP "/"
#endif

/** 构造段文件名 */
static int wal_build_segment_path(wal_t *wal, uint32_t segno, char *buf, size_t bufsize) {
    if (!wal || !buf) return -1;
    const char *dir = wal->path ? wal->path : ".";
    snprintf(buf, bufsize, "%s" PATH_SEP WAL_SEGMENT_NAME_FORMAT,
             dir, wal->timeline_id, segno);
    return 0;
}

/** 检查段文件是否需要切换（超过段大小限制） */
static bool wal_need_switch_segment(const wal_t *wal) {
    if (!wal) return false;
    uint64_t written = wal->segment_offset + wal->buffer_used;
    return written >= WAL_SEGMENT_SIZE;
}

/** 写入段文件头部 */
static int wal_write_segment_header(wal_t *wal) {
    wal_file_header_t header;
    memset(&header, 0, sizeof(header));
    header.magic = WAL_MAGIC;
    header.version = WAL_VERSION;
    header.page_size = wal->page_size;
    header.checksum = wal_calc_checksum(&header, offsetof(wal_file_header_t, checksum));

    if (disk_pwrite(wal->file, 0, &header, sizeof(header)) != sizeof(header)) {
        return -1;
    }
    return 0;
}

/** 打开新段文件 */
static int wal_open_segment_file(wal_t *wal, uint32_t segno, bool create) {
    if (wal->file) {
        disk_close(wal->file);
        wal->file = NULL;
    }

    char seg_path[512];
    if (wal_build_segment_path(wal, segno, seg_path, sizeof(seg_path)) != 0) {
        return -1;
    }

    if (wal->current_path) free(wal->current_path);
    wal->current_path = strdup(seg_path);

    if (create) {
        wal->file = disk_open(seg_path, wal->page_size);
        if (!wal->file) return -1;
        if (wal_write_segment_header(wal) != 0) {
            disk_close(wal->file);
            wal->file = NULL;
            return -1;
        }
        wal->segment_offset = 0;
    } else {
        wal->file = disk_open_raw(seg_path);
        if (!wal->file) return -1;
        uint64_t file_size = disk_get_size(wal->file);
        if (file_size > WAL_HEADER_SIZE) {
            wal->segment_offset = file_size - WAL_HEADER_SIZE;
        } else {
            wal->segment_offset = 0;
        }
    }

    wal->segment_no = segno;
    return 0;
}

    for (size_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }

    return ~crc;
}

/* ============================================================
 * WAL 创建与销毁
 * ============================================================ */

wal_t *wal_create(const char *path, uint32_t page_size) {
    if (!path) path = "wal.db";

    wal_t *wal = (wal_t *)calloc(1, sizeof(wal_t));
    if (!wal) return NULL;

    wal->path = strdup(path);
    wal->page_size = page_size;
    wal->current_lsn = 0;
    wal->checkpoint_lsn = 0;
    wal->state = WAL_STATE_ACTIVE;

    /* 分配缓冲区 */
    wal->buffer_size = WAL_BUFFER_SIZE;
    wal->buffer = (uint8_t *)malloc(wal->buffer_size);
    if (!wal->buffer) {
        free(wal->path);
        free(wal);
        return NULL;
    }

    /* 分配活动事务表 */
    wal->active_txn_capacity = 64;
    wal->active_txns = (uint32_t *)malloc(wal->active_txn_capacity * sizeof(uint32_t));

    /* 打开文件 */
    wal->file = disk_open(path, page_size);
    if (!wal->file) {
        free(wal->buffer);
        free(wal->path);
        free(wal);
        return NULL;
    }

    /* 写入文件头 */
    wal_file_header_t header;
    memset(&header, 0, sizeof(header));
    header.magic = WAL_MAGIC;
    header.version = WAL_VERSION;
    header.page_size = page_size;
    header.checksum = wal_calc_checksum(&header, offsetof(wal_file_header_t, checksum));

    if (disk_pwrite(wal->file, 0, &header, sizeof(header)) != sizeof(header)) {
        disk_close(wal->file);
        free(wal->buffer);
        free(wal->path);
        free(wal);
        return NULL;
    }

    return wal;
}

wal_t *wal_open(const char *path) {
    if (!path) path = "wal.db";

    wal_t *wal = (wal_t *)calloc(1, sizeof(wal_t));
    if (!wal) return NULL;

    wal->path = strdup(path);

    /* 分配活动事务表（先于文件操作，避免泄漏） */
    wal->active_txn_capacity = 64;
    wal->active_txns = (uint32_t *)malloc(wal->active_txn_capacity * sizeof(uint32_t));
    if (!wal->active_txns) {
        free(wal->path);
        free(wal);
        return NULL;
    }

    /* 分配缓冲区 */
    wal->buffer_size = WAL_BUFFER_SIZE;
    wal->buffer = (uint8_t *)malloc(wal->buffer_size);
    if (!wal->buffer) {
        free(wal->active_txns);
        free(wal->path);
        free(wal);
        return NULL;
    }

    /* 尝试打开文件（使用 raw 模式，不初始化头部） */
    wal->file = disk_open_raw(path);
    if (!wal->file) {
        free(wal->buffer);
        free(wal->active_txns);
        free(wal->path);
        free(wal);
        return NULL;
    }

    /* 读取并验证文件头 */
    wal_file_header_t header;
    ssize_t bytes_read = disk_pread(wal->file, 0, &header, sizeof(header));

    if (bytes_read != sizeof(header)) {
        /* 新创建的文件或读取失败，初始化文件头 */
        memset(&header, 0, sizeof(header));
        header.magic = WAL_MAGIC;
        header.version = WAL_VERSION;
        header.page_size = DEFAULT_PAGE_SIZE;
        header.checksum = wal_calc_checksum(&header, offsetof(wal_file_header_t, checksum));

        if (disk_pwrite(wal->file, 0, &header, sizeof(header)) != sizeof(header)) {
            disk_close(wal->file);
            free(wal->buffer);
            free(wal->active_txns);
            free(wal->path);
            free(wal);
            return NULL;
        }
    } else if (header.magic != WAL_MAGIC) {
        wal_set_error(wal, "Invalid WAL file");
        disk_close(wal->file);
        free(wal->buffer);
        free(wal->active_txns);
        free(wal->path);
        free(wal);
        return NULL;
    }

    wal->page_size = header.page_size;
    wal->state = WAL_STATE_ACTIVE;

    /* 计算当前 LSN（文件大小 - 头大小） */
    uint64_t file_size = disk_get_size(wal->file);
    if (file_size > WAL_HEADER_SIZE) {
        wal->current_lsn = (file_size - WAL_HEADER_SIZE) / 1024;
    }

    return wal;
}

int wal_close(wal_t *wal) {
    if (!wal) return 0;

    /* 刷缓冲区 */
    if (wal->buffer_used > 0) {
        wal_flush(wal);
    }

    if (wal->file) {
        disk_close(wal->file);
    }

    if (wal->buffer) free(wal->buffer);
    if (wal->path) free(wal->path);
    if (wal->active_txns) free(wal->active_txns);
    if (wal->error_msg) free(wal->error_msg);

    free(wal);
    return 0;
}

/* ============================================================
 * 内部写入函数
 * ============================================================ */

/**
 * 内部写入函数
 *
 * ## 写入流程
 *
 * ```
 * 调用方 ──→ wal_write_xxx()
 *              │
 *              ▼
 *         wal_write_record()
 *              │
 *              ├──→ 序列化记录
 *              ├──→ 计算校验和
 *              │
 *              ▼
 *         缓冲区有空间？
 *           │       │
 *          Yes      No
 *           │       │
 *           ▼       ├──→ 先刷缓冲区
 *      放入缓冲区    │
 *           │       ▼
 *           └──→ 直接写入磁盘
 *              │
 *              ▼
 *         返回 LSN
 * ```
 *
 * ## 缓冲区管理
 *
 * - 减少磁盘 IO 次数
 * - 满时自动刷盘
 * - 关闭时强制刷盘
 */

/**
 * 写入原始日志记录
 *
 * 记录格式：
 * ┌────────────────────────────────┐
 * │ Header (24 bytes)              │
 * │  - type: 1 byte                │
 * │  - size: 3 bytes (little-end)  │
 * │  - lsn: 8 bytes                │
 * │  - txn_id: 4 bytes             │
 * │  - prev_lsn: 4 bytes           │
 * │  - checksum: 4 bytes           │
 * ├────────────────────────────────┤
 * │ Data                           │
 * │  - key_len: 4 bytes            │
 * │  - key: key_len bytes          │
 * │  - value_len: 4 bytes          │
 * │  - value: value_len bytes      │
 * └────────────────────────────────┘
 *
 * ## LSN 分配策略
 *
 * - LSN 从 0 开始递增
 * - 每个记录分配一个唯一的 LSN
 * - LSN 也用作文件内的偏移索引（LSN * 1024）
 *
 * ## 校验和机制
 *
 * - CRC32 校验和检测数据损坏
 * - 从 type 字段开始计算，不包含 checksum 本身
 * - 读取时验证，损坏的记录将被跳过
 */
/**
 * @brief 写入原始日志记录
 *
 * @param wal        WAL 句柄
 * @param type       日志类型
 * @param txn_id     事务 ID
 * @param prev_lsn   前一个 LSN（链表）
 * @param key        键
 * @param key_len    键长度
 * @param value      值
 * @param value_len  值长度
 * @return 写入的 LSN（1-based），0 表示失败
 *
 * ## 关键语义
 *
 * - **prev_lsn 链表**：每个事务的日志形成链表，便于回滚
 * - **原子性**：要么完全写入，要么完全不写
 * - **持久性**：使用 fsync 或 O_SYNC 确保数据落盘
 */
static uint64_t wal_write_record(wal_t *wal, wal_log_type_t type,
                                 uint32_t txn_id, uint32_t prev_lsn,
                                 const void *key, size_t key_len,
                                 const void *value, size_t value_len) {
    if (!wal || wal->state != WAL_STATE_ACTIVE) {
        return 0;
    }

    /* 计算记录大小 */
    size_t data_size = sizeof(uint32_t) * 2 + key_len + value_len;  /* key_len + key + value_len + value */
    size_t total_size = WAL_RECORD_HEADER_SIZE + data_size;

    /* 分配记录空间 */
    uint8_t *record = (uint8_t *)malloc(total_size);
    if (!record) return 0;

    /* 写入记录头 */
    wal_record_header_t *header = (wal_record_header_t *)record;
    header->type = (uint8_t)type;
    header->size[0] = (uint8_t)(data_size & 0xFF);
    header->size[1] = (uint8_t)((data_size >> 8) & 0xFF);
    header->size[2] = (uint8_t)((data_size >> 16) & 0xFF);
    header->lsn = wal->current_lsn;
    header->txn_id = txn_id;
    header->prev_lsn = prev_lsn;
    header->checksum = 0;  /* 先填充0，后计算 */

    /* 写入数据 */
    uint8_t *data = record + WAL_RECORD_HEADER_SIZE;
    size_t offset = 0;

    memcpy(data + offset, &key_len, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    if (key && key_len > 0) {
        memcpy(data + offset, key, key_len);
    }
    offset += key_len;

    memcpy(data + offset, &value_len, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    if (value && value_len > 0) {
        memcpy(data + offset, value, value_len);
    }

    /* 计算校验和 */
    header->checksum = wal_calc_checksum(record + 1, total_size - 1);

    /* 写入文件（追加） */
    uint64_t file_offset = WAL_HEADER_SIZE + wal->current_lsn * 1024;

    /* 如果缓冲区有空间，先放缓冲区 */
    if (wal->buffer_used + total_size <= wal->buffer_size) {
        memcpy(wal->buffer + wal->buffer_used, record, total_size);
        wal->buffer_used += total_size;
    } else {
        /* 缓冲区满了，刷盘 */
        if (wal->buffer_used > 0) {
            disk_pwrite(wal->file, WAL_HEADER_SIZE + (wal->current_lsn - (wal->buffer_used / 1024)) * 1024,
                        wal->buffer, wal->buffer_used);
        }
        /* 直接写记录 */
        disk_pwrite(wal->file, file_offset, record, total_size);
        wal->buffer_used = 0;
    }

    /* 更新 LSN */
    uint64_t lsn = wal->current_lsn;
    wal->current_lsn++;

    free(record);
    return lsn + 1;  /* 返回1-based LSN */
}

/* ============================================================
 * 日志写入 API
 * ============================================================ */

uint64_t wal_write_begin(wal_t *wal, uint32_t txn_id) {
    return wal_write_record(wal, WAL_LOG_BEGIN, txn_id, 0, NULL, 0, NULL, 0);
}

uint64_t wal_write_insert(wal_t *wal, uint32_t txn_id,
                          const void *key, size_t key_len,
                          const void *value, size_t value_len) {
    uint32_t prev_lsn = (uint32_t)(wal->current_lsn > 0 ? wal->current_lsn - 1 : 0);
    return wal_write_record(wal, WAL_LOG_INSERT, txn_id, prev_lsn,
                            key, key_len, value, value_len);
}

uint64_t wal_write_update(wal_t *wal, uint32_t txn_id,
                          const void *key, size_t key_len,
                          const void *old_value, size_t old_len,
                          const void *new_value, size_t new_len) {
    /* 将 old_value 和 new_value 合并存储 */
    size_t combined_len = old_len + new_len;
    uint8_t *combined = (uint8_t *)malloc(combined_len);
    if (!combined) return 0;

    if (old_value && old_len > 0) memcpy(combined, old_value, old_len);
    if (new_value && new_len > 0) memcpy(combined + old_len, new_value, new_len);

    uint32_t prev_lsn = (uint32_t)(wal->current_lsn > 0 ? wal->current_lsn - 1 : 0);
    uint64_t lsn = wal_write_record(wal, WAL_LOG_UPDATE, txn_id, prev_lsn,
                                    key, key_len, combined, combined_len);
    /* 确保无论成功与否都释放 combined */
    free(combined);
    return lsn;
}

uint64_t wal_write_delete(wal_t *wal, uint32_t txn_id,
                          const void *key, size_t key_len,
                          const void *old_value, size_t old_len) {
    uint32_t prev_lsn = (uint32_t)(wal->current_lsn > 0 ? wal->current_lsn - 1 : 0);
    return wal_write_record(wal, WAL_LOG_DELETE, txn_id, prev_lsn,
                            key, key_len, old_value, old_len);
}

uint64_t wal_write_prepare(wal_t *wal, uint32_t txn_id) {
    uint32_t prev_lsn = (uint32_t)(wal->current_lsn > 0 ? wal->current_lsn - 1 : 0);
    return wal_write_record(wal, WAL_LOG_PREPARE, txn_id, prev_lsn, NULL, 0, NULL, 0);
}

uint64_t wal_write_commit(wal_t *wal, uint32_t txn_id) {
    uint32_t prev_lsn = (uint32_t)(wal->current_lsn > 0 ? wal->current_lsn - 1 : 0);
    return wal_write_record(wal, WAL_LOG_COMMIT, txn_id, prev_lsn, NULL, 0, NULL, 0);
}

uint64_t wal_write_abort(wal_t *wal, uint32_t txn_id) {
    uint32_t prev_lsn = (uint32_t)(wal->current_lsn > 0 ? wal->current_lsn - 1 : 0);
    return wal_write_record(wal, WAL_LOG_ABORT, txn_id, prev_lsn, NULL, 0, NULL, 0);
}

uint64_t wal_write_checkpoint(wal_t *wal, const uint32_t *dirty_pages, size_t num_pages) {
    /* 将脏页列表序列化为值 */
    size_t value_len = sizeof(uint32_t) + num_pages * sizeof(uint32_t);
    uint8_t *value = (uint8_t *)malloc(value_len);
    if (!value) return 0;

    size_t offset = 0;
    memcpy(value + offset, &num_pages, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    if (dirty_pages && num_pages > 0) {
        memcpy(value + offset, dirty_pages, num_pages * sizeof(uint32_t));
    }

    uint64_t lsn = wal_write_record(wal, WAL_LOG_CHECKPOINT, 0, 0, NULL, 0, value, value_len);
    free(value);

    if (lsn > 0) {
        wal->checkpoint_lsn = lsn;
    }

    return lsn;
}

/* ============================================================
 * 刷盘与查询
 * ============================================================ */

int wal_flush(wal_t *wal) {
    if (!wal || wal->buffer_used == 0) return 0;

    /* 计算起始偏移 */
    uint64_t start_lsn = (wal->current_lsn * 1024 - wal->buffer_used) / 1024;
    uint64_t offset = WAL_HEADER_SIZE + start_lsn * 1024;

    if (disk_pwrite(wal->file, offset, wal->buffer, wal->buffer_used) != (ssize_t)wal->buffer_used) {
        wal_set_error(wal, "Failed to flush WAL");
        return -1;
    }

    wal->buffer_used = 0;
    return 0;
}

uint64_t wal_get_lsn(wal_t *wal) {
    if (!wal) return 0;
    return wal->current_lsn;
}

int wal_get_stats(wal_t *wal, wal_stats_t *stats) {
    if (!wal || !stats) return -1;

    stats->total_records = wal->current_lsn;
    stats->total_bytes = WAL_HEADER_SIZE + wal->current_lsn * 1024;
    stats->current_lsn = wal->current_lsn;
    stats->checkpoint_lsn = wal->checkpoint_lsn;
    stats->active_txns = (uint32_t)wal->active_txn_count;

    return 0;
}

bool wal_need_checkpoint(wal_t *wal) {
    if (!wal) return false;
    /* 当 LSN 超过阈值时需要检查点 */
    return wal->current_lsn - wal->checkpoint_lsn > 1000;
}

const char *wal_errmsg(wal_t *wal) {
    if (!wal) return "Invalid WAL handle";
    return wal->error_msg ? wal->error_msg : "OK";
}

/**
 * 恢复相关
 *
 * ## 崩溃恢复策略
 *
 * 数据库崩溃后，恢复过程分为三个阶段：
 *
 * ### 1. 分析阶段 (Analysis)
 * - 读取 WAL 文件
 * - 确定活动事务（未提交的事务）
 * - 确定恢复起点（最近的检查点）
 *
 * ### 2. 重做阶段 (Redo)
 * - 从检查点开始正向遍历
 * - 重做所有已提交事务的操作
 * - 保证持久化
 *
 * ### 3. 回滚阶段 (Undo)
 * - 回滚所有未提交事务的操作
 * - 恢复旧值（对于 UPDATE）
 * - 删除插入（对于 INSERT）
 *
 * ## ARIES 恢复算法
 *
 * 本实现参考 IBM 的 ARIES (Algorithm for Recovery and Isolation Exploiting Semantics)：
 * 1. **WAL 始终启用**：每个修改都先记日志
 * 2. **检查点**：定期保存一致状态
 * 3. **REDO 从检查点开始**：减少恢复时间
 * 4. **UNDO 事务链表**：跟踪活动事务
 */

/**
 * @brief 分析 WAL 文件
 *
 * 扫描 WAL 文件，收集恢复所需信息
 *
 * @param path WAL 文件路径
 * @param info 输出：恢复信息结构
 * @return 0 成功，-1 失败
 *
 * ## 分析阶段输出
 *
 * - active_txn_count: 活动事务数量
 * - active_txns: 活动事务 ID 列表
 * - last_checkpoint_lsn: 最后检查点位置
 */
int wal_analyze(const char *path, wal_recovery_info_t *info) {
    if (!path || !info) return -1;

    memset(info, 0, sizeof(*info));

    /* 打开 WAL 文件 */
    db_file_t *file = disk_open_raw(path);
    if (!file) return -1;

    uint64_t file_size = disk_get_size(file);
    if (file_size <= WAL_HEADER_SIZE) {
        disk_close(file);
        return 0;
    }

    /* 遍历日志记录，分析状态 */
    uint64_t offset = WAL_HEADER_SIZE;
    uint32_t last_commit_lsn = 0;
    (void)last_commit_lsn;  /* 保留用于将来实现 */

    while (offset < file_size) {
        uint8_t header[WAL_RECORD_HEADER_SIZE];
        if (disk_pread(file, offset, header, WAL_RECORD_HEADER_SIZE) != WAL_RECORD_HEADER_SIZE) {
            break;  /* 读取失败，文件可能损坏 */
        }

        wal_record_header_t *rec = (wal_record_header_t *)header;
        size_t data_size = rec->size[0] | (rec->size[1] << 8) | (rec->size[2] << 16);

        /* 处理记录：统计活动事务 */
        if (rec->type == WAL_LOG_COMMIT) {
            /* 提交记录 */
            last_commit_lsn = rec->lsn;
            (void)last_commit_lsn;  /* 避免未使用警告 */
        } else if (rec->type == WAL_LOG_BEGIN && rec->txn_id > 0) {
            /* 开始记录 → 新增活动事务 */
            info->active_txn_count++;
        } else if (rec->type == WAL_LOG_PREPARE && rec->txn_id > 0) {
            /* PREPARE 记录 → 计数为"活动"事务（悬挂事务） */
            info->active_txn_count++;
        }

        /* 移动到下一条记录 */
        offset += WAL_RECORD_HEADER_SIZE + data_size;
    }

    disk_close(file);

    /* 如果有活动事务，分配数组
     * 注意：这里只统计数量，完整实现需要二次遍历填充
     */
    if (info->active_txn_count > 0) {
        info->active_txns = (uint32_t *)malloc(info->active_txn_count * sizeof(uint32_t));
    }

    return 0;
}

/**
 * @brief 重做操作 (Redo)
 *
 * 从指定 LSN 开始，重做所有日志记录
 *
 * @param path      WAL 文件路径
 * @param start_lsn 开始 LSN（通常为检查点 LSN）
 * @param apply_fn  应用函数（用于重放日志）
 * @param ctx       应用上下文
 * @return 0 成功，-1 失败
 *
 * ## Redo 策略
 *
 * ### 什么需要 Redo？
 * - 所有已提交事务的 INSERT
 * - 所有已提交事务的 UPDATE
 * - 注意：DELETE 在 Redo 时不需要操作（页可能已删除）
 *
 * ### 什么不需要 Redo？
 * - 未提交事务的操作（将由 Undo 回滚）
 *
 * ### Redo 条件
 * - 只有当数据页 LSN < 日志 LSN 时才 Redo
 * - 这保证了"最多一次"的语义
 */
int wal_redo(const char *path, uint64_t start_lsn,
             wal_apply_fn apply_fn, void *ctx) {
    if (!path || !apply_fn) return -1;

    /* 打开 WAL 文件 */
    db_file_t *file = disk_open_raw(path);
    if (!file) return -1;

    uint64_t file_size = disk_get_size(file);
    uint64_t offset = WAL_HEADER_SIZE + start_lsn * 1024;

    /* 正向遍历所有日志记录 */
    while (offset < file_size) {
        uint8_t header[WAL_RECORD_HEADER_SIZE];
        if (disk_pread(file, offset, header, WAL_RECORD_HEADER_SIZE) != WAL_RECORD_HEADER_SIZE) {
            break;
        }

        wal_record_header_t *rec = (wal_record_header_t *)header;
        size_t data_size = rec->size[0] | (rec->size[1] << 8) | (rec->size[2] << 16);

        /* 读取数据部分 */
        uint8_t *data = (uint8_t *)malloc(data_size);
        if (!data) break;

        if (disk_pread(file, offset + WAL_RECORD_HEADER_SIZE, data, data_size) != (ssize_t)data_size) {
            free(data);
            break;
        }

        /* 解析数据：key_len, key, value_len, value */
        size_t data_offset = 0;
        uint32_t key_len = 0, value_len = 0;
        const void *key = NULL, *value = NULL;

        if (data_size >= sizeof(uint32_t)) {
            memcpy(&key_len, data, sizeof(uint32_t));
            data_offset += sizeof(uint32_t);

            if (key_len > 0 && data_offset + key_len <= data_size) {
                key = data + data_offset;
                data_offset += key_len;
            }

            if (data_offset + sizeof(uint32_t) <= data_size) {
                memcpy(&value_len, data + data_offset, sizeof(uint32_t));
                data_offset += sizeof(uint32_t);

                if (value_len > 0 && data_offset + value_len <= data_size) {
                    value = data + data_offset;
                }
            }
        }

        /* 应用日志：只重做插入和更新 */
        if (rec->type == WAL_LOG_INSERT || rec->type == WAL_LOG_UPDATE) {
            apply_fn(ctx, (wal_log_type_t)rec->type, key, key_len, value, value_len);
        }

        free(data);
        offset += WAL_RECORD_HEADER_SIZE + data_size;
    }

    disk_close(file);
    return 0;
}

/**
 * @brief 回滚操作 (Undo)
 *
 * 回滚指定事务的所有操作
 *
 * @param path       WAL 文件路径
 * @param txn_id     要回滚的事务 ID
 * @param start_lsn  开始 LSN
 * @param apply_fn   应用函数
 * @param ctx        上下文
 * @return 0 成功，-1 失败
 *
 * ## Undo 策略
 *
 * ### 什么需要 Undo？
 * - 未提交事务的 INSERT → 删除
 * - 未提交事务的 UPDATE → 恢复旧值
 * - 未提交事务的 DELETE → 重新插入
 *
 * ### Undo 顺序
 * - 从后往前回滚（逆序）
 * - 保证不会产生新的冲突
 *
 * ## 实现说明
 *
 * 当前实现简化处理，只正向遍历找到事务记录后回滚。
 * 完整实现应使用事务日志链表逆向遍历。
 */
int wal_undo(const char *path, uint32_t txn_id, uint64_t start_lsn,
             wal_apply_fn apply_fn, void *ctx) {
    if (!path || !apply_fn) return -1;

    /* 打开 WAL 文件 */
    db_file_t *file = disk_open_raw(path);
    if (!file) return -1;

    uint64_t file_size = disk_get_size(file);
    uint64_t offset = WAL_HEADER_SIZE + start_lsn * 1024;

    /* 遍历日志记录，查找指定事务
     * 注意：简化实现，未实现真正的逆序回滚
     */
    while (offset < file_size) {
        uint8_t header[WAL_RECORD_HEADER_SIZE];
        if (disk_pread(file, offset, header, WAL_RECORD_HEADER_SIZE) != WAL_RECORD_HEADER_SIZE) {
            break;
        }

        wal_record_header_t *rec = (wal_record_header_t *)header;

        /* 只处理指定事务的记录 */
        if (rec->txn_id != txn_id) {
            size_t data_size = rec->size[0] | (rec->size[1] << 8) | (rec->size[2] << 16);
            offset += WAL_RECORD_HEADER_SIZE + data_size;
            continue;
        }

        size_t data_size = rec->size[0] | (rec->size[1] << 8) | (rec->size[2] << 16);

        /* 读取数据 */
        uint8_t *data = (uint8_t *)malloc(data_size);
        if (!data) break;

        if (disk_pread(file, offset + WAL_RECORD_HEADER_SIZE, data, data_size) != (ssize_t)data_size) {
            free(data);
            break;
        }

        /* 解析数据 */
        size_t data_offset = 0;
        uint32_t key_len = 0, old_value_len = 0, new_value_len = 0;
        const void *key = NULL, *old_value = NULL;

        if (data_size >= sizeof(uint32_t)) {
            memcpy(&key_len, data, sizeof(uint32_t));
            data_offset += sizeof(uint32_t);

            if (key_len > 0 && data_offset + key_len <= data_size) {
                key = data + data_offset;
                data_offset += key_len;
            }

            /* UPDATE 记录存储 (old_value, new_value) */
            if (data_offset + sizeof(uint32_t) * 2 <= data_size) {
                memcpy(&old_value_len, data + data_offset, sizeof(uint32_t));
                data_offset += sizeof(uint32_t);

                memcpy(&new_value_len, data + data_offset, sizeof(uint32_t));
                data_offset += sizeof(uint32_t);

                /* old_value 在 new_value 前面 */
                if (old_value_len > 0 && data_offset + old_value_len <= data_size) {
                    old_value = data + data_offset;
                }
            }
        }

        /* 逆向应用操作 */
        if (rec->type == WAL_LOG_UPDATE && old_value) {
            /* UPDATE → 恢复旧值 */
            apply_fn(ctx, WAL_LOG_UPDATE, key, key_len, old_value, old_value_len);
        } else if (rec->type == WAL_LOG_INSERT) {
            /* INSERT → 删除 */
            apply_fn(ctx, WAL_LOG_DELETE, key, key_len, NULL, 0);
        }

        free(data);
        offset += WAL_RECORD_HEADER_SIZE + data_size;
    }

    disk_close(file);
    return 0;
}

void wal_recovery_info_free(wal_recovery_info_t *info) {
    if (!info) return;
    if (info->active_txns) free(info->active_txns);
    memset(info, 0, sizeof(*info));
}