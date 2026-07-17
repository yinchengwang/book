/**
 * @file data_integrity.h
 * @brief 数据完整性头文件
 *
 * 实现数据完整性保护机制：
 * 1. Page Checksum（页面校验和）
 * 2. 损坏检测接口
 * 3. WAL 一致性验证
 * 4. 自修复机制
 */
#ifndef DB_DATA_INTEGRITY_H
#define DB_DATA_INTEGRITY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Page Checksum
 * ======================================================================== */

/** Page Checksum 大小 */
#define PAGE_CHECKSUM_SIZE 2

/** Page Checksum 偏移量（页面末尾） */
#define PAGE_CHECKSUM_OFFSET 14

/**
 * @brief 计算页面的 CRC16 校验和
 *
 * @param page_data 页面数据
 * @param page_size 页面大小
 * @return CRC16 校验和
 */
uint16_t page_compute_checksum(const void *page_data, size_t page_size);

/**
 * @brief 验证页面的校验和
 *
 * @param page_data 页面数据
 * @param page_size 页面大小
 * @param stored_checksum 存储的校验和
 * @return true 校验通过
 */
bool page_verify_checksum(const void *page_data, size_t page_size,
                         uint16_t stored_checksum);

/**
 * @brief 设置页面校验和
 *
 * @param page_data 页面数据
 * @param page_size 页面大小
 */
void page_set_checksum(void *page_data, size_t page_size);

/**
 * @brief 获取页面校验和
 *
 * @param page_data 页面数据
 * @return 存储的校验和
 */
uint16_t page_get_checksum(const void *page_data);

/**
 * @brief 检查页面是否损坏
 *
 * @param page_data 页面数据
 * @param page_size 页面大小
 * @return true 页面损坏
 */
bool page_is_corrupted(const void *page_data, size_t page_size);

/* ========================================================================
 * 损坏检测
 * ======================================================================== */

/** 损坏类型 */
typedef enum CorruptionType_e {
    CORRUPTION_NONE = 0,
    CORRUPTION_CHECKSUM = 1,       /**< 校验和失败 */
    CORRUPTION_MAGIC = 2,          /**< 魔数错误 */
    CORRUPTION_SIZE = 3,           /**< 大小错误 */
    CORRUPTION_OFFSET = 4,         /**< 偏移错误 */
    CORRUPTION_POINTER = 5,         /**< 指针错误 */
    CORRUPTION_FORMAT = 6,         /**< 格式错误 */
    CORRUPTION_VERSION = 7,        /**< 版本不匹配 */
    CORRUPTION_INTERNAL = 8        /**< 内部一致性错误 */
} CorruptionType;

/** 损坏详情 */
typedef struct CorruptionInfo_s {
    CorruptionType type;        /**< 损坏类型 */
    uint32_t location;          /**< 损坏位置 */
    uint32_t context;           /**< 上下文信息 */
    const char *message;        /**< 描述信息 */
    const char *file;           /**< 相关文件 */
    uint32_t line;             /**< 相关行号 */
} CorruptionInfo;

/** 损坏检测结果 */
typedef struct CorruptionResult_s {
    bool corrupted;              /**< 是否损坏 */
    CorruptionInfo info;         /**< 损坏详情 */
} CorruptionResult;

/* ========================================================================
 * 引擎损坏检测接口
 * ======================================================================== */

/** 引擎类型 */
typedef enum EngineType_e {
    ENGINE_KV = 0,
    ENGINE_VECTOR = 1,
    ENGINE_TIMESERIES = 2,
    ENGINE_DOCUMENT = 3,
    ENGINE_SPATIAL = 4,
    ENGINE_GRAPH = 5,
    ENGINE_YANG = 6
} EngineType;

/**
 * @brief 引擎损坏检测函数类型
 */
typedef CorruptionResult (*EngineCheckFn)(const void *engine_data,
                                         size_t data_size,
                                         EngineType engine_type);

/**
 * @brief 引擎修复函数类型
 */
typedef int (*EngineRepairFn)(void *engine_data,
                             size_t data_size,
                             const CorruptionInfo *info);

/* ========================================================================
 * 统一损坏检测接口
 * ======================================================================== */

/**
 * @brief 检查 KV 引擎数据完整性
 */
CorruptionResult integrity_check_kv(const void *data, size_t size);

/**
 * @brief 检查向量引擎数据完整性
 */
CorruptionResult integrity_check_vector(const void *data, size_t size);

/**
 * @brief 检查时序引擎数据完整性
 */
CorruptionResult integrity_check_timeseries(const void *data, size_t size);

/**
 * @brief 检查文档引擎数据完整性
 */
CorruptionResult integrity_check_document(const void *data, size_t size);

/**
 * @brief 检查空间引擎数据完整性
 */
CorruptionResult integrity_check_spatial(const void *data, size_t size);

/**
 * @brief 检查图引擎数据完整性
 */
CorruptionResult integrity_check_graph(const void *data, size_t size);

/**
 * @brief 检查树引擎数据完整性
 */
CorruptionResult integrity_check_tree(const void *data, size_t size);

/**
 * @brief 检查 OPQ 索引数据完整性
 */
CorruptionResult integrity_check_opq(const void *data, size_t size);

/**
 * @brief 检查 RTree 索引数据完整性
 */
CorruptionResult integrity_check_rtree(const void *data, size_t size);

/**
 * @brief 统一检查接口
 */
CorruptionResult integrity_check(EngineType engine_type,
                                const void *data,
                                size_t size);

/* ========================================================================
 * WAL 一致性验证
 * ======================================================================== */

/** WAL 一致性检查级别 */
typedef enum WalConsistencyLevel_e {
    WAL_CONSISTENCY_NONE = 0,     /**< 不检查 */
    WAL_CONSISTENCY_BASIC = 1,    /**< 基本检查 */
    WAL_CONSISTENCY_FULL = 2     /**< 完全检查 */
} WalConsistencyLevel;

/**
 * @brief 验证 WAL 记录的一致性
 *
 * @param wal_data WAL 数据
 * @param data_size 数据大小
 * @param level 检查级别
 * @return true 一致
 */
bool wal_verify_consistency(const void *wal_data, size_t data_size,
                           WalConsistencyLevel level);

/**
 * @brief 验证 WAL 文件的完整性
 *
 * @param wal_path WAL 文件路径
 * @param level 检查级别
 * @return true 完整
 */
bool wal_file_verify(const char *wal_path, WalConsistencyLevel level);

/**
 * @brief 验证 WAL 记录序列
 *
 * 检查 LSN 序列和事务边界
 *
 * @param wal_data WAL 数据
 * @param data_size 数据大小
 * @return true 序列正确
 */
bool wal_sequence_verify(const void *wal_data, size_t data_size);

/**
 * @brief 验证事务完整性
 *
 * @param wal_data WAL 数据
 * @param data_size 数据大小
 * @param txn_id 事务 ID
 * @return true 事务完整
 */
bool wal_transaction_verify(const void *wal_data, size_t data_size,
                           uint32_t txn_id);

/* ========================================================================
 * 自修复机制
 * ======================================================================== */

/** 修复级别 */
typedef enum RepairLevel_e {
    REPAIR_NONE = 0,          /**< 不修复 */
    REPAIR_PAGE = 1,         /**< 修复页面 */
    REPAIR_INDEX = 2,        /**< 修复索引 */
    REPAIR_TABLE = 3         /**< 修复表 */
} RepairLevel;

/**
 * @brief 修复损坏的页面
 *
 * @param page_data 页面数据
 * @param page_size 页面大小
 * @param level 修复级别
 * @return 修复后的页面数据（可能与输入不同）
 */
void *page_repair(void *page_data, size_t page_size, RepairLevel level);

/**
 * @brief 从 WAL 重建损坏页面
 *
 * @param wal_data WAL 数据
 * @param wal_size WAL 大小
 * @param page_id 目标页面 ID
 * @param out_page 输出页面数据
 * @param out_size 输出页面大小
 * @return 0 成功
 */
int page_rebuild_from_wal(const void *wal_data, size_t wal_size,
                         uint32_t page_id,
                         void *out_page, size_t *out_size);

/**
 * @brief 验证并修复页面
 *
 * @param page_data 页面数据
 * @param page_size 页面大小
 * @param repair 是否自动修复
 * @return 修复结果
 */
CorruptionResult page_verify_and_repair(void *page_data, size_t page_size,
                                       bool repair);

/**
 * @brief 获取页面损坏的严重程度
 *
 * @param info 损坏信息
 * @return 严重程度（0-100）
 */
int corruption_severity(const CorruptionInfo *info);

/**
 * @brief 检查页面是否可修复
 *
 * @param info 损坏信息
 * @return true 可修复
 */
bool corruption_repairable(const CorruptionInfo *info);

/* ========================================================================
 * 完整性管理器
 * ======================================================================== */

/** 完整性配置 */
typedef struct IntegrityConfig_s {
    bool enable_checksum;           /**< 启用校验和 */
    bool enable_corruption_check;   /**< 启用损坏检测 */
    bool enable_auto_repair;       /**< 启用自动修复 */
    bool enable_wal_verify;        /**< 启用 WAL 验证 */
    WalConsistencyLevel wal_level;  /**< WAL 验证级别 */
    uint32_t repair_max_pages;     /**< 最大修复页面数 */
} IntegrityConfig;

/** 默认完整性配置 */
#define DEFAULT_INTEGRITY_CONFIG { \
    .enable_checksum = true, \
    .enable_corruption_check = true, \
    .enable_auto_repair = false, \
    .enable_wal_verify = true, \
    .wal_level = WAL_CONSISTENCY_BASIC, \
    .repair_max_pages = 100 \
}

/**
 * @brief 初始化完整性管理器
 */
void integrity_init(const IntegrityConfig *config);

/**
 * @brief 获取完整性配置
 */
const IntegrityConfig *integrity_get_config(void);

/**
 * @brief 设置完整性配置
 */
void integrity_set_config(const IntegrityConfig *config);

/* ========================================================================
 * 调试
 * ======================================================================== */

/**
 * @brief 打印损坏信息
 */
void corruption_dump(const CorruptionInfo *info);

/**
 * @brief 打印完整性统计
 */
void integrity_dump_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* DB_DATA_INTEGRITY_H */
