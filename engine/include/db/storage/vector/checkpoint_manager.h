/**
 * @file checkpoint_manager.h
 * @brief Vector WAL 后台 CheckpointManager（P1-B Task 7）
 *
 * 设计要点：
 *  - pthread + mutex + cond_timedwait 实现周期唤醒
 *  - 每次唤醒执行 flush + checkpoint 记录 LSN
 *  - **不做 truncate**：WAL 是向量数据的唯一真相，而后台线程只持有 wal 句柄、
 *    看不到集合数据，无法在截断前把数据落成快照。历史实现在此处无条件 truncate，
 *    等于周期性丢弃尚未落盘的已确认写入。
 *    截断统一由 vector_api_save 在"快照写入成功"之后执行。
 *  - 提供 LSN 暴露接口（checkpoint_manager_get_lsn）供测试验证
 *  - destroy 时先 stop 再 free，避免线程与释放竞争
 */
#ifndef DB_STORAGE_VECTOR_CHECKPOINT_MANAGER_H
#define DB_STORAGE_VECTOR_CHECKPOINT_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <db/storage/vector/vector_persist.h>  /* vector_wal_t / vector_checkpoint_type_t */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CheckpointManager_s CheckpointManager;

/**
 * @brief 创建 CheckpointManager（不启动线程）
 * @param wal         目标 WAL 句柄（不能为空）
 * @param interval_ms 周期（毫秒）
 * @return 句柄，失败返回 NULL
 */
CheckpointManager *checkpoint_manager_create(vector_wal_t *wal, uint32_t interval_ms);

/**
 * @brief 销毁（先 stop 再 free；mgr 为 NULL 时安全 noop）
 */
void checkpoint_manager_destroy(CheckpointManager *mgr);

/**
 * @brief 启动后台线程
 * @return 0 成功，-1 失败
 */
int checkpoint_manager_start(CheckpointManager *mgr);

/**
 * @brief 停止后台线程（pthread_join）
 * @return 0 成功，-1 失败
 */
int checkpoint_manager_stop(CheckpointManager *mgr);

/**
 * @brief 暴露当前 last_checkpoint LSN（测试用，0 表示尚未 checkpoint）
 */
uint64_t checkpoint_manager_get_lsn(CheckpointManager *mgr);

#ifdef __cplusplus
}
#endif

#endif /* DB_STORAGE_VECTOR_CHECKPOINT_MANAGER_H */
