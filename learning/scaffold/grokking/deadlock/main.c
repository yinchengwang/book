/**
 * @file deadlock/main.c
 * @brief 死锁演示：哲学家就餐问题、死锁四条件、资源排序与等待图检测
 *
 * 演示内容:
 * 1. dine_deadlock: 经典哲学家就餐——先拿左叉再拿右叉，触发死锁
 * 2. dine_safe: 资源排序——先拿编号小的叉子，打破循环等待
 * 3. detect_cycle: 等待图（Wait-For Graph）DFS 环检测
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>

/* ============================================================================
 * 哲学家就餐数据结构
 * ============================================================================ */

#define NUM_PHILOSOPHERS 5

/** 哲学家状态 */
typedef enum {
    THINKING,
    HUNGRY,
    EATING
} PhilosopherState;

/** 哲学家控制块 */
typedef struct {
    int id;
    pthread_mutex_t *left_fork;
    pthread_mutex_t *right_fork;
    PhilosopherState state;
    int eat_count;
} Philosopher;

static Philosopher philosophers[NUM_PHILOSOPHERS];
static pthread_mutex_t forks[NUM_PHILOSOPHERS];

/* ============================================================================
 * 1. 死锁版本：每位哲学家先拿左叉再拿右叉
 * ============================================================================ */

static void *dine_deadlock(void *arg) {
    Philosopher *p = (Philosopher *)arg;

    for (int i = 0; i < 3; i++) {
        /* 思考 */
        printf("  哲学家%d: 思考中...\n", p->id);
        usleep((rand() % 500 + 100) * 1000);

        /* 拿起左叉 */
        printf("  哲学家%d: 拿起左叉 (叉子%d)\n", p->id, p->id);
        pthread_mutex_lock(p->left_fork);
        p->state = HUNGRY;

        /* !!! 关键：等待确保所有哲学家都拿到左叉 !!! */
        sleep(1);

        /* 尝试拿起右叉 */
        printf("  哲学家%d: 尝试拿起右叉 (叉子%d)...\n",
               p->id, (p->id + 1) % NUM_PHILOSOPHERS);
        pthread_mutex_lock(p->right_fork);

        /* 吃饭 */
        p->state = EATING;
        printf("  哲学家%d: 开始吃饭 (第%d次)\n", p->id, i + 1);
        usleep((rand() % 300 + 100) * 1000);
        p->eat_count++;

        /* 放下叉子 */
        pthread_mutex_unlock(p->right_fork);
        pthread_mutex_unlock(p->left_fork);
        p->state = THINKING;
        printf("  哲学家%d: 放下叉子\n", p->id);
    }

    return NULL;
}

/* ============================================================================
 * 2. 安全版本：资源排序——先拿编号小的叉子
 * ============================================================================ */

static void *dine_safe(void *arg) {
    Philosopher *p = (Philosopher *)arg;

    for (int i = 0; i < 3; i++) {
        printf("  哲学家%d: 思考中...\n", p->id);
        usleep((rand() % 500 + 100) * 1000);

        /*
         * 资源排序（Resource Ordering）：
         * 始终先拿编号较小的叉子，后拿编号较大的叉子。
         * 这打破了循环等待条件，从而避免死锁。
         */
        pthread_mutex_t *first  = p->left_fork;
        pthread_mutex_t *second = p->right_fork;
        if (p->left_fork > p->right_fork) {
            first  = p->right_fork;
            second = p->left_fork;
        }

        int first_id  = (first  == p->left_fork) ? p->id : (p->id + 1) % NUM_PHILOSOPHERS;
        int second_id = (second == p->right_fork) ? (p->id + 1) % NUM_PHILOSOPHERS : p->id;

        printf("  哲学家%d: 拿起叉子%d (较小编号优先)\n", p->id, first_id);
        pthread_mutex_lock(first);

        printf("  哲学家%d: 拿起叉子%d\n", p->id, second_id);
        pthread_mutex_lock(second);

        printf("  哲学家%d: 开始吃饭 (第%d次)\n", p->id, i + 1);
        usleep((rand() % 300 + 100) * 1000);
        p->eat_count++;

        pthread_mutex_unlock(second);
        pthread_mutex_unlock(first);
        printf("  哲学家%d: 放下叉子\n", p->id);
    }

    return NULL;
}

/* ============================================================================
 * 3. 等待图（Wait-For Graph）环检测
 * ============================================================================ */

/**
 * @brief DFS 检测环
 *
 * @param graph     等待图邻接矩阵 graph[u][v] = true 表示 u 在等 v
 * @param n         节点数
 * @param visited   访问标记
 * @param rec_stack 递归栈标记（当前 DFS 路径上的节点）
 * @param v         当前节点
 * @return  true=检测到环
 */
static bool dfs_cycle(bool graph[NUM_PHILOSOPHERS][NUM_PHILOSOPHERS],
                      int n, bool visited[], bool rec_stack[], int v) {
    if (!visited[v]) {
        visited[v] = true;
        rec_stack[v] = true;

        for (int u = 0; u < n; u++) {
            if (graph[v][u]) {
                if (!visited[u] && dfs_cycle(graph, n, visited, rec_stack, u))
                    return true;
                else if (rec_stack[u])
                    return true;
            }
        }
    }
    rec_stack[v] = false;
    return false;
}

/**
 * @brief 检测等待图中是否存在环
 * @param graph  邻接矩阵
 * @param n      节点数
 * @return  true=存在死锁
 */
static bool detect_cycle(bool graph[NUM_PHILOSOPHERS][NUM_PHILOSOPHERS], int n) {
    bool visited[NUM_PHILOSOPHERS]   = {false};
    bool rec_stack[NUM_PHILOSOPHERS] = {false};

    for (int i = 0; i < n; i++) {
        if (dfs_cycle(graph, n, visited, rec_stack, i))
            return true;
    }
    return false;
}

/**
 * @brief 根据当前哲学家状态构建等待图
 *
 * 每个 HUNGRY 状态的哲学家持有左叉，正在等待右叉，
 * 在图中添加一条指向右叉持有者的边。
 */
static void build_wait_for_graph(bool graph[NUM_PHILOSOPHERS][NUM_PHILOSOPHERS]) {
    for (int i = 0; i < NUM_PHILOSOPHERS; i++)
        for (int j = 0; j < NUM_PHILOSOPHERS; j++)
            graph[i][j] = false;

    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        if (philosophers[i].state == HUNGRY) {
            int neighbor = (i + 1) % NUM_PHILOSOPHERS;
            graph[i][neighbor] = true;  /* i 在等待 neighbor 的叉子 */
        }
    }
}

/** 打印等待图邻接矩阵 */
static void print_wait_for_graph(bool graph[NUM_PHILOSOPHERS][NUM_PHILOSOPHERS]) {
    printf("  等待图 (邻接矩阵):\n");
    printf("      ");
    for (int j = 0; j < NUM_PHILOSOPHERS; j++)
        printf(" P%d ", j);
    printf("\n");
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        printf("   P%d: ", i);
        for (int j = 0; j < NUM_PHILOSOPHERS; j++)
            printf("  %d ", graph[i][j] ? 1 : 0);
        printf("\n");
    }
}

/* ============================================================================
 * 主函数
 * ============================================================================ */

int main(void)
{
    printf("=== 死锁 —— 哲学家就餐、资源排序与等待图检测 ===\n\n");

    /* ---------------------------------------------------------------
     * 初始化叉子（互斥锁） 和哲学家
     * --------------------------------------------------------------- */
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        pthread_mutex_init(&forks[i], NULL);
    }

    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        philosophers[i].id         = i;
        philosophers[i].left_fork  = &forks[i];
        philosophers[i].right_fork = &forks[(i + 1) % NUM_PHILOSOPHERS];
        philosophers[i].state      = THINKING;
        philosophers[i].eat_count  = 0;
    }

    /* ===============================================================
     * 演示 1: 死锁版本 —— 哲学家就餐
     * =============================================================== */
    printf("--- 1. 死锁版本: 先左后右（触发死锁） ---\n");
    printf("  每位哲学家先拿左叉再拿右叉，导致循环等待\n");
    printf("  -> 死锁四条件: 互斥 + 持有并等待 + 不可剥夺 + 循环等待\n\n");

    pthread_t deadlock_threads[NUM_PHILOSOPHERS];
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        pthread_create(&deadlock_threads[i], NULL,
                       dine_deadlock, &philosophers[i]);
    }

    /*
     * 用 sleep + detach 检测死锁（兼容 POSIX，不依赖 timedjoin_np）。
     * 500ms x 4 轮 = 2s，足够让所有哲学家拿到左叉后陷入死锁。
     */
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        pthread_detach(deadlock_threads[i]);
    }

    struct timespec ts_500ms = {0, 500000000L};
    nanosleep(&ts_500ms, NULL);
    nanosleep(&ts_500ms, NULL);
    nanosleep(&ts_500ms, NULL);
    nanosleep(&ts_500ms, NULL);

    /* 构建等待图并检测死锁 */
    bool wfg[NUM_PHILOSOPHERS][NUM_PHILOSOPHERS];
    build_wait_for_graph(wfg);
    bool deadlocked = detect_cycle(wfg, NUM_PHILOSOPHERS);

    printf("\n  等待图环检测: %s\n",
           deadlocked ? "检测到死锁!" : "未检测到死锁");

    if (deadlocked) {
        print_wait_for_graph(wfg);
        printf("  环路径: ");
        for (int i = 0; i < NUM_PHILOSOPHERS; i++)
            for (int j = 0; j < NUM_PHILOSOPHERS; j++)
                if (wfg[i][j])
                    printf("P%d->P%d ", i, j);
        printf("(形成循环等待)\n");
    }

    printf("\n  (主线程超时继续, 死锁线程仍在等待)\n");

    /* 重置锁状态 */
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        pthread_mutex_destroy(&forks[i]);
        pthread_mutex_init(&forks[i], NULL);
        philosophers[i].state     = THINKING;
        philosophers[i].eat_count = 0;
    }
    printf("\n");

    /* ===============================================================
     * 演示 2: 安全版本 —— 资源排序
     * =============================================================== */
    printf("--- 2. 安全版本: 资源排序（先拿编号小的叉子） ---\n");
    printf("  始终先拿编号小的叉子, 打破循环等待条件\n\n");

    pthread_t safe_threads[NUM_PHILOSOPHERS];
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        pthread_create(&safe_threads[i], NULL,
                       dine_safe, &philosophers[i]);
    }

    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        pthread_join(safe_threads[i], NULL);
    }

    printf("\n  所有哲学家完成就餐! 资源排序成功避免死锁\n");

    /* ===============================================================
     * 演示 3: 等待图检测单独演示
     * =============================================================== */
    printf("\n--- 3. 等待图环检测演示 ---\n");

    /* 构造有环图: 0->1->2->0 */
    bool test_graph[NUM_PHILOSOPHERS][NUM_PHILOSOPHERS] = {false};
    test_graph[0][1] = true;
    test_graph[1][2] = true;
    test_graph[2][0] = true;

    printf("  构造图: P0->P1->P2->P0\n");
    print_wait_for_graph(test_graph);
    printf("  检测结果: %s\n\n",
           detect_cycle(test_graph, NUM_PHILOSOPHERS)
               ? "检测到死锁" : "无死锁");

    /* 构造无环图: 0->1->2->3 */
    bool test_graph2[NUM_PHILOSOPHERS][NUM_PHILOSOPHERS] = {false};
    test_graph2[0][1] = true;
    test_graph2[1][2] = true;
    test_graph2[2][3] = true;

    printf("  构造图: P0->P1->P2->P3\n");
    print_wait_for_graph(test_graph2);
    printf("  检测结果: %s\n",
           detect_cycle(test_graph2, NUM_PHILOSOPHERS)
               ? "检测到死锁" : "无死锁");

    /* ---------------------------------------------------------------
     * 清理
     * --------------------------------------------------------------- */
    for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
        pthread_mutex_destroy(&forks[i]);
    }

    printf("\n=== 演示完成 ===\n");
    return 0;
}
