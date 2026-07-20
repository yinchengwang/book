/**
 * @file bgwriter.h
 * @brief 后台写进程（Background Writer）接口
 */
#ifndef DB_STORAGE_BUFFER_BGWRITER_H
#define DB_STORAGE_BUFFER_BGWRITER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* buf_initialized 在 db/buf.h 中 extern 声明 */

/* ============================================================
 * GUC 参数默认值
 * ============================================================ */

/** 后台写进程扫描间隔（毫秒） */
#define BGWRITER_DELAY_DEFAULT    100

/** 后台写进程最大写页数 */
#define BGWRITER_MAX_PAGES_DEFAULT 10

/** 后台写进程最大扫描次数（超过则强制全量扫描） */
#define BGWRITER_MAX_WRITES_DEFAULT 10

/* ============================================================
 * 统计信息
 * ============================================================ */

/** 后台写进程统计 */
typedef struct BgWriterStats {
    uint64_t checkpoints_timed;     /**< 定时检查点次数 */
    uint64_t checkpoints_req;       /**< 请求检查点次数 */
    uint64_t checkpoints_written;   /**< 检查点写出的页面数 */
    uint64_t bgwriter_writes;       /**< 后台写出的页面数 */
    uint64_t bgwriter_startups;     /**< 后台写进程启动次数 */
    uint64_t bgwriter_shutdowns;    /**< 后台写进程关闭次数 */
} BgWriterStats;

/* ============================================================
 * 后台写进程控制
 * ============================================================ */

/**
 * @brief 启动后台写进程
 * @return 0 成功，-1 失败
 */
int bgwriter_start(void);

/**
 * @brief 停止后台写进程
 * @note 会等待当前写操作完成
 */
void bgwriter_stop(void);

/**
 * @brief 检查后台写进程是否在运行
 * @return true 运行中
 */
bool bgwriter_is_running(void);

/**
 * @brief 强制后台写进程执行一次扫描和写操作
 * @return 写入的页面数
 */
int bgwriter_do_checkpoint(void);

/* ============================================================
 * GUC 参数（供外部配置）
 * ============================================================ */

/**
 * @brief 设置扫描间隔
 * @param delay_ms 间隔（毫秒）
 */
void bgwriter_set_delay(int delay_ms);

/**
 * @brief 获取扫描间隔
 * @return 间隔（毫秒）
 */
int bgwriter_get_delay(void);

/**
 * @brief 设置每次最大写页数
 * @param max_pages 最大页数
 */
void bgwriter_set_max_pages(int max_pages);

/* ============================================================
 * 统计信息
 * ============================================================ */

/**
 * @brief 获取统计信息
 * @param stats 输出统计信息
 */
void bgwriter_get_stats(BgWriterStats *stats);

/**
 * @brief 重置统计信息
 */
void bgwriter_reset_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* DB_STORAGE_BUFFER_BGWRITER_H */
