/**
 * @file cdc.h
 * @brief 变更数据捕获 (Change Data Capture) 接口
 *
 * 定义 CDC 的核心接口，用于捕获数据库的变更操作。
 */
#ifndef DB_CDC_H
#define DB_CDC_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 常量定义
 * ======================================================================== */

/** CDC 文件魔数 */
#define CDC_FILE_MAGIC 0x43444301  /* "CDC\1" */

/** CDC 文件版本 */
#define CDC_FILE_VERSION 1

/** 最大关系数 */
#define CDC_MAX_RELATIONS 256

/* ========================================================================
 * 类型定义
 * ======================================================================== */

/**
 * @brief CDC 变更类型枚举
 */
typedef enum {
    CDC_CHANGE_INSERT = 0,   /**< 插入操作 */
    CDC_CHANGE_UPDATE = 1,   /**< 更新操作 */
    CDC_CHANGE_DELETE = 2,   /**< 删除操作 */
    CDC_CHANGE_TRUNCATE = 3, /**< 截断操作 */
} cdc_change_type_t;

/**
 * @brief CDC 变更记录
 */
typedef struct cdc_change_s {
    uint64_t lsn;                /**< WAL 日志序列号 */
    cdc_change_type_t type;      /**< 变更类型 */
    int32_t relation_id;         /**< 关系 ID */
    uint64_t transaction_id;     /**< 事务 ID */
    uint64_t timestamp;          /**< 变更时间戳（微秒） */
    int32_t old_tuple_size;      /**< 旧元组大小 */
    int32_t new_tuple_size;      /**< 新元组大小 */
    void *old_tuple;             /**< 旧元组数据（UPDATE/DELETE） */
    void *new_tuple;             /**< 新元组数据（INSERT/UPDATE） */
    struct cdc_change_s *next;   /**< 链表下一项 */
} cdc_change_t;

/**
 * @brief CDC 上下文
 */
typedef struct cdc_context_s {
    char data_dir[512];          /**< 数据目录 */
    uint64_t start_lsn;          /**< 起始 LSN */
    uint64_t current_lsn;        /**< 当前 LSN */
    int32_t relation_count;      /**< 监控的表数量 */
    int32_t *relation_ids;       /**< 监控的表 ID 数组 */
    void *wal_reader;            /**< WAL 读取器（实现时具体化） */
} cdc_context_t;

/* ========================================================================
 * 生命周期 API
 * ======================================================================== */

/**
 * @brief 创建 CDC 上下文
 *
 * @param data_dir 数据目录
 * @param start_lsn 起始 LSN（0 表示从头开始）
 * @return CDC 上下文指针，失败返回 NULL
 */
cdc_context_t *cdc_create(const char *data_dir, uint64_t start_lsn);

/**
 * @brief 销毁 CDC 上下文
 *
 * @param ctx CDC 上下文
 */
void cdc_destroy(cdc_context_t *ctx);

/**
 * @brief 重置 CDC 状态
 *
 * @param ctx CDC 上下文
 */
void cdc_reset(cdc_context_t *ctx);

/* ========================================================================
 * 配置 API
 * ======================================================================== */

/**
 * @brief 添加监控的表
 *
 * @param ctx CDC 上下文
 * @param relation_id 表 ID
 * @return 0 成功，-1 失败
 */
int cdc_add_relation(cdc_context_t *ctx, int32_t relation_id);

/**
 * @brief 移除监控的表
 *
 * @param ctx CDC 上下文
 * @param relation_id 表 ID
 * @return 0 成功，-1 失败
 */
int cdc_remove_relation(cdc_context_t *ctx, int32_t relation_id);

/**
 * @brief 检查表是否被监控
 *
 * @param ctx CDC 上下文
 * @param relation_id 表 ID
 * @return true 被监控，false 未监控
 */
bool cdc_is_relation_monitored(const cdc_context_t *ctx, int32_t relation_id);

/* ========================================================================
 * 变更捕获 API
 * ======================================================================== */

/**
 * @brief 记录插入操作
 *
 * @param ctx CDC 上下文
 * @param relation_id 表 ID
 * @param transaction_id 事务 ID
 * @param tuple 元组数据
 * @param tuple_size 元组大小
 * @return 0 成功，-1 失败
 */
int cdc_record_insert(cdc_context_t *ctx,
                      int32_t relation_id,
                      uint64_t transaction_id,
                      const void *tuple,
                      int32_t tuple_size);

/**
 * @brief 记录更新操作
 *
 * @param ctx CDC 上下文
 * @param relation_id 表 ID
 * @param transaction_id 事务 ID
 * @param old_tuple 旧元组
 * @param old_tuple_size 旧元组大小
 * @param new_tuple 新元组
 * @param new_tuple_size 新元组大小
 * @return 0 成功，-1 失败
 */
int cdc_record_update(cdc_context_t *ctx,
                      int32_t relation_id,
                      uint64_t transaction_id,
                      const void *old_tuple,
                      int32_t old_tuple_size,
                      const void *new_tuple,
                      int32_t new_tuple_size);

/**
 * @brief 记录删除操作
 *
 * @param ctx CDC 上下文
 * @param relation_id 表 ID
 * @param transaction_id 事务 ID
 * @param tuple 元组数据
 * @param tuple_size 元组大小
 * @return 0 成功，-1 失败
 */
int cdc_record_delete(cdc_context_t *ctx,
                      int32_t relation_id,
                      uint64_t transaction_id,
                      const void *tuple,
                      int32_t tuple_size);

/* ========================================================================
 * 变更获取 API
 * ======================================================================== */

/**
 * @brief 获取下一个变更记录
 *
 * @param ctx CDC 上下文
 * @return 变更记录指针，NULL 表示无更多记录
 */
cdc_change_t *cdc_get_next_change(cdc_context_t *ctx);

/**
 * @brief 释放变更记录
 *
 * @param change 变更记录
 */
void cdc_free_change(cdc_change_t *change);

/**
 * @brief 获取未发送的变更数量
 *
 * @param ctx CDC 上下文
 * @return 未发送数量
 */
int32_t cdc_get_pending_count(const cdc_context_t *ctx);

/* ========================================================================
 * 持久化 API
 * ======================================================================== */

/**
 * @brief 保存 CDC 状态
 *
 * @param ctx CDC 上下文
 * @return 0 成功，-1 失败
 */
int cdc_save_state(const cdc_context_t *ctx);

/**
 * @brief 加载 CDC 状态
 *
 * @param ctx CDC 上下文
 * @return 0 成功，-1 失败
 */
int cdc_load_state(cdc_context_t *ctx);

/**
 * @brief 获取当前 LSN
 *
 * @param ctx CDC 上下文
 * @return 当前 LSN
 */
uint64_t cdc_get_current_lsn(const cdc_context_t *ctx);

/**
 * @brief 设置 LSN
 *
 * @param ctx CDC 上下文
 * @param lsn 新的 LSN
 */
void cdc_set_lsn(cdc_context_t *ctx, uint64_t lsn);

/* ========================================================================
 * WAL 解析 API
 * ======================================================================== */

/**
 * @brief WAL 解析器（内部使用）
 */
typedef struct wal_parser_s wal_parser_t;

/**
 * @brief 创建 WAL 解析器
 *
 * @param wal_dir WAL 目录
 * @return 解析器指针，失败返回 NULL
 */
wal_parser_t *wal_parser_create(const char *wal_dir);

/**
 * @brief 销毁 WAL 解析器
 *
 * @param parser 解析器
 */
void wal_parser_destroy(wal_parser_t *parser);

/**
 * @brief 打开 WAL 文件
 *
 * @param parser 解析器
 * @param start_lsn 起始 LSN
 * @return 0 成功，-1 失败
 */
int wal_parser_open(wal_parser_t *parser, uint64_t start_lsn);

/**
 * @brief 关闭 WAL 文件
 *
 * @param parser 解析器
 */
void wal_parser_close(wal_parser_t *parser);

/**
 * @brief 获取下一条变更记录
 *
 * @param parser 解析器
 * @return 变更记录，NULL 表示结束
 */
cdc_change_t *wal_parser_next_change(wal_parser_t *parser);

/**
 * @brief 设置解析起始位置
 *
 * @param parser 解析器
 * @param lsn 起始 LSN
 */
void wal_parser_set_position(wal_parser_t *parser, uint64_t lsn);

/**
 * @brief 获取当前解析位置
 *
 * @param parser 解析器
 * @return 当前 LSN
 */
uint64_t wal_parser_get_position(const wal_parser_t *parser);

/**
 * @brief 从 WAL 解析变更
 *
 * @param ctx CDC 上下文
 * @param wal_dir WAL 目录
 * @param start_lsn 起始 LSN
 * @param out_changes 输出变更数组
 * @param max_changes 最大变更数
 * @return 实际解析的变更数
 */
int32_t cdc_parse_wal(cdc_context_t *ctx,
                      const char *wal_dir,
                      uint64_t start_lsn,
                      cdc_change_t **out_changes,
                      int32_t max_changes);

/**
 * @brief 提取单条变更记录
 *
 * @param wal_dir WAL 目录
 * @param lsn 要提取的 LSN
 * @return 变更记录，NULL 表示未找到
 */
cdc_change_t *cdc_extract_change(const char *wal_dir, uint64_t lsn);

#ifdef __cplusplus
}
#endif

#endif /* DB_CDC_H */
