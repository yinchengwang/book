/**
 * 数据库锁机制演示
 *
 * 演示内容：
 * - 锁类型：共享锁（S锁）和排他锁（X锁）
 * - 锁升级：从共享锁升级为排他锁
 * - 锁粒度：行锁、页锁、表锁
 * - 并发场景：读并发、写阻塞、锁升级冲突
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ========== 锁类型定义 ========== */
typedef enum {
    LOCK_NONE = 0,   // 无锁
    LOCK_S = 1,       // 共享锁（读锁）
    LOCK_X = 2        // 排他锁（写锁）
} LockType;

/* ========== 锁持有者结构 ========== */
typedef struct {
    int txn_id;           // 事务 ID
    LockType lock_type;   // 锁类型
    bool granted;         // 是否已授予
} LockHolder;

/* ========== 锁结构 ========== */
typedef struct {
    int resource_id;       // 资源 ID（行/页/表）
    LockType current_lock;// 当前持有的锁类型
    LockHolder holders[8];// 锁持有者列表（最多 8 个并发持有者）
    int holder_count;     // 当前持有者数量
} Lock;

/* ========== 全局锁表 ========== */
Lock lock_table[16];
int lock_count = 0;

/* ========== 初始化锁表 ========== */
void init_lock_table(void) {
    printf("[初始化] 创建锁表，模拟资源锁状态\n");
    lock_count = 0;
    for (int i = 0; i < 16; i++) {
        lock_table[i].resource_id = -1;
        lock_table[i].current_lock = LOCK_NONE;
        lock_table[i].holder_count = 0;
    }
}

/* ========== 查找或创建锁 ========== */
Lock* get_or_create_lock(int resource_id) {
    for (int i = 0; i < lock_count; i++) {
        if (lock_table[i].resource_id == resource_id) {
            return &lock_table[i];
        }
    }
    if (lock_count < 16) {
        lock_table[lock_count].resource_id = resource_id;
        lock_table[lock_count].current_lock = LOCK_NONE;
        lock_table[lock_count].holder_count = 0;
        return &lock_table[lock_count++];
    }
    return NULL;
}

/* ========== 锁兼容性矩阵 ========== */
bool locks_compatible(LockType held, LockType requested) {
    // S-S 兼容，S-X 和 X-X 不兼容
    if (held == LOCK_S && requested == LOCK_S) return true;
    return false;
}

/* ========== 获取共享锁（S锁） ========== */
bool lock_shared(int txn_id, int resource_id) {
    Lock *lock = get_or_create_lock(resource_id);
    if (!lock) return false;

    // 已有排他锁，互斥
    if (lock->current_lock == LOCK_X) {
        printf("[S锁失败] 事务%d 请求资源%d: 已有X锁(排他锁)，互斥\n", txn_id, resource_id);
        return false;
    }

    // 授予共享锁
    lock->holders[lock->holder_count].txn_id = txn_id;
    lock->holders[lock->holder_count].lock_type = LOCK_S;
    lock->holders[lock->holder_count].granted = true;
    lock->holder_count++;
    lock->current_lock = LOCK_S;

    printf("[S锁授予] 事务%d 获得资源%d的共享锁(读锁)\n", txn_id, resource_id);
    printf("  锁状态: %d个持有者(均为S锁)\n", lock->holder_count);
    return true;
}

/* ========== 获取排他锁（X锁） ========== */
bool lock_exclusive(int txn_id, int resource_id) {
    Lock *lock = get_or_create_lock(resource_id);
    if (!lock) return false;

    // 已有任何锁，互斥
    if (lock->current_lock != LOCK_NONE) {
        printf("[X锁失败] 事务%d 请求资源%d: 已有锁\n", txn_id, resource_id);
        return false;
    }

    // 授予排他锁
    lock->holders[0].txn_id = txn_id;
    lock->holders[0].lock_type = LOCK_X;
    lock->holders[0].granted = true;
    lock->holder_count = 1;
    lock->current_lock = LOCK_X;

    printf("[X锁授予] 事务%d 获得资源%d的排他锁(写锁)\n", txn_id, resource_id);
    printf("  锁状态: 1个持有者(X锁)\n");
    return true;
}

/* ========== 锁升级（S -> X） ========== */
bool lock_upgrade(int txn_id, int resource_id) {
    Lock *lock = get_or_create_lock(resource_id);
    if (!lock) return false;

    // 检查是否只有当前事务持有S锁
    if (lock->current_lock != LOCK_S || lock->holder_count != 1) {
        printf("[升级失败] 事务%d: 资源%d上有%d个S锁持有者，无法升级\n",
               txn_id, resource_id, lock->holder_count);
        return false;
    }

    // 检查是否是同一事务
    if (lock->holders[0].txn_id != txn_id) {
        printf("[升级失败] 事务%d: 持有者是事务%d，不是自身\n",
               txn_id, lock->holders[0].txn_id);
        return false;
    }

    // 执行升级
    lock->holders[0].lock_type = LOCK_X;
    lock->current_lock = LOCK_X;
    printf("[锁升级] 事务%d: S锁 -> X锁\n", txn_id);
    return true;
}

/* ========== 释放锁 ========== */
void unlock(int txn_id, int resource_id) {
    Lock *lock = get_or_create_lock(resource_id);
    if (!lock || lock->current_lock == LOCK_NONE) return;

    for (int i = 0; i < lock->holder_count; i++) {
        if (lock->holders[i].txn_id == txn_id) {
            printf("[解锁] 事务%d 释放资源%d的%c锁\n",
                   txn_id, resource_id,
                   lock->holders[i].lock_type == LOCK_S ? 'S' : 'X');

            // 移除持有者
            for (int j = i; j < lock->holder_count - 1; j++) {
                lock->holders[j] = lock->holders[j + 1];
            }
            lock->holder_count--;

            // 重置锁状态
            if (lock->holder_count == 0) {
                lock->current_lock = LOCK_NONE;
            } else if (lock->holders[0].lock_type == LOCK_S) {
                lock->current_lock = LOCK_S;
            }
            break;
        }
    }
}

/* ========== 测试场景 ========== */

// 场景1: 共享锁并发读
void test_shared_reads(void) {
    printf("\n========== 测试1: 共享锁并发读 ==========\n");
    printf("场景: 事务1、事务2、事务3同时读取资源1\n\n");

    lock_shared(1, 1);  // 事务1获取S锁
    lock_shared(2, 1);  // 事务2获取S锁
    lock_shared(3, 1);  // 事务3获取S锁

    printf("  结果: 3个事务可以同时持有共享锁，并发读取\n");
    printf("  原理: S-S兼容，多个读者可同时访问\n\n");

    unlock(1, 1);
    unlock(2, 1);
    unlock(3, 1);
}

// 场景2: 写阻塞
void test_write_blocks(void) {
    printf("========== 测试2: 写操作阻塞 ==========\n");
    printf("场景: 事务1持有S锁，事务2请求X锁\n\n");

    lock_shared(1, 2);  // 事务1获取S锁
    printf("  事务1正在读取资源2...\n");

    bool result = lock_exclusive(2, 2);  // 事务2请求X锁
    printf("  事务2请求X锁: %s\n\n", result ? "成功" : "阻塞");

    unlock(1, 2);
}

// 场景3: 锁升级
void test_lock_upgrade(void) {
    printf("========== 测试3: 锁升级 ==========\n");
    printf("场景: 事务1先读取(S锁)，再写入(升级为X锁)\n\n");

    lock_shared(1, 3);  // 事务1先获取S锁
    printf("  事务1: 先读取资源3\n");
    printf("  模拟业务逻辑处理...\n");

    lock_upgrade(1, 3);  // 升级为X锁
    printf("  事务1: 升级为X锁后写入\n");

    unlock(1, 3);
    printf("  事务1: 释放X锁\n\n");
}

// 场景4: 锁升级冲突
void test_upgrade_conflict(void) {
    printf("========== 测试4: 锁升级冲突 ==========\n");
    printf("场景: 事务1、事务2各持S锁，事务1尝试升级\n\n");

    lock_shared(1, 4);  // 事务1获取S锁
    lock_shared(2, 4);  // 事务2获取S锁

    printf("  当前状态: 2个S锁持有者\n");
    lock_upgrade(1, 4);  // 事务1尝试升级 -> 失败

    printf("  原因: 存在多个S锁持有者时不允许升级\n");
    printf("  解决方案: 必须等所有S锁释放后才能获得X锁\n\n");

    unlock(1, 4);
    unlock(2, 4);
}

// 场景5: 锁粒度演示
void test_lock_granularity(void) {
    printf("========== 测试5: 锁粒度 ==========\n");
    printf("锁粒度从小到大: 行锁 < 页锁 < 表锁\n\n");

    printf("  行锁(row):  锁定单行数据，并发度最高\n");
    printf("  页锁(page): 锁定整个页面(通常8KB)\n");
    printf("  表锁(table): 锁定整张表，并发度最低\n\n");

    // 模拟表锁
    printf("  模拟表锁: 对整张表加X锁\n");
    lock_exclusive(10, 100);  // 资源ID 100 代表表级锁
    printf("  效果: 任何行/页操作都被阻塞\n\n");

    unlock(10, 100);
}

/* ========== 主函数 ========== */
int main(void) {
    printf("========================================\n");
    printf("       数据库锁机制演示\n");
    printf("========================================\n\n");

    init_lock_table();

    test_shared_reads();      // S锁并发读
    test_write_blocks();       // X锁阻塞
    test_lock_upgrade();       // 锁升级
    test_upgrade_conflict();   // 升级冲突
    test_lock_granularity();   // 锁粒度

    printf("========================================\n");
    printf("锁机制总结:\n");
    printf("  S锁(共享锁): 多个事务可同时持有，用于读\n");
    printf("  X锁(排他锁): 只有一个事务持有，用于写\n");
    printf("  锁升级: S->X，但需满足单一持有者条件\n");
    printf("  锁粒度: 行/页/表，越细并发越高，开销越大\n");
    printf("========================================\n");

    return 0;
}
