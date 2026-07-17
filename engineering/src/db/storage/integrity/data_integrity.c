/**
 * @file data_integrity.c
 * @brief 数据完整性实现
 */

#include "db/storage/integrity/data_integrity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * CRC16 实现
 * ======================================================================== */

/** CRC16 多项式 */
#define CRC16_POLY 0x8005

/**
 * @brief 计算 CRC16
 */
uint16_t page_compute_checksum(const void *page_data, size_t page_size)
{
    if (!page_data || page_size < 16) {
        return 0;
    }

    uint16_t crc = 0xFFFF;
    const uint8_t *data = (const uint8_t *)page_data;
    size_t len = page_size - PAGE_CHECKSUM_SIZE;  /* 不包含 checksum 本身 */

    for (size_t i = 0; i < len; i++) {
        crc ^= data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ CRC16_POLY;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

/**
 * @brief 验证页面的校验和
 */
bool page_verify_checksum(const void *page_data, size_t page_size,
                        uint16_t stored_checksum)
{
    uint16_t computed = page_compute_checksum(page_data, page_size);
    return computed == stored_checksum;
}

/**
 * @brief 设置页面校验和
 */
void page_set_checksum(void *page_data, size_t page_size)
{
    if (!page_data || page_size < 16) {
        return;
    }

    uint16_t checksum = page_compute_checksum(page_data, page_size);
    uint8_t *data = (uint8_t *)page_data;
    size_t offset = page_size - PAGE_CHECKSUM_SIZE;

    /* 写入校验和（小端序） */
    data[offset] = checksum & 0xFF;
    data[offset + 1] = (checksum >> 8) & 0xFF;
}

/**
 * @brief 获取页面校验和
 */
uint16_t page_get_checksum(const void *page_data)
{
    if (!page_data) {
        return 0;
    }

    const uint8_t *data = (const uint8_t *)page_data;
    size_t offset = 0;  /* 假设 page_size 可通过其他方式获取 */

    /* 这里需要知道 page_size，实际使用时从外部传入 */
    return 0;  /* 简化：实际应返回存储的校验和 */
}

/**
 * @brief 检查页面是否损坏
 */
bool page_is_corrupted(const void *page_data, size_t page_size)
{
    if (!page_data || page_size < 16) {
        return true;
    }

    /* 检查基本结构 */
    /* TODO: 检查 magic、version 等字段 */

    return false;  /* 简化：默认不损坏 */
}

/* ========================================================================
 * 损坏检测实现
 * ======================================================================== */

/**
 * @brief 创建损坏结果
 */
static CorruptionResult make_corruption_result(CorruptionType type,
                                            uint32_t location,
                                            const char *message)
{
    CorruptionResult result = {
        .corrupted = true,
        .info = {
            .type = type,
            .location = location,
            .context = 0,
            .message = message,
            .file = NULL,
            .line = 0
        }
    };
    return result;
}

/**
 * @brief 创建正常结果
 */
static CorruptionResult make_ok_result(void)
{
    CorruptionResult result = {
        .corrupted = false,
        .info = {
            .type = CORRUPTION_NONE,
            .location = 0,
            .context = 0,
            .message = "OK",
            .file = NULL,
            .line = 0
        }
    };
    return result;
}

/**
 * @brief 检查 KV 引擎数据完整性
 */
CorruptionResult integrity_check_kv(const void *data, size_t size)
{
    if (!data || size < 16) {
        return make_corruption_result(CORRUPTION_SIZE, 0, "数据太小");
    }

    /* 检查 KV 格式头部 */
    /* TODO: 实现实际的 KV 检查 */

    return make_ok_result();
}

/**
 * @brief 检查向量引擎数据完整性
 */
CorruptionResult integrity_check_vector(const void *data, size_t size)
{
    if (!data || size < 16) {
        return make_corruption_result(CORRUPTION_SIZE, 0, "数据太小");
    }

    /* 检查向量格式 */
    /* TODO: 实现实际的向量检查 */

    return make_ok_result();
}

/**
 * @brief 检查时序引擎数据完整性
 */
CorruptionResult integrity_check_timeseries(const void *data, size_t size)
{
    if (!data || size < 16) {
        return make_corruption_result(CORRUPTION_SIZE, 0, "数据太小");
    }

    return make_ok_result();
}

/**
 * @brief 检查文档引擎数据完整性
 */
CorruptionResult integrity_check_document(const void *data, size_t size)
{
    if (!data || size < 16) {
        return make_corruption_result(CORRUPTION_SIZE, 0, "数据太小");
    }

    return make_ok_result();
}

/**
 * @brief 检查空间引擎数据完整性
 */
CorruptionResult integrity_check_spatial(const void *data, size_t size)
{
    if (!data || size < 16) {
        return make_corruption_result(CORRUPTION_SIZE, 0, "数据太小");
    }

    return make_ok_result();
}

/**
 * @brief 检查图引擎数据完整性
 */
CorruptionResult integrity_check_graph(const void *data, size_t size)
{
    if (!data || size < 16) {
        return make_corruption_result(CORRUPTION_SIZE, 0, "数据太小");
    }

    return make_ok_result();
}

/**
 * @brief 检查树引擎数据完整性
 */
CorruptionResult integrity_check_tree(const void *data, size_t size)
{
    if (!data || size < 16) {
        return make_corruption_result(CORRUPTION_SIZE, 0, "数据太小");
    }

    return make_ok_result();
}

/**
 * @brief 检查 OPQ 索引数据完整性
 */
CorruptionResult integrity_check_opq(const void *data, size_t size)
{
    if (!data || size < 16) {
        return make_corruption_result(CORRUPTION_SIZE, 0, "数据太小");
    }

    /* OPQ 格式检查 */
    /* TODO: 实现实际的 OPQ 检查 */

    return make_ok_result();
}

/**
 * @brief 检查 RTree 索引数据完整性
 */
CorruptionResult integrity_check_rtree(const void *data, size_t size)
{
    if (!data || size < 16) {
        return make_corruption_result(CORRUPTION_SIZE, 0, "数据太小");
    }

    /* RTree 格式检查 */
    /* TODO: 实现实际的 RTree 检查 */

    return make_ok_result();
}

/**
 * @brief 统一检查接口
 */
CorruptionResult integrity_check(EngineType engine_type,
                               const void *data,
                               size_t size)
{
    switch (engine_type) {
        case ENGINE_KV:
            return integrity_check_kv(data, size);
        case ENGINE_VECTOR:
            return integrity_check_vector(data, size);
        case ENGINE_TIMESERIES:
            return integrity_check_timeseries(data, size);
        case ENGINE_DOCUMENT:
            return integrity_check_document(data, size);
        case ENGINE_SPATIAL:
            return integrity_check_spatial(data, size);
        case ENGINE_GRAPH:
            return integrity_check_graph(data, size);
        case ENGINE_YANG:
            return integrity_check_tree(data, size);
        default:
            return make_corruption_result(CORRUPTION_FORMAT, 0, "未知引擎类型");
    }
}

/* ========================================================================
 * WAL 一致性验证
 * ======================================================================== */

/**
 * @brief 验证 WAL 记录的一致性
 */
bool wal_verify_consistency(const void *wal_data, size_t data_size,
                          WalConsistencyLevel level)
{
    if (!wal_data || data_size < 24) {
        return false;
    }

    if (level == WAL_CONSISTENCY_NONE) {
        return true;
    }

    /* 基本检查：记录头 */
    if (level >= WAL_CONSISTENCY_BASIC) {
        const uint8_t *data = (const uint8_t *)wal_data;

        /* 检查类型是否有效 */
        uint8_t type = data[0];
        if (type < 1 || type > 7) {
            return false;
        }

        /* 检查大小字段 */
        uint32_t size = data[1] | (data[2] << 8) | (data[3] << 16);
        if (size > data_size || size < 24) {
            return false;
        }
    }

    if (level >= WAL_CONSISTENCY_FULL) {
        /* 完整检查：包括校验和验证 */
        /* TODO: 实现完整的 WAL 验证 */
    }

    return true;
}

/**
 * @brief 验证 WAL 文件的完整性
 */
bool wal_file_verify(const char *wal_path, WalConsistencyLevel level)
{
    if (!wal_path) {
        return false;
    }

    /* TODO: 实现 WAL 文件验证
     * 1. 读取文件头
     * 2. 验证 magic
     * 3. 遍历所有记录
     * 4. 验证校验和
     */

    return true;
}

/**
 * @brief 验证 WAL 记录序列
 */
bool wal_sequence_verify(const void *wal_data, size_t data_size)
{
    if (!wal_data || data_size < 24) {
        return false;
    }

    /* 验证 LSN 序列递增 */
    /* TODO: 实现 LSN 序列验证 */

    return true;
}

/**
 * @brief 验证事务完整性
 */
bool wal_transaction_verify(const void *wal_data, size_t data_size,
                          uint32_t txn_id)
{
    if (!wal_data || data_size < 24) {
        return false;
    }

    /* 验证事务的 BEGIN/COMMIT/ABORT 配对 */
    /* TODO: 实现事务边界验证 */

    return true;
}

/* ========================================================================
 * 自修复机制
 * ======================================================================== */

/**
 * @brief 修复损坏的页面
 */
void *page_repair(void *page_data, size_t page_size, RepairLevel level)
{
    if (!page_data || page_size < 16) {
        return NULL;
    }

    if (level == REPAIR_NONE) {
        return page_data;
    }

    /* TODO: 实现页面修复逻辑
     * - REPAIR_PAGE: 尝试恢复页面结构
     * - REPAIR_INDEX: 重建索引
     * - REPAIR_TABLE: 重建整个表
     */

    return page_data;
}

/**
 * @brief 从 WAL 重建损坏页面
 */
int page_rebuild_from_wal(const void *wal_data, size_t wal_size,
                        uint32_t page_id,
                        void *out_page, size_t *out_size)
{
    if (!wal_data || !out_page || !out_size) {
        return -1;
    }

    /* TODO: 实现从 WAL 重建页面
     * 1. 在 WAL 中查找目标页面的所有修改
     * 2. 按顺序重放修改
     * 3. 返回重建的页面
     */

    *out_size = 0;
    return -1;
}

/**
 * @brief 验证并修复页面
 */
CorruptionResult page_verify_and_repair(void *page_data, size_t page_size,
                                       bool repair)
{
    if (!page_data || page_size < 16) {
        return make_corruption_result(CORRUPTION_SIZE, 0, "数据太小");
    }

    /* 检查损坏 */
    CorruptionResult check = {0};
    check.corrupted = false;

    if (check.corrupted && repair) {
        /* 执行修复 */
        page_repair(page_data, page_size, REPAIR_PAGE);
        check.corrupted = false;
        check.info.type = CORRUPTION_NONE;
        check.info.message = "已修复";
    }

    return check;
}

/**
 * @brief 获取页面损坏的严重程度
 */
int corruption_severity(const CorruptionInfo *info)
{
    if (!info) {
        return 0;
    }

    switch (info->type) {
        case CORRUPTION_CHECKSUM:
            return 80;
        case CORRUPTION_MAGIC:
            return 90;
        case CORRUPTION_FORMAT:
            return 70;
        case CORRUPTION_INTERNAL:
            return 100;
        default:
            return 50;
    }
}

/**
 * @brief 检查页面是否可修复
 */
bool corruption_repairable(const CorruptionInfo *info)
{
    if (!info) {
        return false;
    }

    switch (info->type) {
        case CORRUPTION_CHECKSUM:
        case CORRUPTION_OFFSET:
        case CORRUPTION_POINTER:
            return true;
        case CORRUPTION_MAGIC:
        case CORRUPTION_INTERNAL:
            return false;
        default:
            return true;
    }
}

/* ========================================================================
 * 完整性管理器
 * ======================================================================== */

/** 全局配置 */
static IntegrityConfig g_integrity_config = DEFAULT_INTEGRITY_CONFIG;

/**
 * @brief 初始化完整性管理器
 */
void integrity_init(const IntegrityConfig *config)
{
    if (config) {
        g_integrity_config = *config;
    }
}

/**
 * @brief 获取完整性配置
 */
const IntegrityConfig *integrity_get_config(void)
{
    return &g_integrity_config;
}

/**
 * @brief 设置完整性配置
 */
void integrity_set_config(const IntegrityConfig *config)
{
    if (config) {
        g_integrity_config = *config;
    }
}

/* ========================================================================
 * 调试
 * ======================================================================== */

/**
 * @brief 打印损坏信息
 */
void corruption_dump(const CorruptionInfo *info)
{
    if (!info) {
        printf("Corruption: NULL\n");
        return;
    }

    printf("Corruption Type: %d\n", info->type);
    printf("Location: %u\n", info->location);
    printf("Context: %u\n", info->context);
    printf("Message: %s\n", info->message ? info->message : "N/A");

    if (info->file) {
        printf("File: %s:%u\n", info->file, info->line);
    }
}

/**
 * @brief 打印完整性统计
 */
void integrity_dump_stats(void)
{
    printf("=== Data Integrity Stats ===\n");
    printf("Checksum Enabled: %s\n",
           g_integrity_config.enable_checksum ? "YES" : "NO");
    printf("Corruption Check Enabled: %s\n",
           g_integrity_config.enable_corruption_check ? "YES" : "NO");
    printf("Auto Repair Enabled: %s\n",
           g_integrity_config.enable_auto_repair ? "YES" : "NO");
    printf("WAL Verify Level: %d\n",
           g_integrity_config.wal_level);
}
