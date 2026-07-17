/**
 * @file main.c
 * @brief WAL 与崩溃恢复演示
 *
 * 演示内容：
 * 1. WAL 日志格式（LSN, txid, op, data）
 * 2. Redo 日志写入
 * 3. 检查点（Checkpoint）生成
 * 4. 崩溃后恢复流程
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * WAL 日志类型定义
 * ============================================================ */

typedef enum {
    WAL_OP_BEGIN = 1,     /**< 事务开始 */
    WAL_OP_INSERT = 2,    /**< 插入操作 */
    WAL_OP_UPDATE = 3,    /**< 更新操作 */
    WAL_OP_DELETE = 4,    /**< 删除操作 */
    WAL_OP_COMMIT = 5,    /**< 事务提交 */
    WAL_OP_ABORT = 6,     /**< 事务回滚 */
    WAL_OP_CHECKPOINT = 7 /**< 检查点 */
} wal_op_t;

/** WAL 日志记录头（24 字节） */
typedef struct {
    uint8_t  type;        /**< 操作类型 */
    uint8_t  size[3];     /**< 数据大小（小端序，3 字节） */
    uint32_t lsn;         /**< 日志序列号 */
    uint32_t txn_id;      /**< 事务 ID */
    uint32_t prev_lsn;    /**< 前一条日志 LSN */
    uint32_t checksum;    /**< 校验和 */
} __attribute__((packed)) wal_record_header_t;

/** WAL 日志记录 */
typedef struct {
    wal_record_header_t header;
    char               *data; /**< 变长数据 */
} wal_record_t;

/** 数据库状态模拟 */
#define MAX_KEYS 256
typedef struct {
    char keys[MAX_KEYS][64];
    char values[MAX_KEYS][256];
    int  key_count;
    bool committed[MAX_KEYS];
} db_state_t;

/* ============================================================
 * 全局状态
 * ============================================================ */

static FILE       *g_wal_file = NULL;   /**< WAL 文件 */
static uint32_t    g_current_lsn = 0;   /**< 当前 LSN */
static uint32_t    g_checkpoint_lsn = 0;/**< 上次检查点 LSN */
static db_state_t  g_db_state;          /**< 数据库状态 */
static uint32_t   *g_dirty_pages = NULL;/**< 脏页列表 */
static int         g_dirty_count = 0;

/* ============================================================
 * 工具函数
 * ============================================================ */

/** 计算简单校验和 */
static uint32_t calc_checksum(const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
        }
    }
    return ~crc;
}

/** 打印日志记录 */
static void print_log_record(wal_record_header_t *hdr, const char *data) {
    const char *op_name[] = {"", "BEGIN", "INSERT", "UPDATE", "DELETE", "COMMIT", "ABORT", "CHECKPOINT"};
    const char *op = (hdr->type >= 1 && hdr->type <= 7) ? op_name[hdr->type] : "UNKNOWN";

    printf("[wal] LSN=%u txn=%u op=%s", hdr->lsn, hdr->txn_id, op);

    if (hdr->type == WAL_OP_INSERT || hdr->type == WAL_OP_UPDATE) {
        size_t data_size = hdr->size[0] | (hdr->size[1] << 8) | (hdr->size[2] << 16);
        if (data_size > 0 && data) {
            size_t offset = 0;
            uint32_t key_len, val_len;

            /* 读取 key_len */
            memcpy(&key_len, data, sizeof(uint32_t));
            offset += sizeof(uint32_t);

            /* 读取 key */
            const char *key_str = data + offset;
            offset += key_len;

            /* 读取 value_len */
            memcpy(&val_len, data + offset, sizeof(uint32_t));
            offset += sizeof(uint32_t);

            /* 读取 value */
            const char *val_str = data + offset;

            printf(" key=\"%.*s\" value=\"%.*s\"",
                   (int)key_len, key_str,
                   (int)val_len, val_str);
        }
    }
    printf("\n");
}

/* ============================================================
 * WAL 写入函数
 * ============================================================ */

/** 写入 WAL 日志记录 */
static uint32_t wal_write(wal_op_t op, uint32_t txn_id,
                          const char *key, size_t key_len,
                          const char *value, size_t value_len) {
    /* 构建数据区：始终写入 key_len + key + value_len + value */
    size_t data_size = 0;
    char data[512] = {0};
    size_t offset = 0;

    /* key_len + key */
    memcpy(data + offset, &key_len, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    data_size += sizeof(uint32_t);

    if (key && key_len > 0) {
        memcpy(data + offset, key, key_len);
        offset += key_len;
        data_size += key_len;
    }

    /* value_len + value */
    memcpy(data + offset, &value_len, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    data_size += sizeof(uint32_t);

    if (value && value_len > 0) {
        memcpy(data + offset, value, value_len);
        data_size += value_len;
    }

    /* 构建记录头 */
    wal_record_header_t header = {
        .type = (uint8_t)op,
        .size = {(uint8_t)(data_size & 0xFF),
                 (uint8_t)((data_size >> 8) & 0xFF),
                 (uint8_t)((data_size >> 16) & 0xFF)},
        .lsn = g_current_lsn,
        .txn_id = txn_id,
        .prev_lsn = (g_current_lsn > 0) ? g_current_lsn - 1 : 0,
        .checksum = 0
    };

    /* 计算校验和 */
    header.checksum = calc_checksum(&header.type, sizeof(header) - sizeof(header.checksum));

    /* 写入 WAL 文件 */
    if (g_wal_file) {
        fwrite(&header, sizeof(header), 1, g_wal_file);
        if (data_size > 0) {
            fwrite(data, 1, data_size, g_wal_file);
        }
        fflush(g_wal_file);
    }

    /* 更新状态 */
    uint32_t lsn = g_current_lsn++;
    print_log_record(&header, data);

    /* 如果是检查点，更新检查点 LSN */
    if (op == WAL_OP_CHECKPOINT) {
        g_checkpoint_lsn = lsn;
    }

    return lsn;
}

/* ============================================================
 * 检查点操作
 * ============================================================ */

/** 生成检查点 */
static void checkpoint(void) {
    printf("[wal] === 生成检查点 ===\n");

    /* 刷所有脏页到磁盘（模拟） */
    printf("[wal] 刷脏页: %d 个页面\n", g_dirty_count);

    /* 写入检查点日志 */
    wal_write(WAL_OP_CHECKPOINT, 0, NULL, 0, NULL, 0);

    /* 重置脏页列表 */
    g_dirty_count = 0;

    printf("[wal] 检查点完成，恢复点: LSN=%u\n\n", g_checkpoint_lsn);
}

/* ============================================================
 * 崩溃恢复
 * ============================================================ */

/** 模拟崩溃恢复 */
static void crash_recovery(void) {
    printf("[wal] === 崩溃恢复开始 ===\n");
    printf("[wal] 上次检查点: LSN=%u, 当前: LSN=%u\n\n",
           g_checkpoint_lsn, g_current_lsn);

    /* 重新打开 WAL 文件 */
    FILE *wal = fopen("wal.log", "rb");
    if (!wal) {
        printf("[wal] 无 WAL 文件，从头开始\n");
        return;
    }

    /* 跳过文件头（如果有） */
    wal_record_header_t header;

    while (fread(&header, sizeof(header), 1, wal) == 1) {
        size_t data_size = header.size[0] | (header.size[1] << 8) | (header.size[2] << 16);

        /* 只处理检查点之后的日志 */
        if (header.lsn <= g_checkpoint_lsn) {
            if (data_size > 0) {
                fseek(wal, data_size, SEEK_CUR);
            }
            continue;
        }

        /* 读取数据 */
        char data[512] = {0};
        if (data_size > 0 && data_size < sizeof(data)) {
            fread(data, 1, data_size, wal);
        }

        printf("[wal] 恢复: ");
        print_log_record(&header, data);

        /* Redo: 重做已提交事务 */
        if (header.type == WAL_OP_INSERT || header.type == WAL_OP_UPDATE) {
            printf("[wal]   -> Redo: 重做数据变更\n");
        } else if (header.type == WAL_OP_COMMIT) {
            printf("[wal]   -> 已提交事务确认\n");
        } else if (header.type == WAL_OP_ABORT) {
            printf("[wal]   -> 未提交事务（需 Undo）\n");
        }
    }

    fclose(wal);
    printf("\n[wal] === 崩溃恢复完成 ===\n");
}

/* ============================================================
 * 模拟事务处理
 * ============================================================ */

static void simulate_transaction(uint32_t txn_id) {
    printf("[wal] --- 事务 T%d 开始 ---\n", txn_id);

    /* BEGIN */
    wal_write(WAL_OP_BEGIN, txn_id, NULL, 0, NULL, 0);

    /* 模拟数据操作 */
    char key[64], value[256];
    snprintf(key, sizeof(key), "key_%u", txn_id);
    snprintf(value, sizeof(value), "value_%u", txn_id);

    /* INSERT */
    printf("[wal] 插入: %s = %s\n", key, value);
    wal_write(WAL_OP_INSERT, txn_id, key, strlen(key), value, strlen(value));

    /* 标记脏页 */
    g_dirty_pages = realloc(g_dirty_pages, (g_dirty_count + 1) * sizeof(uint32_t));
    g_dirty_pages[g_dirty_count++] = txn_id % 10;

    /* 模拟 UPDATE */
    snprintf(value, sizeof(value), "updated_%u", txn_id);
    printf("[wal] 更新: %s = %s\n", key, value);
    wal_write(WAL_OP_UPDATE, txn_id, key, strlen(key), value, strlen(value));

    /* COMMIT */
    printf("[wal] 提交事务 T%d\n", txn_id);
    wal_write(WAL_OP_COMMIT, txn_id, NULL, 0, NULL, 0);

    /* 确保日志刷盘 */
    fflush(g_wal_file);
    printf("\n");
}

/* ============================================================
 * 主函数
 * ============================================================ */

int main(void) {
    printf("========================================\n");
    printf("   WAL 与崩溃恢复演示\n");
    printf("========================================\n\n");

    /* 初始化数据库状态 */
    memset(&g_db_state, 0, sizeof(g_db_state));

    /* 创建 WAL 文件 */
    g_wal_file = fopen("wal.log", "wb");
    if (!g_wal_file) {
        perror("[wal] 创建 WAL 文件失败");
        return 1;
    }
    printf("[wal] WAL 文件已创建\n\n");

    /* 模拟正常操作阶段 */
    printf("========== 阶段 1: 正常操作 ==========\n\n");
    simulate_transaction(1);
    simulate_transaction(2);

    /* 生成检查点 */
    checkpoint();

    simulate_transaction(3);

    /* 模拟崩溃 */
    printf("========== 阶段 2: 模拟崩溃 ==========\n\n");
    printf("[wal] !!! 系统崩溃 !!!\n\n");

    /* 关闭 WAL 文件 */
    if (g_wal_file) {
        fclose(g_wal_file);
        g_wal_file = NULL;
    }

    /* 执行崩溃恢复 */
    printf("========== 阶段 3: 崩溃恢复 ==========\n\n");
    crash_recovery();

    /* 清理 */
    free(g_dirty_pages);
    remove("wal.log");

    printf("\n========================================\n");
    printf("   演示完成\n");
    printf("========================================\n");

    return 0;
}
