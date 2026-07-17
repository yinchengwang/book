/**
 * ReadView 机制演示程序
 *
 * 本程序演示 PostgreSQL/ MVCC 中 ReadView 快照机制的核心概念：
 * - ReadView 结构：xmin, xmax, xip_list
 * - 活跃事务列表的构建与管理
 * - 可见性判断算法（XidInMVCCSnapshot）
 *
 * 编译：gcc -std=c11 -Wall -o readview main.c
 * 运行：./readview
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/*============================================================================
 * 常量定义
 *============================================================================*/

/* 事务 ID 无效值（PostgreSQL 中为 InvalidTransactionId = 0） */
#define INVALID_XID 0
/* 最大活跃事务数 */
#define MAX_ACTIVE_XIDS 64

/*============================================================================
 * 数据结构定义
 *============================================================================*/

/**
 * ReadView 快照结构
 *
 * PostgreSQL 中每个事务在开始时会获取一个 ReadView，用于判断元组版本的可见性。
 *
 * 字段说明：
 * - xmin: 快照创建时已提交事务的最小 XID，所有小于 xmin 的事务已提交
 * - xmax: 快照创建时的最大 XID，所有大于等于 xmax 的事务未提交
 * - xip_list: 快照创建时正在执行的事务 ID 数组
 * - xip_count: 活跃事务的数量
 */
typedef struct ReadView {
    uint32_t xmin;              /* 最小活动事务 ID */
    uint32_t xmax;              /* 最大活动事务 ID */
    uint32_t xip_list[MAX_ACTIVE_XIDS];  /* 活动事务 ID 列表 */
    uint32_t xip_count;         /* 活动事务数量 */
} ReadView;

/**
 * 元组版本结构（简化的 MVCC 元组）
 *
 * 用于演示多版本并发控制中的元组版本链。
 */
typedef struct TupleVersion {
    uint32_t xmin;              /* 插入事务 ID */
    uint32_t xmax;              /* 删除事务 ID（0 表示未被删除） */
    char data[64];              /* 数据内容 */
    struct TupleVersion *next;  /* 指向更新后的版本（版本链） */
} TupleVersion;

/*============================================================================
 * ReadView 操作函数
 *============================================================================*/

/**
 * 创建并初始化一个 ReadView
 *
 * @param xmin  快照创建时的最小活动事务 ID
 * @param xmax  快照创建时的最大活动事务 ID
 * @param xip_list  活动事务 ID 数组
 * @param xip_count  活动事务数量
 * @return 分配的 ReadView 指针
 */
ReadView *readview_create(uint32_t xmin, uint32_t xmax,
                          uint32_t *xip_list, uint32_t xip_count) {
    ReadView *rv = (ReadView *)malloc(sizeof(ReadView));
    if (!rv) {
        fprintf(stderr, "内存分配失败\n");
        return NULL;
    }

    rv->xmin = xmin;
    rv->xmax = xmax;
    rv->xip_count = xip_count;

    /* 复制活动事务列表 */
    for (uint32_t i = 0; i < xip_count && i < MAX_ACTIVE_XIDS; i++) {
        rv->xip_list[i] = xip_list[i];
    }

    return rv;
}

/**
 * 释放 ReadView 内存
 */
void readview_free(ReadView *rv) {
    free(rv);
}

/*============================================================================
 * 核心可见性判断算法
 *============================================================================*/

/**
 * XidInMVCCSnapshot - 判断事务 ID 是否在快照中"可见"
 *
 * PostgreSQL 可见性判断规则（简化版）：
 *
 * 规则 1: xid < xmin  → 事务已提交，该版本对快照可见
 * 规则 2: xid >= xmax  → 事务在快照创建时尚未开始/未提交，该版本不可见
 * 规则 3: xid 在 xip_list 中 → 事务在快照创建时正在执行，该版本不可见
 *
 * @param xid   要检查的事务 ID
 * @param rv    ReadView 快照
 * @return true 表示该事务对快照可见，false 表示不可见
 */
bool xid_in_mvcc_snapshot(uint32_t xid, const ReadView *rv) {
    /* 规则 1: xid < xmin 表示事务在快照创建前已提交，可见 */
    if (xid < rv->xmin) {
        return true;
    }

    /* 规则 2: xid >= xmax 表示事务在快照创建时尚未开始，不可见 */
    if (xid >= rv->xmax) {
        return false;
    }

    /* 规则 3: 检查是否在活动事务列表中 */
    for (uint32_t i = 0; i < rv->xip_count; i++) {
        if (rv->xip_list[i] == xid) {
            return false;  /* 活动事务的修改不可见 */
        }
    }

    /* 经过以上检查，xid 在 [xmin, xmax) 范围内且不在活动列表中
     * 这意味着事务在快照创建时已提交 */
    return true;
}

/**
 * 判断元组版本对给定 ReadView 是否可见
 *
 * @param tuple  元组版本
 * @param rv     ReadView 快照
 * @return true 表示可见，false 表示不可见
 */
bool tuple_visible(const TupleVersion *tuple, const ReadView *rv) {
    /* xmin 事务必须对快照可见 */
    if (!xid_in_mvcc_snapshot(tuple->xmin, rv)) {
        return false;
    }

    /* 如果元组已被删除，检查 xmax 是否对快照可见 */
    if (tuple->xmax != INVALID_XID) {
        /* xmax 对快照可见表示删除已提交，该版本不可见 */
        if (xid_in_mvcc_snapshot(tuple->xmax, rv)) {
            return false;
        }
    }

    return true;
}

/*============================================================================
 * 辅助函数
 *============================================================================*/

/**
 * 打印 ReadView 内容
 */
void readview_print(const char *label, const ReadView *rv) {
    printf("\n%s ReadView:\n", label);
    printf("  xmin = %u\n", rv->xmin);
    printf("  xmax = %u\n", rv->xmax);
    printf("  活动事务列表 (%u 个):\n", rv->xip_count);
    for (uint32_t i = 0; i < rv->xip_count; i++) {
        printf("    xip_list[%u] = %u\n", i, rv->xip_list[i]);
    }
}

/**
 * 打印元组版本信息
 */
void tuple_print(const char *label, const TupleVersion *tuple) {
    printf("%s: xmin=%u, xmax=%u, data=\"%s\"\n",
           label, tuple->xmin, tuple->xmax, tuple->data);
}

/*============================================================================
 * 测试场景
 *============================================================================*/

/**
 * 测试场景 1: 基础可见性判断
 *
 * 模拟不同事务 ID 对快照的可见性。
 */
void test_basic_visibility(void) {
    printf("\n");
    printf("========================================\n");
    printf("测试场景 1: 基础可见性判断\n");
    printf("========================================\n");

    /* 创建快照: xmin=100, xmax=200, 活动事务={150, 160, 170} */
    uint32_t active_xids[] = {150, 160, 170};
    ReadView *rv = readview_create(100, 200, active_xids, 3);
    readview_print("初始", rv);

    /* 测试各种事务 ID 的可见性 */
    struct {
        uint32_t xid;
        const char *expected;
        const char *reason;
    } tests[] = {
        {50,   "可见", "xid < xmin (50 < 100)，事务已提交"},
        {99,   "可见", "xid < xmin (99 < 100)，事务已提交"},
        {100,  "不可见", "xid >= xmin (100 >= 100) 且不在活动列表中，事务未提交"},
        {149,  "可见", "xid 在 [xmin, xmax) 范围内且不在活动列表中，已提交"},
        {150,  "不可见", "xid 在活动事务列表中，正在执行"},
        {160,  "不可见", "xid 在活动事务列表中，正在执行"},
        {170,  "不可见", "xid 在活动事务列表中，正在执行"},
        {175,  "可见", "xid 在 [xmin, xmax) 范围内且不在活动列表中，已提交"},
        {200,  "不可见", "xid >= xmax (200 >= 200)，事务未开始"},
        {300,  "不可见", "xid >= xmax (300 >= 200)，事务未开始"},
    };

    printf("\n事务 ID 可见性测试:\n");
    printf("%-8s %-10s %s\n", "XID", "结果", "原因");
    printf("%-8s %-10s %s\n", "---", "----", "----");

    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        bool visible = xid_in_mvcc_snapshot(tests[i].xid, rv);
        bool correct = (visible && strcmp(tests[i].expected, "可见") == 0) ||
                       (!visible && strcmp(tests[i].expected, "不可见") == 0);
        const char *result = visible ? "可见" : "不可见";

        printf("%-8u %-10s %s %s\n",
               tests[i].xid, result, tests[i].reason,
               correct ? "[OK]" : "[FAIL]");
    }

    readview_free(rv);
}

/**
 * 测试场景 2: 元组版本可见性
 *
 * 模拟数据库中元组的多个版本，判断特定快照下的可见版本。
 */
void test_tuple_visibility(void) {
    printf("\n");
    printf("========================================\n");
    printf("测试场景 2: 元组版本可见性\n");
    printf("========================================\n");

    /* 假设有以下事务序列：
     * T1 (xid=10): INSERT tuple = "Hello"
     * T2 (xid=20): UPDATE tuple = "World"
     * T3 (xid=30): DELETE tuple
     * T4 (xid=40): INSERT tuple = "New"
     *
     * 版本链（从新到旧）:
     * [T4] -> [T3] -> [T2] -> [T1]
     */

    TupleVersion v1 = {10, INVALID_XID, "Hello", NULL};       /* 最早版本 */
    TupleVersion v2 = {20, 30, "World", &v1};                  /* 被 T3 删除 */
    TupleVersion v3 = {30, 40, "Deleted", &v2};                /* 被 T4 更新 */
    TupleVersion v4 = {40, INVALID_XID, "New", &v3};          /* 最新版本 */

    printf("\n版本链:\n");
    tuple_print("v4(最新)", &v4);
    tuple_print("v3", &v3);
    tuple_print("v2", &v2);
    tuple_print("v1(最早)", &v1);

    /* 场景 2a: 事务 T5 (快照 xmin=15, xmax=25, 活动={}) 执行 SELECT */
    /* 期望: 只能看到 T1 插入的 "Hello"，T2 的修改不可见 */
    {
        printf("\n--- 场景 2a: T5 快照 (xmin=15, xmax=25) ---\n");
        uint32_t empty_list[] = {};
        ReadView *rv = readview_create(15, 25, empty_list, 0);
        readview_print("T5", rv);

        printf("\n版本可见性判断:\n");
        printf("v1(Hello): %s (T1 已提交，T2 未提交)\n",
               tuple_visible(&v1, rv) ? "可见" : "不可见");
        printf("v2(World): %s (T2 对快照不可见)\n",
               tuple_visible(&v2, rv) ? "可见" : "不可见");

        readview_free(rv);
    }

    /* 场景 2b: 事务 T6 (快照 xmin=25, xmax=35, 活动={}) 执行 SELECT */
    /* 期望: 看到 T2 修改的 "World"，但看不到 T3 的删除（未提交） */
    {
        printf("\n--- 场景 2b: T6 快照 (xmin=25, xmax=35) ---\n");
        uint32_t empty_list[] = {};
        ReadView *rv = readview_create(25, 35, empty_list, 0);
        readview_print("T6", rv);

        printf("\n版本可见性判断:\n");
        printf("v1(Hello): %s (对快照不可见)\n",
               tuple_visible(&v1, rv) ? "可见" : "不可见");
        printf("v2(World): %s (T2 已提交，T3 未提交)\n",
               tuple_visible(&v2, rv) ? "可见" : "不可见");
        printf("v3(Deleted): %s (xmax 对快照不可见)\n",
               tuple_visible(&v3, rv) ? "可见" : "不可见");

        readview_free(rv);
    }

    /* 场景 2c: 事务 T7 (快照 xmin=35, xmax=50, 活动={45}) 执行 SELECT */
    /* 期望: 看到 "New"，因为 T3 和 T4 都已提交 */
    {
        printf("\n--- 场景 2c: T7 快照 (xmin=35, xmax=50, 活动={45}) ---\n");
        uint32_t active_xids[] = {45};
        ReadView *rv = readview_create(35, 50, active_xids, 1);
        readview_print("T7", rv);

        printf("\n版本可见性判断:\n");
        printf("v1(Hello): %s\n",
               tuple_visible(&v1, rv) ? "可见" : "不可见");
        printf("v2(World): %s (被 T3 删除)\n",
               tuple_visible(&v2, rv) ? "可见" : "不可见");
        printf("v3(Deleted): %s\n",
               tuple_visible(&v3, rv) ? "可见" : "不可见");
        printf("v4(New): %s (最新可见版本)\n",
               tuple_visible(&v4, rv) ? "可见" : "不可见");

        readview_free(rv);
    }
}

/*============================================================================
 * 主函数
 *============================================================================*/

int main(void) {
    printf("========================================\n");
    printf("    ReadView 机制演示\n");
    printf("    MVCC 快照与可见性判断\n");
    printf("========================================\n");

    test_basic_visibility();
    test_tuple_visibility();

    printf("\n========================================\n");
    printf("演示完成\n");
    printf("========================================\n");

    return 0;
}
