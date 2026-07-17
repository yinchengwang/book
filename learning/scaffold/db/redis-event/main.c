/**
 * @file main.c
 * @brief Redis 事件循环与 Reactor 模式演示
 *
 * 演示 Redis 的 Reactor 模式、I/O 多路复用（select/poll/epoll）、
 * 定时器管理（最小堆 + 时间轮）以及事件分发机制。
 *
 * @see engineering/src/redis/event.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

/* ============================================================
 * 模拟数据类型
 * ============================================================ */

/** 文件描述符事件类型 */
typedef enum {
    AE_NONE      = 0,
    AE_READABLE  = 1 << 0,
    AE_WRITABLE  = 1 << 1,
    AE_BARRIER   = 1 << 2
} AE_File_Flags;

/** 事件处理器 */
typedef void (*FileProc)(int fd, void *clientData);
typedef void (*TimeProc)(void *clientData);

/** 定时器节点 */
typedef struct AE_TimerNode {
    int64_t         id;           /* 唯一标识 */
    int64_t         when;         /* 触发时间（毫秒） */
    int64_t         interval;     /* 周期（0=一次性） */
    TimeProc        proc;         /* 回调函数 */
    void           *clientData;
} AE_TimerNode;

/** 文件事件 */
typedef struct AE_FileEvent {
    int             mask;         /* AE_NONE/AE_READABLE/AE_WRITABLE */
    FileProc        rfileProc;     /* 读事件处理器 */
    FileProc        wfileProc;     /* 写事件处理器 */
    void           *clientData;
} AE_FileEvent;

/** 事件循环状态 */
typedef struct AE_EventLoop {
    int             maxfd;         /* 最大 fd */
    int             setsize;       /* fd 集合大小 */
    AE_FileEvent   *events;        /* 文件事件数组 */
    int64_t         timeEventId;   /* 定时器 ID 计数器 */
    AE_TimerNode  **timer_heap;    /* 最小堆 */
    int             timer_count;   /* 定时器数量 */
    int             timer_capacity;
    int64_t         lastTime;     /* 上次时间 */
} AE_EventLoop;

/* ============================================================
 * [event] Reactor 事件循环核心
 * ============================================================ */

static AE_EventLoop *event_loop_create(int setsize)
{
    AE_EventLoop *loop = calloc(1, sizeof(AE_EventLoop));
    loop->setsize = setsize;
    loop->maxfd = -1;
    loop->events = calloc(setsize, sizeof(AE_FileEvent));
    loop->timer_capacity = 64;
    loop->timer_heap = calloc(64, sizeof(AE_TimerNode *));
    loop->timer_count = 0;
    loop->timeEventId = 0;
    loop->lastTime = 0;
    return loop;
}

static void event_loop_free(AE_EventLoop *loop)
{
    free(loop->events);
    free(loop->timer_heap);
    free(loop);
}

/* 文件事件注册 */
static int ae_create_file_event(AE_EventLoop *loop, int fd, int mask,
                                 FileProc proc, void *data)
{
    if (fd >= loop->setsize) return -1;
    loop->events[fd].mask |= mask;
    if (mask & AE_READABLE) loop->events[fd].rfileProc = proc;
    if (mask & AE_WRITABLE) loop->events[fd].wfileProc = proc;
    loop->events[fd].clientData = data;
    if (fd > loop->maxfd) loop->maxfd = fd;
    return 0;
}

static void ae_delete_file_event(AE_EventLoop *loop, int fd, int mask)
{
    if (fd >= loop->setsize) return;
    loop->events[fd].mask &= ~mask;
    if (fd == loop->maxfd && loop->events[fd].mask == AE_NONE) {
        /* 找到新的最大 fd */
        while (loop->maxfd > 0 && loop->events[loop->maxfd].mask == AE_NONE)
            loop->maxfd--;
    }
}

/* ============================================================
 * [timer] 定时器管理（最小堆）
 * ============================================================ */

static void timer_swap(AE_TimerNode **a, AE_TimerNode **b)
{
    AE_TimerNode *tmp = *a;
    *a = *b;
    *b = tmp;
}

static void timer_heapify_up(AE_EventLoop *loop, int pos)
{
    while (pos > 0) {
        int parent = (pos - 1) / 2;
        if (loop->timer_heap[pos]->when < loop->timer_heap[parent]->when) {
            timer_swap(&loop->timer_heap[pos], &loop->timer_heap[parent]);
            pos = parent;
        } else {
            break;
        }
    }
}

static void timer_heapify_down(AE_EventLoop *loop, int pos)
{
    while (1) {
        int left = 2 * pos + 1;
        int right = 2 * pos + 2;
        int smallest = pos;

        if (left < loop->timer_count &&
            loop->timer_heap[left]->when < loop->timer_heap[smallest]->when)
            smallest = left;
        if (right < loop->timer_count &&
            loop->timer_heap[right]->when < loop->timer_heap[smallest]->when)
            smallest = right;

        if (smallest != pos) {
            timer_swap(&loop->timer_heap[pos], &loop->timer_heap[smallest]);
            pos = smallest;
        } else {
            break;
        }
    }
}

/** 添加定时器（最小堆） */
static int64_t ae_create_time_event(AE_EventLoop *loop, int64_t milliseconds,
                                    TimeProc proc, void *data)
{
    int64_t id = ++loop->timeEventId;
    AE_TimerNode *node = malloc(sizeof(AE_TimerNode));
    node->id = id;
    node->when = milliseconds;  /* 绝对时间 */
    node->interval = 0;
    node->proc = proc;
    node->clientData = data;

    if (loop->timer_count >= loop->timer_capacity) {
        loop->timer_capacity *= 2;
        loop->timer_heap = realloc(loop->timer_heap,
            loop->timer_capacity * sizeof(AE_TimerNode *));
    }
    loop->timer_heap[loop->timer_count++] = node;
    timer_heapify_up(loop, loop->timer_count - 1);
    return id;
}

static void process_time_events(AE_EventLoop *loop, int64_t now_ms)
{
    int processed = 0;
    while (loop->timer_count > 0 &&
           loop->timer_heap[0]->when <= now_ms) {
        AE_TimerNode *node = loop->timer_heap[0];
        /* 移除堆顶 */
        loop->timer_heap[0] = loop->timer_heap[--loop->timer_count];
        timer_heapify_down(loop, 0);

        printf("[timer] 触发定时器 id=%lld, when=%lldms\n",
               (long long)node->id, (long long)node->when);
        if (node->proc) node->proc(node->clientData);
        processed++;

        /* 周期性定时器重新入堆 */
        if (node->interval > 0) {
            node->when = now_ms + node->interval;
            loop->timer_heap[loop->timer_count++] = node;
            timer_heapify_up(loop, loop->timer_count - 1);
        } else {
            free(node);
        }
    }
    printf("[timer] 本轮处理 %d 个定时器，剩余 %d 个\n",
           processed, loop->timer_count);
}

/* ============================================================
 * [dispatch] 事件分发器（select 模型）
 * ============================================================ */

static int64_t get_current_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int ae_process_events(AE_EventLoop *loop, int flags)
{
    int processed = 0;
    int64_t now_ms = get_current_ms();
    int64_t sleep_ms = 0;

    /* 处理定时器事件 */
    if (flags != 0 && (flags & 1)) {  /* AE_TIME_EVENTS */
        if (loop->timer_count > 0) {
            sleep_ms = loop->timer_heap[0]->when - now_ms;
            if (sleep_ms <= 0) {
                process_time_events(loop, now_ms);
            }
        }
    }

    /* 处理文件事件（简化模拟） */
    if (flags & 2) {  /* AE_FILE_EVENTS */
        for (int i = 0; i <= loop->maxfd && i < 10; i++) {
            if (loop->events[i].mask != AE_NONE) {
                if (loop->events[i].mask & AE_READABLE) {
                    printf("[dispatch] fd=%d 可读\n", i);
                    if (loop->events[i].rfileProc)
                        loop->events[i].rfileProc(i, loop->events[i].clientData);
                }
                if (loop->events[i].mask & AE_WRITABLE) {
                    printf("[dispatch] fd=%d 可写\n", i);
                    if (loop->events[i].wfileProc)
                        loop->events[i].wfileProc(i, loop->events[i].clientData);
                }
                processed++;
            }
        }
    }

    return processed;
}

/* ============================================================
 * [reactor] Reactor 主循环
 * ============================================================ */

static void client_timeout_callback(void *data)
{
    printf("[reactor] 客户端连接超时，关闭连接 fd=%d\n", *(int*)data);
}

static void client_read_callback(int fd, void *data)
{
    printf("[reactor] 客户端 fd=%d 发送命令\n", fd);
}

static void client_write_callback(int fd, void *data)
{
    printf("[reactor] 回复客户端 fd=%d\n", fd);
}

static void demo_event_loop(void)
{
    printf("[event] ===== Redis 事件循环 =====\n");

    /* 1. 创建事件循环 */
    AE_EventLoop *loop = event_loop_create(1024);
    printf("[event] 创建事件循环 setsize=1024\n");

    /* 2. 注册文件事件 */
    printf("\n[event] 注册文件事件:\n");
    int client_fd = 5;
    ae_create_file_event(loop, client_fd, AE_READABLE,
                        client_read_callback, &client_fd);
    printf("[event]   fd=%d 注册 READ 事件\n", client_fd);

    ae_create_file_event(loop, client_fd, AE_WRITABLE,
                        client_write_callback, &client_fd);
    printf("[event]   fd=%d 注册 WRITE 事件\n", client_fd);

    /* 3. 注册定时器 */
    printf("\n[event] 注册定时器:\n");
    int64_t now = get_current_ms();
    ae_create_time_event(loop, now + 1000, client_timeout_callback, &client_fd);
    printf("[event]   注册 1 秒定时器（一次性）\n");

    ae_create_time_event(loop, now + 500, NULL, NULL);
    printf("[event]   注册 500ms 定时器（演示用）\n");

    /* 4. 处理事件 */
    printf("\n[event] 处理事件（第一轮 1100ms）:\n");
    ae_process_events(loop, 3);

    printf("\n[event] 处理事件（第二轮 200ms）:\n");
    ae_process_events(loop, 3);

    /* 5. 删除事件 */
    printf("\n[event] 删除文件事件:\n");
    ae_delete_file_event(loop, client_fd, AE_WRITABLE);
    printf("[event]   fd=%d 移除 WRITE 事件\n", client_fd);

    /* 清理 */
    event_loop_free(loop);
    printf("\n");
}

/* ============================================================
 * [io] I/O 多路复用模型对比
 * ============================================================ */

static void demo_io_multiplexing(void)
{
    printf("[io] ===== I/O 多路复用模型对比 =====\n");

    /* select */
    printf("\n[io] select 模型:\n");
    printf("[io]   fd_set read_fds, write_fds;\n");
    printf("[io]   FD_ZERO(&read_fds);\n");
    printf("[io]   FD_SET(fd, &read_fds);\n");
    printf("[io]   select(maxfd+1, &read_fds, NULL, NULL, timeout);\n");
    printf("[io]   缺点: FD_SETSIZE=1024，O(n) 扫描所有 fd\n");

    /* poll */
    printf("\n[io] poll 模型:\n");
    printf("[io]   struct pollfd fds[256];\n");
    printf("[io]   fds[0].fd = fd;\n");
    printf("[io]   fds[0].events = POLLIN;\n");
    printf("[io]   poll(fds, nfds, timeout_ms);\n");
    printf("[io]   优点: 无 FD_SETSIZE 限制\n");
    printf("[io]   缺点: 每次调用需重建数组，O(n) 扫描\n");

    /* epoll */
    printf("\n[io] epoll 模型（Linux）:\n");
    printf("[io]   epfd = epoll_create(1);\n");
    printf("[io]   epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);\n");
    printf("[io]   epoll_wait(epfd, events, MAX_EVENTS, timeout);\n");
    printf("[io]   优点: O(1) 事件通知，内核拷贝优化\n");
    printf("[io]   缺点: 只能在 Linux 使用\n");

    printf("\n");
}

/* ============================================================
 * [timer] 定时器算法对比
 * ============================================================ */

static void demo_timer_algorithms(void)
{
    printf("[timer] ===== 定时器算法对比 =====\n");

    /* 最小堆 */
    printf("\n[timer] 最小堆（Min-Heap）:\n");
    printf("[timer]   - push: O(log n)\n");
    printf("[timer]   - pop: O(log n)\n");
    printf("[timer]   - Redis 定时器采用\n");
    printf("[timer]   适用: 高频定时器，有序高效\n");

    /* 时间轮 */
    printf("\n[timer] 时间轮（Timing Wheel）:\n");
    printf("[timer]   - add: O(1)\n");
    printf("[timer]   - del: O(1) (需额外指针)\n");
    printf("[timer]   - 扫描精度取决于 slot 数量\n");
    printf("[timer]   Nginx/Kafka 采用\n");
    printf("[timer]   适用: 大量短连接超时\n");

    printf("\n");
}

/* ============================================================
 * 入口
 * ============================================================ */

int main(void)
{
    printf("========================================\n");
    printf("   Redis 事件循环与 Reactor 模式\n");
    printf("========================================\n\n");

    demo_event_loop();
    demo_io_multiplexing();
    demo_timer_algorithms();

    printf("========================================\n");
    printf("   演示结束\n");
    printf("========================================\n");
    return 0;
}
