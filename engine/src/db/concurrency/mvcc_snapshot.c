/**
 * @file mvcc_snapshot.c
 * @brief MVCC 快照管理实现
 *
 * ## MVCC 快照原理
 *
 * MVCC (Multi-Version Concurrency Control) 通过为每个事务创建快照，
 * 实现读写不阻塞的并发控制。
 *
 * ## 快照数据结构
 *
 * ```c
 * mvcc_snapshot_t {
 *     xmin:      // 快照创建时的最小活跃事务ID
 *     xmax:      // 快照创建时的最大事务ID（之后的都视为未来）
 *     xip_list:  // 活跃事务ID列表（正在运行的事务）
 *     xip_count: // 活跃事务数量
 * }
 * ```
 *
 * ## 可见性判断规则
 *
 * | 版本状态 | 可见性 |
 * |---------|--------|
 * | xmin 之前提交 | ✓ 可见 |
 * | xmin~xmax 之间，但不在 xip_list | ✓ 可见 |
 * | 在 xip_list 中（自己除外） | ✗ 不可见 |
 * | xmax 之后开始 | ✗ 不可见（未来事务） |
 * | 当前事务自己创建的 | ✓ 可见 |
 *
 * ## 快照隔离级别
 *
 * SI (Snapshot Isolation) 保证：
 * - 事务只能看到快照创建时的数据
 * - 不会看到其他事务的中间状态
 * - 避免脏读、不可重复读
 *
 * ## 与 PostgreSQL 的差异
 *
 * PostgreSQL 使用 SSI (Serializable Snapshot Isolation)，
 * 通过检测写入冲突来防止序列化异常。
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <db/concurrency/mvcc.h>

/* ─────────────────────────────────────────────────────────────────
 * 快照操作
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 创建 MVCC 快照
 *
 * 快照记录了某个时间点的数据库状态，用于可见性判断
 *
 * @param xmin         最小活跃事务ID（此值之前的事务都已提交）
 * @param xmax         最大事务ID（此值之后的事务都未开始）
 * @param active_txns  活跃事务ID数组
 * @param active_count 活跃事务数量
 * @return 新创建的快照，NULL 表示失败
 *
 * ## xip_list 的作用
 *
 * 快照需要记录在 xmin~xmax 区间内正在运行的事务。
 * 这些事务的修改对当前快照不可见。
 *
 * ## 内存管理
 *
 * 快照使用独立内存：
 * - 快照结构体
 * - xip_list 数组（如果活跃事务 > 0）
 */
mvcc_snapshot_t *mvcc_snapshot_create(mvcc_txn_id_t xmin,
                                       mvcc_txn_id_t xmax,
                                       const mvcc_txn_id_t *active_txns,
                                       int active_count)
{
    mvcc_snapshot_t *snapshot = (mvcc_snapshot_t *)malloc(
        sizeof(mvcc_snapshot_t));
    if (snapshot == NULL) {
        return NULL;
    }

    snapshot->xmin = xmin;
    snapshot->xmax = xmax;
    snapshot->xip_count = active_count;

    /* xip_list 容量：至少 4，最小为 active_count */
    snapshot->xip_capacity = (active_count > 0) ? active_count : 4;
    snapshot->xip_capacity = (snapshot->xip_capacity < 4) ? 4 : snapshot->xip_capacity;

    /* 分配并复制活跃事务列表 */
    if (active_count > 0) {
        snapshot->xip_list = (mvcc_txn_id_t *)malloc(
            sizeof(mvcc_txn_id_t) * snapshot->xip_capacity);
        if (snapshot->xip_list == NULL) {
            free(snapshot);
            return NULL;
        }
        memcpy(snapshot->xip_list, active_txns,
               sizeof(mvcc_txn_id_t) * active_count);
    } else {
        snapshot->xip_list = NULL;
    }

    return snapshot;
}

/**
 * @brief 销毁快照
 *
 * 释放快照及其关联的内存
 */
void mvcc_snapshot_destroy(mvcc_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return;
    }
    if (snapshot->xip_list != NULL) {
        free(snapshot->xip_list);
        snapshot->xip_list = NULL;
    }
    free(snapshot);
}

/**
 * @brief 复制快照
 *
 * 创建快照的深拷贝
 */
mvcc_snapshot_t *mvcc_snapshot_copy(const mvcc_snapshot_t *src)
{
    if (src == NULL) {
        return NULL;
    }
    return mvcc_snapshot_create(src->xmin, src->xmax,
                                 src->xip_list, src->xip_count);
}

/**
 * @brief 检查事务是否在快照的活跃列表中
 *
 * @param snapshot 快照
 * @param txn_id   待检查的事务ID
 * @return true 如果在活跃列表中，false 否则
 *
 * ## 实现说明
 *
 * 使用线性搜索，因为：
 * - xip_list 通常很小（并发事务数有限）
 * - 避免额外的数据结构开销
 *
 * ## 为什么用线性搜索？
 *
 * | 方案 | 查找 | 插入 | 空间 |
 * |------|------|------|------|
 * | 数组 | O(n) | O(1) | O(n) |
 * | Hash | O(1) | O(1) | O(n) |
 * | 跳表 | O(log n) | O(log n) | O(n) |
 *
 * 考虑到并发事务数通常 < 100，线性搜索最简单高效。
 */
bool mvcc_snapshot_contains_txn(const mvcc_snapshot_t *snapshot,
                                 mvcc_txn_id_t txn_id)
{
    if (snapshot == NULL) {
        return false;
    }

    /* 线性搜索活跃事务列表 */
    for (int i = 0; i < snapshot->xip_count; i++) {
        if (snapshot->xip_list[i] == txn_id) {
            return true;
        }
    }
    return false;
}

/**
 * @brief 计算快照序列化后的大小
 *
 * @return 序列化所需的字节数，0 表示无效快照
 */
size_t mvcc_snapshot_serialize_size(const mvcc_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return 0;
    }

    /* 序列化格式：
     * xmin (8 bytes) + xmax (8 bytes) + count (4 bytes) +
     * xip_list (8 bytes * count)
     */
    return sizeof(mvcc_txn_id_t) * 2 + sizeof(int) +
           sizeof(mvcc_txn_id_t) * snapshot->xip_count;
}

/**
 * @brief 序列化快照到缓冲区
 *
 * 用于持久化快照（如检查点）
 *
 * @param snapshot 快照
 * @param buf      目标缓冲区
 * @param buf_size 缓冲区大小
 * @return 写入的字节数，0 表示失败
 */
size_t mvcc_snapshot_serialize(const mvcc_snapshot_t *snapshot,
                                void *buf,
                                size_t buf_size)
{
    if (snapshot == NULL || buf == NULL) {
        return 0;
    }

    size_t needed = mvcc_snapshot_serialize_size(snapshot);
    if (buf_size < needed) {
        return 0;
    }

    char *pos = (char *)buf;

    /* 写入 xmin */
    memcpy(pos, &snapshot->xmin, sizeof(mvcc_txn_id_t));
    pos += sizeof(mvcc_txn_id_t);

    /* 写入 xmax */
    memcpy(pos, &snapshot->xmax, sizeof(mvcc_txn_id_t));
    pos += sizeof(mvcc_txn_id_t);

    /* 写入活跃事务数量 */
    memcpy(pos, &snapshot->xip_count, sizeof(int));
    pos += sizeof(int);

    /* 写入活跃事务列表 */
    if (snapshot->xip_count > 0 && snapshot->xip_list != NULL) {
        memcpy(pos, snapshot->xip_list,
               sizeof(mvcc_txn_id_t) * snapshot->xip_count);
    }

    return needed;
}

/**
 * @brief 从缓冲区反序列化快照
 *
 * 用于从检查点恢复快照
 *
 * @param buf      源缓冲区
 * @param buf_size 缓冲区大小
 * @return 恢复的快照，NULL 表示失败
 */
mvcc_snapshot_t *mvcc_snapshot_deserialize(const void *buf,
                                            size_t buf_size)
{
    if (buf == NULL || buf_size < sizeof(mvcc_txn_id_t) * 2 + sizeof(int)) {
        return NULL;
    }

    const char *pos = (const char *)buf;

    /* 读取 xmin */
    mvcc_txn_id_t xmin;
    memcpy(&xmin, pos, sizeof(mvcc_txn_id_t));
    pos += sizeof(mvcc_txn_id_t);

    /* 读取 xmax */
    mvcc_txn_id_t xmax;
    memcpy(&xmax, pos, sizeof(mvcc_txn_id_t));
    pos += sizeof(mvcc_txn_id_t);

    /* 读取活跃事务数量 */
    int count;
    memcpy(&count, pos, sizeof(int));
    pos += sizeof(int);

    /* 验证缓冲区大小 */
    size_t needed = sizeof(mvcc_txn_id_t) * 2 + sizeof(int) +
                    sizeof(mvcc_txn_id_t) * count;
    if (buf_size < needed) {
        return NULL;
    }

    /* 读取活跃事务列表 */
    mvcc_txn_id_t *xip_list = NULL;
    if (count > 0) {
        xip_list = (mvcc_txn_id_t *)malloc(sizeof(mvcc_txn_id_t) * count);
        if (xip_list == NULL) {
            return NULL;
        }
        memcpy(xip_list, pos, sizeof(mvcc_txn_id_t) * count);
    }

    /* 创建快照对象 */
    mvcc_snapshot_t *snapshot = mvcc_snapshot_create(xmin, xmax,
                                                      xip_list, count);
    if (snapshot == NULL && xip_list != NULL) {
        free(xip_list);
    }

    return snapshot;
}