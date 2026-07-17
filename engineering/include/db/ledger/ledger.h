/**
 * @file ledger.h
 * @brief 账本接口
 *
 * 定义对账模块的核心接口，支持账目录入、查询和一致性校验。
 */
#ifndef DB_LEDGER_H
#define DB_LEDGER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 常量定义
 * ======================================================================== */

/** 账本文件魔数 */
#define LEDGER_FILE_MAGIC 0x4C454447  /* "LEDG" */

/** 账本文件版本 */
#define LEDGER_FILE_VERSION 1

/** 最大幂等键长度 */
#define LEDGER_MAX_IDEMPOTENCY_KEY 128

/** 最大描述长度 */
#define LEDGER_MAX_DESCRIPTION 256

/* ========================================================================
 * 类型定义
 * ======================================================================== */

/**
 * @brief 账目录入类型
 */
typedef enum {
    LEDGER_ENTRY_DEBIT = 0,   /**< 借方（支出） */
    LEDGER_ENTRY_CREDIT = 1,  /**< 贷方（收入） */
} ledger_entry_type_t;

/**
 * @brief 账目条目
 */
typedef struct ledger_entry_s {
    uint64_t sequence;                     /**< 序列号（递增） */
    char idempotency_key[LEDGER_MAX_IDEMPOTENCY_KEY];  /**< 幂等键 */
    ledger_entry_type_t type;              /**< 类型 */
    int64_t amount;                        /**< 金额（最小单位，如分） */
    int64_t balance_after;                 /**< 操作后余额 */
    char description[LEDGER_MAX_DESCRIPTION];  /**< 描述 */
    uint64_t timestamp;                    /**< 时间戳（微秒） */
    uint8_t prev_hash[32];                 /**< 前一条记录的哈希 */
    uint8_t entry_hash[32];                /**< 本条记录的哈希 */
} ledger_entry_t;

/**
 * @brief 对账结果
 */
typedef struct ledger_reconcile_result_s {
    bool is_balanced;               /**< 是否平衡 */
    int64_t total_debit;            /**< 借方总额 */
    int64_t total_credit;           /**< 贷方总额 */
    int64_t expected_balance;       /**< 期望余额 */
    int64_t actual_balance;         /**< 实际余额 */
    int64_t discrepancy;            /**< 差异金额 */
    int32_t entry_count;            /**< 账目总数 */
    int32_t hash_chain_errors;      /**< 哈希链错误数 */
    int32_t duplicate_keys;         /**< 重复幂等键数 */
} ledger_reconcile_result_t;

/**
 * @brief 账本状态
 */
typedef enum {
    LEDGER_STATE_OK = 0,           /**< 正常 */
    LEDGER_STATE_CORRUPTED = 1,    /**< 数据损坏 */
    LEDGER_STATE_RECONCILING = 2,  /**< 对账中 */
} ledger_state_t;

/**
 * @brief 账本
 */
typedef struct ledger_s {
    char name[64];                  /**< 账本名称 */
    char data_dir[512];             /**< 数据目录 */
    ledger_state_t state;           /**< 状态 */

    int64_t current_balance;        /**< 当前余额 */
    uint64_t last_sequence;         /**< 最后序列号 */

    ledger_entry_t **entries;       /**< 账目数组 */
    int32_t entry_count;            /**< 账目数量 */
    int32_t capacity;               /**< 容量 */

    /* 幂等性保障 */
    void *idempotency_map;          /**< 幂等键映射（实现时具体化） */
} ledger_t;

/* ========================================================================
 * 生命周期 API
 * ======================================================================== */

/**
 * @brief 创建账本
 *
 * @param name 账本名称
 * @param data_dir 数据目录
 * @return 账本指针，失败返回 NULL
 */
ledger_t *ledger_create(const char *name, const char *data_dir);

/**
 * @brief 打开账本
 *
 * @param name 账本名称
 * @param data_dir 数据目录
 * @return 账本指针，失败返回 NULL
 */
ledger_t *ledger_open(const char *name, const char *data_dir);

/**
 * @brief 关闭账本
 *
 * @param ledger 账本
 * @return 0 成功，-1 失败
 */
int ledger_close(ledger_t *ledger);

/**
 * @brief 销毁账本
 *
 * @param ledger 账本
 */
void ledger_destroy(ledger_t *ledger);

/* ========================================================================
 * 账目录入 API
 * ======================================================================== */

/**
 * @brief 添加账目
 *
 * @param ledger 账本
 * @param type 类型（借方/贷方）
 * @param amount 金额
 * @param idempotency_key 幂等键（可为空）
 * @param description 描述
 * @return 序列号，-1 表示失败（可能是重复幂等键）
 */
int64_t ledger_add_entry(ledger_t *ledger,
                         ledger_entry_type_t type,
                         int64_t amount,
                         const char *idempotency_key,
                         const char *description);

/**
 * @brief 批量添加账目
 *
 * @param ledger 账本
 * @param entries 账目数组
 * @param count 数量
 * @return 成功添加的数量
 */
int32_t ledger_add_entries_batch(ledger_t *ledger,
                                 const ledger_entry_t *entries,
                                 int32_t count);

/* ========================================================================
 * 账目查询 API
 * ======================================================================== */

/**
 * @brief 获取账目
 *
 * @param ledger 账本
 * @param sequence 序列号
 * @return 账目指针，未找到返回 NULL
 */
const ledger_entry_t *ledger_get_entry(const ledger_t *ledger, uint64_t sequence);

/**
 * @brief 获取最后 N 条账目
 *
 * @param ledger 账本
 * @param count 数量
 * @param out_entries 输出数组
 * @param max_entries 最大数量
 * @return 实际数量
 */
int32_t ledger_get_last_entries(const ledger_t *ledger,
                                int32_t count,
                                ledger_entry_t *out_entries,
                                int32_t max_entries);

/**
 * @brief 按时间范围查询
 *
 * @param ledger 账本
 * @param start_time 起始时间
 * @param end_time 结束时间
 * @param out_entries 输出数组
 * @param max_entries 最大数量
 * @return 实际数量
 */
int32_t ledger_query_by_time(const ledger_t *ledger,
                             uint64_t start_time,
                             uint64_t end_time,
                             ledger_entry_t *out_entries,
                             int32_t max_entries);

/**
 * @brief 检查幂等键是否存在
 *
 * @param ledger 账本
 * @param idempotency_key 幂等键
 * @return true 存在，false 不存在
 */
bool ledger_has_idempotency_key(const ledger_t *ledger, const char *idempotency_key);

/* ========================================================================
 * 账本校验 API
 * ======================================================================== */

/**
 * @brief 验证哈希链完整性
 *
 * @param ledger 账本
 * @return true 完整，false 损坏
 */
bool ledger_verify_hash_chain(const ledger_t *ledger);

/**
 * @brief 验证余额一致性
 *
 * @param ledger 账本
 * @return true 一致，false 不一致
 */
bool ledger_verify_balance(const ledger_t *ledger);

/**
 * @brief 执行对账
 *
 * @param ledger 账本
 * @return 对账结果（调用者需释放）
 */
ledger_reconcile_result_t *ledger_reconcile(ledger_t *ledger);

/**
 * @brief 释放对账结果
 *
 * @param result 对账结果
 */
void ledger_free_reconcile_result(ledger_reconcile_result_t *result);

/* ========================================================================
 * 持久化 API
 * ======================================================================== */

/**
 * @brief 保存账本
 *
 * @param ledger 账本
 * @return 0 成功，-1 失败
 */
int ledger_save(const ledger_t *ledger);

/**
 * @brief 加载账本
 *
 * @param name 账本名称
 * @param data_dir 数据目录
 * @return 账本指针，失败返回 NULL
 */
ledger_t *ledger_load(const char *name, const char *data_dir);

/* ========================================================================
 * 统计 API
 * ======================================================================== */

/**
 * @brief 获取账本统计
 *
 * @param ledger 账本
 * @param out_balance 输出当前余额
 * @param out_count 输出账目数量
 * @param out_debit 输出借方总额
 * @param out_credit 输出贷方总额
 */
void ledger_get_stats(const ledger_t *ledger,
                      int64_t *out_balance,
                      uint64_t *out_count,
                      int64_t *out_debit,
                      int64_t *out_credit);

#ifdef __cplusplus
}
#endif

#endif /* DB_LEDGER_H */
