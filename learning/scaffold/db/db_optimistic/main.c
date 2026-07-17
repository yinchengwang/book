/**
 * 乐观并发控制 (Optimistic Concurrency Control, OCC) 演示
 *
 * 核心思想：假设并发冲突较少，先乐观地执行操作，提交时再检测冲突。
 *
 * 三阶段：
 *   1. 读取 (Read)    - 记录数据的版本号
 *   2. 验证 (Validate)- 提交时检查版本是否变化
 *   3. 写入 (Write)   - 版本匹配则写入并递增版本号
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* 最大重试次数，防止无限重试 */
#define MAX_RETRIES 5

/* ============================================================================
 * 模拟数据记录结构
 * ============================================================================ */

/* 账户记录 */
typedef struct {
    int    id;          /* 账户 ID */
    int    balance;     /* 余额 */
    int    version;     /* 版本号，用于乐观锁冲突检测 */
} Account;

/* ============================================================================
 * 模拟存储（用内存模拟数据库表）
 * ============================================================================ */

/* 账户数组，模拟数据库表 */
static Account g_accounts[10];
static int     g_account_count = 3;

/* 初始化模拟数据 */
static void init_accounts(void)
{
    for (int i = 0; i < g_account_count; i++) {
        g_accounts[i].id      = i + 1;
        g_accounts[i].balance = 1000;  /* 每个账户初始余额 1000 */
        g_accounts[i].version = 1;
    }
    printf("[初始化] 创建 %d 个账户，每个账户余额 1000，版本号 1\n", g_account_count);
}

/* 模拟读取：复制数据并记录读取时的版本号 */
static void read_account(int id, int *balance, int *version)
{
    for (int i = 0; i < g_account_count; i++) {
        if (g_accounts[i].id == id) {
            *balance = g_accounts[i].balance;
            *version = g_accounts[i].version;
            return;
        }
    }
    *balance = -1;
    *version = -1;
}

/* ============================================================================
 * 乐观锁事务：转账操作
 *
 * 返回值：
 *   0  - 成功
 *  -1  - 账户不存在
 *  -2  - 版本冲突，触发重试
 *  -3  - 重试次数耗尽
 * ============================================================================ */

static int transfer_with_occ(int from_id, int to_id, int amount,
                              int read_version_from, int read_version_to)
{
    /* 查找源账户和目标账户 */
    Account *from = NULL;
    Account *to   = NULL;

    for (int i = 0; i < g_account_count; i++) {
        if (g_accounts[i].id == from_id) from = &g_accounts[i];
        if (g_accounts[i].id == to_id)   to   = &g_accounts[i];
    }

    if (!from || !to) {
        printf("      [验证失败] 账户不存在\n");
        return -1;
    }

    /* 关键步骤：提交前再次检查版本号（Validation Phase） */
    if (from->version != read_version_from) {
        printf("      [验证失败] 源账户 %d 版本冲突: 读取时 %d, 当前 %d\n",
               from_id, read_version_from, from->version);
        return -2;
    }
    if (to->version != read_version_to) {
        printf("      [验证失败] 目标账户 %d 版本冲突: 读取时 %d, 当前 %d\n",
               to_id, read_version_to, to->version);
        return -2;
    }

    /* 检查余额是否充足 */
    if (from->balance < amount) {
        printf("      [验证失败] 源账户余额不足: %d < %d\n", from->balance, amount);
        return -1;
    }

    /* 版本匹配，执行写入（Write Phase） */
    from->balance -= amount;
    to->balance   += amount;
    from->version++;
    to->version++;

    printf("      [写入成功] 转账 %d: %d -> %d, 版本更新 [%d->%d] [%d->%d]\n",
           amount, from_id, to_id,
           read_version_from, from->version,
           read_version_to,   to->version);
    return 0;
}

/* ============================================================================
 * 带重试机制的转账操作
 * ============================================================================ */

static bool transfer_with_retry(int from_id, int to_id, int amount)
{
    int attempt = 0;

    while (attempt < MAX_RETRIES) {
        attempt++;
        printf("  [尝试 %d/%d] 从账户 %d 向账户 %d 转账 %d\n",
               attempt, MAX_RETRIES, from_id, to_id, amount);

        /* 读取阶段：记录当前版本号（Read Phase） */
        int balance_from, version_from;
        int balance_to,   version_to;
        read_account(from_id, &balance_from, &version_from);
        read_account(to_id,   &balance_to,   &version_to);

        printf("      [读取完成] 源账户余额=%d 版本=%d, 目标账户余额=%d 版本=%d\n",
               balance_from, version_from, balance_to, version_to);

        /* 验证并写入 */
        int result = transfer_with_occ(from_id, to_id, amount,
                                       version_from, version_to);

        if (result == 0) {
            return true;  /* 成功 */
        }
        if (result == -1) {
            return false; /* 账户不存在或余额不足，不重试 */
        }

        /* result == -2: 版本冲突，等待后重试 */
        printf("      [等待重试] 检测到版本冲突，%d ms 后重试...\n", attempt * 100);
    }

    printf("  [失败] 重试次数耗尽 (%d 次)\n", MAX_RETRIES);
    return false;
}

/* ============================================================================
 * 模拟并发：另一个事务在后台修改数据
 * ============================================================================ */

static void simulate_external_update(int account_id, int new_balance)
{
    for (int i = 0; i < g_account_count; i++) {
        if (g_accounts[i].id == account_id) {
            /* 直接修改，模拟外部事务提交 */
            g_accounts[i].balance += new_balance;
            g_accounts[i].version++;
            printf("[外部更新] 账户 %d 被修改: 余额变化, 版本 %d -> %d\n",
                   account_id, g_accounts[i].version - 1, g_accounts[i].version);
            return;
        }
    }
}

/* ============================================================================
 * 打印账户状态
 * ============================================================================ */

static void print_accounts(const char *title)
{
    printf("\n=== %s ===\n", title);
    printf("%-8s %-10s %-10s\n", "账户ID", "余额", "版本号");
    printf("%-8s %-10s %-10s\n", "------", "------", "------");
    for (int i = 0; i < g_account_count; i++) {
        printf("%-8d %-10d %-10d\n",
               g_accounts[i].id, g_accounts[i].balance, g_accounts[i].version);
    }
    printf("\n");
}

/* ============================================================================
 * 测试场景
 * ============================================================================ */

/* 测试1：无冲突场景 */
static void test_no_conflict(void)
{
    printf("\n");
    printf("####################################################################\n");
    printf("# 测试1: 无冲突场景 - 转账 100\n");
    printf("####################################################################\n");

    init_accounts();
    print_accounts("转账前");

    /* 账户1 -> 账户2 转账 100，无外部干扰 */
    bool ok = transfer_with_retry(1, 2, 100);

    print_accounts("转账后");
    printf("结果: %s\n", ok ? "成功" : "失败");
}

/* 测试2：冲突检测与重试场景 */
static void test_conflict_with_retry(void)
{
    printf("\n");
    printf("####################################################################\n");
    printf("# 测试2: 冲突检测与重试 - 模拟并发修改触发版本冲突\n");
    printf("####################################################################\n");

    init_accounts();
    print_accounts("初始状态");

    /* 在读取之后、提交之前，模拟外部事务修改了目标账户 */
    printf("\n[模拟并发] 账户 3 被外部事务直接修改（模拟并发提交）\n");
    simulate_external_update(3, 500);

    /* 现在尝试账户1 -> 账户3 转账，应该触发版本冲突并重试 */
    printf("\n[主事务] 现在执行账户1 -> 账户3 转账 200\n");
    printf("         （由于目标账户已被外部修改，应检测到版本冲突）\n\n");

    bool ok = transfer_with_retry(1, 3, 200);

    print_accounts("最终状态");
    printf("结果: %s\n", ok ? "成功" : "失败");
}

/* 测试3：多轮冲突场景 */
static void test_multiple_conflicts(void)
{
    printf("\n");
    printf("####################################################################\n");
    printf("# 测试3: 多轮冲突 - 连续被外部修改，需要多次重试\n");
    printf("####################################################################\n");

    init_accounts();

    /* 读取后立即被修改，制造冲突 */
    printf("\n[主事务] 读取账户2状态...\n");
    int b, v;
    read_account(2, &b, &v);
    printf("         读取到: 余额=%d 版本=%d\n", b, v);

    printf("\n[模拟并发] 外部事务修改账户2...\n");
    simulate_external_update(2, 200);

    /* 尝试提交，此时版本已变化，应检测到冲突 */
    printf("\n[主事务] 提交转账，由于版本冲突需重试...\n");
    bool ok = transfer_with_retry(1, 2, 50);

    print_accounts("最终状态");
    printf("结果: %s\n", ok ? "成功" : "失败");
}

/* ============================================================================
 * 入口
 * ============================================================================ */

int main(void)
{
    printf("========================================================\n");
    printf("    乐观并发控制 (OCC) 演示\n");
    printf("    Optimistic Concurrency Control\n");
    printf("========================================================\n");
    printf("核心机制: 读取时记录版本，提交时校验版本，冲突则重试\n\n");

    test_no_conflict();         /* 测试1 */
    test_conflict_with_retry(); /* 测试2 */
    test_multiple_conflicts();   /* 测试3 */

    printf("\n========================================================\n");
    printf("    演示结束\n");
    printf("========================================================\n");

    return 0;
}
