/**
 * ACID 特性演示 - 银行转账示例
 *
 * 演示数据库事务的四个核心特性：
 * - 原子性（Atomicity）：事务要么全部成功，要么全部失败
 * - 一致性（Consistency）：事务前后数据库状态一致
 * - 隔离性（Isolation）：并发事务互不干扰
 * - 持久性（Durability）：事务提交后永久保存
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ========== 模拟账户结构 ========== */
typedef struct {
    int account_id;
    char name[32];
    double balance;
} Account;

/* ========== 模拟事务日志（Undo Log） ========== */
typedef struct {
    int account_id;
    double old_balance;
    double new_balance;
    bool committed;
} UndoLogEntry;

/* ========== 全局状态 ========== */
Account accounts[3];           // 模拟账户表
UndoLogEntry undo_log[10];     // 模拟 Undo Log
int undo_log_count = 0;
bool transaction_active = false;

/* ========== 初始化账户 ========== */
void init_accounts(void) {
    accounts[0] = (Account){1, "Alice", 1000.0};
    accounts[1] = (Account){2, "Bob", 500.0};
    accounts[2] = (Account){3, "Charlie", 200.0};
    printf("[初始化] 账户状态:\n");
    for (int i = 0; i < 3; i++) {
        printf("  %s (ID=%d): %.2f 元\n", accounts[i].name, accounts[i].account_id, accounts[i].balance);
    }
    printf("\n");
}

/* ========== 原子性演示 ========== */

// 记录 Undo Log
void log_undo(int account_id, double old_balance, double new_balance) {
    undo_log[undo_log_count].account_id = account_id;
    undo_log[undo_log_count].old_balance = old_balance;
    undo_log[undo_log_count].new_balance = new_balance;
    undo_log[undo_log_count].committed = false;
    undo_log_count++;
}

// 回滚操作（原子性保证）
void rollback(void) {
    printf("[回滚] 执行 Undo Log 回滚...\n");
    for (int i = undo_log_count - 1; i >= 0; i--) {
        if (!undo_log[i].committed) {
            for (int j = 0; j < 3; j++) {
                if (accounts[j].account_id == undo_log[i].account_id) {
                    accounts[j].balance = undo_log[i].old_balance;
                    printf("  回滚账户 %s: %.2f -> %.2f\n",
                           accounts[j].name, undo_log[i].new_balance, undo_log[i].old_balance);
                }
            }
        }
    }
    undo_log_count = 0;
    transaction_active = false;
}

/* ========== 转账事务（原子性 + 一致性） ========== */

bool transfer(int from_id, int to_id, double amount) {
    printf("[事务] 开始转账: 账户 %d -> 账户 %d, 金额 %.2f\n", from_id, to_id, amount);
    transaction_active = true;

    // 查找账户
    Account *from = NULL, *to = NULL;
    for (int i = 0; i < 3; i++) {
        if (accounts[i].account_id == from_id) from = &accounts[i];
        if (accounts[i].account_id == to_id) to = &accounts[i];
    }

    // 一致性检查：账户必须存在
    if (!from || !to) {
        printf("[错误] 账户不存在\n");
        rollback();
        return false;
    }

    // 一致性检查：余额必须充足
    if (from->balance < amount) {
        printf("[错误] 余额不足 (当前: %.2f, 需要: %.2f)\n", from->balance, amount);
        rollback();
        return false;
    }

    // 记录 Undo Log
    log_undo(from_id, from->balance, from->balance - amount);
    log_undo(to_id, to->balance, to->balance + amount);

    // 执行扣款
    from->balance -= amount;
    printf("  扣款: %s 余额 %.2f -> %.2f\n", from->name, from->balance + amount, from->balance);

    // 模拟故障点（测试原子性）
    #ifdef SIMULATE_FAILURE
    printf("[故障模拟] 在扣款后、存款前发生崩溃\n");
    rollback();
    return false;
    #endif

    // 执行存款
    to->balance += amount;
    printf("  存款: %s 余额 %.2f -> %.2f\n", to->name, to->balance - amount, to->balance);

    // 一致性验证：总金额不变
    double total_before = 0, total_after = 0;
    printf("\n[一致性验证]\n");
    printf("  转账前总额: 1000 + 500 + 200 = 1700 元\n");
    for (int i = 0; i < 3; i++) {
        total_after += accounts[i].balance;
    }
    printf("  转账后总额: %.2f 元\n", total_after);
    if (total_after == 1700.0) {
        printf("  ✓ 一致性保持: 总金额不变\n");
    }

    // 标记 Undo Log 为已提交
    for (int i = 0; i < undo_log_count; i++) {
        undo_log[i].committed = true;
    }
    undo_log_count = 0;
    transaction_active = false;

    printf("[事务] 转账成功提交\n\n");
    return true;
}

/* ========== 隔离性演示 ========== */

// 脏读演示
void demo_dirty_read(void) {
    printf("[隔离性] 脏读问题演示:\n");
    printf("  事务A修改数据但未提交 -> 事务B读取到未提交数据 -> 事务A回滚\n");
    printf("  解决方案: Read Committed 隔离级别，只读取已提交数据\n\n");
}

// 不可重复读演示
void demo_non_repeatable_read(void) {
    printf("[隔离性] 不可重复读演示:\n");
    printf("  事务A读取数据 -> 事务B修改并提交 -> 事务A再次读取结果不同\n");
    printf("  解决方案: Repeatable Read 隔离级别，使用 MVCC 快照\n\n");
}

// 幻读演示
void demo_phantom_read(void) {
    printf("[隔离性] 幻读演示:\n");
    printf("  事务A查询满足条件的行 -> 事务B插入新行 -> 事务A再次查询出现'幻影'\n");
    printf("  解决方案: Serializable 隔离级别，使用范围锁\n\n");
}

/* ========== 持久性演示 ========== */

void demo_durability(void) {
    printf("[持久性] 事务提交后的保证:\n");
    printf("  1. Redo Log 已写入磁盘（WAL 原则）\n");
    printf("  2. 即使系统崩溃，重启后可通过 Redo Log 恢复\n");
    printf("  3. 只有提交的事务才会被恢复\n\n");
}

/* ========== 显示最终状态 ========== */

void show_final_state(void) {
    printf("========== 最终账户状态 ==========\n");
    for (int i = 0; i < 3; i++) {
        printf("  %s (ID=%d): %.2f 元\n", accounts[i].name, accounts[i].account_id, accounts[i].balance);
    }
    double total = 0;
    for (int i = 0; i < 3; i++) total += accounts[i].balance;
    printf("  总计: %.2f 元\n", total);
}

/* ========== 主函数 ========== */

int main(void) {
    printf("========================================\n");
    printf("       ACID 特性演示 - 银行转账\n");
    printf("========================================\n\n");

    // 初始化
    init_accounts();

    // 演示原子性 + 一致性：正常转账
    printf("========== 测试1: 正常转账 ==========\n");
    transfer(1, 2, 100.0);
    show_final_state();

    // 演示原子性：余额不足回滚
    printf("\n========== 测试2: 余额不足回滚 ==========\n");
    transfer(1, 3, 5000.0);  // Alice 余额不足
    show_final_state();

    // 演示原子性：账户不存在
    printf("\n========== 测试3: 账户不存在回滚 ==========\n");
    transfer(99, 1, 100.0);  // 不存在的账户
    show_final_state();

    // 演示隔离性问题
    printf("\n========== 隔离性问题演示 ==========\n\n");
    demo_dirty_read();
    demo_non_repeatable_read();
    demo_phantom_read();

    // 演示持久性
    printf("========== 持久性保证 ==========\n\n");
    demo_durability();

    printf("========================================\n");
    printf("ACID 四大特性总结:\n");
    printf("  A - 原子性: Undo Log 回滚机制\n");
    printf("  C - 一致性: 约束检查 + 业务规则验证\n");
    printf("  I - 隔离性: 锁机制 + MVCC\n");
    printf("  D - 持久性: Redo Log + WAL\n");
    printf("========================================\n");

    return 0;
}
