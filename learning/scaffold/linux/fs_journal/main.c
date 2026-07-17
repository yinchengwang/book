/**
 * @file main.c
 * @brief Linux 文件系统日志学习演示
 *
 * 演示内容：
 *   - ext4/xfs 日志机制（journal_commit、journal_revoke）
 *   - 日志恢复流程（Redo/Undo）
 *   - 数据模式（ordered/writeback/data）
 *
 * 编译：gcc -std=c11 -Wall -o fs_journal main.c
 * 运行：./fs_journal
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>

/* ============================================================
 * 模拟日志记录类型
 * ============================================================ */
typedef enum {
    JOURNAL_DESCRIPTOR = 1,   /* 描述块：记录本次事务的变更块列表 */
    JOURNAL_COMMIT     = 2,   /* 提交块：标记事务完成 */
    JOURNAL_REVOKE     = 3,   /* 撤销块：标记某块被释放，跳过恢复 */
    JOURNAL_CHECKPOINT = 4    /* 检查点：标记可回收的日志空间 */
} journal_block_type_t;

static const char *type_name(journal_block_type_t t) {
    switch (t) {
        case JOURNAL_DESCRIPTOR:  return "DESCRIPTOR";
        case JOURNAL_COMMIT:      return "COMMIT";
        case JOURNAL_REVOKE:      return "REVOKE";
        case JOURNAL_CHECKPOINT:  return "CHECKPOINT";
        default:                  return "UNKNOWN";
    }
}

/* ============================================================
 * Demo 1: 文件系统日志基本概念
 * ============================================================ */
static void demo_journal_basics(void) {
    printf("[fs_journal] === 文件系统日志基本概念 ===\n\n");

    printf("[fs_journal] 1. 为什么需要文件系统日志？\n");
    printf("[fs_journal]    文件系统操作涉及多个块的更新（元数据 + 数据）\n");
    printf("[fs_journal]    如果停电/崩溃发生在操作中途，文件系统会不一致\n");
    printf("[fs_journal]    日志（Journal）提供原子性：要么全做，要么全不做\n\n");

    printf("[fs_journal] 2. 日志块类型：\n");
    printf("[fs_journal]    DESCRIPTOR 块: 记录本次事务将修改哪些数据块\n");
    printf("[fs_journal]    COMMIT 块:     标记事务已完整写入日志\n");
    printf("[fs_journal]    REVOKE 块:     标记某块已释放，恢复时跳过\n");
    printf("[fs_journal]    CHECKPOINT 块: 标记回收点，日志空间可复用\n\n");

    printf("[fs_journal] 3. 日志工作流程：\n");
    printf("[fs_journal]    Step 1: 写 DESCRIPTOR（哪些块要被修改）\n");
    printf("[fs_journal]    Step 2: 写数据块副本到日志区\n");
    printf("[fs_journal]    Step 3: 写 COMMIT（标记事务完成）\n");
    printf("[fs_journal]    Step 4: 检查点——将日志数据写回原位\n\n");

    /* 模拟日志写入 */
    const char *blocks[] = {"inode_42", "dir_block_7", "extent_tree"};
    printf("[fs_journal] 模拟事务: 修改 %zu 个块\n", sizeof(blocks)/sizeof(blocks[0]));
    for (size_t i = 0; i < sizeof(blocks)/sizeof(blocks[0]); i++) {
        printf("[fs_journal]   DESCRIPTOR: 登记块 '%s'\n", blocks[i]);
        printf("[fs_journal]   写数据副本到日志区 ... \n");
    }
    printf("[fs_journal]   COMMIT: 事务提交完成\n");
}

/* ============================================================
 * Demo 2: 数据模式（ordered / writeback / data）
 * ============================================================ */
static void demo_data_modes(void) {
    printf("\n[fs_journal] === 三种数据安全模式 ===\n\n");

    printf("[fs_journal] Linux 文件系统支持三种日志模式：\n\n");

    printf("[fs_journal] ① data=ordered（默认，推荐）\n");
    printf("[fs_journal]    元数据写日志，数据先于元数据落盘\n");
    printf("[fs_journal]    保证: 文件数据不会在元数据之前暴露\n");
    printf("[fs_journal]    性能: 中等（数据先写盘有开销）\n\n");

    printf("[fs_journal] ② data=writeback\n");
    printf("[fs_journal]    元数据写日志，数据可以随时落盘\n");
    printf("[fs_journal]    保证: 仅文件系统结构一致性\n");
    printf("[fs_journal]    风险: 崩溃后文件可能包含旧数据（垃圾数据泄露）\n");
    printf("[fs_journal]    性能: 最快（数据异步写）\n\n");

    printf("[fs_journal] ③ data=journal\n");
    printf("[fs_journal]    数据和元数据都写入日志（双重写入）\n");
    printf("[fs_journal]    保证: 最高安全性，数据和元数据均原子\n");
    printf("[fs_journal]    性能: 最慢（数据写两遍：日志 + 原位）\n\n");

    printf("[fs_journal] 查看当前模式: cat /proc/mounts | grep ext4\n");
    printf("[fs_journal] 挂载时指定: mount -o data=ordered /dev/sda1 /mnt\n");
}

/* ============================================================
 * Demo 3: 日志恢复流程
 * ============================================================ */
static void demo_recovery(void) {
    printf("\n[fs_journal] === 日志恢复流程 ===\n\n");

    printf("[fs_journal] 崩溃恢复分三个阶段：\n\n");

    printf("[fs_journal] Phase 1 — 日志扫描 (Journal Scan)：\n");
    printf("[fs_journal]   从上次检查点开始扫描日志\n");
    printf("[fs_journal]   找到所有有 COMMIT 的事务 → 需 Redo\n");
    printf("[fs_journal]   找到所有无 COMMIT 的事务 → 不处理（丢弃）\n");
    printf("[fs_journal]   处理 REVOKE 块 → 跳过已释放的块\n\n");

    printf("[fs_journal] Phase 2 — Redo（重做已提交事务）：\n");
    printf("[fs_journal]   遍历已提交事务的 DESCRIPTOR 块\n");
    printf("[fs_journal]   将日志中的数据副本写回磁盘原位\n");
    printf("[fs_journal]   这步可以重复执行（幂等）\n\n");

    printf("[fs_journal] Phase 3 — 未提交事务处理：\n");
    printf("[fs_journal]   文件系统日志不显式 Undo（未提交的修改尚未写回）\n");
    printf("[fs_journal]   简单丢弃日志记录即可\n\n");

    /* 模拟恢复 */
    printf("[fs_journal] 模拟恢复场景:\n");
    printf("[fs_journal]   检查点 LSN = 1000\n");
    struct { const char *txn; int committed; } txns[] = {
        {"T1", 1}, {"T2", 0}, {"T3", 1}, {"T4", 0}
    };
    for (size_t i = 0; i < sizeof(txns)/sizeof(txns[0]); i++) {
        if (txns[i].committed)
            printf("[fs_journal]   Redo  %s: 已提交，重做修改\n", txns[i].txn);
        else
            printf("[fs_journal]   Skip %s: 未提交，丢弃修改\n", txns[i].txn);
    }
    printf("[fs_journal] 恢复完成，文件系统一致\n");
}

/* ============================================================
 * Demo 4: journal_revoke 机制
 * ============================================================ */
static void demo_revoke(void) {
    printf("\n[fs_journal] === journal_revoke 机制 ===\n\n");

    printf("[fs_journal] Revoke 块的作用: 优化恢复\n");
    printf("[fs_journal]   场景: 事务 T1 修改块 B，后事务 T2 删除块 B 所属文件\n");
    printf("[fs_journal]   问题: 恢复时块 B 的原位已不存在\n");
    printf("[fs_journal]   解决: T2 写入 REVOKE 标记，恢复时跳过块 B\n\n");

    printf("[fs_journal] 示例流程:\n");
    printf("[fs_journal]   T1: DESCRIPTOR[块A, 块B] → 数据副本 → COMMIT\n");
    printf("[fs_journal]   T2: 删除文件（含块B）→ DESCRIPTOR → REVOKE[块B] → COMMIT\n");
    printf("[fs_journal]   恢复时:\n");
    printf("[fs_journal]     T1 Redo: 块A ✓, 块B ✗（被 revoke）\n");
    printf("[fs_journal]     T2 Redo: 正常处理\n");
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("========================================\n");
    printf("  Linux 文件系统日志学习演示\n");
    printf("========================================\n\n");

    printf("--- Demo 1: 日志基本概念 ---\n");
    demo_journal_basics();

    printf("\n--- Demo 2: 数据安全模式 ---\n");
    demo_data_modes();

    printf("\n--- Demo 3: 日志恢复流程 ---\n");
    demo_recovery();

    printf("\n--- Demo 4: journal_revoke 机制 ---\n");
    demo_revoke();

    printf("\n========================================\n");
    printf("=== PASS ===\n");
    printf("========================================\n");
    return 0;
}
