/**
 * @file px_scheduler.h
 * @brief Gap#4 并行执行——进程级懒初始化线程池
 *
 * 单例（pthread_once），worker 数 = min(CPU 核数, PX_SCHED_MAX_WORKERS)。
 * 任务模型：调用方持有 cancel_flag；任务函数须协作式自查。
 * wait_idle 等待全部已提交任务完成（含被取消的）。
 * 单例随进程退出，不提供 destroy —— 悬挂 worker 由 Exchange 层
 * cancel+wait_idle 回收（见 exec_exchange.h）。
 */
#ifndef DB_EXECUTOR_PX_SCHEDULER_H
#define DB_EXECUTOR_PX_SCHEDULER_H

#ifdef __cplusplus
extern "C" {
#endif

#define PX_SCHED_MAX_WORKERS 16

typedef struct px_scheduler px_scheduler_t;
typedef void (*px_task_fn)(void *arg, volatile int *cancel_flag);

px_scheduler_t *px_scheduler_default(void);
int  px_scheduler_submit(px_scheduler_t *s, px_task_fn fn,
                         void *arg, volatile int *cancel_flag);
void px_scheduler_wait_idle(px_scheduler_t *s);
int  px_scheduler_num_workers(const px_scheduler_t *s);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_PX_SCHEDULER_H */
