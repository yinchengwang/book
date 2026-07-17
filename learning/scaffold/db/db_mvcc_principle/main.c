/**
 * db_mvcc_principle/main.c — MVCC 原理演示
 *
 * 演示 MVCC 的三大核心机制：
 * 1. 版本链（xmin/xmax/ctid）：每个元组保存多个历史版本
 * 2. 事务快照（Snapshot）：事务开始时捕获的数据库状态视图
 * 3. 可见性判断（Visibility）：根据快照决定返回哪个版本
 *
 * 测试场景：
 * - T1 插入版本 V1 (xmin=1)
 * - T2 更新 V1 产生 V2，V1 被标记 xmax=2
 * - T3 更新 V2 产生 V3，V2 被标记 xmax=3
 * - T4 查询：遍历版本链，返回对快照可见的版本
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ─────────────────────────────────────────────────────────────────
 * 多版本元组结构（模拟 PostgreSQL 的 HeapTupleHeader）
 *
 * PostgreSQL 元组头关键字段：
 * - t_xmin：创建该版本的事务 ID
 * - t_xmax：删除该版本的事务 ID（0 表示未删除）
 * - t_ctid：版本链指针，指向下一个版本（block, offset）
 * ───────────────────────────────────────────────────────────────── */

typedef struct tuple_ver {
    int xmin;          /* 创建事务 ID */
    int xmax;          /* 删除事务 ID（0=未删除） */
    int block_num;     /* ctid.block_num */
    int offset;        /* ctid.offset */
    char data[64];     /* 元组数据 payload */
    struct tuple_ver *chain;  /* 版本链指针（内存中用链表模拟） */
} tuple_ver_t;

/* ─────────────────────────────────────────────────────────────────
 * 事务快照（模拟 PostgreSQL 的 SnapshotData）
 *
 * 快照在事务开始时创建，包含：
 * - xmin：活跃事务的最小 ID
 * - xmax：已分配的最大事务 ID
 * - xip_list：快照创建时活跃的所有事务 ID
 * ───────────────────────────────────────────────────────────────── */

typedef struct {
    int xmin;          /* 快照中最小活动事务 ID */
    int xmax;          /* 快照中最大已分配事务 ID */
    int *xip_list;     /* 活跃事务 ID 数组 */
    int xip_count;     /* 活跃事务数量 */
    int self_txn_id;   /* 当前事务自身 ID */
} mvcc_snapshot_t;

/* ─────────────────────────────────────────────────────────────────
 * 辅助函数
 * ───────────────────────────────────────────────────────────────── */

/* 创建新版本 */
static tuple_ver_t* make_version(int xmin, const char *data) {
    tuple_ver_t *v = calloc(1, sizeof(tuple_ver_t));
    v->xmin = xmin;
    v->xmax = 0;
    v->chain = NULL;
    strncpy(v->data, data, 63);
    return v;
}

/* 将旧版本链接到新版本（模拟 UPDATE：旧版本的 xmax = 新事务 ID） */
static void link_versions(tuple_ver_t *old_ver, tuple_ver_t *new_ver, int txn_id) {
    old_ver->xmax = txn_id;      /* 旧版本被事务 txn_id 删除 */
    new_ver->chain = old_ver;     /* 版本链：新 -> 旧 */
}

/* 创建快照：记录当前活跃事务列表 */
static mvcc_snapshot_t* make_snapshot(int self_id, int xmin, int xmax,
                                       int *active, int count) {
    mvcc_snapshot_t *snap = calloc(1, sizeof(mvcc_snapshot_t));
    snap->self_txn_id = self_id;
    snap->xmin = xmin;
    snap->xmax = xmax;
    snap->xip_list = active;
    snap->xip_count = count;
    return snap;
}

/* 判断事务是否在快照中可见 */
static bool txn_visible(int txn_id, mvcc_snapshot_t *snap) {
    if (txn_id == snap->self_txn_id) return true;          /* 自身事务始终可见 */
    if (txn_id < snap->xmin) return true;                   /* 在 xmin 之前已提交 */
    for (int i = 0; i < snap->xip_count; i++)               /* 活跃列表中不可见 */
        if (snap->xip_list[i] == txn_id) return false;
    return true;                                            /* 已提交事务可见 */
}

/* ─────────────────────────────────────────────────────────────────
 * 核心可见性判断（对应工程 mvcc_visibility.c 的 mvcc_version_visible）
 *
 * PostgreSQL 规则：
 *   版本可见 <=> xmin 可见 && (xmax=0 || xmax 不可见)
 * ───────────────────────────────────────────────────────────────── */

static bool version_visible(tuple_ver_t *ver, mvcc_snapshot_t *snap) {
    /* 规则 1：xmin 事务必须已提交 */
    if (!txn_visible(ver->xmin, snap))
        return false;

    /* 规则 2：如果 xmax > 0，检查删除事务状态 */
    if (ver->xmax > 0) {
        /* 删除事务已提交（可见）-> 版本不可见 */
        if (txn_visible(ver->xmax, snap))
            return false;
        /* 删除事务未提交 -> 版本仍可见 */
        return true;
    }

    return true;  /* 未被删除，可见 */
}

/* 遍历版本链，返回对快照可见的最新版本 */
static tuple_ver_t* find_visible_ver(tuple_ver_t *head, mvcc_snapshot_t *snap) {
    tuple_ver_t *cur = head;
    while (cur != NULL) {
        if (version_visible(cur, snap))
            return cur;
        cur = cur->chain;
    }
    return NULL;  /* 所有版本都不可见，说明元组已被删除 */
}

/* ─────────────────────────────────────────────────────────────────
 * 打印函数
 * ───────────────────────────────────────────────────────────────── */

static void print_ver(tuple_ver_t *v, const char *prefix) {
    printf("    [%s] xmin=%d, xmax=%d, data=\"%s\"\n",
           prefix, v->xmin, v->xmax, v->data);
}

static void print_snapshot(mvcc_snapshot_t *snap, const char *name) {
    printf("  快照 [%s]: self=%d, xmin=%d, xmax=%d, 活跃=[",
           name, snap->self_txn_id, snap->xmin, snap->xmax);
    for (int i = 0; i < snap->xip_count; i++)
        printf("%d ", snap->xip_list[i]);
    printf("]\n");
}

/* ─────────────────────────────────────────────────────────────────
 * 演示主流程
 * ───────────────────────────────────────────────────────────────── */

int main(void) {
    printf("========== MVCC 原理演示 ==========\n\n");

    /* ── 阶段 1：构建版本链 ── */
    printf("【阶段 1】构建版本链\n");

    /* T1=1 插入初始版本 V1 */
    tuple_ver_t *V1 = make_version(1, "Alice: balance=1000");
    printf("  T1(id=1) 插入初始版本 V1:\n");
    print_ver(V1, "V1");

    /* T2=2 更新 V1，产生新版本 V2 */
    tuple_ver_t *V2 = make_version(2, "Alice: balance=1500");
    link_versions(V1, V2, 2);
    printf("\n  T2(id=2) 更新 V1，产生 V2:\n");
    print_ver(V2, "V2");
    print_ver(V1, "V1");

    /* T3=3 更新 V2，产生新版本 V3 */
    tuple_ver_t *V3 = make_version(3, "Alice: balance=2000");
    link_versions(V2, V3, 3);
    printf("\n  T3(id=3) 更新 V2，产生 V3:\n");
    print_ver(V3, "V3");
    printf("  版本链: V3 -> V2 -> V1\n\n");

    /* ── 阶段 2：可见性判断 ── */
    printf("【阶段 2】事务快照与可见性判断\n\n");

    /* 场景 A：T4(id=4) 开始，事务 5 正在运行 */
    int active_5[] = {5};
    mvcc_snapshot_t *snap4 = make_snapshot(4, 1, 10, active_5, 1);
    print_snapshot(snap4, "T4");

    tuple_ver_t *vis = find_visible_ver(V3, snap4);
    printf("  T4 看到的版本: %s\n", vis ? vis->data : "(无)");
    printf("  原因: V3(xmin=3) 可见, V2(xmin=2,xmax=3) 被 T3 删除, V1(xmax=2) 也被删除\n\n");

    /* 场景 B：T6(id=6) 开始，事务 3 还未提交 */
    int active_3[] = {3};
    mvcc_snapshot_t *snap6 = make_snapshot(6, 1, 10, active_3, 1);
    print_snapshot(snap6, "T6");

    vis = find_visible_ver(V3, snap6);
    printf("  T6 看到的版本: %s\n", vis ? vis->data : "(无)");
    printf("  原因: V3(xmin=3) 在活跃列表中不可见, V2(xmin=2,xmax=3) 删除事务已提交不可见, V1(xmin=1) 可见\n\n");

    /* ── 阶段 3：T7(id=7) 删除 V3 ── */
    printf("【阶段 3】T7 删除 V3\n");
    V3->xmax = 7;
    printf("  T7(id=7) 设置 V3.xmax=7\n\n");

    /* T8(id=8) 重新查询 */
    int active_empty[] = {};
    mvcc_snapshot_t *snap8 = make_snapshot(8, 1, 10, active_empty, 0);
    print_snapshot(snap8, "T8");

    vis = find_visible_ver(V3, snap8);
    printf("  T8 看到的版本: %s\n", vis ? vis->data : "(无)");
    printf("  原因: V3(xmin=3,xmax=7) 删除事务已提交 -> 不可见\n");
    printf("        V2(xmin=2,xmax=3) T3 删除已提交 -> 不可见\n");
    printf("        V1(xmin=1,xmax=2) T2 删除已提交 -> 不可见\n\n");

    /* ── 场景 4：T9(id=9) 开始，T7(id=7) 标记删除但尚未提交 ── */
    printf("【场景 4】T9 查询，但 T7 删除操作尚未提交\n");
    int active_7[] = {7};
    mvcc_snapshot_t *snap9 = make_snapshot(9, 1, 10, active_7, 1);
    print_snapshot(snap9, "T9");

    vis = find_visible_ver(V3, snap9);
    printf("  T9 看到的版本: %s\n", vis ? vis->data : "(无)");
    printf("  原因: V3(xmin=3,xmax=7) 删除事务 7 未提交 -> 仍可见\n\n");

    /* ── 清理 ── */
    printf("【阶段 4】清理资源\n");
    free(V1); free(V2); free(V3);
    free(snap4); free(snap6); free(snap8); free(snap9);

    printf("\n========== 演示结束 ==========\n");
    return 0;
}
