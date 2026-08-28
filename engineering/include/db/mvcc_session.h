/**
 * @file mvcc_session.h
 * @brief MVCC 线程局部会话状态（C2-1 T2/T5）
 *
 * 提供线程局部的"当前事务 xid"，供 heap_insert 戳 xmin 与 SeqScan
 * 可见性过滤查询 ReadView。
 *
 * 设计：__thread 变量，简单 setters/getters；commit/rollback 后 clear。
 * 完整的 MVCC tuple header（xmin/xmax）扩展需要 kv_record_t 布局变化，
 * 留待后续变更展开（当前变更仅落地 wiring 与 API 骨架）。
 */
#ifndef DB_MVCC_SESSION_H
#define DB_MVCC_SESSION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 获取当前线程的活跃 xid（0 表示无事务/快照读模式） */
int64_t mvcc_current_xid(void);

/** 设置当前线程的活跃 xid（txn_begin/commit/rollback 调用） */
void mvcc_set_current_xid(int64_t xid);

/** 清除当前线程的活跃 xid（提交/回滚后） */
void mvcc_clear_current_xid(void);

#ifdef __cplusplus
}
#endif

#endif /* DB_MVCC_SESSION_H */