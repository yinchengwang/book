/**
 * 两阶段提交（2PC）演示程序
 *
 * 本程序模拟两阶段提交协议的基本流程，包括：
 * - 协调者（Coordinator）与参与者（Participant）结构
 * - Prepare 阶段：协调者询问所有参与者是否可以提交
 * - Commit 阶段：协调者通知所有参与者提交或回滚
 * - 故障恢复：模拟参与者超时、协调者重启等场景
 *
 * 编译：gcc -std=c11 -Wall -o 2pc main.c
 * 运行：./2pc
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/*============================================================================
 * 常量与枚举定义
 *============================================================================*/

/** 参与者状态枚举 */
typedef enum {
    PARTICIPANT_INIT = 0,      /* 初始状态 */
    PARTICIPANT_READY,         /* 已响应 Prepare */
    PARTICIPANT_COMMITTED,     /* 已提交 */
    PARTICIPANT_ABORTED,       /* 已回滚 */
    PARTICIPANT_UNKNOWN       /* 未知状态（需要恢复） */
} ParticipantState;

/** 事务状态枚举 */
typedef enum {
    TX_INIT = 0,               /* 初始状态 */
    TX_PREPARING,              /* 正在 Prepare 阶段 */
    TX_COMMITTED,              /* 已提交 */
    TX_ABORTED                 /* 已回滚 */
} TransactionState;

/** 投票结果枚举 */
typedef enum {
    VOTE_COMMIT = 0,           /* 投票提交 */
    VOTE_ABORT                 /* 投票回滚 */
} VoteResult;

/*============================================================================
 * 数据结构定义
 *============================================================================*/

/** 参与者结构 */
typedef struct {
    int         id;            /* 参与者 ID */
    char        name[32];      /* 参与者名称 */
    ParticipantState state;   /* 当前状态 */
    bool        can_commit;    /* 是否可以提交（模拟业务条件） */
    bool        responded;     /* 是否已响应 Prepare */
    int         log_position;  /* 日志位置（用于恢复） */
} Participant;

/** 协调者结构 */
typedef struct {
    int             id;             /* 协调者 ID */
    TransactionState state;          /* 事务状态 */
    Participant     *participants[4];/* 参与者列表（最多4个） */
    int             participant_count;
    int             commit_log;     /* 提交日志记录 */
} Coordinator;

/*============================================================================
 * 辅助函数
 *============================================================================*/

/** 打印分隔线 */
static void print_separator(const char *title)
{
    printf("\n");
    printf("========================================\n");
    printf("  %s\n", title);
    printf("========================================\n");
}

/** 打印参与者状态 */
static void print_participant_status(Participant *p)
{
    const char *state_str;
    switch (p->state) {
        case PARTICIPANT_INIT:    state_str = "初始";   break;
        case PARTICIPANT_READY:   state_str = "就绪";   break;
        case PARTICIPANT_COMMITTED: state_str = "已提交"; break;
        case PARTICIPANT_ABORTED: state_str = "已回滚"; break;
        case PARTICIPANT_UNKNOWN: state_str = "未知";   break;
        default:                  state_str = "未知";   break;
    }
    printf("  [%s] ID=%d, 状态=%s, 可提交=%s, 已响应=%s\n",
           p->name, p->id, state_str,
           p->can_commit ? "是" : "否",
           p->responded ? "是" : "否");
}

/** 打印协调者状态 */
static void print_coordinator_status(Coordinator *c)
{
    const char *state_str;
    switch (c->state) {
        case TX_INIT:       state_str = "初始";    break;
        case TX_PREPARING:  state_str = "准备中";  break;
        case TX_COMMITTED:  state_str = "已提交";  break;
        case TX_ABORTED:    state_str = "已回滚";  break;
        default:            state_str = "未知";    break;
    }
    printf("  协调者: ID=%d, 事务状态=%s, 参与者数=%d\n",
           c->id, state_str, c->participant_count);
}

/*============================================================================
 * 核心函数
 *============================================================================*/

/**
 * 向参与者发送 Prepare 请求
 * 模拟网络通信，返回投票结果
 */
static bool send_prepare(Participant *p)
{
    printf("[协调者] --> [%s]: PREPARE 请求\n", p->name);

    /* 模拟网络延迟 */
    printf("[%s] 收到 PREPARE，正在检查资源...\n", p->name);

    /* 模拟业务逻辑：检查是否可以提交 */
    if (p->can_commit) {
        p->state = PARTICIPANT_READY;
        p->responded = true;
        printf("[%s] --> [协调者]: VOTE_COMMIT\n", p->name);
        return true;
    } else {
        p->state = PARTICIPANT_ABORTED;
        p->responded = true;
        printf("[%s] --> [协调者]: VOTE_ABORT\n", p->name);
        return false;
    }
}

/**
 * 发送 Commit 消息
 */
static void send_commit(Participant *p)
{
    printf("[协调者] --> [%s]: COMMIT 消息\n", p->name);
    if (p->state == PARTICIPANT_READY || p->state == PARTICIPANT_INIT) {
        p->state = PARTICIPANT_COMMITTED;
        printf("[%s] 提交事务，释放资源\n", p->name);
    }
}

/**
 * 发送 Rollback 消息
 */
static void send_rollback(Participant *p)
{
    printf("[协调者] --> [%s]: ROLLBACK 消息\n", p->name);
    p->state = PARTICIPANT_ABORTED;
    printf("[%s] 回滚事务，释放锁\n", p->name);
}

/**
 * 模拟参与者超时
 */
static bool simulate_timeout(Participant *p)
{
    printf("[模拟] [%s] 发生超时！\n", p->name);
    p->state = PARTICIPANT_UNKNOWN;
    p->responded = false;
    return false;
}

/**
 * 参与者恢复过程
 * 从日志中恢复事务状态
 */
static void participant_recovery(Participant *p)
{
    printf("[%s] 启动恢复程序...\n", p->name);

    if (p->log_position > 0) {
        /* 从 Prepare 日志恢复 */
        printf("[%s] 发现 Prepare 日志，状态应为: 就绪\n", p->name);
        p->state = PARTICIPANT_READY;
        p->responded = true;
        printf("[%s] 恢复成功，继续等待协调者决策\n", p->name);
    } else {
        printf("[%s] 无 Prepare 日志，执行回滚\n", p->name);
        p->state = PARTICIPANT_ABORTED;
    }
}

/*============================================================================
 * 测试场景
 *============================================================================*/

/**
 * 场景1：正常提交流程
 */
static void test_normal_commit(void)
{
    print_separator("场景1：正常提交流程");

    Coordinator coord = { .id = 1, .state = TX_INIT, .participant_count = 2 };
    Participant p1 = { .id = 1, .name = "账户服务", .state = PARTICIPANT_INIT, .can_commit = true, .responded = false };
    Participant p2 = { .id = 2, .name = "库存服务", .state = PARTICIPANT_INIT, .can_commit = true, .responded = false };

    coord.participants[0] = &p1;
    coord.participants[1] = &p2;

    printf("准备执行跨服务事务：转帐 100 元\n");
    printf("参与者1: 账户服务（检查余额 >= 100）\n");
    printf("参与者2: 库存服务（检查库存 >= 1）\n\n");

    /* Phase 1: Prepare */
    coord.state = TX_PREPARING;
    printf("========== Phase 1: Prepare ==========\n");
    bool all_vote_commit = true;

    for (int i = 0; i < coord.participant_count; i++) {
        if (!send_prepare(coord.participants[i])) {
            all_vote_commit = false;
        }
    }

    if (all_vote_commit) {
        printf("\n[协调者] 所有参与者投票 Commit，进入 Phase 2\n");
    } else {
        printf("\n[协调者] 收到 ABORT 投票，执行回滚\n");
        coord.state = TX_ABORTED;
        for (int i = 0; i < coord.participant_count; i++) {
            send_rollback(coord.participants[i]);
        }
        return;
    }

    /* Phase 2: Commit */
    printf("\n========== Phase 2: Commit ==========\n");
    coord.state = TX_COMMITTED;
    coord.commit_log = 1;  /* 写入提交日志 */

    for (int i = 0; i < coord.participant_count; i++) {
        send_commit(coord.participants[i]);
    }

    printf("\n[协调者] 事务提交成功！\n");
    print_coordinator_status(&coord);
}

/**
 * 场景2：参与者拒绝
 */
static void test_participant_abort(void)
{
    print_separator("场景2：参与者拒绝（余额不足）");

    Coordinator coord = { .id = 1, .state = TX_INIT, .participant_count = 2 };
    Participant p1 = { .id = 1, .name = "账户服务", .state = PARTICIPANT_INIT, .can_commit = true, .responded = false };
    Participant p2 = { .id = 2, .name = "库存服务", .state = PARTICIPANT_INIT, .can_commit = false, .responded = false }; /* 库存不足 */

    coord.participants[0] = &p1;
    coord.participants[1] = &p2;

    printf("准备执行跨服务事务：转帐 100 元\n");
    printf("参与者1: 账户服务（余额充足）\n");
    printf("参与者2: 库存服务（库存不足！）\n\n");

    /* Phase 1: Prepare */
    coord.state = TX_PREPARING;
    printf("========== Phase 1: Prepare ==========\n");

    for (int i = 0; i < coord.participant_count; i++) {
        send_prepare(coord.participants[i]);
    }

    /* 发现有参与者拒绝，发送全局回滚 */
    printf("\n[协调者] 收到参与者拒绝，发送 GLOBAL_ABORT\n");

    for (int i = 0; i < coord.participant_count; i++) {
        send_rollback(coord.participants[i]);
    }

    printf("\n[协调者] 事务回滚完成\n");
    print_coordinator_status(&coord);
}

/**
 * 场景3：故障恢复
 */
static void test_failure_recovery(void)
{
    print_separator("场景3：故障恢复（参与者超时 + 协调者重启）");

    printf("初始状态：\n");
    printf("- 协调者向参与者发送 PREPARE\n");
    printf("- 参与者1 正常响应 VOTE_COMMIT\n");
    printf("- 参与者2 发生网络超时\n\n");

    Coordinator coord = { .id = 1, .state = TX_PREPARING, .participant_count = 2 };
    Participant p1 = { .id = 1, .name = "账户服务", .state = PARTICIPANT_READY, .responded = true, .log_position = 1 };
    Participant p2 = { .id = 2, .name = "库存服务", .state = PARTICIPANT_UNKNOWN, .responded = false, .log_position = 0 };

    coord.participants[0] = &p1;
    coord.participants[1] = &p2;

    printf("========== 故障发生 ==========\n");
    printf("[模拟] 网络分区，参与者2 无法响应\n");
    simulate_timeout(&p2);  /* 模拟超时 */
    printf("[协调者] 等待超时，决定：回滚？提交？\n\n");

    /* 协调者决策：回滚（保守策略） */
    printf("========== 协调者决策 ==========\n");
    printf("[协调者] 策略：保守回滚\n");
    printf("[协调者] 参与者2 超时，发送 GLOBAL_ABORT\n\n");

    /* 参与者1 执行回滚 */
    send_rollback(&p1);
    printf("\n[注意] 参与者2 不知道协调者的决定！\n\n");

    /* 参与者2 恢复 */
    printf("========== 参与者2 恢复 ==========\n");
    participant_recovery(&p2);

    printf("\n========== 最终状态 ==========\n");
    print_coordinator_status(&coord);
    print_participant_status(&p1);
    print_participant_status(&p2);

    printf("\n[说明] 2PC 的问题：\n");
    printf("- 协调者单点故障时，参与者可能处于不确定状态\n");
    printf("- 解决方案：超时后默认回滚（或等待协调者恢复）\n");
    printf("- 改进协议：3PC（增加 pre-commit 阶段）\n");
}

/*============================================================================
 * 主函数
 *============================================================================*/

int main(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════╗\n");
    printf("║     两阶段提交协议（2PC）演示程序          ║\n");
    printf("╚══════════════════════════════════════════╝\n");
    printf("两阶段提交是一种分布式事务协议，用于确保\n");
    printf("跨多个节点的事务的原子性。\n");

    /* 执行三个测试场景 */
    test_normal_commit();
    test_participant_abort();
    test_failure_recovery();

    print_separator("演示结束");
    printf("通过本演示，您应该理解：\n");
    printf("1. Prepare 阶段：协调者询问所有参与者的投票\n");
    printf("2. Commit 阶段：所有投票为 Commit 时才提交\n");
    printf("3. 故障恢复：参与者在不确定状态下的行为\n");
    printf("4. 2PC 的局限性：协调者单点、阻塞问题\n");

    return 0;
}
