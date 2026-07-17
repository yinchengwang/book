/**
 * SQL DCL 事务控制学习演示
 *
 * 本程序演示 SQL 事务控制语言（DCL）的核心概念：
 * - BEGIN / COMMIT / ROLLBACK
 * - SAVEPOINT / ROLLBACK TO SAVEPOINT
 * - AUTOCOMMIT 模式
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/*------------------------- 数据结构定义 -------------------------*/

/* 事务状态枚举 */
typedef enum {
    TXN_IDLE,           /* 空闲状态，无活跃事务 */
    TXN_ACTIVE,         /* 事务活跃中 */
    TXN_PARTIAL_ROLLBACK /* 部分回滚到 Savepoint */
} TxnState;

/* Savepoint 结构 */
typedef struct Savepoint {
    char name[64];               /* Savepoint 名称 */
    int statement_count;         /* 到达此 Savepoint 时的语句计数 */
    struct Savepoint *next;      /* 链表下一个节点 */
} Savepoint;

/* 事务控制块 */
typedef struct {
    TxnState state;              /* 当前事务状态 */
    bool autocommit;             /* 是否启用自动提交 */
    int statement_count;         /* 已执行语句计数 */
    Savepoint *savepoints;       /* Savepoint 链表头 */
    Savepoint *last_savepoint;   /* 最近创建的 Savepoint */
} Transaction;

/*------------------------- 事务管理函数 -------------------------*/

/* 初始化事务控制块 */
static void txn_init(Transaction *txn) {
    txn->state = TXN_IDLE;
    txn->autocommit = false;    /* 默认关闭自动提交 */
    txn->statement_count = 0;
    txn->savepoints = NULL;
    txn->last_savepoint = NULL;
}

/* 开始事务（BEGIN） */
static void txn_begin(Transaction *txn) {
    if (txn->state != TXN_IDLE) {
        printf("[错误] 无法 BEGIN：当前已有活跃事务\n");
        return;
    }
    txn->state = TXN_ACTIVE;
    txn->statement_count = 0;
    printf(">>> BEGIN -- 事务已启动\n\n");
}

/* 提交事务（COMMIT） */
static void txn_commit(Transaction *txn) {
    if (txn->state == TXN_IDLE) {
        printf("[错误] 无法 COMMIT：当前无活跃事务\n");
        return;
    }
    printf(">>> COMMIT -- 事务已提交，%d 条语句永久生效\n",
           txn->statement_count);
    /* 释放所有 Savepoint */
    Savepoint *sp = txn->savepoints;
    while (sp) {
        Savepoint *next = sp->next;
        free(sp);
        sp = next;
    }
    txn->state = TXN_IDLE;
    txn->savepoints = NULL;
    txn->last_savepoint = NULL;
    printf("    事务已结束，状态重置为 IDLE\n\n");
}

/* 回滚事务（ROLLBACK） */
static void txn_rollback(Transaction *txn) {
    if (txn->state == TXN_IDLE) {
        printf("[错误] 无法 ROLLBACK：当前无活跃事务\n");
        return;
    }
    printf(">>> ROLLBACK -- 事务已回滚，所有更改已撤销\n");
    /* 释放所有 Savepoint */
    Savepoint *sp = txn->savepoints;
    while (sp) {
        Savepoint *next = sp->next;
        free(sp);
        sp = next;
    }
    txn->state = TXN_IDLE;
    txn->savepoints = NULL;
    txn->last_savepoint = NULL;
    printf("    事务已结束，状态重置为 IDLE\n\n");
}

/* 创建 Savepoint */
static void txn_savepoint(Transaction *txn, const char *name) {
    if (txn->state == TXN_IDLE) {
        printf("[错误] 无法 SAVEPOINT：当前无活跃事务\n");
        return;
    }
    Savepoint *sp = malloc(sizeof(Savepoint));
    strncpy(sp->name, name, sizeof(sp->name) - 1);
    sp->name[sizeof(sp->name) - 1] = '\0';
    sp->statement_count = txn->statement_count;
    sp->next = NULL;

    /* 插入链表尾部 */
    if (txn->last_savepoint) {
        txn->last_savepoint->next = sp;
    } else {
        txn->savepoints = sp;
    }
    txn->last_savepoint = sp;

    printf(">>> SAVEPOINT '%s' -- 已创建（语句计数：%d）\n\n",
           name, sp->statement_count);
}

/* 回滚到 Savepoint */
static bool txn_rollback_to(Transaction *txn, const char *name) {
    if (txn->state == TXN_IDLE) {
        printf("[错误] 无法 ROLLBACK TO：当前无活跃事务\n");
        return false;
    }

    Savepoint *sp = txn->savepoints;
    while (sp) {
        if (strcmp(sp->name, name) == 0) {
            /* 删除此 Savepoint 之后的所有 Savepoint */
            Savepoint *cur = sp->next;
            while (cur) {
                Savepoint *next = cur->next;
                free(cur);
                cur = next;
            }
            sp->next = NULL;
            txn->last_savepoint = sp;

            txn->statement_count = sp->statement_count;
            printf(">>> ROLLBACK TO SAVEPOINT '%s' -- 已回滚到保存点\n", name);
            printf("    语句计数已重置为：%d\n\n", sp->statement_count);
            return true;
        }
        sp = sp->next;
    }

    printf("[错误] SAVEPOINT '%s' 不存在\n\n", name);
    return false;
}

/* 模拟执行一条 SQL 语句 */
static void execute_sql(Transaction *txn, const char *sql) {
    if (txn->state == TXN_IDLE) {
        if (txn->autocommit) {
            /* 自动提交模式：每条语句自动开启和提交事务 */
            printf(">>> [AUTOCOMMIT] 自动开启事务\n");
            txn_begin(txn);
        } else {
            printf("[错误] 无法执行：当前无活跃事务（需先 BEGIN）\n");
            return;
        }
    }

    txn->statement_count++;
    printf(">>> [%02d] %s\n", txn->statement_count, sql);
}

/* 设置 Autocommit 模式 */
static void set_autocommit(Transaction *txn, bool enabled) {
    txn->autocommit = enabled;
    printf(">>> SET AUTOCOMMIT = %s\n\n", enabled ? "ON" : "OFF");
}

/*------------------------- 测试场景 -------------------------*/

/* 场景1：完整事务流程 */
static void test_full_transaction(void) {
    printf("========================================\n");
    printf("场景1：完整事务流程（BEGIN -> SQL -> COMMIT）\n");
    printf("========================================\n\n");

    Transaction txn;
    txn_init(&txn);

    txn_begin(&txn);
    execute_sql(&txn, "INSERT INTO users (id, name) VALUES (1, 'Alice')");
    execute_sql(&txn, "UPDATE users SET name = 'Alicia' WHERE id = 1");
    execute_sql(&txn, "DELETE FROM users WHERE id = 2");
    txn_commit(&txn);
}

/* 场景2：回滚到 Savepoint */
static void test_savepoint_rollback(void) {
    printf("========================================\n");
    printf("场景2：部分回滚到 Savepoint\n");
    printf("========================================\n\n");

    Transaction txn;
    txn_init(&txn);

    txn_begin(&txn);
    execute_sql(&txn, "INSERT INTO orders (id, product) VALUES (100, 'Book')");
    execute_sql(&txn, "INSERT INTO orders (id, product) VALUES (101, 'Pen')");

    /* 创建 Savepoint */
    txn_savepoint(&txn, "before_large_order");

    execute_sql(&txn, "INSERT INTO orders (id, product) VALUES (200, 'Laptop')");
    execute_sql(&txn, "INSERT INTO orders (id, product) VALUES (201, 'Phone')");

    printf("--- 决定取消最后两笔大额订单 ---\n");
    txn_rollback_to(&txn, "before_large_order");

    execute_sql(&txn, "INSERT INTO orders (id, product) VALUES (102, 'Paper')");
    txn_commit(&txn);
}

/* 模拟在 Autocommit 模式下执行并自动提交一条语句 */
static void execute_sql_autocommit(Transaction *txn, const char *sql) {
    /* 自动提交模式：每条语句自动开启和提交事务 */
    txn_begin(txn);
    txn->statement_count++;
    printf(">>> [%02d] %s\n", txn->statement_count, sql);
    txn_commit(txn);
}

/* 场景3：Autocommit 模式 */
static void test_autocommit_mode(void) {
    printf("========================================\n");
    printf("场景3：Autocommit 模式\n");
    printf("========================================\n\n");

    Transaction txn;
    txn_init(&txn);

    /* 启用 Autocommit */
    set_autocommit(&txn, true);

    /* 每条语句自动在独立事务中执行并提交 */
    execute_sql_autocommit(&txn, "INSERT INTO config (key, value) VALUES ('theme', 'dark')");
    execute_sql_autocommit(&txn, "INSERT INTO config (key, value) VALUES ('lang', 'zh')");

    printf("--- 切换回手动提交模式 ---\n\n");
    set_autocommit(&txn, false);

    txn_begin(&txn);
    execute_sql(&txn, "UPDATE config SET value = 'light' WHERE key = 'theme'");
    execute_sql(&txn, "UPDATE config SET value = 'en' WHERE key = 'lang'");
    txn_rollback(&txn);  /* 撤销这两条更新 */
}

/*------------------------- 主函数 -------------------------*/
int main(void) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║        SQL DCL 事务控制语言学习演示                      ║\n");
    printf("║   涵盖：BEGIN/COMMIT/ROLLBACK, SAVEPOINT, AUTOCOMMIT    ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n\n");

    test_full_transaction();
    test_savepoint_rollback();
    test_autocommit_mode();

    printf("========================================\n");
    printf("演示结束\n");
    printf("========================================\n\n");

    return 0;
}
