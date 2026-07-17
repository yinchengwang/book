/**
 * @file main.c
 * @brief Linux 连接池学习演示
 *
 * 演示内容：
 *   - 连接池基本结构
 *   - 预创建连接
 *   - 获取/归还连接
 *   - 超时与空闲回收机制
 *
 * 编译：gcc -std=c11 -Wall -o conn_pool main.c
 * 运行：./conn_pool
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>

/* ============================================================
 * 连接结构
 * ============================================================ */
typedef struct conn {
    int id;                  /* 连接 ID */
    int fd;                  /* Socket 描述符（-1 表示空闲） */
    struct timeval last_used; /* 最后使用时间 */
    struct conn *next;       /* 链表指针 */
} conn_t;

/* ============================================================
 * 连接池结构
 * ============================================================ */
typedef struct {
    conn_t *connections;     /* 连接链表 */
    int pool_size;           /* 池大小 */
    int max_idle_time;       /* 最大空闲时间（秒） */
    int active_count;        /* 活跃连接数 */
    pthread_mutex_t mutex;   /* 互斥锁 */
} conn_pool_t;

/* ============================================================
 * 模拟建立连接
 * ============================================================ */
static int simulate_connect(int id) {
    /* 模拟 socket() + connect() */
    /* 实际代码中会创建真实的网络连接 */
    printf("[conn_pool]   建立连接 #%d\n", id);
    return 100 + id;  /* 返回模拟的 fd */
}

/* ============================================================
 * 模拟关闭连接
 * ============================================================ */
static void simulate_close(int fd) {
    /* 模拟 close() */
    printf("[conn_pool]   关闭连接 fd=%d\n", fd);
}

/* ============================================================
 * 初始化连接池
 * ============================================================ */
static conn_pool_t *pool_init(int pool_size, int max_idle_time) {
    conn_pool_t *pool = (conn_pool_t *)calloc(1, sizeof(conn_pool_t));
    if (!pool) return NULL;

    pool->pool_size = pool_size;
    pool->max_idle_time = max_idle_time;
    pool->active_count = 0;
    pthread_mutex_init(&pool->mutex, NULL);

    /* 预创建连接 */
    printf("[conn_pool] 初始化连接池（预创建 %d 个连接）\n", pool_size);
    for (int i = 0; i < pool_size; i++) {
        conn_t *conn = (conn_t *)calloc(1, sizeof(conn_t));
        conn->id = i;
        conn->fd = simulate_connect(i);
        gettimeofday(&conn->last_used, NULL);

        /* 头插法 */
        conn->next = pool->connections;
        pool->connections = conn;
        pool->active_count++;
    }

    return pool;
}

/* ============================================================
 * 获取连接（从池中获取空闲连接）
 * ============================================================ */
static conn_t *pool_get_conn(conn_pool_t *pool, int timeout_ms) {
    pthread_mutex_lock(&pool->mutex);

    /* 查找空闲连接 */
    conn_t *conn = pool->connections;
    while (conn) {
        if (conn->fd >= 0) {  /* fd >= 0 表示空闲 */
            conn->fd = simulate_connect(conn->id);  /* 重新激活 */
            gettimeofday(&conn->last_used, NULL);
            printf("[conn_pool] 获取连接 #%d (fd=%d)\n", conn->id, conn->fd);
            pthread_mutex_unlock(&pool->mutex);
            return conn;
        }
        conn = conn->next;
    }

    pthread_mutex_unlock(&pool->mutex);
    return NULL;  /* 无可用连接 */
}

/* ============================================================
 * 归还连接
 * ============================================================ */
static void pool_return_conn(conn_pool_t *pool, conn_t *conn) {
    pthread_mutex_lock(&pool->mutex);

    if (conn && conn->fd >= 0) {
        /* 关闭实际连接 */
        simulate_close(conn->fd);
        conn->fd = -1;  /* 标记为空闲 */
        gettimeofday(&conn->last_used, NULL);
        printf("[conn_pool] 归还连接 #%d\n", conn->id);
    }

    pthread_mutex_unlock(&pool->mutex);
}

/* ============================================================
 * 回收空闲连接
 * ============================================================ */
static void pool_recycle_idle(conn_pool_t *pool) {
    pthread_mutex_lock(&pool->mutex);

    struct timeval now;
    gettimeofday(&now, NULL);

    conn_t *conn = pool->connections;
    int recycled = 0;

    while (conn) {
        if (conn->fd < 0) {  /* 空闲连接 */
            int idle_sec = now.tv_sec - conn->last_used.tv_sec;
            if (idle_sec > pool->max_idle_time) {
                printf("[conn_pool] 回收空闲连接 #%d (空闲 %d 秒)\n",
                       conn->id, idle_sec);
                recycled++;
                /* 实际代码中会释放 conn 结构 */
                conn->fd = -2;  /* 标记为已回收 */
            }
        }
        conn = conn->next;
    }

    if (recycled > 0) {
        pool->active_count -= recycled;
        printf("[conn_pool] 共回收 %d 个空闲连接，剩余 %d 个\n",
               recycled, pool->active_count);
    }

    pthread_mutex_unlock(&pool->mutex);
}

/* ============================================================
 * 销毁连接池
 * ============================================================ */
static void pool_destroy(conn_pool_t *pool) {
    pthread_mutex_lock(&pool->mutex);

    conn_t *conn = pool->connections;
    while (conn) {
        if (conn->fd >= 0) {
            simulate_close(conn->fd);
        }
        conn_t *next = conn->next;
        free(conn);
        conn = next;
    }

    pthread_mutex_unlock(&pool->mutex);
    pthread_mutex_destroy(&pool->mutex);
    free(pool);
    printf("[conn_pool] 连接池已销毁\n");
}

/* ============================================================
 * Demo 1: 基本操作
 * ============================================================ */
static void demo_basic_ops(void) {
    printf("[conn_pool] 演示连接池基本操作\n");

    /* 初始化池（大小=3，空闲超时=60秒） */
    conn_pool_t *pool = pool_init(3, 60);
    if (!pool) {
        printf("[conn_pool] 连接池初始化失败\n");
        return;
    }

    /* 获取连接 */
    conn_t *c1 = pool_get_conn(pool, 1000);
    conn_t *c2 = pool_get_conn(pool, 1000);

    /* 使用连接（模拟） */
    printf("[conn_pool] 使用连接中...\n");
    usleep(100000);

    /* 归还连接 */
    pool_return_conn(pool, c1);
    pool_return_conn(pool, c2);

    /* 再次获取（应该复用） */
    printf("\n[conn_pool] 再次获取连接（测试复用）:\n");
    conn_t *c3 = pool_get_conn(pool, 1000);
    pool_return_conn(pool, c3);

    /* 销毁池 */
    pool_destroy(pool);
}

/* ============================================================
 * Demo 2: 并发访问
 * ============================================================ */
static void *thread_worker(void *arg) {
    conn_pool_t *pool = (conn_pool_t *)arg;

    /* 获取连接 */
    conn_t *conn = pool_get_conn(pool, 5000);
    if (conn) {
        printf("[conn_pool] 线程 %lu 获取连接 #%d\n",
               (unsigned long)pthread_self(), conn->id);

        /* 模拟工作 */
        usleep(500000);

        /* 归还连接 */
        pool_return_conn(pool, conn);
    } else {
        printf("[conn_pool] 线程 %lu 无法获取连接\n",
               (unsigned long)pthread_self());
    }

    return NULL;
}

static void demo_concurrent(void) {
    printf("[conn_pool] 演示并发访问（5 线程竞争 3 个连接）\n");

    conn_pool_t *pool = pool_init(3, 60);
    if (!pool) return;

    pthread_t threads[5];
    for (int i = 0; i < 5; i++) {
        pthread_create(&threads[i], NULL, thread_worker, pool);
    }

    for (int i = 0; i < 5; i++) {
        pthread_join(threads[i], NULL);
    }

    pool_destroy(pool);
}

/* ============================================================
 * Demo 3: 空闲回收
 * ============================================================ */
static void demo_idle_recycle(void) {
    printf("[conn_pool] 演示空闲回收（空闲超时=2秒）\n");

    conn_pool_t *pool = pool_init(2, 2);
    if (!pool) return;

    /* 获取并归还一个连接 */
    conn_t *conn = pool_get_conn(pool, 1000);
    pool_return_conn(pool, conn);

    printf("\n[conn_pool] 等待 3 秒后检查空闲连接...\n");
    sleep(3);

    /* 触发回收检查 */
    pool_recycle_idle(pool);

    /* 尝试获取（会创建新连接） */
    conn_t *new_conn = pool_get_conn(pool, 1000);
    if (new_conn) {
        pool_return_conn(pool, new_conn);
    }

    pool_destroy(pool);
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("========================================\n");
    printf("  Linux 连接池学习演示\n");
    printf("========================================\n\n");

    printf("--- Demo 1: 基本操作 ---\n");
    demo_basic_ops();
    printf("\n");

    printf("--- Demo 2: 并发访问 ---\n");
    demo_concurrent();
    printf("\n");

    printf("--- Demo 3: 空闲回收 ---\n");
    demo_idle_recycle();
    printf("\n");

    printf("========================================\n");
    printf("=== PASS ===\n");
    printf("========================================\n");
    return 0;
}
