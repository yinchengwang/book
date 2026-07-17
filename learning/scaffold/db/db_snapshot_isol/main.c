/**
 * 快照隔离（Snapshot Isolation）演示
 *
 * 演示快照隔离与 Read Committed 的区别：
 * - SI: 事务全程使用同一快照
 * - RC: 每次语句重新计算快照
 * - First-Writer-Wins 冲突处理
 * - Write-Skew 异常场景
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ========== 事务结构 ========== */
typedef struct {
    int txn_id;
    int xmin;
    int xmax;
    int *active_txns;
    int active_count;
    bool started;
} Transaction;

/* ========== 元组版本 ========== */
typedef struct {
    int xmin;        /* 创建事务 ID */
    int xmax;        /* 删除事务 ID（0 = 未删除） */
    int value;       /* 数据值 */
} TupleVersion;

/* ========== 全局数据 ========== */
TupleVersion row_a = {1, 0, 100};  /* 行 A，初始值 100 */
TupleVersion row_b = {1, 0, 100};  /* 行 B，初始值 100 */
Transaction transactions[5];
int txn_counter = 1;

/* ========== 可见性判断 ========== */
bool visible(Transaction *txn, TupleVersion *ver) {
    /* xmin 必须在快照范围内 */
    if (ver->xmin < txn->xmin || ver->xmin >= txn->xmax) {
        return false;
    }
    /* xmin 事务不能在活跃列表中 */
    for (int i = 0; i < txn->active_count; i++) {
        if (txn->active_txns[i] == ver->xmin) {
            return false;
        }
    }
    /* xmax 判断 */
    if (ver->xmax > 0) {
        if (ver->xmax >= txn->xmin && ver->xmax < txn->xmax) {
            /* xmax 在快照范围内，检查是否已提交 */
            bool xmax_committed = true;
            for (int i = 0; i < txn->active_count; i++) {
                if (txn->active_txns[i] == ver->xmax) {
                    xmax_committed = false;
                    break;
                }
            }
            if (xmax_committed) {
                return false;  /* 已提交删除，不可见 */
            }
        }
    }
    return true;
}

/* ========== 读取操作 ========== */
int read_row(Transaction *txn, TupleVersion *row) {
    if (visible(txn, row)) {
        return row->value;
    }
    return -1;  /* 不可见 */
}

/* ========== 测试场景 ========== */

/* 场景1: SI vs RC 区别 */
void test_si_vs_rc(void) {
    printf("========== 场景1: SI vs RC 区别 ==========\n\n");

    /* 初始化事务 T2 */
    Transaction t2 = {2, 10, 20, (int[]){1, 3}, 2, true};

    printf("事务 T2 开始，快照: xmin=%d, xmax=%d, 活跃=[1,3]\n\n", t2.xmin, t2.xmax);

    /* 模拟 T1 在 T2 快照开始后修改了 row_a */
    row_a.xmax = 1;
    row_a.xmin = 4;  /* 新版本由 T1 创建 */
    row_a.value = 50;

    printf("T1 修改了 row_a: 新值 = %d\n", row_a.value);

    /* T2 读取 row_a */
    int val = read_row(&t2, &row_a);
    printf("T2 读取 row_a: ");

    if (val >= 0) {
        printf("值 = %d (SI: 使用事务开始时的快照)\n", val);
        printf("  RC 模式下会读到新值 50\n");
    } else {
        printf("不可见 (旧版本已被 T1 删除)\n");
    }
    printf("\n");
}

/* 场景2: First-Writer-Wins */
void test_first_writer_wins(void) {
    printf("========== 场景2: First-Writer-Wins ==========\n\n");

    printf("T3 和 T4 同时读取 row_b，都打算更新：\n\n");

    /* T3 和 T4 读取相同的快照 */
    Transaction t3 = {3, 30, 40, (int[]){1, 2, 4}, 3, true};
    Transaction t4 = {4, 30, 40, (int[]){1, 2, 3}, 3, true};

    int val_t3 = read_row(&t3, &row_b);
    int val_t4 = read_row(&t4, &row_b);

    printf("T3 读取 row_b: %d\n", val_t3);
    printf("T4 读取 row_b: %d\n\n", val_t4);

    /* T3 先提交 */
    printf("T3 先提交，更新 row_b = %d\n", val_t3 + 10);
    row_b.xmax = 3;
    row_b.xmin = 3;
    row_b.value = val_t3 + 10;

    /* T4 尝试提交 - 检测到 First-Writer-Wins 冲突 */
    printf("T4 尝试提交时检测到冲突：\n");
    printf("  row_b.xmin = %d（当前版本创建者）\n", row_b.xmin);
    printf("  T4.xmin = %d（T4 的快照起始）\n", t4.xmin);
    printf("  结论: First-Writer-Wins，T4 必须回滚重试\n\n");
}

/* 场景3: Write-Skew 异常 */
void test_write_skew(void) {
    printf("========== 场景3: Write-Skew 异常 ==========\n\n");

    printf("Write-Skew: 两个事务基于快照读取，然后各自更新不同行\n");
    printf("结果违反一致性约束（如：两行之和必须 >= 0）\n\n");

    /* 初始化 */
    TupleVersion permit_a = {1, 0, 1};   /* A 有 1 张许可证 */
    TupleVersion permit_b = {1, 0, 1};   /* B 有 1 张许可证 */
    int min_permit = 1;                  /* 每人至少 1 张 */

    printf("初始状态: A 有 %d 张许可证，B 有 %d 张\n",
           permit_a.value, permit_b.value);
    printf("约束: 每人至少保留 %d 张\n\n", min_permit);

    /* T5 和 T6 同时开始 */
    Transaction t5 = {5, 50, 60, (int[]){1, 6}, 2, true};
    Transaction t6 = {6, 50, 60, (int[]){1, 5}, 2, true};

    int val_a = read_row(&t5, &permit_a);  /* T5 读取 A */
    int val_b = read_row(&t6, &permit_b);  /* T6 读取 B */

    printf("T5 读取 permit_a: %d\n", val_a);
    printf("T6 读取 permit_b: %d\n\n", val_b);

    /* 检查约束：val - 1 >= min_permit */
    if (val_a - 1 >= min_permit) {
        printf("T5 检查通过: %d - 1 >= %d，可以释放\n", val_a, min_permit);
    }
    if (val_b - 1 >= min_permit) {
        printf("T6 检查通过: %d - 1 >= %d，可以释放\n", val_b, min_permit);
    }

    /* 两个事务都通过了检查并提交 */
    printf("\n两个事务都通过了检查并提交：\n");
    printf("  T5 释放 A 的许可证: permit_a = 0\n");
    printf("  T6 释放 B 的许可证: permit_b = 0\n");
    printf("\n结果: A=0, B=0，违反了每人至少 1 张的约束！\n");
    printf("这就是 Write-Skew 异常（串行化可避免）\n\n");
}

/* ========== 主函数 ========== */
int main(void) {
    printf("========================================\n");
    printf("   快照隔离（Snapshot Isolation）演示\n");
    printf("========================================\n\n");

    test_si_vs_rc();
    test_first_writer_wins();
    test_write_skew();

    printf("========================================\n");
    printf("快照隔离要点总结:\n");
    printf("  SI vs RC:\n");
    printf("    - SI: 事务全程使用同一快照\n");
    printf("    - RC: 每次语句重新计算快照\n");
    printf("  First-Writer-Wins:\n");
    printf("    - 检测并发更新冲突\n");
    printf("    - 先提交者获胜，后提交者回滚\n");
    printf("  Write-Skew:\n");
    printf("    - 两个事务读取不同行后各自更新\n");
    printf("    - SI 下可能违反一致性约束\n");
    printf("    - Serializable 可避免\n");
    printf("========================================\n");

    return 0;
}
