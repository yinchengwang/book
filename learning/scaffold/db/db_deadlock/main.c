/**
 * 死锁处理学习演示
 *
 * 演示内容：
 * 1. 等待图（Wait-For Graph）数据结构
 * 2. 死锁检测算法（环检测）
 * 3. 超时机制
 * 4. 死锁解决策略
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* -------------------- 常量定义 -------------------- */
#define MAX_TRANSACTIONS 8       // 最大事务数
#define MAX_RESOURCES    4       // 最大资源数
#define EDGE_CAPACITY    16     // 等待图边容量

/* -------------------- 数据结构 -------------------- */

// 事务状态
typedef enum {
    TX_RUNNING = 0,    // 运行中
    TX_WAITING,        // 等待资源
    TX_COMMITTED,      // 已提交
    TX_ABORTED         // 已回滚
} TxState;

// 资源类型
typedef enum {
    RES_SHARED = 0,    // 共享资源
    RES_EXCLUSIVE      // 排他资源
} ResourceType;

// 等待图中的边：表示事务 T1 等待事务 T2 释放资源
typedef struct {
    int from_tx;       // 等待者事务 ID
    int to_tx;         // 持有者事务 ID
    int resource_id;   // 争抢的资源 ID
} WaitEdge;

// 事务描述符
typedef struct {
    int         id;            // 事务 ID
    TxState     state;         // 事务状态
    int         hold_resource; // 持有的资源（-1 表示无）
    int         wait_resource; // 等待的资源（-1 表示不等待）
    int         start_time;    // 开始时间（模拟）
    int         timeout;       // 超时阈值
} Transaction;

// 等待图
typedef struct {
    WaitEdge edges[EDGE_CAPACITY];
    int      edge_count;
} WaitForGraph;

/* -------------------- 全局变量 -------------------- */
static Transaction transactions[MAX_TRANSACTIONS];
static int resource_owners[MAX_RESOURCES];      // 资源 -> 占有者事务ID（-1 表示空闲）
static WaitForGraph wfg;
static int current_time = 0;

/* -------------------- 函数声明 -------------------- */
static void init_system(void);
static void init_transaction(int id, int hold_res, int timeout_val);
static void init_resource(int res_id);
static void wfg_add_edge(int from_tx, int to_tx, int resource_id);
static void wfg_remove_edge(int from_tx, int to_tx);
static void wfg_clear(void);
static void display_wait_graph(void);
static bool detect_deadlock(int *deadlock_cycle, int *cycle_len);
static bool has_deadlock(void);
static void resolve_deadlock_by_victim(int victim_tx);
static void resolve_deadlock_by_timeout(int tx_id);
static void simulate_step(const char *description);
static void print_tx_status(void);

/* ==================== 核心实现 ==================== */

/**
 * 初始化系统
 */
static void init_system(void) {
    memset(transactions, 0, sizeof(transactions));
    memset(resource_owners, -1, sizeof(resource_owners));
    memset(&wfg, 0, sizeof(wfg));
    current_time = 0;

    // 初始化资源：资源0和1是排他的，2和3是共享的
    for (int i = 0; i < MAX_RESOURCES; i++) {
        resource_owners[i] = -1;
    }
}

/**
 * 初始化事务
 * @param id         事务ID
 * @param hold_res   持有的资源ID（-1 表示不持有）
 * @param timeout_val 超时阈值
 */
static void init_transaction(int id, int hold_res, int timeout_val) {
    transactions[id].id = id;
    transactions[id].state = TX_RUNNING;
    transactions[id].hold_resource = hold_res;
    transactions[id].wait_resource = -1;
    transactions[id].start_time = current_time;
    transactions[id].timeout = timeout_val;

    if (hold_res >= 0) {
        resource_owners[hold_res] = id;  // 占用资源
    }
}

/**
 * 初始化资源
 */
static void init_resource(int res_id) {
    if (res_id >= 0 && res_id < MAX_RESOURCES) {
        resource_owners[res_id] = -1;  // 释放资源
    }
}

/**
 * 向等待图添加边：T1 等待 T2 释放 resource_id
 */
static void wfg_add_edge(int from_tx, int to_tx, int resource_id) {
    if (wfg.edge_count < EDGE_CAPACITY) {
        wfg.edges[wfg.edge_count].from_tx = from_tx;
        wfg.edges[wfg.edge_count].to_tx = to_tx;
        wfg.edges[wfg.edge_count].resource_id = resource_id;
        wfg.edge_count++;
    }
}

/**
 * 从等待图移除边
 */
static void wfg_remove_edge(int from_tx, int to_tx) {
    for (int i = 0; i < wfg.edge_count; i++) {
        if (wfg.edges[i].from_tx == from_tx && wfg.edges[i].to_tx == to_tx) {
            // 移动后面的边覆盖
            for (int j = i; j < wfg.edge_count - 1; j++) {
                wfg.edges[j] = wfg.edges[j + 1];
            }
            wfg.edge_count--;
            break;
        }
    }
}

/**
 * 清空等待图
 */
static void wfg_clear(void) {
    wfg.edge_count = 0;
}

/**
 * 显示等待图
 */
static void display_wait_graph(void) {
    printf("    [等待图状态]\n");
    printf("    资源占用: ");
    for (int i = 0; i < MAX_RESOURCES; i++) {
        if (resource_owners[i] >= 0) {
            printf("R%d->T%d ", i, resource_owners[i]);
        } else {
            printf("R%d->- ", i);
        }
    }
    printf("\n");
    printf("    等待关系: ");
    if (wfg.edge_count == 0) {
        printf("(无等待)\n");
    } else {
        for (int i = 0; i < wfg.edge_count; i++) {
            printf("T%d --[R%d]--> T%d  ",
                   wfg.edges[i].from_tx,
                   wfg.edges[i].resource_id,
                   wfg.edges[i].to_tx);
        }
        printf("\n");
    }
}

/**
 * DFS 检测环：深度优先搜索查找从 start_tx 出发的环
 * visited[] 记录当前 DFS 路径上的节点
 * path[]    记录路径
 */
static bool dfs_detect_cycle(int start_tx, int current_tx,
                              bool visited[], int path[], int path_len,
                             int deadlock_cycle[]) {
    visited[current_tx] = true;
    path[path_len] = current_tx;
    path_len++;

    // 遍历所有从 current_tx 出发的边
    for (int i = 0; i < wfg.edge_count; i++) {
        if (wfg.edges[i].from_tx == current_tx) {
            int next_tx = wfg.edges[i].to_tx;

            // 发现环：回到了起始节点
            if (next_tx == start_tx && path_len >= 2) {
                // 记录环
                for (int j = 0; j < path_len; j++) {
                    deadlock_cycle[j] = path[j];
                }
                deadlock_cycle[path_len] = start_tx;  // 闭环
                return true;
            }

            // 继续 DFS
            if (!visited[next_tx]) {
                if (dfs_detect_cycle(start_tx, next_tx, visited, path, path_len, deadlock_cycle)) {
                    return true;
                }
            }
        }
    }

    return false;
}

/**
 * 检测死锁：尝试从每个节点出发 DFS 查找环
 * @param deadlock_cycle 输出死锁环
 * @param cycle_len      输出环长度
 * @return 是否存在死锁
 */
static bool detect_deadlock(int *deadlock_cycle, int *cycle_len) {
    bool visited[MAX_TRANSACTIONS];
    int path[MAX_TRANSACTIONS];

    // 清空输出
    *cycle_len = 0;

    // 从每个可能是死锁源的节点出发
    for (int i = 0; i < MAX_TRANSACTIONS; i++) {
        if (transactions[i].state != TX_RUNNING && transactions[i].state != TX_WAITING) {
            continue;  // 跳过已结束的事务
        }

        memset(visited, 0, sizeof(visited));
        memset(path, -1, sizeof(path));

        if (dfs_detect_cycle(i, i, visited, path, 0, deadlock_cycle)) {
            // 找到了环
            *cycle_len = 0;
            for (int j = 0; j < MAX_TRANSACTIONS; j++) {
                if (deadlock_cycle[j] == -1) break;
                (*cycle_len)++;
            }
            return true;
        }
    }

    return false;
}

/**
 * 简化检测：检查是否存在死锁
 */
static bool has_deadlock(void) {
    int cycle[MAX_TRANSACTIONS];
    int cycle_len = 0;
    return detect_deadlock(cycle, &cycle_len);
}

/**
 * 通过选择牺牲者解决死锁
 * 策略：选择等待时间最长或持有资源最少的事务作为牺牲者
 */
static void resolve_deadlock_by_victim(int victim_tx) {
    printf("\n>>> [死锁解决] 选择 T%d 作为牺牲者进行回滚\n", victim_tx);

    // 释放事务持有的资源
    int hold_res = transactions[victim_tx].hold_resource;
    if (hold_res >= 0) {
        printf("    释放 T%d 持有的资源 R%d\n", victim_tx, hold_res);
        init_resource(hold_res);
    }

    // 从等待图中移除与该事务相关的所有边
    wfg_remove_edge(victim_tx, -1);  // 清理占位

    // 标记事务为已回滚
    transactions[victim_tx].state = TX_ABORTED;
    transactions[victim_tx].hold_resource = -1;

    // 重建等待图
    wfg_clear();
    for (int i = 0; i < MAX_TRANSACTIONS; i++) {
        if (transactions[i].state == TX_WAITING &&
            transactions[i].wait_resource >= 0 &&
            resource_owners[transactions[i].wait_resource] >= 0) {
            wfg_add_edge(i,
                        resource_owners[transactions[i].wait_resource],
                        transactions[i].wait_resource);
        }
    }

    printf(">>> 死锁已解决，等待图已更新\n");
}

/**
 * 通过超时机制解决死锁
 */
static void resolve_deadlock_by_timeout(int tx_id) {
    int elapsed = current_time - transactions[tx_id].start_time;
    if (elapsed >= transactions[tx_id].timeout) {
        printf("\n>>> [超时处理] T%d 等待时间 %d >= 超时阈值 %d，回滚事务\n",
               tx_id, elapsed, transactions[tx_id].timeout);

        // 释放资源
        int hold_res = transactions[tx_id].hold_resource;
        if (hold_res >= 0) {
            printf("    释放 T%d 持有的资源 R%d\n", tx_id, hold_res);
            init_resource(hold_res);
        }

        transactions[tx_id].state = TX_ABORTED;
        transactions[tx_id].hold_resource = -1;
        transactions[tx_id].wait_resource = -1;
    }
}

/**
 * 模拟一步：推进时间，显示状态
 */
static void simulate_step(const char *description) {
    current_time++;
    printf("\n========== [Step %d] %s ==========\n", current_time, description);
    print_tx_status();
    display_wait_graph();
}

/**
 * 打印事务状态
 */
static void print_tx_status(void) {
    printf("    [事务状态]\n");
    const char *state_name[] = {"RUNNING", "WAITING", "COMMITTED", "ABORTED"};

    for (int i = 0; i < MAX_TRANSACTIONS; i++) {
        if (transactions[i].id == i && transactions[i].state != 0) {
            printf("    T%d: %s", i, state_name[transactions[i].state]);
            if (transactions[i].hold_resource >= 0) {
                printf(" | 持有 R%d", transactions[i].hold_resource);
            }
            if (transactions[i].wait_resource >= 0) {
                printf(" | 等待 R%d", transactions[i].wait_resource);
            }
            int elapsed = current_time - transactions[i].start_time;
            printf(" | 已等待 %d", elapsed);
            if (transactions[i].timeout > 0) {
                printf("/%d", transactions[i].timeout);
            }
            printf("\n");
        }
    }
}

/* ==================== 场景演示 ==================== */

int main(void) {
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║          数据库死锁处理学习演示                              ║\n");
    printf("║  演示内容：等待图、死锁检测、超时机制、回滚策略              ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");

    /* ========== 场景 1: 死锁环构建与检测 ========== */
    printf("【场景 1】构建死锁环并检测\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

    init_system();

    // 初始状态：T0 持有 R0，等待 R1
    //          T1 持有 R1，等待 R0
    // 形成死锁环：T0 -> T1 -> T0

    init_transaction(0, 0, 10);  // T0 持有 R0，超时 10
    transactions[0].state = TX_WAITING;
    transactions[0].wait_resource = 1;

    init_transaction(1, 1, 10);  // T1 持有 R1，超时 10
    transactions[1].state = TX_WAITING;
    transactions[1].wait_resource = 0;

    // 构建等待图
    wfg_add_edge(0, 1, 1);  // T0 等待 T1 释放 R1
    wfg_add_edge(1, 0, 0);  // T1 等待 T0 释放 R0

    simulate_step("死锁环已形成");

    // 检测死锁
    int cycle[MAX_TRANSACTIONS];
    int cycle_len = 0;
    if (detect_deadlock(cycle, &cycle_len)) {
        printf("\n>>> [死锁检测] 检测到死锁！环: ");
        for (int i = 0; i < cycle_len; i++) {
            printf("T%d -> ", cycle[i]);
        }
        printf("T%d\n", cycle[0]);
    }

    /* ========== 场景 2: 死锁解决 ========== */
    printf("\n【场景 2】死锁解决（选择牺牲者回滚）\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

    resolve_deadlock_by_victim(0);  // 选择 T0 作为牺牲者

    simulate_step("T0 已回滚，死锁解除");

    // 再次检测，确认无死锁
    if (!has_deadlock()) {
        printf("\n>>> [验证] 死锁已完全解除，无等待环\n");
    }

    /* ========== 场景 3: 超时机制 ========== */
    printf("\n【场景 3】超时机制演示\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

    init_system();

    // 设置短超时的事务
    init_transaction(2, 2, 3);   // T2 持有 R2，超时 3
    transactions[2].state = TX_WAITING;
    transactions[2].wait_resource = 0;

    init_transaction(3, 0, 8);  // T3 持有 R0，超时 8
    transactions[3].state = TX_WAITING;
    transactions[3].wait_resource = 2;

    wfg_add_edge(2, 3, 0);  // T2 等待 T3 释放 R0
    wfg_add_edge(3, 2, 2);  // T3 等待 T2 释放 R2

    simulate_step("T2 和 T3 形成死锁，T2 超时=3，T3 超时=8");

    // 推进时间到超时阈值
    current_time = 3;
    printf("\n--- 推进到时间 %d ---\n", current_time);
    resolve_deadlock_by_timeout(2);  // T2 超时

    simulate_step("T2 因超时被回滚");

    /* ========== 场景 4: 更复杂的死锁 ========== */
    printf("\n【场景 4】多事务死锁环（T0->T1->T2->T0）\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

    init_system();

    // T0 持有 R0，等待 R1
    init_transaction(0, 0, 10);
    transactions[0].state = TX_WAITING;
    transactions[0].wait_resource = 1;

    // T1 持有 R1，等待 R2
    init_transaction(1, 1, 10);
    transactions[1].state = TX_WAITING;
    transactions[1].wait_resource = 2;

    // T2 持有 R2，等待 R0
    init_transaction(2, 2, 10);
    transactions[2].state = TX_WAITING;
    transactions[2].wait_resource = 0;

    wfg_add_edge(0, 1, 1);  // T0 等待 T1
    wfg_add_edge(1, 2, 2);  // T1 等待 T2
    wfg_add_edge(2, 0, 0);  // T2 等待 T0

    simulate_step("三角死锁环已形成");

    // 检测死锁
    if (detect_deadlock(cycle, &cycle_len)) {
        printf("\n>>> [死锁检测] 检测到 %d-环死锁: ", cycle_len);
        for (int i = 0; i < cycle_len; i++) {
            printf("T%d", cycle[i]);
            if (i < cycle_len - 1) printf(" -> ");
        }
        printf("\n");
    }

    // 选择一个牺牲者解决
    resolve_deadlock_by_victim(1);  // 选择 T1

    simulate_step("T1 回滚后，死锁解除");

    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║  总结：死锁处理策略                                          ║\n");
    printf("║  1. 等待图检测：构建事务间等待关系，检测环的存在             ║\n");
    printf("║  2. 超时机制：简单但可能误伤正常事务                         ║\n");
    printf("║  3. 牺牲者选择：按等待时间/持有资源数等策略选择回滚对象       ║\n");
    printf("║  4. 预防策略：按序申请资源、一次性申请、层次化锁定           ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");

    return 0;
}
