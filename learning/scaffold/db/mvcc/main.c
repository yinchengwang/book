/**
 * mvcc.c — MVCC 多版本并发控制演示
 *
 * 本文件演示 MVCC 的核心机制：
 * - 版本链管理（t_xmin, t_xmax, t_chain）
 * - 快照隔离（Snapshot Isolation）
 * - 事务可见性判断
 * - GC 清理旧版本
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* 事务状态枚举 */
typedef enum {
    TXN_RUNNING = 0,
    TXN_COMMITTED = 1,
    TXN_ABORTED = 2
} txn_state_t;

/* MVCC 版本结构 */
typedef struct mvcc_version {
    int version_id;          /* 版本号 */
    int t_xmin;              /* 创建版本的事务ID */
    int t_xmax;              /* 删除版本的事务ID（0表示未删除） */
    char data[64];           /* 版本数据 */
    struct mvcc_version *next;  /* 版本链指针 */

    /* 元组头字段（模拟 PostgreSQL 的 t_xmin/t_xmax/t_chain） */
    int t_min;               /* 同 t_xmin：创建事务ID */
    int t_max;               /* 同 t_xmax：删除事务ID */
    struct mvcc_version *chain; /* 版本链指针 */
} mvcc_version_t;

/* 事务快照 */
typedef struct snapshot {
    int self_txn_id;         /* 当前事务ID */
    int xmin;                /* 快照中活跃事务的最小ID */
    int xmax;                /* 快照中已分配的最大事务ID */
    int *active_txns;        /* 活跃事务列表 */
    int active_count;        /* 活跃事务数量 */
} snapshot_t;

/* 全局状态 */
static int g_version_counter = 0;
static mvcc_version_t *g_version_list = NULL;

/* 日志宏 */
#define LOG(tag, fmt, ...) printf("[" tag "] " fmt "\n", ##__VA_ARGS__)

/* ============================================================================
 * 工具函数
 * ============================================================================ */

/* 创建新版本 */
static mvcc_version_t* version_create(int txn_id, const char *data) {
    mvcc_version_t *v = calloc(1, sizeof(mvcc_version_t));
    v->version_id = ++g_version_counter;
    v->t_xmin = txn_id;
    v->t_xmax = 0;
    v->t_min = txn_id;
    v->t_max = 0;
    strncpy(v->data, data, sizeof(v->data) - 1);
    v->next = NULL;
    v->chain = NULL;

    /* 加入全局版本列表 */
    v->next = g_version_list;
    g_version_list = v;

    return v;
}

/* 标记版本为已删除 */
static void version_delete(mvcc_version_t *v, int txn_id) {
    v->t_xmax = txn_id;
    v->t_max = txn_id;
}

/* ============================================================================
 * 快照隔离
 * ============================================================================ */

/* 创建快照 */
static snapshot_t* snapshot_create(int txn_id, int xmin, int *active_txns, int count) {
    snapshot_t *snap = calloc(1, sizeof(snapshot_t));
    snap->self_txn_id = txn_id;
    snap->xmin = xmin;
    snap->xmax = txn_id;
    snap->active_txns = active_txns;
    snap->active_count = count;
    return snap;
}

/* 判断事务是否在快照中可见 */
static bool txn_visible_in_snapshot(int txn_id, snapshot_t *snap) {
    if (txn_id == snap->self_txn_id) {
        return true;  /* 自己的事务总是可见 */
    }

    if (txn_id < snap->xmin) {
        return true;  /* 已提交事务可见 */
    }

    /* 检查是否在活跃事务列表中 */
    for (int i = 0; i < snap->active_count; i++) {
        if (snap->active_txns[i] == txn_id) {
            return false;  /* 活跃事务不可见 */
        }
    }

    return true;  /* 已提交事务可见 */
}

/* ============================================================================
 * 版本可见性判断
 * ============================================================================ */

/* 判断某个版本对给定快照是否可见 */
static bool version_visible(mvcc_version_t *v, snapshot_t *snap) {
    /* t_xmin 事务必须在快照中可见 */
    if (!txn_visible_in_snapshot(v->t_xmin, snap)) {
        return false;
    }

    /* 如果版本被删除，t_xmax 事务必须在快照中不可见 */
    if (v->t_xmax > 0) {
        if (!txn_visible_in_snapshot(v->t_xmax, snap)) {
            return false;  /* 删除事务未提交，版本仍可见 */
        }
        /* 删除事务已提交，版本不可见 */
        return false;
    }

    return true;
}

/* 获取某个版本链中对于快照可见的最新版本 */
static mvcc_version_t* get_visible_version(mvcc_version_t *head, snapshot_t *snap) {
    mvcc_version_t *current = head;
    mvcc_version_t *visible = NULL;

    /* 沿版本链遍历（从新到旧） */
    while (current != NULL) {
        if (version_visible(current, snap)) {
            visible = current;  /* 更新为最新可见版本 */
            break;  /* 找到第一个可见版本即为最新 */
        }
        current = current->chain;
    }

    return visible;
}

/* ============================================================================
 * GC 清理
 * ============================================================================ */

/* 统计可以清理的版本数量 */
static int count_garbage_versions(int current_txn_id) {
    int garbage_count = 0;
    mvcc_version_t *current = g_version_list;

    while (current != NULL) {
        /* 可清理条件：版本已删除且两端事务都已结束 */
        if (current->t_xmax > 0 &&
            current->t_xmax < current_txn_id - 10 &&
            current->t_xmin < current_txn_id - 10) {
            garbage_count++;
        }
        current = current->next;
    }

    return garbage_count;
}

/* 执行 GC 清理 */
static void gc_cleanup(int current_txn_id) {
    mvcc_version_t **indirect = &g_version_list;
    mvcc_version_t *current = g_version_list;
    int cleaned = 0;

    LOG("mvcc", "=== 开始 GC 清理 ===");

    while (*indirect != NULL) {
        current = *indirect;

        /* 可清理条件：版本已删除且两端事务都已结束（超过 5 个事务周期） */
        if (current->t_xmax > 0 &&
            current->t_xmax < current_txn_id - 5 &&
            current->t_xmin < current_txn_id - 5) {

            LOG("mvcc", "清理版本 #%d (t_xmin=%d, t_xmax=%d)",
                current->version_id, current->t_xmin, current->t_xmax);

            *indirect = current->next;
            free(current);
            cleaned++;
        } else {
            indirect = &(*indirect)->next;
        }
    }

    LOG("mvcc", "GC 清理完成，共清理 %d 个版本", cleaned);
}

/* ============================================================================
 * 演示主函数
 * ============================================================================ */

int main(void) {
    LOG("mvcc", "========== MVCC 多版本并发控制演示 ==========\n");

    /* 1. 模拟版本链创建 */
    LOG("mvcc", "=== 1. 创建版本链 ===");

    mvcc_version_t *v1 = version_create(101, "初始版本 - Alice 的账户余额");
    LOG("mvcc", "创建版本 #%d: %s", v1->version_id, v1->data);

    mvcc_version_t *v2 = version_create(102, "更新版本 - 存款 +1000");
    v2->chain = v1;  /* v2 指向旧版本 v1 */
    LOG("mvcc", "创建版本 #%d 并链接到 #%d", v2->version_id, v1->version_id);

    mvcc_version_t *v3 = version_create(103, "再次更新 - 消费 -200");
    v3->chain = v2;
    LOG("mvcc", "创建版本 #%d 并链接到 #%d", v3->version_id, v2->version_id);

    LOG("mvcc", "版本链: v3 -> v2 -> v1");

    /* 2. 模拟快照隔离 */
    LOG("mvcc", "\n=== 2. 快照隔离演示 ===");

    /* 事务 200 看到什么？事务 101/102/103 已提交，只有当前事务 104 在运行 */
    int active_txns[] = {104};  /* 模拟活跃事务（只有当前事务 104） */
    snapshot_t *snap200 = snapshot_create(200, 100, active_txns, 1);

    mvcc_version_t *visible = get_visible_version(v3, snap200);
    if (visible) {
        LOG("mvcc", "事务 200 看到的版本: #%d - %s", visible->version_id, visible->data);
    }

    /* 3. 版本可见性判断 */
    LOG("mvcc", "\n=== 3. 事务可见性判断 ===");

    LOG("mvcc", "版本 #%d: t_xmin=%d, t_xmax=%d -> %s",
        v1->version_id, v1->t_xmin, v1->t_xmax,
        version_visible(v1, snap200) ? "可见" : "不可见");

    LOG("mvcc", "版本 #%d: t_xmin=%d, t_xmax=%d -> %s",
        v2->version_id, v2->t_xmin, v2->t_xmax,
        version_visible(v2, snap200) ? "可见" : "不可见");

    LOG("mvcc", "版本 #%d: t_xmin=%d, t_xmax=%d -> %s",
        v3->version_id, v3->t_xmin, v3->t_xmax,
        version_visible(v3, snap200) ? "可见" : "不可见");

    /* 4. 删除版本演示 */
    LOG("mvcc", "\n=== 4. 删除版本演示 ===");

    version_delete(v3, 105);
    LOG("mvcc", "事务 105 删除版本 #%d, t_xmax=%d", v3->version_id, v3->t_xmax);

    /* 5. GC 清理演示 */
    LOG("mvcc", "\n=== 5. GC 清理演示 ===");

    int garbage = count_garbage_versions(200);
    LOG("mvcc", "当前可清理的旧版本数: %d", garbage);

    gc_cleanup(200);

    /* 6. 清理资源 */
    LOG("mvcc", "\n=== 6. 清理资源 ===");

    while (g_version_list != NULL) {
        mvcc_version_t *tmp = g_version_list;
        g_version_list = g_version_list->next;
        free(tmp);
    }
    free(snap200);

    LOG("mvcc", "资源清理完成");
    LOG("mvcc", "\n========== MVCC 演示结束 ==========");

    return 0;
}
