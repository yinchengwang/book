/**
 * @file yang_wal_stub.h
 * @brief Yang/Netconf datastore WAL 接入占位（C0-2 T6）
 *
 * 状态：DEFERRED，依赖 C2-5 Tree XML 解析与 datastore 三态落地。
 * 当前仅暴露 wal_write_yang_ds() 调用点说明，不实现 datastore 写集成。
 *
 * 待 C2-5 完成（NETCONF hello + candidate/running/startup 三态）后启用：
 *   1. datastore.c 三态 commit 时调用 wal_write_yang_ds()
 *   2. 失败返回 DBERR_WAL_FAILED 并回滚 candidate
 *   3. 启动恢复入口 db_startup_recover() 重放 datastore 写
 */
#ifndef DB_YANG_WAL_STUB_H
#define DB_YANG_WAL_STUB_H

/* 接口已在 storage/wal/wal.h 暴露：
 *   uint64_t wal_write_yang_ds(wal_t *wal, uint32_t datastore_id,
 *                              const void *data, size_t data_len);
 *
 * 本变更不做 datastore 集成；C2-5 完成前不要在 datastore.c 调用。
 */

#endif /* DB_YANG_WAL_STUB_H */
