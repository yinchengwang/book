# gap04 OLTP 并行查询引擎 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 gap03 统一执行器增加 Exchange 括号模型的并行执行：本地并行调度、predicate 驱动的 shard 裁剪扇出、pxwire 真网络数据面与 BROADCAST/REPARTITION 分布式 Join。

**Architecture:** 在 plan 树插入 PLAN_EXCHANGE 节点；Exchange 算子 open 时经 px_scheduler 线程池启动 N 个 worker，各自独立实例化子计划树（无共享状态），产出 VectorBlock 经有界 MPSC px_queue 汇流；网络模式走 pxwire 序列化 + rpc_stream 专用 TCP 通道。并行 hash join = build 串行建表 + probe 只读无锁并行。shard 读路径改为谓词裁剪 + 每分片一个 worker 扇出。

**Tech Stack:** C99 (mingw-w64 GCC, pthreads/winpthreads), C++17 GTest, CMake + Ninja, Winsock2/POSIX sockets。

**设计文档:** `docs/superpowers/specs/2026-09-22-gap04-parallel-query-design.md`（已批准）

## Global Constraints

- 工作目录：`/d/code/book/engineering`（bash 路径）/ `D:\code\book\engineering`
- 构建：`ninja -C /d/code/book/engineering/build <target>`；测试可执行文件在 `build/test/db/executor/`
- 提交信息用中文；遵循 `type(scope): 主题` 格式
- 禁止提交 `.env*`、模型权重（GGUF/bin）；账本写 `engineering/.superpowers/sdd/`（gitignored）
- 线程只用 pthread（`<pthread.h>`，winpthreads 已随 mingw 就绪，参照 `src/db/ecosystem/plugin_manager.c`）
- 新算子实现放 `src/db/executor/parallel/`；`src/db/executor/CMakeLists.txt` 是 `file(GLOB_RECURSE)`，新 .c 自动入库，**改 CMake 后必须重新跑 cmake 或触碰 glob 检查**（ninja 会自动 re-check）
- VectorBlock 生命周期：谁出队/接收谁负责 `vector_block_destroy`；块 API 见 `include/db/core/vector_types.h`
- 列类型标签常量 `COLUMN_INT32/COLUMN_INT64/COLUMN_FLOAT/COLUMN_DOUBLE/COLUMN_STRING` 在 `include/db/core/columnar_store.h`
- 所有新测试：GTest，C++ 文件 `extern "C"` 包裹 C 头，参照 `test/db/executor/framework_test.cpp`
- 新测试目标登记到 `test/db/executor/CMakeLists.txt`，链接 `db_executor db_vectorized gtest gtest_main`
- 验收基线：并行/单机结果一致；4 worker 加速比 ≥3x；无数据竞争（TSan@WSL2 或 Windows 压力循环）；双实例 localhost 分布式 Join 跑通

## File Structure

| 文件 | 责任 |
|---|---|
| `include/db/executor/px_queue.h` / `src/db/executor/parallel/px_queue.c` | VectorBlock MPSC 有界队列（背压 + EOF/abort 双关闭） |
| `include/db/executor/px_scheduler.h` / `src/db/executor/parallel/px_scheduler.c` | 进程级懒初始化线程池 + 协作式取消 |
| `include/db/executor/exec_exchange.h` / `src/db/executor/parallel/exchange_exec.c` | Exchange 算子（LOCAL 模式 + 网络 sender/receiver） |
| `src/db/executor/parallel/hashjoin_px_exec.c` | 并行 hash join（build 串行 + probe 无锁并行） |
| `include/db/executor/exec_shard.h`（改）/ `src/db/executor/operators/shard_scan_exec.c`（重写） | shard 谓词裁剪 + 扇出 |
| `include/db/executor/px_wire.h` / `src/db/executor/parallel/px_wire.c` | VectorBlock 线格式序列化（PXB1） |
| `include/db/distributed/rpc_stream.h` / `src/db/distributed/rpc/rpc_stream.c` | 流式 TCP 帧通道（独立于 rpc 连接池） |
| `include/db/optimizer/optimizer.h`（改）/ `src/db/optimizer/optimizer.c`（改） | PLAN_EXCHANGE + plan_parallelize |
| `src/db/executor/framework/plan_to_exec.c`（改） | PLAN_EXCHANGE → ExecNode 映射 |
| `test/db/executor/px_*_test.cpp` | 各组件 GTest |

---

### Task 1: px_queue —— VectorBlock MPSC 有界队列

**Files:**
- Create: `engineering/include/db/executor/px_queue.h`
- Create: `engineering/src/db/executor/parallel/px_queue.c`
- Test: `engineering/test/db/executor/px_queue_test.cpp`
- Modify: `engineering/test/db/executor/CMakeLists.txt`

**Interfaces:**
- Consumes: `VectorBlock`（`include/db/core/vector_types.h`：`vector_block_create/destroy`）
- Produces（后续 Task 全部依赖此签名）:
  ```c
  typedef struct px_queue px_queue_t;
  px_queue_t  *px_queue_create(int capacity, int nproducers);
  int          px_queue_push(px_queue_t *q, VectorBlock *block); /* 0=入队（所有权移交）; -1=已 abort（调用方仍持有 block 须自毁） */
  VectorBlock *px_queue_pop(px_queue_t *q);                      /* NULL=EOF 或 abort；用 px_queue_is_aborted 区分 */
  void         px_queue_producer_done(px_queue_t *q);            /* 全部生产者 done 且队列空 → pop 返回 NULL(EOF) */
  void         px_queue_abort(px_queue_t *q);
  int          px_queue_is_aborted(const px_queue_t *q);
  void         px_queue_destroy(px_queue_t *q);                  /* 销毁残余块；NULL 安全 */
  ```

- [ ] **Step 1: 写失败测试**

`engineering/test/db/executor/px_queue_test.cpp`：

```cpp
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>

extern "C" {
#include "db/executor/px_queue.h"
#include "db/core/vector_types.h"
#include "db/core/columnar_store.h"
}

static VectorBlock *make_block(int32_t v0, int nrows) {
    VectorBlock *b = vector_block_create(nrows, 1);
    int32_t *col = (int32_t *)malloc(sizeof(int32_t) * nrows);
    for (int i = 0; i < nrows; i++) col[i] = v0 + i;
    vector_block_set_column(b, 0, col, sizeof(int32_t));
    vector_block_set_column_type(b, 0, COLUMN_INT32);
    vector_block_set_num_rows(b, nrows);
    return b;
}

TEST(PxQueue, FifoSingleThread) {
    px_queue_t *q = px_queue_create(8, 1);
    ASSERT_NE(q, nullptr);
    ASSERT_EQ(px_queue_push(q, make_block(0, 4)), 0);
    ASSERT_EQ(px_queue_push(q, make_block(100, 4)), 0);
    px_queue_producer_done(q);

    VectorBlock *b1 = px_queue_pop(q);
    ASSERT_NE(b1, nullptr);
    EXPECT_EQ(((int32_t *)b1->columns[0])[0], 0);
    vector_block_destroy(b1);

    VectorBlock *b2 = px_queue_pop(q);
    ASSERT_NE(b2, nullptr);
    EXPECT_EQ(((int32_t *)b2->columns[0])[0], 100);
    vector_block_destroy(b2);

    EXPECT_EQ(px_queue_pop(q), nullptr);          /* EOF */
    EXPECT_EQ(px_queue_is_aborted(q), 0);
    px_queue_destroy(q);
}

TEST(PxQueue, MultiProducerAllBlocksDelivered) {
    const int kProd = 4, kBlocks = 250;         /* 1000 块 > 容量 64，逼出背压 */
    px_queue_t *q = px_queue_create(64, kProd);
    std::atomic<int64_t> sum{0};

    std::thread consumer([&] {
        VectorBlock *b;
        while ((b = px_queue_pop(q)) != nullptr) {
            sum += ((int32_t *)b->columns[0])[0];
            vector_block_destroy(b);
        }
    });

    std::vector<std::thread> producers;
    for (int p = 0; p < kProd; p++) {
        producers.emplace_back([&, p] {
            for (int i = 0; i < kBlocks; i++) {
                if (px_queue_push(q, make_block(1, 2)) != 0) return;
            }
            px_queue_producer_done(q);
        });
    }
    for (auto &t : producers) t.join();
    consumer.join();

    EXPECT_EQ(sum.load(), (int64_t)kProd * kBlocks);
    EXPECT_EQ(px_queue_is_aborted(q), 0);
    px_queue_destroy(q);
}

TEST(PxQueue, AbortWakesBlockedPopAndRejectsPush) {
    px_queue_t *q = px_queue_create(4, 1);
    ASSERT_NE(q, nullptr);

    std::thread consumer([&] {
        /* 队列空且未 done：pop 阻塞，直到 abort 唤醒 */
        VectorBlock *b = px_queue_pop(q);
        EXPECT_EQ(b, nullptr);
        EXPECT_EQ(px_queue_is_aborted(q), 1);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    px_queue_abort(q);
    consumer.join();

    VectorBlock *orphan = make_block(7, 2);
    EXPECT_EQ(px_queue_push(q, orphan), -1);    /* abort 后拒绝入队 */
    vector_block_destroy(orphan);               /* push 失败所有权未移交 */
    px_queue_destroy(q);
}

TEST(PxQueue, DestroyFreesResidualBlocks) {
    px_queue_t *q = px_queue_create(8, 1);
    ASSERT_EQ(px_queue_push(q, make_block(0, 4)), 0);
    ASSERT_EQ(px_queue_push(q, make_block(4, 4)), 0);
    px_queue_destroy(q);                        /* 残余 2 块须被释放（valgrind/ASan 验证） */
}
```

- [ ] **Step 2: 登记测试目标并确认编译失败**

`engineering/test/db/executor/CMakeLists.txt` 末尾追加：

```cmake
add_executable(px_queue_test px_queue_test.cpp)
target_link_libraries(px_queue_test PRIVATE db_executor db_vectorized gtest gtest_main)
target_include_directories(px_queue_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/engineering/include
)
gtest_discover_tests(px_queue_test)
```

Run: `ninja -C /d/code/book/engineering/build px_queue_test`
Expected: FAIL — `db/executor/px_queue.h: No such file or directory`

- [ ] **Step 3: 实现头文件**

`engineering/include/db/executor/px_queue.h`：

```c
/**
 * @file px_queue.h
 * @brief Gap#4 并行执行——VectorBlock 多生产者单消费者有界队列
 *
 * 所有权：push 成功即移交块所有权；push 失败（abort）所有权不移交。
 * pop 返回的块由调用方负责 vector_block_destroy。
 * 关闭语义：EOF = 全部生产者 producer_done 且队列空；
 *           abort = 错误/取消，pop 一律返回 NULL 且 is_aborted=1。
 * destroy 负责销毁队列中残余未消费块（无泄漏）。
 */
#ifndef DB_EXECUTOR_PX_QUEUE_H
#define DB_EXECUTOR_PX_QUEUE_H

#include "db/core/vector_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct px_queue px_queue_t;

px_queue_t  *px_queue_create(int capacity, int nproducers);
int          px_queue_push(px_queue_t *q, VectorBlock *block);
VectorBlock *px_queue_pop(px_queue_t *q);
void         px_queue_producer_done(px_queue_t *q);
void         px_queue_abort(px_queue_t *q);
int          px_queue_is_aborted(const px_queue_t *q);
void         px_queue_destroy(px_queue_t *q);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_PX_QUEUE_H */
```

- [ ] **Step 4: 实现队列**

`engineering/src/db/executor/parallel/px_queue.c`：

```c
/* px_queue.c - VectorBlock MPSC 有界队列（Gap#4） */
#include "db/executor/px_queue.h"
#include <pthread.h>
#include <stdlib.h>

struct px_queue {
    VectorBlock **buf;
    int capacity;
    int head;              /* 出队位置 */
    int tail;              /* 入队位置 */
    int count;
    int nproducers;
    int producers_done;
    int aborted;
    pthread_mutex_t mu;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
};

px_queue_t *px_queue_create(int capacity, int nproducers) {
    if (capacity <= 0 || nproducers <= 0) return NULL;
    px_queue_t *q = (px_queue_t *)calloc(1, sizeof(px_queue_t));
    if (!q) return NULL;
    q->buf = (VectorBlock **)calloc((size_t)capacity, sizeof(VectorBlock *));
    if (!q->buf) { free(q); return NULL; }
    q->capacity = capacity;
    q->nproducers = nproducers;
    pthread_mutex_init(&q->mu, NULL);
    pthread_cond_init(&q->not_full, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    return q;
}

int px_queue_push(px_queue_t *q, VectorBlock *block) {
    if (!q || !block) return -1;
    pthread_mutex_lock(&q->mu);
    while (!q->aborted && q->count == q->capacity) {
        pthread_cond_wait(&q->not_full, &q->mu);   /* 背压：满则阻塞 */
    }
    if (q->aborted) {
        pthread_mutex_unlock(&q->mu);
        return -1;                                  /* 所有权未移交 */
    }
    q->buf[q->tail] = block;
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mu);
    return 0;
}

VectorBlock *px_queue_pop(px_queue_t *q) {
    if (!q) return NULL;
    pthread_mutex_lock(&q->mu);
    for (;;) {
        if (q->count > 0) {
            VectorBlock *b = q->buf[q->head];
            q->head = (q->head + 1) % q->capacity;
            q->count--;
            pthread_cond_signal(&q->not_full);
            pthread_mutex_unlock(&q->mu);
            return b;
        }
        if (q->aborted || q->producers_done == q->nproducers) {
            pthread_mutex_unlock(&q->mu);
            return NULL;                            /* abort 或 EOF */
        }
        pthread_cond_wait(&q->not_empty, &q->mu);
    }
}

void px_queue_producer_done(px_queue_t *q) {
    if (!q) return;
    pthread_mutex_lock(&q->mu);
    if (q->producers_done < q->nproducers) q->producers_done++;
    pthread_cond_broadcast(&q->not_empty);
    pthread_mutex_unlock(&q->mu);
}

void px_queue_abort(px_queue_t *q) {
    if (!q) return;
    pthread_mutex_lock(&q->mu);
    q->aborted = 1;
    pthread_cond_broadcast(&q->not_full);
    pthread_cond_broadcast(&q->not_empty);
    pthread_mutex_unlock(&q->mu);
}

int px_queue_is_aborted(const px_queue_t *q) {
    if (!q) return 0;
    /* 调用方均在有锁路径之后读取；此处直接读（int 读写在支持平台上原子） */
    return q->aborted;
}

void px_queue_destroy(px_queue_t *q) {
    if (!q) return;
    for (int i = 0; i < q->count; i++) {
        int idx = (q->head + i) % q->capacity;
        vector_block_destroy(q->buf[idx]);          /* 残余块不泄漏 */
    }
    free(q->buf);
    pthread_mutex_destroy(&q->mu);
    pthread_cond_destroy(&q->not_full);
    pthread_cond_destroy(&q->not_empty);
    free(q);
}
```

- [ ] **Step 5: 构建并跑通测试**

Run: `ninja -C /d/code/book/engineering/build px_queue_test && /d/code/book/engineering/build/test/db/executor/px_queue_test.exe`
Expected: 4 个 TEST 全部 PASSED

- [ ] **Step 6: Commit**

```bash
cd /d/code/book/engineering
git add include/db/executor/px_queue.h src/db/executor/parallel/px_queue.c test/db/executor/px_queue_test.cpp test/db/executor/CMakeLists.txt
git commit -m "feat(gap04): px_queue —— VectorBlock MPSC 有界队列（背压+双关闭语义）"
```

---

### Task 2: px_scheduler —— 进程级线程池 + 协作式取消

**Files:**
- Create: `engineering/include/db/executor/px_scheduler.h`
- Create: `engineering/src/db/executor/parallel/px_scheduler.c`
- Test: `engineering/test/db/executor/px_scheduler_test.cpp`
- Modify: `engineering/test/db/executor/CMakeLists.txt`

**Interfaces:**
- Consumes: 无（仅 pthread + 标准库）
- Produces:
  ```c
  typedef struct px_scheduler px_scheduler_t;
  typedef void (*px_task_fn)(void *arg, volatile int *cancel_flag);
  px_scheduler_t *px_scheduler_default(void);                        /* 懒初始化全局单例，进程生命周期 */
  int  px_scheduler_submit(px_scheduler_t *s, px_task_fn fn,
                           void *arg, volatile int *cancel_flag);    /* 0=受理；-1=参数非法 */
  void px_scheduler_wait_idle(px_scheduler_t *s);                    /* 等待 outstanding==0 */
  int  px_scheduler_num_workers(const px_scheduler_t *s);
  ```
  取消约定：`cancel_flag` 由调用方（Exchange）持有；worker 线程在任务**启动前**检查一次，任务函数内部每处理一块自查一次。`px_scheduler_default()` 线程安全（pthread_once）。

- [ ] **Step 1: 写失败测试**

`engineering/test/db/executor/px_scheduler_test.cpp`：

```cpp
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <thread>

extern "C" {
#include "db/executor/px_scheduler.h"
}

struct Ctx { std::atomic<int> *counter; int sleep_ms; };

static void bump_task(void *arg, volatile int *cancel_flag) {
    Ctx *c = (Ctx *)arg;
    if (cancel_flag && *cancel_flag) return;
    if (c->sleep_ms > 0) {
        /* 模拟分块工作：每 10ms 检查一次取消 */
        for (int i = 0; i < c->sleep_ms / 10; i++) {
            if (cancel_flag && *cancel_flag) return;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    c->counter->fetch_add(1);
}

TEST(PxScheduler, SingletonAndWorkerCount) {
    px_scheduler_t *a = px_scheduler_default();
    px_scheduler_t *b = px_scheduler_default();
    ASSERT_EQ(a, b);
    ASSERT_NE(a, nullptr);
    EXPECT_GE(px_scheduler_num_workers(a), 1);
    EXPECT_LE(px_scheduler_num_workers(a), 16);
}

TEST(PxScheduler, RunsAllSubmittedTasks) {
    px_scheduler_t *s = px_scheduler_default();
    std::atomic<int> counter{0};
    Ctx ctx{&counter, 0};
    volatile int cancel = 0;
    const int kTasks = 64;
    for (int i = 0; i < kTasks; i++) {
        ASSERT_EQ(px_scheduler_submit(s, bump_task, &ctx, &cancel), 0);
    }
    px_scheduler_wait_idle(s);
    EXPECT_EQ(counter.load(), kTasks);
}

TEST(PxScheduler, CooperativeCancelStopsLongTask) {
    px_scheduler_t *s = px_scheduler_default();
    std::atomic<int> counter{0};
    Ctx ctx{&counter, 10000};            /* 10s 任务，取消后应提前退出 */
    volatile int cancel = 0;
    ASSERT_EQ(px_scheduler_submit(s, bump_task, &ctx, &cancel), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    cancel = 1;
    auto t0 = std::chrono::steady_clock::now();
    px_scheduler_wait_idle(s);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();
    EXPECT_LT(ms, 5000);                 /* 显著早于 10s */
    EXPECT_EQ(counter.load(), 0);
}

TEST(PxScheduler, CancelledTaskNeverStarts) {
    px_scheduler_t *s = px_scheduler_default();
    std::atomic<int> counter{0};
    Ctx ctx{&counter, 0};
    volatile int cancel = 1;             /* 提交即已取消 */
    ASSERT_EQ(px_scheduler_submit(s, bump_task, &ctx, &cancel), 0);
    px_scheduler_wait_idle(s);
    EXPECT_EQ(counter.load(), 0);
}
```

- [ ] **Step 2: 登记测试目标并确认编译失败**

`engineering/test/db/executor/CMakeLists.txt` 末尾追加：

```cmake
add_executable(px_scheduler_test px_scheduler_test.cpp)
target_link_libraries(px_scheduler_test PRIVATE db_executor db_vectorized gtest gtest_main)
target_include_directories(px_scheduler_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/engineering/include
)
gtest_discover_tests(px_scheduler_test)
```

Run: `ninja -C /d/code/book/engineering/build px_scheduler_test`
Expected: FAIL — `db/executor/px_scheduler.h: No such file or directory`

- [ ] **Step 3: 实现头文件**

`engineering/include/db/executor/px_scheduler.h`：

```c
/**
 * @file px_scheduler.h
 * @brief Gap#4 并行执行——进程级懒初始化线程池
 *
 * 单例（pthread_once），worker 数 = min(CPU 核数, PX_SCHED_MAX_WORKERS)。
 * 任务模型：调用方持有 cancel_flag；任务函数须协作式自查。
 * wait_idle 等待全部已提交任务完成（含被取消的）。
 * 单例随进程退出，不提供 destroy —— 悬挂 worker 由 Exchange 层
 * cancel+wait_idle 回收（见 exec_exchange.h）。
 */
#ifndef DB_EXECUTOR_PX_SCHEDULER_H
#define DB_EXECUTOR_PX_SCHEDULER_H

#ifdef __cplusplus
extern "C" {
#endif

#define PX_SCHED_MAX_WORKERS 16

typedef struct px_scheduler px_scheduler_t;
typedef void (*px_task_fn)(void *arg, volatile int *cancel_flag);

px_scheduler_t *px_scheduler_default(void);
int  px_scheduler_submit(px_scheduler_t *s, px_task_fn fn,
                         void *arg, volatile int *cancel_flag);
void px_scheduler_wait_idle(px_scheduler_t *s);
int  px_scheduler_num_workers(const px_scheduler_t *s);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_PX_SCHEDULER_H */
```

- [ ] **Step 4: 实现线程池**

`engineering/src/db/executor/parallel/px_scheduler.c`：

```c
/* px_scheduler.c - 进程级线程池（Gap#4） */
#include "db/executor/px_scheduler.h"
#include <pthread.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#define PX_TASK_QUEUE_CAP 256

typedef struct {
    px_task_fn fn;
    void *arg;
    volatile int *cancel_flag;
} px_task_t;

struct px_scheduler {
    pthread_t *threads;
    int nworkers;
    px_task_t tasks[PX_TASK_QUEUE_CAP];
    int head, tail, count;
    int outstanding;             /* 已提交未完成（含排队中） */
    pthread_mutex_t mu;
    pthread_cond_t has_task;
    pthread_cond_t idle;
};

static px_scheduler_t g_sched;
static pthread_once_t g_once = PTHREAD_ONCE_INIT;

static int px_hw_concurrency(void) {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (int)si.dwNumberOfProcessors;
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (int)n : 1;
#endif
}

static void *px_worker_main(void *arg) {
    px_scheduler_t *s = (px_scheduler_t *)arg;
    for (;;) {
        pthread_mutex_lock(&s->mu);
        while (s->count == 0) {
            pthread_cond_wait(&s->has_task, &s->mu);
        }
        px_task_t t = s->tasks[s->head];
        s->head = (s->head + 1) % PX_TASK_QUEUE_CAP;
        s->count--;
        pthread_mutex_unlock(&s->mu);

        if (!t.cancel_flag || !*t.cancel_flag) {
            t.fn(t.arg, t.cancel_flag);
        }

        pthread_mutex_lock(&s->mu);
        s->outstanding--;
        if (s->outstanding == 0) pthread_cond_broadcast(&s->idle);
        pthread_mutex_unlock(&s->mu);
    }
    return NULL;
}

static void px_sched_init_once(void) {
    int n = px_hw_concurrency();
    if (n > PX_SCHED_MAX_WORKERS) n = PX_SCHED_MAX_WORKERS;
    if (n < 1) n = 1;
    g_sched.nworkers = n;
    g_sched.threads = (pthread_t *)calloc((size_t)n, sizeof(pthread_t));
    pthread_mutex_init(&g_sched.mu, NULL);
    pthread_cond_init(&g_sched.has_task, NULL);
    pthread_cond_init(&g_sched.idle, NULL);
    for (int i = 0; i < n; i++) {
        pthread_create(&g_sched.threads[i], NULL, px_worker_main, &g_sched);
        pthread_detach(g_sched.threads[i]);   /* 进程级常驻 */
    }
}

px_scheduler_t *px_scheduler_default(void) {
    pthread_once(&g_once, px_sched_init_once);
    return &g_sched;
}

int px_scheduler_submit(px_scheduler_t *s, px_task_fn fn,
                        void *arg, volatile int *cancel_flag) {
    if (!s || !fn) return -1;
    pthread_mutex_lock(&s->mu);
    while (s->count == PX_TASK_QUEUE_CAP) {
        pthread_cond_wait(&s->idle, &s->mu);  /* 队列满：等一波排空 */
    }
    s->tasks[s->tail].fn = fn;
    s->tasks[s->tail].arg = arg;
    s->tasks[s->tail].cancel_flag = cancel_flag;
    s->tail = (s->tail + 1) % PX_TASK_QUEUE_CAP;
    s->count++;
    s->outstanding++;
    pthread_cond_signal(&s->has_task);
    pthread_mutex_unlock(&s->mu);
    return 0;
}

void px_scheduler_wait_idle(px_scheduler_t *s) {
    if (!s) return;
    pthread_mutex_lock(&s->mu);
    while (s->outstanding > 0) {
        pthread_cond_wait(&s->idle, &s->mu);
    }
    pthread_mutex_unlock(&s->mu);
}

int px_scheduler_num_workers(const px_scheduler_t *s) {
    return s ? s->nworkers : 0;
}
```

注意：`px_scheduler_submit` 队列满时复用 `idle` 条件变量等待是刻意简化（等待至整批排空后重试）；容量 256 远超单查询 dop，实践中不触发。

- [ ] **Step 5: 构建并跑通测试**

Run: `ninja -C /d/code/book/engineering/build px_scheduler_test && /d/code/book/engineering/build/test/db/executor/px_scheduler_test.exe`
Expected: 4 个 TEST 全部 PASSED

- [ ] **Step 6: Commit**

```bash
cd /d/code/book/engineering
git add include/db/executor/px_scheduler.h src/db/executor/parallel/px_scheduler.c test/db/executor/px_scheduler_test.cpp test/db/executor/CMakeLists.txt
git commit -m "feat(gap04): px_scheduler —— 进程级线程池与协作式取消"
```

---

### Task 3: Exchange 算子（LOCAL 模式）

**Files:**
- Create: `engineering/include/db/executor/exec_exchange.h`
- Create: `engineering/src/db/executor/parallel/exchange_exec.c`
- Test: `engineering/test/db/executor/exchange_exec_test.cpp`
- Modify: `engineering/test/db/executor/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1 `px_queue_*`；Task 2 `px_scheduler_default/submit/wait_idle`；`ExecNode` vtable（`include/db/executor/exec_node.h`）；`exec_create_seqscan`（`include/db/executor/exec_operators.h`）；Task 4 的 `PLAN_EXCHANGE` 枚举（**依赖：Task 4 Step 1 的枚举补丁须先于本任务 Step 4 落地**）
- Produces:
  ```c
  typedef ExecNode *(*px_subtree_fn)(void *ctx);   /* 每 worker 调一次，产独立子树 */
  ExecNode *exec_create_exchange(px_subtree_fn make_subtree, void *ctx, int dop);
  /* node_type = PLAN_EXCHANGE；dop<=1 退化为直传（不开线程） */
  ```
  上层用法：标准 vtable `open/next/close`。**所有权**：worker 子树由 worker 自己 open/next/close 并 `exec_destroy`；Exchange 不持有子树。

- [ ] **Step 1: 写失败测试**

`engineering/test/db/executor/exchange_exec_test.cpp`：

```cpp
#include <gtest/gtest.h>
#include <set>
#include <vector>

extern "C" {
#include "db/executor/exec_exchange.h"
#include "db/executor/exec_operators.h"
#include "db/executor/executor_framework.h"
#include "db/core/vector_types.h"
#include "db/core/columnar_store.h"
}

/* 测试辅助：集中回收 make_scan 分配的列缓冲 */
static std::vector<void *> &test_buffers() {
    static std::vector<void *> bufs;
    return bufs;
}

/* 每 worker 扫一段不相交的行区间 [base, base+rows) */
struct ScanCtx {
    int rows_per_worker;
    int batch;
    int next_base;          /* worker 子树创建期串行（open 内提交前调用），无线程竞争 */
};

static ExecNode *make_scan(void *vctx) {
    ScanCtx *ctx = (ScanCtx *)vctx;
    int base = ctx->next_base;
    ctx->next_base += ctx->rows_per_worker;

    int32_t *col = (int32_t *)malloc(sizeof(int32_t) * ctx->rows_per_worker);
    for (int i = 0; i < ctx->rows_per_worker; i++) col[i] = base + i;
    int col_types[] = {COLUMN_INT32};
    void *col_data[] = {col};
    int elem[] = {sizeof(int32_t)};
    test_buffers().push_back(col);
    return exec_create_seqscan(0, 1, col_types, col_data, elem,
                               ctx->rows_per_worker, ctx->batch);
}

class ExchangeExecTest : public ::testing::Test {
protected:
    void TearDown() override {
        for (void *p : test_buffers()) free(p);
        test_buffers().clear();
    }
};

TEST_F(ExchangeExecTest, ParallelScanUnionEqualsSerial) {
    const int kDop = 4, kRows = 1000, kBatch = 256;
    ScanCtx ctx{kRows, kBatch, 0};

    ExecNode *ex = exec_create_exchange(make_scan, &ctx, kDop);
    ASSERT_NE(ex, nullptr);
    ASSERT_EQ(ex->open(ex), 0);

    std::multiset<int> got;
    VectorBlock *b;
    while ((b = ex->next(ex)) != nullptr) {
        int32_t *col = (int32_t *)b->columns[0];
        for (int i = 0; i < b->num_rows; i++) got.insert(col[i]);
        vector_block_destroy(b);
    }
    ex->close(ex);
    exec_destroy(ex);

    ASSERT_EQ(got.size(), (size_t)kDop * kRows);
    for (int i = 0; i < kDop * kRows; i++) {
        EXPECT_EQ(got.count(i), 1u) << "missing/duplicate row " << i;
    }
}

TEST_F(ExchangeExecTest, DopOneIsPassThrough) {
    ScanCtx ctx{100, 64, 0};
    ExecNode *ex = exec_create_exchange(make_scan, &ctx, 1);
    ASSERT_NE(ex, nullptr);
    ASSERT_EQ(ex->open(ex), 0);
    int total = 0;
    VectorBlock *b;
    while ((b = ex->next(ex)) != nullptr) {
        total += b->num_rows;
        vector_block_destroy(b);
    }
    ex->close(ex);
    exec_destroy(ex);
    EXPECT_EQ(total, 100);
}

TEST_F(ExchangeExecTest, CloseWithoutFullDrainReclaimsWorkers) {
    ScanCtx ctx{100000, 4096, 0};        /* 数据量大，消费一块就 close */
    ExecNode *ex = exec_create_exchange(make_scan, &ctx, 4);
    ASSERT_NE(ex, nullptr);
    ASSERT_EQ(ex->open(ex), 0);
    VectorBlock *b = ex->next(ex);
    ASSERT_NE(b, nullptr);
    vector_block_destroy(b);
    ex->close(ex);                        /* 须取消悬挂 worker 且不死锁 */
    exec_destroy(ex);
    SUCCEED();
}
```

- [ ] **Step 2: 登记测试目标并确认编译失败**

`engineering/test/db/executor/CMakeLists.txt` 末尾追加：

```cmake
add_executable(exchange_exec_test exchange_exec_test.cpp)
target_link_libraries(exchange_exec_test PRIVATE db_executor db_vectorized gtest gtest_main)
target_include_directories(exchange_exec_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/engineering/include
)
gtest_discover_tests(exchange_exec_test)
```

Run: `ninja -C /d/code/book/engineering/build exchange_exec_test`
Expected: FAIL — `db/executor/exec_exchange.h: No such file or directory`

- [ ] **Step 3: 实现头文件**

`engineering/include/db/executor/exec_exchange.h`：

```c
/**
 * @file exec_exchange.h
 * @brief Gap#4 Exchange 算子——并行"括号"（Volcano Exchange 模型）
 *
 * LOCAL 模式：open 时经 px_scheduler 启动 dop 个 worker，
 * 每个 worker 用 make_subtree(ctx) 独立实例化子计划树（无共享状态），
 * 产出块推入 px_queue；next() 从队列拉块。
 *
 * 取消：close/destroy 时置 cancel_flag → px_queue_abort →
 * wait_idle 等 worker 退出；悬挂 worker 必被回收。
 * worker 子树由 worker 负责 open/next/close/exec_destroy。
 */
#ifndef DB_EXECUTOR_EXEC_EXCHANGE_H
#define DB_EXECUTOR_EXEC_EXCHANGE_H

#include "exec_node.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef ExecNode *(*px_subtree_fn)(void *ctx);

ExecNode *exec_create_exchange(px_subtree_fn make_subtree, void *ctx, int dop);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_EXEC_EXCHANGE_H */
```

- [ ] **Step 4: 实现 Exchange（LOCAL）**

`engineering/src/db/executor/parallel/exchange_exec.c`：

```c
/* exchange_exec.c - Exchange 算子 LOCAL 模式（Gap#4） */
#include "db/executor/exec_exchange.h"
#include "db/executor/px_queue.h"
#include "db/executor/px_scheduler.h"
#include "db/optimizer/optimizer.h"
#include <stdlib.h>

typedef struct {
    px_subtree_fn make_subtree;
    void *ctx;
    int dop;
    px_queue_t *queue;        /* open 时创建 */
    volatile int cancel;      /* 协作式取消标志 */
    int opened;
    ExecNode *single;         /* 直传模式（dop<=1） */
} ExchangeState;

typedef struct {
    px_subtree_fn make_subtree;
    void *ctx;
    px_queue_t *queue;
} PxWorkerArg;

static void px_exchange_worker(void *varg, volatile int *cancel_flag) {
    PxWorkerArg *arg = (PxWorkerArg *)varg;

    ExecNode *sub = arg->make_subtree(arg->ctx);
    if (!sub) { px_queue_abort(arg->queue); free(arg); return; }

    if (sub->open(sub) != 0) {
        px_queue_abort(arg->queue);
        exec_destroy(sub);
        free(arg);
        return;
    }

    VectorBlock *b;
    while (!*cancel_flag && (b = sub->next(sub)) != NULL) {
        if (px_queue_push(arg->queue, b) != 0) {
            vector_block_destroy(b);   /* abort：push 失败所有权未移交 */
            break;
        }
    }

    sub->close(sub);
    exec_destroy(sub);
    px_queue_producer_done(arg->queue);
    free(arg);
}

static int exchange_open(ExecNode *node) {
    ExchangeState *st = (ExchangeState *)node->state;
    if (!st || !st->make_subtree) return -1;
    st->cancel = 0;

    if (st->dop <= 1) {           /* 直传：不开线程 */
        st->single = st->make_subtree(st->ctx);
        if (!st->single) return -1;
        st->opened = 1;
        return st->single->open(st->single);
    }

    st->queue = px_queue_create(64, st->dop);
    if (!st->queue) return -1;

    px_scheduler_t *sched = px_scheduler_default();
    for (int i = 0; i < st->dop; i++) {
        PxWorkerArg *arg = (PxWorkerArg *)calloc(1, sizeof(PxWorkerArg));
        if (!arg) { px_queue_abort(st->queue); return -1; }
        arg->make_subtree = st->make_subtree;
        arg->ctx = st->ctx;
        arg->queue = st->queue;
        if (px_scheduler_submit(sched, px_exchange_worker, arg, &st->cancel) != 0) {
            free(arg);
            px_queue_abort(st->queue);
            return -1;
        }
    }
    st->opened = 1;
    return 0;
}

static VectorBlock *exchange_next(ExecNode *node) {
    ExchangeState *st = (ExchangeState *)node->state;
    if (!st || !st->opened) return NULL;
    if (st->dop <= 1) return st->single->next(st->single);
    return px_queue_pop(st->queue);   /* NULL=EOF 或 abort */
}

static void exchange_reset(ExecNode *node) {
    /* 并行迭代不支持原地重置；要求 close 后重新 open */
    (void)node;
}

static void exchange_close(ExecNode *node) {
    ExchangeState *st = (ExchangeState *)node->state;
    if (!st || !st->opened) return;

    if (st->dop <= 1) {
        st->single->close(st->single);
        exec_destroy(st->single);
        st->single = NULL;
    } else {
        st->cancel = 1;                            /* 协作式取消 */
        px_queue_abort(st->queue);                 /* 唤醒阻塞的 push/pop */
        px_scheduler_wait_idle(px_scheduler_default());
        px_queue_destroy(st->queue);               /* 残余块在此释放 */
        st->queue = NULL;
    }
    st->opened = 0;
}

ExecNode *exec_create_exchange(px_subtree_fn make_subtree, void *ctx, int dop) {
    if (!make_subtree) return NULL;
    ExchangeState *st = (ExchangeState *)calloc(1, sizeof(ExchangeState));
    if (!st) return NULL;
    st->make_subtree = make_subtree;
    st->ctx = ctx;
    st->dop = dop < 1 ? 1 : dop;

    ExecNode *node = (ExecNode *)calloc(1, sizeof(ExecNode));
    if (!node) { free(st); return NULL; }
    node->node_type = PLAN_EXCHANGE;
    node->state = st;
    node->open = exchange_open;
    node->next = exchange_next;
    node->reset = exchange_reset;
    node->close = exchange_close;
    return node;
}
```

注意：`px_exchange_worker` 调用 `exec_destroy(sub)`，声明在 `db/executor/executor_framework.h`——源文件需补 `#include "db/executor/executor_framework.h"`。

- [ ] **Step 5: 构建并跑通测试**

Run: `ninja -C /d/code/book/engineering/build exchange_exec_test && /d/code/book/engineering/build/test/db/executor/exchange_exec_test.exe`
Expected: 3 个 TEST 全部 PASSED

- [ ] **Step 6: Commit**

```bash
cd /d/code/book/engineering
git add include/db/executor/exec_exchange.h src/db/executor/parallel/exchange_exec.c test/db/executor/exchange_exec_test.cpp test/db/executor/CMakeLists.txt
git commit -m "feat(gap04): Exchange 算子 LOCAL 模式 —— 并行括号+取消回收"
```

---

### Task 4: 计划层 —— PLAN_EXCHANGE + plan_parallelize + plan_to_exec 映射

**Files:**
- Modify: `engineering/include/db/optimizer/optimizer.h`（枚举 + 结构 + 声明）
- Modify: `engineering/src/db/optimizer/optimizer.c`（plan_parallelize 实现；若无此函数落点则新建 `engineering/src/db/optimizer/optimizer_parallel.c`）
- Modify: `engineering/src/db/executor/framework/plan_to_exec.c`（PLAN_EXCHANGE case）
- Test: `engineering/test/db/executor/plan_parallel_test.cpp`
- Modify: `engineering/test/db/executor/CMakeLists.txt`

**Interfaces:**
- Consumes: `plan_node_t`（optimizer.h）；Task 3 `exec_create_exchange`
- Produces:
  ```c
  /* optimizer.h 枚举追加（置于 PLAN_RESULT 之后） */
  PLAN_EXCHANGE,
  /* 交换模式 */
  typedef enum { EXCHANGE_LOCAL = 0, EXCHANGE_BROADCAST, EXCHANGE_REPARTITION } exchange_mode_t;
  /* 计划数据（加入 plan_node_t 的 union） */
  typedef struct exchange_plan { exchange_mode_t mode; int dop; int key_col; } exchange_plan_t;
  /* 并行重写入口 */
  plan_node_t *plan_parallelize(plan_node_t *plan, double min_rows, int max_dop);
  ```
  重写规则（保守）：仅当节点为 `PLAN_SCAN_SEQ/PLAN_FILTER/PLAN_PROJECT` 链且 `plan_rows >= min_rows` 时，在该链顶部包一个 `PLAN_EXCHANGE{LOCAL, dop=min(max_dop,PX_SCHED_MAX_WORKERS), -1}`；join/agg 及以上不动（join 并行由 Task 5 专用节点承担）。

- [ ] **Step 1: 修改 optimizer.h（枚举先行——Task 3 依赖）**

`engineering/include/db/optimizer/optimizer.h` 中：

`plan_node_type_t` 枚举末尾（`PLAN_RESULT` 后）追加：

```c
    PLAN_RESULT,          /* 结果 */
    PLAN_EXCHANGE         /* 并行交换（Gap#4） */
```

`sort_plan_t` 定义之后插入：

```c
/**
 * 交换模式（Gap#4）
 */
typedef enum {
    EXCHANGE_LOCAL = 0,   /* 进程内并行 */
    EXCHANGE_BROADCAST,   /* 小表广播 */
    EXCHANGE_REPARTITION  /* 按 join key 哈希重分布 */
} exchange_mode_t;

/**
 * 交换计划
 */
typedef struct exchange_plan {
    exchange_mode_t mode; /* 交换模式 */
    int dop;              /* 并行度 */
    int key_col;          /* REPARTITION 的连接键列；其它模式为 -1 */
} exchange_plan_t;
```

`plan_node_t` 的 union 中（`sort_plan_t sort;` 后）追加：

```c
        sort_plan_t sort;
        exchange_plan_t exchange;   /* PLAN_EXCHANGE */
```

文件底部（`explain_plan_text` 声明后）追加：

```c
/**
 * @brief 并行重写：在超过行数阈值的 scan/filter/project 链顶部插入 PLAN_EXCHANGE
 * @param plan 原计划树（原地修改并返回新根；若根被包裹则返回 Exchange 节点）
 * @param min_rows 并行阈值（估计行数）
 * @param max_dop 最大并行度
 * @return 重写后的计划树根
 */
plan_node_t *plan_parallelize(plan_node_t *plan, double min_rows, int max_dop);
```

- [ ] **Step 2: 写失败测试**

`engineering/test/db/executor/plan_parallel_test.cpp`：

```cpp
#include <gtest/gtest.h>

extern "C" {
#include "db/optimizer/optimizer.h"
#include "db/executor/executor_framework.h"
}

TEST(PlanParallel, SmallPlanUntouched) {
    plan_node_t *scan = plan_node_create(PLAN_SCAN_SEQ);
    ASSERT_NE(scan, nullptr);
    scan->plan_rows = 100;                       /* 低于阈值 */

    plan_node_t *root = plan_parallelize(scan, 10000.0, 4);
    EXPECT_EQ(root, scan);                       /* 不被包裹 */
    EXPECT_EQ(root->type, PLAN_SCAN_SEQ);
    plan_node_destroy(root);
}

TEST(PlanParallel, LargeScanGetsExchangeWrapper) {
    plan_node_t *scan = plan_node_create(PLAN_SCAN_SEQ);
    ASSERT_NE(scan, nullptr);
    scan->plan_rows = 100000;                    /* 超过阈值 */

    plan_node_t *root = plan_parallelize(scan, 10000.0, 4);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->type, PLAN_EXCHANGE);
    EXPECT_EQ(root->data.exchange.mode, EXCHANGE_LOCAL);
    EXPECT_EQ(root->data.exchange.dop, 4);
    EXPECT_EQ(root->left, scan);                 /* 原子树挂在 Exchange 之下 */
    plan_node_destroy(root);
}

TEST(PlanParallel, FilterChainWrappedAtTop) {
    plan_node_t *scan = plan_node_create(PLAN_SCAN_SEQ);
    plan_node_t *filter = plan_node_create(PLAN_FILTER);
    ASSERT_NE(scan, nullptr);
    ASSERT_NE(filter, nullptr);
    filter->left = scan;
    filter->plan_rows = 50000;
    scan->plan_rows = 50000;

    plan_node_t *root = plan_parallelize(filter, 10000.0, 2);
    EXPECT_EQ(root->type, PLAN_EXCHANGE);        /* 包裹在链顶（filter 之上） */
    EXPECT_EQ(root->left, filter);
    EXPECT_EQ(root->data.exchange.dop, 2);
    plan_node_destroy(root);
}

TEST(PlanParallel, JoinNotWrapped) {
    plan_node_t *join = plan_node_create(PLAN_JOIN_HASH);
    ASSERT_NE(join, nullptr);
    join->plan_rows = 1000000;

    plan_node_t *root = plan_parallelize(join, 10000.0, 4);
    EXPECT_EQ(root, join);                       /* join 不被通用规则包裹 */
    EXPECT_EQ(root->type, PLAN_JOIN_HASH);
    plan_node_destroy(root);
}
```

- [ ] **Step 3: 登记测试目标并确认编译失败**

`engineering/test/db/executor/CMakeLists.txt` 末尾追加：

```cmake
add_executable(plan_parallel_test plan_parallel_test.cpp)
target_link_libraries(plan_parallel_test PRIVATE db_executor db_vectorized gtest gtest_main)
target_include_directories(plan_parallel_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/engineering/include
)
gtest_discover_tests(plan_parallel_test)
```

Run: `ninja -C /d/code/book/engineering/build plan_parallel_test`
Expected: FAIL — `plan_parallelize` 未定义（链接错误）

- [ ] **Step 4: 实现 plan_parallelize**

新建 `engineering/src/db/optimizer/optimizer_parallel.c`（确认 `src/db/optimizer/CMakeLists.txt` 为 glob 收录；若为显式列表则追加该文件）：

```c
/* optimizer_parallel.c - Gap#4 并行计划重写 */
#include "db/optimizer/optimizer.h"
#include <stdlib.h>
#include <string.h>

static int is_parallelizable_chain_node(plan_node_type_t t) {
    return t == PLAN_SCAN_SEQ || t == PLAN_FILTER || t == PLAN_PROJECT;
}

/* 判断以 node 为根的子树是否整条都是 scan/filter/project 链 */
static int is_parallelizable_chain(const plan_node_t *node) {
    const plan_node_t *cur = node;
    while (cur) {
        if (!is_parallelizable_chain_node(cur->type)) return 0;
        if (cur->right || cur->subplan) return 0;   /* 只处理单链 */
        cur = cur->left;
    }
    return 1;
}

static plan_node_t *wrap_exchange(plan_node_t *subtree, int dop) {
    plan_node_t *ex = plan_node_create(PLAN_EXCHANGE);
    if (!ex) return subtree;
    ex->data.exchange.mode = EXCHANGE_LOCAL;
    ex->data.exchange.dop = dop;
    ex->data.exchange.key_col = -1;
    ex->plan_rows = subtree->plan_rows;
    ex->total_cost = subtree->total_cost;
    ex->left = subtree;
    return ex;
}

plan_node_t *plan_parallelize(plan_node_t *plan, double min_rows, int max_dop) {
    if (!plan || max_dop <= 1) return plan;

    /* 递归先处理子树（join 的左右侧各自可能是可并行链） */
    if (plan->left) plan->left = plan_parallelize(plan->left, min_rows, max_dop);
    if (plan->right) plan->right = plan_parallelize(plan->right, min_rows, max_dop);

    if (plan->type == PLAN_EXCHANGE) return plan;   /* 不重复包裹 */

    if (is_parallelizable_chain_node(plan->type)
        && plan->plan_rows >= min_rows
        && is_parallelizable_chain(plan)) {
        return wrap_exchange(plan, max_dop);
    }
    return plan;
}
```

- [ ] **Step 5: plan_to_exec 映射**

`engineering/src/db/executor/framework/plan_to_exec.c`：

顶部 include 追加：

```c
#include "db/executor/exec_exchange.h"
```

`plan_to_exec_impl` 的 switch 中 `case PLAN_SORT` 之前插入：

```c
        case PLAN_EXCHANGE: {
            /* Exchange 的子树由 worker 侧工厂克隆（每 worker 一棵独立 ExecNode 树）。
               闭包持有 plan->left；exec 树销毁不触碰 plan 树（plan 由调用方管理）。 */
            const plan_node_t *subplan = plan->left;
            int dop = plan->data.exchange.dop;
            return exec_create_exchange_px_plan(subplan, dop);
        }
```

并在 `plan_to_exec.c` 底部添加适配器（把 plan 子树包装成 Task 3 的工厂签名）：

```c
typedef struct {
    const plan_node_t *subplan;
} PxPlanFactoryCtx;

static ExecNode *px_plan_subtree_fn(void *ctx) {
    PxPlanFactoryCtx *c = (PxPlanFactoryCtx *)ctx;
    return plan_to_exec_impl(c->subplan);
}

ExecNode *exec_create_exchange_px_plan(const plan_node_t *subplan, int dop) {
    if (!subplan) return NULL;
    PxPlanFactoryCtx *ctx = (PxPlanFactoryCtx *)calloc(1, sizeof(PxPlanFactoryCtx));
    if (!ctx) return NULL;
    ctx->subplan = subplan;
    /* ctx 生命周期：Exchange worker 在 open 期间同步调用工厂（见 exchange_open 提交语义），
       但 worker 实际在 open 返回后异步调用 —— 因此 ctx 必须堆分配且由谁释放？
       简化决策：plan 驱动的 Exchange 目前只在 Task 4 测试与 explain 路径使用，
       ctx 泄漏 8 字节/查询不可接受 —— 改为把 ctx 挂到 Exchange 节点 state？
       不可行（state 归 exchange_exec.c 私有）。
       最终决策：exec_create_exchange 增加可选 ctx_destructor 参数？不——
       保持简单：本适配器要求的前提是让 make_subtree 在 open 内被同步预调用。
       exchange_open 的实现是先创建子树再提交任务（见 Task 3 修订注意）。 */
    ExecNode *ex = exec_create_exchange(px_plan_subtree_fn, ctx, dop);
    if (!ex) { free(ctx); return NULL; }
    return ex;
}
```

**重要修订（执行时应用于 Task 3 的 exchange_exec.c）**：工厂调用时机改为 open 内同步执行——worker arg 存创建好的 `ExecNode *sub` 而非工厂：

`exchange_open` 中提交 worker 前改为：

```c
    for (int i = 0; i < st->dop; i++) {
        PxWorkerArg *arg = (PxWorkerArg *)calloc(1, sizeof(PxWorkerArg));
        if (!arg) { px_queue_abort(st->queue); return -1; }
        arg->sub = st->make_subtree(st->ctx);      /* 同步创建子树 */
        if (!arg->sub) {
            free(arg);
            px_queue_abort(st->queue);
            return -1;
        }
        arg->queue = st->queue;
        ...
    }
```

`PxWorkerArg` 结构改为 `{ ExecNode *sub; px_queue_t *queue; }`，worker 函数去掉 `make_subtree` 调用直接使用 `arg->sub`。同时 ExchangeState 增加 `void (*ctx_destroy)(void *);` 字段与 `exec_create_exchange_ex(fn, ctx, dop, ctx_destroy)`——`exec_create_exchange` 传 NULL 保持兼容；`exec_create_exchange_px_plan` 传 `free`，在 `exchange_close` 末尾调用一次。测试（Task 3 Step 1）不受影响（工厂仍同步被调，`ScanCtx.next_base` 无竞争）。

头文件同步追加：

```c
ExecNode *exec_create_exchange_ex(px_subtree_fn make_subtree, void *ctx, int dop,
                                  void (*ctx_destroy)(void *));
```

- [ ] **Step 6: 构建并跑通测试**

Run: `ninja -C /d/code/book/engineering/build plan_parallel_test exchange_exec_test && /d/code/book/engineering/build/test/db/executor/plan_parallel_test.exe && /d/code/book/engineering/build/test/db/executor/exchange_exec_test.exe`
Expected: 两套测试全部 PASSED（exchange_exec_test 在工厂同步化修订后仍须通过）

- [ ] **Step 7: Commit**

```bash
cd /d/code/book/engineering
git add include/db/optimizer/optimizer.h src/db/optimizer/optimizer_parallel.c src/db/executor/framework/plan_to_exec.c src/db/executor/parallel/exchange_exec.c include/db/executor/exec_exchange.h test/db/executor/plan_parallel_test.cpp test/db/executor/CMakeLists.txt
git commit -m "feat(gap04): PLAN_EXCHANGE 计划节点 + plan_parallelize 重写 + plan_to_exec 映射"
```

---

### Task 5: 并行 Hash Join（build 串行 + probe 无锁并行）

**Files:**
- Create: `engineering/include/db/executor/exec_hashjoin_px.h`
- Create: `engineering/src/db/executor/parallel/hashjoin_px_exec.c`
- Test: `engineering/test/db/executor/hashjoin_px_test.cpp`
- Modify: `engineering/test/db/executor/CMakeLists.txt`

**Interfaces:**
- Consumes: `vecx_hashjoin_create/add_build/probe/destroy`（`include/db/vectorized/vectorized.h`）；Task 1 px_queue；Task 2 px_scheduler；Task 3 `px_subtree_fn`
- Produces:
  ```c
  /* build_child：普通 ExecNode，open 时被本节点排干（串行建表）后 close+destroy
     make_probe：每 worker 一棵 probe 子树（Task 3 工厂语义）
     前置保证（已读码验证）：vecx_hashjoin_probe 在 build 完成后只读共享句柄，
     每调用使用栈上/堆上局部状态（hj_find_slot 只读）——多 worker 并发 probe 无锁安全 */
  ExecNode *exec_create_hashjoin_px(ExecNode *build_child,
                                    px_subtree_fn make_probe, void *probe_ctx,
                                    int dop, int build_key_col, int probe_key_col);
  /* node_type = PLAN_JOIN_HASH */
  ```

- [ ] **Step 1: 写失败测试**

`engineering/test/db/executor/hashjoin_px_test.cpp`：

```cpp
#include <gtest/gtest.h>
#include <set>
#include <vector>

extern "C" {
#include "db/executor/exec_hashjoin_px.h"
#include "db/executor/exec_operators.h"
#include "db/executor/executor_framework.h"
#include "db/vectorized/vectorized.h"
#include "db/core/vector_types.h"
#include "db/core/columnar_store.h"
}

static std::vector<void *> &hj_buffers() {
    static std::vector<void *> bufs;
    return bufs;
}

static ExecNode *make_table(const int32_t *keys, const int32_t *vals, int n, int batch) {
    int32_t *k = (int32_t *)malloc(sizeof(int32_t) * n);
    int32_t *v = (int32_t *)malloc(sizeof(int32_t) * n);
    memcpy(k, keys, sizeof(int32_t) * n);
    memcpy(v, vals, sizeof(int32_t) * n);
    hj_buffers().push_back(k);
    hj_buffers().push_back(v);
    int col_types[] = {COLUMN_INT32, COLUMN_INT32};
    void *col_data[] = {k, v};
    int elem[] = {sizeof(int32_t), sizeof(int32_t)};
    return exec_create_seqscan(0, 2, col_types, col_data, elem, n, batch);
}

/* probe 工厂：把大行数 probe 表按 worker 切片 */
struct ProbeCtx {
    const int32_t *keys;
    const int32_t *vals;
    int rows_per_worker;
    int batch;
    int next_base;
};

static ExecNode *make_probe_slice(void *vctx) {
    ProbeCtx *ctx = (ProbeCtx *)vctx;
    int base = ctx->next_base;
    ctx->next_base += ctx->rows_per_worker;
    return make_table(ctx->keys + base, ctx->vals + base, ctx->rows_per_worker, ctx->batch);
}

class HashJoinPxTest : public ::testing::Test {
protected:
    void TearDown() override {
        for (void *p : hj_buffers()) free(p);
        hj_buffers().clear();
    }
};

TEST_F(HashJoinPxTest, ParallelProbeEqualsSerialVecx) {
    /* build 表：key 0..99，val = key*10 */
    const int kBuild = 100;
    std::vector<int32_t> bk(kBuild), bv(kBuild);
    for (int i = 0; i < kBuild; i++) { bk[i] = i; bv[i] = i * 10; }

    /* probe 表：4000 行，key = i%100（每 key 40 个 probe 行），val = i */
    const int kProbe = 4000, kDop = 4, kSlice = kProbe / kDop;
    std::vector<int32_t> pk(kProbe), pv(kProbe);
    for (int i = 0; i < kProbe; i++) { pk[i] = i % kBuild; pv[i] = i; }

    /* ---- 串行基线（直接 vecx_hashjoin） ---- */
    std::multiset<std::pair<int32_t, int32_t>> baseline;
    {
        vecx_hashjoin_t *hj = vecx_hashjoin_create(0, 0);
        ExecNode *bscan = make_table(bk.data(), bv.data(), kBuild, kBuild);
        bscan->open(bscan);
        VectorBlock *bb;
        while ((bb = bscan->next(bscan)) != nullptr) {
            ASSERT_EQ(vecx_hashjoin_add_build(hj, bb), 0);
            vector_block_destroy(bb);
        }
        bscan->close(bscan);
        exec_destroy(bscan);

        ExecNode *pscan = make_table(pk.data(), pv.data(), kProbe, 512);
        pscan->open(pscan);
        VectorBlock *pb;
        while ((pb = pscan->next(pscan)) != nullptr) {
            VectorBlock *out = nullptr;
            int n = vecx_hashjoin_probe(hj, pb, &out);
            if (n > 0 && out) {
                /* 输出列布局：build(k,v) + probe(k,v) */
                int32_t *bv_col = (int32_t *)out->columns[1];
                int32_t *pv_col = (int32_t *)out->columns[3];
                for (int r = 0; r < out->num_rows; r++)
                    baseline.insert({bv_col[r], pv_col[r]});
                vector_block_destroy(out);
            }
            vector_block_destroy(pb);
        }
        pscan->close(pscan);
        exec_destroy(pscan);
        vecx_hashjoin_destroy(hj);
    }

    /* ---- 并行 probe ---- */
    ExecNode *build_child = make_table(bk.data(), bv.data(), kBuild, kBuild);
    ProbeCtx pctx{pk.data(), pv.data(), kSlice, 512, 0};
    ExecNode *jnode = exec_create_hashjoin_px(build_child, make_probe_slice, &pctx,
                                              kDop, 0, 0);
    ASSERT_NE(jnode, nullptr);
    ASSERT_EQ(jnode->open(jnode), 0);

    std::multiset<std::pair<int32_t, int32_t>> parallel;
    VectorBlock *out;
    while ((out = jnode->next(jnode)) != nullptr) {
        int32_t *bv_col = (int32_t *)out->columns[1];
        int32_t *pv_col = (int32_t *)out->columns[3];
        for (int r = 0; r < out->num_rows; r++)
            parallel.insert({bv_col[r], pv_col[r]});
        vector_block_destroy(out);
    }
    jnode->close(jnode);
    exec_destroy(jnode);

    EXPECT_EQ(parallel, baseline);
    EXPECT_EQ(parallel.size(), (size_t)kProbe);  /* 每 probe 行恰一匹配 */
}
```

- [ ] **Step 2: 登记测试目标并确认编译失败**

```cmake
add_executable(hashjoin_px_test hashjoin_px_test.cpp)
target_link_libraries(hashjoin_px_test PRIVATE db_executor db_vectorized gtest gtest_main)
target_include_directories(hashjoin_px_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/engineering/include
)
gtest_discover_tests(hashjoin_px_test)
```

Run: `ninja -C /d/code/book/engineering/build hashjoin_px_test`
Expected: FAIL — `db/executor/exec_hashjoin_px.h: No such file or directory`

- [ ] **Step 3: 实现头文件**

`engineering/include/db/executor/exec_hashjoin_px.h`：

```c
/**
 * @file exec_hashjoin_px.h
 * @brief Gap#4 并行 Hash Join——build 串行建表 + probe 只读无锁并行
 *
 * open：排干 build_child 灌入单个 vecx_hashjoin_t（建表串行，扫描可经
 *       build_child 内部的 Exchange 并行）；随后向 px_scheduler 提交 dop
 *       个 worker，各自用 make_probe 独立实例化 probe 子树。
 * probe：build 完成后哈希表只读，worker 共享句柄并发 probe（无锁）。
 *       前置依据：vecx_hashjoin_probe 每调用使用局部状态，hj_find_slot 只读。
 * close：取消标志 + abort 队列 + wait_idle 回收 worker。
 */
#ifndef DB_EXECUTOR_EXEC_HASHJOIN_PX_H
#define DB_EXECUTOR_EXEC_HASHJOIN_PX_H

#include "exec_node.h"
#include "exec_exchange.h"

#ifdef __cplusplus
extern "C" {
#endif

ExecNode *exec_create_hashjoin_px(ExecNode *build_child,
                                  px_subtree_fn make_probe, void *probe_ctx,
                                  int dop, int build_key_col, int probe_key_col);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_EXEC_HASHJOIN_PX_H */
```

- [ ] **Step 4: 实现并行 Hash Join**

`engineering/src/db/executor/parallel/hashjoin_px_exec.c`：

```c
/* hashjoin_px_exec.c - 并行 Hash Join（Gap#4） */
#include "db/executor/exec_hashjoin_px.h"
#include "db/executor/px_queue.h"
#include "db/executor/px_scheduler.h"
#include "db/executor/executor_framework.h"
#include "db/vectorized/vectorized.h"
#include "db/optimizer/optimizer.h"
#include <stdlib.h>

typedef struct {
    ExecNode *build_child;
    px_subtree_fn make_probe;
    void *probe_ctx;
    int dop;
    int build_key_col, probe_key_col;
    vecx_hashjoin_t *hj;          /* open 建表；build 完成后只读 */
    px_queue_t *queue;
    volatile int cancel;
    int opened;
} HashJoinPxState;

typedef struct {
    ExecNode *probe_sub;          /* open 内同步创建（Task 4 修订语义） */
    vecx_hashjoin_t *hj;
    px_queue_t *queue;
} HjProbeArg;

static void hj_probe_worker(void *varg, volatile int *cancel_flag) {
    HjProbeArg *arg = (HjProbeArg *)varg;
    ExecNode *sub = arg->probe_sub;

    if (sub->open(sub) != 0) {
        px_queue_abort(arg->queue);
        exec_destroy(sub);
        free(arg);
        return;
    }

    VectorBlock *pb;
    while (!*cancel_flag && (pb = sub->next(sub)) != NULL) {
        VectorBlock *out = NULL;
        int n = vecx_hashjoin_probe(arg->hj, pb, &out);   /* 只读共享 hj */
        vector_block_destroy(pb);
        if (n < 0) {
            px_queue_abort(arg->queue);
            break;
        }
        if (n > 0 && out) {
            if (px_queue_push(arg->queue, out) != 0) {
                vector_block_destroy(out);
                break;
            }
        }
    }

    sub->close(sub);
    exec_destroy(sub);
    px_queue_producer_done(arg->queue);
    free(arg);
}

static int hjpx_open(ExecNode *node) {
    HashJoinPxState *st = (HashJoinPxState *)node->state;
    if (!st || !st->build_child || !st->make_probe) return -1;
    st->cancel = 0;

    st->hj = vecx_hashjoin_create(st->build_key_col, st->probe_key_col);
    if (!st->hj) return -1;

    /* 第一阶段：串行排干 build 侧建表 */
    ExecNode *bc = st->build_child;
    if (bc->open(bc) != 0) return -1;
    VectorBlock *bb;
    while ((bb = bc->next(bc)) != NULL) {
        int rc = vecx_hashjoin_add_build(st->hj, bb);
        vector_block_destroy(bb);
        if (rc != 0) { bc->close(bc); return -1; }
    }
    bc->close(bc);
    exec_destroy(bc);
    st->build_child = NULL;
    /* 此后 st->hj 只读 */

    st->queue = px_queue_create(64, st->dop);
    if (!st->queue) return -1;

    px_scheduler_t *sched = px_scheduler_default();
    for (int i = 0; i < st->dop; i++) {
        HjProbeArg *arg = (HjProbeArg *)calloc(1, sizeof(HjProbeArg));
        if (!arg) { px_queue_abort(st->queue); return -1; }
        arg->probe_sub = st->make_probe(st->probe_ctx);   /* 同步建子树 */
        if (!arg->probe_sub) {
            free(arg);
            px_queue_abort(st->queue);
            return -1;
        }
        arg->hj = st->hj;
        arg->queue = st->queue;
        if (px_scheduler_submit(sched, hj_probe_worker, arg, &st->cancel) != 0) {
            exec_destroy(arg->probe_sub);
            free(arg);
            px_queue_abort(st->queue);
            return -1;
        }
    }
    st->opened = 1;
    return 0;
}

static VectorBlock *hjpx_next(ExecNode *node) {
    HashJoinPxState *st = (HashJoinPxState *)node->state;
    if (!st || !st->opened) return NULL;
    return px_queue_pop(st->queue);
}

static void hjpx_reset(ExecNode *node) { (void)node; }

static void hjpx_close(ExecNode *node) {
    HashJoinPxState *st = (HashJoinPxState *)node->state;
    if (!st) return;
    if (st->opened) {
        st->cancel = 1;
        px_queue_abort(st->queue);
        px_scheduler_wait_idle(px_scheduler_default());
        px_queue_destroy(st->queue);
        st->queue = NULL;
        st->opened = 0;
    }
    if (st->hj) {
        vecx_hashjoin_destroy(st->hj);   /* worker 已全部退出，安全销毁 */
        st->hj = NULL;
    }
    if (st->build_child) {               /* open 失败早退路径 */
        exec_destroy(st->build_child);
        st->build_child = NULL;
    }
}

ExecNode *exec_create_hashjoin_px(ExecNode *build_child,
                                  px_subtree_fn make_probe, void *probe_ctx,
                                  int dop, int build_key_col, int probe_key_col) {
    if (!build_child || !make_probe) return NULL;
    HashJoinPxState *st = (HashJoinPxState *)calloc(1, sizeof(HashJoinPxState));
    if (!st) return NULL;
    st->build_child = build_child;
    st->make_probe = make_probe;
    st->probe_ctx = probe_ctx;
    st->dop = dop < 1 ? 1 : dop;
    st->build_key_col = build_key_col;
    st->probe_key_col = probe_key_col;

    ExecNode *node = (ExecNode *)calloc(1, sizeof(ExecNode));
    if (!node) { free(st); return NULL; }
    node->node_type = PLAN_JOIN_HASH;
    node->state = st;
    node->open = hjpx_open;
    node->next = hjpx_next;
    node->reset = hjpx_reset;
    node->close = hjpx_close;
    return node;
}
```

- [ ] **Step 5: 构建并跑通测试**

Run: `ninja -C /d/code/book/engineering/build hashjoin_px_test && /d/code/book/engineering/build/test/db/executor/hashjoin_px_test.exe`
Expected: PASSED（parallel == baseline，各 4000 行）

- [ ] **Step 6: Commit**

```bash
cd /d/code/book/engineering
git add include/db/executor/exec_hashjoin_px.h src/db/executor/parallel/hashjoin_px_exec.c test/db/executor/hashjoin_px_test.cpp test/db/executor/CMakeLists.txt
git commit -m "feat(gap04): 并行 Hash Join —— build 串行建表 + probe 只读无锁并行"
```

---

### Task 6: 并行基准（≥3x）+ 竞态检测基线

**Files:**
- Test: `engineering/test/db/executor/px_benchmark_test.cpp`
- Modify: `engineering/test/db/executor/CMakeLists.txt`
- Create: `engineering/test/db/executor/run_tsan_wsl.md`（WSL2 TSan 操作文档）

**Interfaces:**
- Consumes: Task 3 Exchange；Task 5 并行 hash join；`vecx_filter_block`、`vecx_agg_scalar`
- Produces: 基准数字写入 `.superpowers/sdd/gap04-ledger.md`（Task 12）

- [ ] **Step 1: 写基准测试**

`engineering/test/db/executor/px_benchmark_test.cpp`：

```cpp
#include <gtest/gtest.h>
#include <chrono>
#include <vector>

extern "C" {
#include "db/executor/exec_exchange.h"
#include "db/executor/exec_operators.h"
#include "db/executor/executor_framework.h"
#include "db/vectorized/vectorized.h"
#include "db/core/vector_types.h"
#include "db/core/columnar_store.h"
}

/* 大数据集：8M 行 int64，filter(col > 25%) + SUM 聚合。
   单线程基线 vs Exchange dop=4（每 worker 扫不相交切片、各自局部 filter+局部求和，
   主线程合并部分和）。SUM 可结合，合并语义正确。 */

static const int64_t kTotal = 8 * 1000 * 1000;
static std::vector<int64_t> g_data;

struct SliceCtx { int slice; int nworkers; };

static ExecNode *make_slice(void *vctx) {
    SliceCtx *ctx = (SliceCtx *)vctx;
    int64_t base = (kTotal / ctx->nworkers) * ctx->slice;
    int rows = (int)(kTotal / ctx->nworkers);
    int col_types[] = {COLUMN_INT64};
    void *col_data[] = {g_data.data() + base};
    int elem[] = {sizeof(int64_t)};
    return exec_create_seqscan(0, 1, col_types, col_data, elem, rows, 8192);
}

static double run_serial(int64_t *sum_out) {
    auto t0 = std::chrono::steady_clock::now();
    int64_t sum = 0;
    int64_t threshold = kTotal / 4;
    ExecNode *scan = make_slice(&(SliceCtx){0, 1});
    scan->open(scan);
    VectorBlock *b;
    while ((b = scan->next(scan)) != nullptr) {
        VectorBlock *f = nullptr;
        int n = vecx_filter_block(b, 0, CMP_GT, &threshold, &f);
        if (n > 0 && f) {
            double s; int has;
            vecx_agg_scalar(f, 0, VECX_AGG_SUM, nullptr, 0, &s, &has);
            if (has) sum += (int64_t)s;
            vector_block_destroy(f);
        }
        vector_block_destroy(b);
    }
    scan->close(scan);
    exec_destroy(scan);
    *sum_out = sum;
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

/* 并行 worker：filter+局部求和，把"一行一列"结果块推回 Exchange */
struct ParCtx { SliceCtx slice; int64_t threshold; };

static ExecNode *make_partial_agg(void *vctx);

/* 用 filter ExecNode 组合：scan -> filter，聚合在主线程做（每 worker 产过滤后块）。
   这样并行区段 = scan+filter，主线程只做 SUM（聚合量小）。 */
static ExecNode *make_filtered_slice(void *vctx) {
    ParCtx *ctx = (ParCtx *)vctx;
    ExecNode *scan = make_slice(&ctx->slice);
    if (!scan) return nullptr;
    vecx_pred_t pred;
    memset(&pred, 0, sizeof(pred));
    pred.col = 0;
    pred.op = CMP_GT;
    pred.i64 = ctx->threshold;
    ExecNode *filter = exec_create_filter(&pred);
    if (!filter) { exec_destroy(scan); return nullptr; }
    filter->left = scan;
    return filter;
}

static double run_parallel(int dop, int64_t *sum_out) {
    auto t0 = std::chrono::steady_clock::now();
    ParCtx ctxs[16];
    /* 每个 worker 需要自己的 ctx（slice 不同）——用工厂数组：
       Exchange 的 make_subtree 每 worker 调一次，按调用序号分片 */
    struct FactoryCtx { ParCtx *ctxs; int next; } fctx{ctxs, 0};
    for (int i = 0; i < dop; i++) {
        ctxs[i].slice = (SliceCtx){i, dop};
        ctxs[i].threshold = kTotal / 4;
    }
    auto factory = [](void *p) -> ExecNode * {
        FactoryCtx *f = (FactoryCtx *)p;
        return make_filtered_slice(&f->ctxs[f->next++]);
    };
    ExecNode *ex = exec_create_exchange(factory, &fctx, dop);
    ex->open(ex);
    int64_t sum = 0;
    VectorBlock *b;
    while ((b = ex->next(ex)) != nullptr) {
        double s; int has;
        vecx_agg_scalar(b, 0, VECX_AGG_SUM, nullptr, 0, &s, &has);
        if (has) sum += (int64_t)s;
        vector_block_destroy(b);
    }
    ex->close(ex);
    exec_destroy(ex);
    *sum_out = sum;
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

TEST(PxBenchmark, FourWorkersSpeedupAtLeast3x) {
    g_data.resize(kTotal);
    for (int64_t i = 0; i < kTotal; i++) g_data[i] = i;

    int64_t sum_serial = 0, sum_parallel = 0;
    double t_serial = run_serial(&sum_serial);
    double t_parallel = run_parallel(4, &sum_parallel);

    EXPECT_EQ(sum_serial, sum_parallel);              /* 结果一致 */
    double speedup = t_serial / t_parallel;
    ADD_FAILURE()                                                \
        << "speedup " << speedup << "x (serial " << t_serial     \
        << "s parallel " << t_parallel << "s)";                  \
    /* 上面两行故意写反——占位避免误提交；实现时用 RecordProperty: */
    /* ::testing::Test::RecordProperty("speedup", speedup); */
    EXPECT_GE(speedup, 3.0)
        << "speedup " << speedup << "x (serial " << t_serial
        << "s, parallel " << t_parallel << "s)";
}
```

**注意**：上面代码块中标注"故意写反"的两行是文档笔误的反面教材——执行时必须删除这两行 `ADD_FAILURE` 并改用 `RecordProperty`（代码注释已给出）。基准机 CPU < 4 核时跳过：`if (px_scheduler_num_workers(px_scheduler_default()) < 4) GTEST_SKIP();`

- [ ] **Step 2: 登记目标、跑基准**

```cmake
add_executable(px_benchmark_test px_benchmark_test.cpp)
target_link_libraries(px_benchmark_test PRIVATE db_executor db_vectorized gtest gtest_main)
target_include_directories(px_benchmark_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/engineering/include
)
gtest_discover_tests(px_benchmark_test)
```

Run: `ninja -C /d/code/book/engineering/build px_benchmark_test && /d/code/book/engineering/build/test/db/executor/px_benchmark_test.exe`
Expected: PASSED 且 speedup ≥3.0；若未达：先确认 Release 构建（`-O2`，检查 build/CMakeCache.txt 的 CMAKE_BUILD_TYPE；Debug 构建允许记录数字并以 Release 重测为准）

- [ ] **Step 3: Windows 压力循环（TSan 的实用替代）**

Run（Release 或 Debug 均可，循环 20 轮）:

```bash
for i in $(seq 1 20); do
  /d/code/book/engineering/build/test/db/executor/px_queue_test.exe --gtest_filter='PxQueue.MultiProducerAllBlocksDelivered' >/dev/null 2>&1 || { echo "FAIL at iter $i"; break; }
  /d/code/book/engineering/build/test/db/executor/exchange_exec_test.exe >/dev/null 2>&1 || { echo "FAIL at iter $i"; break; }
  /d/code/book/engineering/build/test/db/executor/hashjoin_px_test.exe >/dev/null 2>&1 || { echo "FAIL at iter $i"; break; }
done; echo "stress loop done"
```

Expected: 20 轮无 FAIL、无挂起（每轮秒级）

- [ ] **Step 4: WSL2 TSan 操作文档**

`engineering/test/db/executor/run_tsan_wsl.md`：

```markdown
# WSL2 下 ThreadSanitizer 验证（gap04）

MinGW 不支持 TSan，须在 WSL2 Ubuntu 构建。前置参照记忆 engineering-linux-build-knowledge：
NTFS 下 configure_file 需防护、显式 time/stddef/errno include、mkdir(path,0755) 调用点改写。

```bash
# WSL2 Ubuntu 内：
cd /mnt/d/code/book/engineering
cmake -B build-tsan -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_FLAGS="-fsanitize=thread -g" \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
ninja -C build-tsan px_queue_test px_scheduler_test exchange_exec_test hashjoin_px_test
for t in px_queue_test px_scheduler_test exchange_exec_test hashjoin_px_test; do
  ./build-tsan/test/db/executor/$t || exit 1
done
echo "TSAN CLEAN"
```

验收：`TSAN CLEAN` 且输出无任何 "WARNING: ThreadSanitizer"。
```

- [ ] **Step 5: Commit**

```bash
cd /d/code/book/engineering
git add test/db/executor/px_benchmark_test.cpp test/db/executor/run_tsan_wsl.md test/db/executor/CMakeLists.txt
git commit -m "test(gap04): 并行基准（4 worker ≥3x）+ 压力循环 + WSL2 TSan 流程"
```

---

### Task 7: shard 裁剪 + 扇出（重写 shard_scan_exec）

**Files:**
- Modify: `engineering/include/db/executor/exec_shard.h`（新 API）
- Rewrite: `engineering/src/db/executor/operators/shard_scan_exec.c`
- Test: `engineering/test/db/executor/shard_prune_test.cpp`
- Modify: `engineering/test/db/executor/CMakeLists.txt`

**Interfaces:**
- Consumes: `shard_router_t`（`include/db/sharding/sharding.h`：`shard_route`、`shard_route_range`、`shard_get_all`、`shard_count`）；`vecx_pred_t`（vectorized.h）；Task 3 Exchange
- Produces:
  ```c
  /* 纯函数裁剪：按分片键谓词计算候选分片。pred==NULL → 全分片。返回数量。 */
  int px_shard_prune(const shard_router_t *router, const vecx_pred_t *pred,
                     int *out_ids, int max);
  /* 每分片子扫描工厂 */
  typedef ExecNode *(*px_shard_scan_fn)(int shard_id, void *ctx);
  /* 扇出节点：裁剪候选分片，每分片一个 worker（Exchange LOCAL，dop=候选数）。
     node_type = PLAN_SCAN_SEQ（对上层透明）。 */
  ExecNode *exec_create_shard_fanout(shard_router_t *router, const vecx_pred_t *pred,
                                     px_shard_scan_fn open_shard, void *ctx);
  ```
  旧 `exec_create_shard_scan(shard_coordinator_t*, key, key_len)` **删除**（全库无调用方）；`shard_coordinator_select_least_load` 保留给写路径。

- [ ] **Step 1: 写失败测试**

`engineering/test/db/executor/shard_prune_test.cpp`：

```cpp
#include <gtest/gtest.h>
#include <set>
#include <map>
#include <vector>

extern "C" {
#include "db/executor/exec_shard.h"
#include "db/executor/executor_framework.h"
#include "db/sharding/sharding.h"
#include "db/vectorized/vectorized.h"
#include "db/core/vector_types.h"
#include "db/core/columnar_store.h"
}

/* RANGE 分片 4 个：[0,250) [250,500) [500,750) [750,1000) */
static shard_router_t *make_router() {
    shard_config_t *cfg = shard_config_create(SHARD_RANGE, 4);
    cfg->key_type = SHARD_KEY_INT;
    shard_router_t *r = shard_router_create(cfg);
    shard_config_destroy(cfg);
    for (int i = 0; i < 4; i++) {
        shard_info_t *info = shard_info_create(i, "s");
        info->min_value = i * 250;
        info->max_value = (i + 1) * 250 - 1;
        info->host = strdup("127.0.0.1");
        info->port = 9000 + i;
        shard_router_add(r, info);
    }
    return r;
}

TEST(ShardPrune, EqualPredicateRoutesSingleShard) {
    shard_router_t *r = make_router();
    vecx_pred_t pred{};
    pred.op = CMP_EQ;
    pred.i64 = 300;                       /* 落在 [250,500) → shard 1 */
    int ids[4];
    int n = px_shard_prune(r, &pred, ids, 4);
    EXPECT_EQ(n, 1);
    EXPECT_EQ(ids[0], 1);
    shard_router_destroy(r);
}

TEST(ShardPrune, RangePredicatePrunesUnrelated) {
    shard_router_t *r = make_router();
    vecx_pred_t pred{};
    pred.op = CMP_LT;
    pred.i64 = 500;                       /* 只需 shard 0,1 */
    int ids[4];
    int n = px_shard_prune(r, &pred, ids, 4);
    EXPECT_EQ(n, 2);
    std::set<int> got(ids, ids + n);
    EXPECT_EQ(got, (std::set<int>{0, 1}));
    shard_router_destroy(r);
}

TEST(ShardPrune, NoPredicateFansOutAll) {
    shard_router_t *r = make_router();
    int ids[4];
    int n = px_shard_prune(r, nullptr, ids, 4);
    EXPECT_EQ(n, 4);
    shard_router_destroy(r);
}

/* ---- 扇出集成：每分片一个内存表，裁剪后只读入选分片 ---- */
struct FakeShardDb {
    std::map<int, std::vector<int32_t>> rows;   /* shard_id → 数据 */
    std::set<int> opened_shards;                 /* 记录实际被打开的 */
};

static ExecNode *open_fake_shard(int shard_id, void *vctx) {
    FakeShardDb *db = (FakeShardDb *)vctx;
    db->opened_shards.insert(shard_id);
    std::vector<int32_t> &rows = db->rows[shard_id];
    int col_types[] = {COLUMN_INT32};
    void *col_data[] = {rows.data()};
    int elem[] = {sizeof(int32_t)};
    return exec_create_seqscan(0, 1, col_types, col_data, elem,
                               (int64_t)rows.size(), 256);
}

TEST(ShardPrune, FanoutReadsOnlySelectedShards) {
    shard_router_t *r = make_router();
    FakeShardDb db;
    for (int s = 0; s < 4; s++)
        for (int i = 0; i < 100; i++)
            db.rows[s].push_back(s * 250 + i);   /* shard s 存 [s*250, s*250+99] */

    vecx_pred_t pred{};
    pred.op = CMP_LT;
    pred.i64 = 500;

    ExecNode *fan = exec_create_shard_fanout(r, &pred, open_fake_shard, &db);
    ASSERT_NE(fan, nullptr);
    ASSERT_EQ(fan->open(fan), 0);

    std::multiset<int> got;
    VectorBlock *b;
    while ((b = fan->next(fan)) != nullptr) {
        int32_t *col = (int32_t *)b->columns[0];
        for (int i = 0; i < b->num_rows; i++) got.insert(col[i]);
        vector_block_destroy(b);
    }
    fan->close(fan);
    exec_destroy(fan);

    EXPECT_EQ(db->opened_shards, (std::set<int>{0, 1}));   /* 裁剪生效 */
    EXPECT_EQ(got.size(), 200u);                            /* 两分片全量 */
    shard_router_destroy(r);
}
```

- [ ] **Step 2: 登记测试目标并确认编译失败**

```cmake
add_executable(shard_prune_test shard_prune_test.cpp)
target_link_libraries(shard_prune_test PRIVATE db_executor db_vectorized db_sharding gtest gtest_main)
target_include_directories(shard_prune_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/engineering/include
)
gtest_discover_tests(shard_prune_test)
```

若链接报 `db_sharding` 不存在，用 `grep -r "add_library" /d/code/book/engineering/src/db/sharding/CMakeLists.txt` 查实际库名替换。

Run: `ninja -C /d/code/book/engineering/build shard_prune_test`
Expected: FAIL — `px_shard_prune` / `exec_create_shard_fanout` 未定义

- [ ] **Step 3: 重写 exec_shard.h**

`engineering/include/db/executor/exec_shard.h` 全量替换：

```c
/**
 * @file exec_shard.h
 * @brief Gap#4 shard 读路径——谓词裁剪 + 每分片扇出
 *
 * px_shard_prune：按分片键谓词（int64 等值/范围）计算候选分片，
 *   谓词为空退化为全分片。内部接线 shard_route / shard_route_range（gap06）。
 * exec_create_shard_fanout：裁剪候选分片后，每分片经 open_shard 工厂建
 *   一个子扫描，全部喂给 Exchange（dop=候选数）——扇出与并行调度复用同一机制。
 * 写路由仍走 shard_coordinator_select_least_load（不在本头文件）。
 */
#ifndef DB_EXECUTOR_EXEC_SHARD_H
#define DB_EXECUTOR_EXEC_SHARD_H

#include "exec_node.h"
#include "db/sharding/sharding.h"
#include "db/vectorized/vectorized.h"

#ifdef __cplusplus
extern "C" {
#endif

int px_shard_prune(const shard_router_t *router, const vecx_pred_t *pred,
                   int *out_ids, int max);

typedef ExecNode *(*px_shard_scan_fn)(int shard_id, void *ctx);

ExecNode *exec_create_shard_fanout(shard_router_t *router, const vecx_pred_t *pred,
                                   px_shard_scan_fn open_shard, void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_EXEC_SHARD_H */
```

- [ ] **Step 4: 重写 shard_scan_exec.c**

`engineering/src/db/executor/operators/shard_scan_exec.c` 全量替换：

```c
/* shard_scan_exec.c - Gap#4 shard 裁剪 + 扇出（重写 gap06 骨架） */
#include "db/executor/exec_shard.h"
#include "db/executor/exec_exchange.h"
#include <stdlib.h>
#include <string.h>

int px_shard_prune(const shard_router_t *router, const vecx_pred_t *pred,
                   int *out_ids, int max) {
    if (!router || !out_ids || max <= 0) return 0;

    if (!pred) {
        /* 无分片键谓词：全分片扇出 */
        int total = shard_count(router);
        shard_info_t *all = (shard_info_t *)calloc((size_t)total, sizeof(shard_info_t));
        if (!all) return 0;
        int n = shard_get_all(router, all, total);
        int out = 0;
        for (int i = 0; i < n && out < max; i++) out_ids[out++] = all[i].shard_id;
        free(all);
        return out;
    }

    int64_t key = pred->i64;
    switch (pred->op) {
        case CMP_EQ: {
            int id = shard_route(router, &key, sizeof(key));
            if (id < 0) return 0;
            out_ids[0] = id;
            return 1;
        }
        case CMP_LT:  /* (-inf, key) */
        case CMP_LE: {
            int64_t lo = INT64_MIN;
            int64_t hi = (pred->op == CMP_LT) ? key - 1 : key;
            return shard_route_range(router, &lo, &hi, out_ids, max);
        }
        case CMP_GT:  /* (key, +inf) */
        case CMP_GE: {
            int64_t lo = (pred->op == CMP_GT) ? key + 1 : key;
            int64_t hi = INT64_MAX;
            return shard_route_range(router, &lo, &hi, out_ids, max);
        }
        default:
            /* NE 等无法裁剪：全分片 */
            return px_shard_prune(router, NULL, out_ids, max);
    }
}

/* ---- 扇出 ---- */

typedef struct {
    shard_router_t *router;
    vecx_pred_t pred;             /* 拷贝；has_pred=0 表示全分片 */
    int has_pred;
    px_shard_scan_fn open_shard;
    void *ctx;
    int shard_ids[256];
    int nshards;
    int next_idx;                 /* 工厂调用序号（open 内同步，无竞争） */
} ShardFanoutCtx;

static ExecNode *shard_subtree_fn(void *vctx) {
    ShardFanoutCtx *c = (ShardFanoutCtx *)vctx;
    if (c->next_idx >= c->nshards) return NULL;
    int shard_id = c->shard_ids[c->next_idx++];
    return c->open_shard(shard_id, c->ctx);
}

static void shard_fanout_ctx_destroy(void *vctx) {
    free(vctx);
}

ExecNode *exec_create_shard_fanout(shard_router_t *router, const vecx_pred_t *pred,
                                   px_shard_scan_fn open_shard, void *ctx) {
    if (!router || !open_shard) return NULL;

    ShardFanoutCtx *c = (ShardFanoutCtx *)calloc(1, sizeof(ShardFanoutCtx));
    if (!c) return NULL;
    c->router = router;
    c->open_shard = open_shard;
    c->ctx = ctx;
    if (pred) { c->pred = *pred; c->has_pred = 1; }

    c->nshards = px_shard_prune(router, pred, c->shard_ids, 256);
    if (c->nshards <= 0) { free(c); return NULL; }

    /* dop = 候选分片数；ctx_destroy 释放本闭包（Task 4 的 _ex 语义） */
    return exec_create_exchange_ex(shard_subtree_fn, c, c->nshards,
                                   shard_fanout_ctx_destroy);
}
```

- [ ] **Step 5: 构建并跑通测试（含旧引用检查）**

```bash
grep -rn "exec_create_shard_scan\|shard_coordinator_get_router" /d/code/book/engineering/src /d/code/book/engineering/include /d/code/book/engineering/test --include=*.c --include=*.h --include=*.cpp | grep -v shard_coordinator
```

Expected: 无输出（旧 API 无残留调用；若有则一并删除）

Run: `ninja -C /d/code/book/engineering/build shard_prune_test && /d/code/book/engineering/build/test/db/executor/shard_prune_test.exe`
Expected: 4 个 TEST 全部 PASSED

- [ ] **Step 6: Commit**

```bash
cd /d/code/book/engineering
git add include/db/executor/exec_shard.h src/db/executor/operators/shard_scan_exec.c test/db/executor/shard_prune_test.cpp test/db/executor/CMakeLists.txt
git commit -m "feat(gap04): shard 谓词裁剪 + 每分片扇出（接线 shard_route_range，重写 gap06 骨架）"
```

---

### Task 8: px_wire —— VectorBlock 线格式序列化（PXB1）

**Files:**
- Create: `engineering/include/db/executor/px_wire.h`
- Create: `engineering/src/db/executor/parallel/px_wire.c`
- Test: `engineering/test/db/executor/px_wire_test.cpp`
- Modify: `engineering/test/db/executor/CMakeLists.txt`

**Interfaces:**
- Consumes: `VectorBlock` + `vector_block_*` API；`rpc_crc32`（`include/db/distributed/rpc.h`）
- Produces:
  ```c
  #define PXW_FLAG_LAST 0x1u   /* 流结束标记 */
  #define PXW_FLAG_ERR  0x2u   /* 错误帧（payload 为错误消息字符串） */
  /* 序列化；b==NULL 且 flags!=0 → 纯标记帧。out 由调用方 free。返回 0/-1。 */
  int px_wire_serialize(const VectorBlock *b, uint32_t seq, uint32_t flags,
                        const char *err_msg, uint8_t **out, uint32_t *out_size);
  /* 反序列化；纯标记帧返回 NULL 且 *flags_out!=0。CRC 校验失败返回 NULL 且 *flags_out==0xFFFFFFFF。 */
  VectorBlock *px_wire_deserialize(const uint8_t *buf, uint32_t size,
                                   uint32_t *seq_out, uint32_t *flags_out);
  ```
  线格式（小端，按写序）：
  ```
  magic "PXB1"(4B) | version u8 | flags u32 | seq u32 | num_rows i32 | num_cols i32
  每列：type_tag i32 | elem_size i32 | data（定长=elem_size*num_rows；STRING=u32 总数=串数 + 每串 u32 len + bytes）
  null_bitmap：u32 nwords + nwords*u64
  CRC32 u32（以上全部字节的校验和，置尾）
  ```

- [ ] **Step 1: 写失败测试**

`engineering/test/db/executor/px_wire_test.cpp`：

```cpp
#include <gtest/gtest.h>
#include <vector>
#include <cstring>

extern "C" {
#include "db/executor/px_wire.h"
#include "db/core/vector_types.h"
#include "db/core/columnar_store.h"
}

static VectorBlock *make_mixed_block() {
    VectorBlock *b = vector_block_create(4, 3);
    int32_t *c0 = (int32_t *)malloc(sizeof(int32_t) * 4);
    int64_t *c1 = (int64_t *)malloc(sizeof(int64_t) * 4);
    const char **c2 = (const char **)malloc(sizeof(char *) * 4);
    for (int i = 0; i < 4; i++) { c0[i] = i * 7; c1[i] = 1000000LL * i; }
    c2[0] = "alpha"; c2[1] = "b"; c2[2] = ""; c2[3] = "delta-long-string";
    vector_block_set_column(b, 0, c0, sizeof(int32_t));
    vector_block_set_column_type(b, 0, COLUMN_INT32);
    vector_block_set_column(b, 1, c1, sizeof(int64_t));
    vector_block_set_column_type(b, 1, COLUMN_INT64);
    vector_block_set_column(b, 2, (void *)c2, sizeof(char *));
    vector_block_set_column_type(b, 2, COLUMN_STRING);
    vector_block_set_num_rows(b, 4);
    vector_block_set_null(b, 2, true);            /* 第 2 行整行 null */
    return b;
}

TEST(PxWire, RoundtripPreservesAllColumnKinds) {
    VectorBlock *src = make_mixed_block();
    uint8_t *buf = nullptr;
    uint32_t size = 0;
    ASSERT_EQ(px_wire_serialize(src, 42, 0, nullptr, &buf, &size), 0);
    ASSERT_NE(buf, nullptr);
    ASSERT_GT(size, 0u);

    uint32_t seq = 0, flags = 0;
    VectorBlock *dst = px_wire_deserialize(buf, size, &seq, &flags);
    ASSERT_NE(dst, nullptr);
    EXPECT_EQ(seq, 42u);
    EXPECT_EQ(flags, 0u);
    ASSERT_EQ(dst->num_rows, 4);
    ASSERT_EQ(dst->num_columns, 3);

    EXPECT_EQ(vector_block_get_column_type(dst, 0), COLUMN_INT32);
    EXPECT_EQ(vector_block_get_column_type(dst, 1), COLUMN_INT64);
    EXPECT_EQ(vector_block_get_column_type(dst, 2), COLUMN_STRING);

    int32_t *c0 = (int32_t *)dst->columns[0];
    int64_t *c1 = (int64_t *)dst->columns[1];
    const char **c2 = (const char **)dst->columns[2];
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(c0[i], i * 7);
        EXPECT_EQ(c1[i], 1000000LL * i);
    }
    EXPECT_STREQ(c2[0], "alpha");
    EXPECT_STREQ(c2[1], "b");
    EXPECT_STREQ(c2[2], "");
    EXPECT_STREQ(c2[3], "delta-long-string");
    EXPECT_TRUE(vector_block_is_null(dst, 2));
    EXPECT_FALSE(vector_block_is_null(dst, 1));

    vector_block_destroy(dst);
    vector_block_destroy(src);
    free(buf);
}

TEST(PxWire, LastFlagFrameHasNoBlock) {
    uint8_t *buf = nullptr;
    uint32_t size = 0;
    ASSERT_EQ(px_wire_serialize(nullptr, 7, PXW_FLAG_LAST, nullptr, &buf, &size), 0);
    uint32_t seq, flags;
    VectorBlock *b = px_wire_deserialize(buf, size, &seq, &flags);
    EXPECT_EQ(b, nullptr);
    EXPECT_EQ(seq, 7u);
    EXPECT_EQ(flags & PXW_FLAG_LAST, PXW_FLAG_LAST);
    free(buf);
}

TEST(PxWire, CorruptedCrcRejected) {
    VectorBlock *src = make_mixed_block();
    uint8_t *buf = nullptr;
    uint32_t size = 0;
    ASSERT_EQ(px_wire_serialize(src, 1, 0, nullptr, &buf, &size), 0);
    buf[size / 2] ^= 0xFF;                        /* 翻转中间一字节 */
    uint32_t seq, flags;
    VectorBlock *b = px_wire_deserialize(buf, size, &seq, &flags);
    EXPECT_EQ(b, nullptr);
    EXPECT_EQ(flags, 0xFFFFFFFFu);                /* CRC 失败哨兵 */
    free(buf);
    vector_block_destroy(src);
}
```

- [ ] **Step 2: 登记测试目标并确认编译失败**

```cmake
add_executable(px_wire_test px_wire_test.cpp)
target_link_libraries(px_wire_test PRIVATE db_executor db_vectorized db_distributed gtest gtest_main)
target_include_directories(px_wire_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/engineering/include
)
gtest_discover_tests(px_wire_test)
```

`rpc_crc32` 所在库名若不是 `db_distributed`，用 `grep -rn "rpc_crc32" /d/code/book/engineering/src/db/distributed --include=CMakeLists.txt -l` 与 `grep -rn "add_library" /d/code/book/engineering/src/db/distributed/CMakeLists.txt` 确认后替换；若嫌重，可在 px_wire.c 内实现私有 CRC32（表驱动，与 rpc 同多项式 0xEDB88320）而不链接 db_distributed——**推荐私有实现**（px_wire 是 executor 组件，不该反向依赖 distributed）。

Run: `ninja -C /d/code/book/engineering/build px_wire_test`
Expected: FAIL — `db/executor/px_wire.h: No such file or directory`

- [ ] **Step 3: 实现头文件**

`engineering/include/db/executor/px_wire.h`：

```c
/**
 * @file px_wire.h
 * @brief Gap#4 pxwire——VectorBlock 线格式（PXB1）
 *
 * 布局：头（magic/version/flags/seq/rows/cols）+ 每列（type/elem_size/data）
 * + 行级 null 位图 + CRC32 尾。小端。
 * 定长列（INT32/INT64/FLOAT/DOUBLE）：data = elem_size * num_rows。
 * 字符串列（COLUMN_STRING，char**）：u32 串数 + 每串 u32 len + 原始字节。
 * 纯标记帧（b==NULL）：只含头+CRC，用于 LAST/ERR。
 * 错误帧（flags&PXW_FLAG_ERR）：err_msg 以 u32 len + bytes 附于头后。
 */
#ifndef DB_EXECUTOR_PX_WIRE_H
#define DB_EXECUTOR_PX_WIRE_H

#include <stdint.h>
#include "db/core/vector_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PXW_FLAG_LAST 0x1u
#define PXW_FLAG_ERR  0x2u
#define PXW_DESER_CRC_FAIL 0xFFFFFFFFu

int px_wire_serialize(const VectorBlock *b, uint32_t seq, uint32_t flags,
                      const char *err_msg, uint8_t **out, uint32_t *out_size);
VectorBlock *px_wire_deserialize(const uint8_t *buf, uint32_t size,
                                 uint32_t *seq_out, uint32_t *flags_out);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_PX_WIRE_H */
```

- [ ] **Step 4: 实现序列化**

`engineering/src/db/executor/parallel/px_wire.c`：

```c
/* px_wire.c - PXB1 线格式（Gap#4） */
#include "db/executor/px_wire.h"
#include "db/core/columnar_store.h"
#include <stdlib.h>
#include <string.h>

#define PXW_MAGIC 0x31584250u   /* "PXB1" 小端 */
#define PXW_VERSION 1u

/* ---- 私有 CRC32（与 rpc.h 同多项式，避免反向依赖 db_distributed） ---- */
static uint32_t pxw_crc32(const uint8_t *data, size_t size) {
    static uint32_t table[256];
    static int table_init = 0;
    if (!table_init) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        table_init = 1;
    }
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; i++)
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

/* ---- 写游标 ---- */
typedef struct { uint8_t *buf; size_t cap, len; } wcur_t;

static int wput(wcur_t *w, const void *src, size_t n) {
    if (w->len + n > w->cap) {
        size_t ncap = w->cap ? w->cap * 2 : 4096;
        while (ncap < w->len + n) ncap *= 2;
        uint8_t *nb = (uint8_t *)realloc(w->buf, ncap);
        if (!nb) return -1;
        w->buf = nb;
        w->cap = ncap;
    }
    memcpy(w->buf + w->len, src, n);
    w->len += n;
    return 0;
}

static int wput_u32(wcur_t *w, uint32_t v) { return wput(w, &v, 4); }
static int wput_i32(wcur_t *w, int32_t v)  { return wput(w, &v, 4); }
static int wput_u8 (wcur_t *w, uint8_t v)  { return wput(w, &v, 1); }

int px_wire_serialize(const VectorBlock *b, uint32_t seq, uint32_t flags,
                      const char *err_msg, uint8_t **out, uint32_t *out_size) {
    if (!out || !out_size) return -1;
    *out = NULL;
    *out_size = 0;
    if (!b && !(flags & (PXW_FLAG_LAST | PXW_FLAG_ERR))) return -1;

    wcur_t w = {0};
    int rc = -1;
    do {
        if (wput_u32(&w, PXW_MAGIC) || wput_u8(&w, PXW_VERSION)
            || wput_u32(&w, flags) || wput_u32(&w, seq)) break;

        int32_t nrows = b ? b->num_rows : 0;
        int32_t ncols = b ? b->num_columns : 0;
        if (wput_i32(&w, nrows) || wput_i32(&w, ncols)) break;

        if (flags & PXW_FLAG_ERR) {
            uint32_t mlen = err_msg ? (uint32_t)strlen(err_msg) : 0;
            if (wput_u32(&w, mlen)) break;
            if (mlen && wput(&w, err_msg, mlen)) break;
        }

        if (b) {
            int nwords = (nrows + 63) / 64;
            for (int32_t c = 0; c < ncols; c++) {
                int32_t type = vector_block_get_column_type(b, c);
                int32_t esz = b->column_sizes[c];
                if (wput_i32(&w, type) || wput_i32(&w, esz)) goto done;
                if (type == COLUMN_STRING) {
                    const char **strs = (const char **)b->columns[c];
                    if (wput_u32(&w, (uint32_t)nrows)) goto done;
                    for (int32_t r = 0; r < nrows; r++) {
                        uint32_t slen = strs[r] ? (uint32_t)strlen(strs[r]) : 0;
                        if (wput_u32(&w, slen)) goto done;
                        if (slen && wput(&w, strs[r], slen)) goto done;
                    }
                } else {
                    if (wput(&w, b->columns[c], (size_t)esz * (size_t)nrows)) goto done;
                }
            }
            if (wput_u32(&w, (uint32_t)nwords)) goto done;
            if (nwords > 0) {
                if (b->null_bitmap) {
                    if (wput(&w, b->null_bitmap, (size_t)nwords * 8)) goto done;
                } else {
                    uint64_t zero = 0;
                    for (int i = 0; i < nwords; i++)
                        if (wput(&w, &zero, 8)) goto done;
                }
            }
        }

        uint32_t crc = pxw_crc32(w.buf, w.len);
        if (wput_u32(&w, crc)) break;
        rc = 0;
    } while (0);

done:
    if (rc == 0) {
        *out = w.buf;
        *out_size = (uint32_t)w.len;
    } else {
        free(w.buf);
    }
    return rc;
}

/* ---- 读游标 ---- */
typedef struct { const uint8_t *buf; size_t len, pos; } rcur_t;

static int rget(rcur_t *r, void *dst, size_t n) {
    if (r->pos + n > r->len) return -1;
    memcpy(dst, r->buf + r->pos, n);
    r->pos += n;
    return 0;
}

static int rget_u32(rcur_t *r, uint32_t *v) { return rget(r, v, 4); }
static int rget_i32(rcur_t *r, int32_t *v)  { return rget(r, v, 4); }
static int rget_u8 (rcur_t *r, uint8_t *v)  { return rget(r, v, 1); }

VectorBlock *px_wire_deserialize(const uint8_t *buf, uint32_t size,
                                 uint32_t *seq_out, uint32_t *flags_out) {
    if (!buf || size < 4 + 4 || !seq_out || !flags_out) return NULL;
    *flags_out = PXW_DESER_CRC_FAIL;

    /* CRC 校验（尾部 4 字节） */
    uint32_t stored_crc;
    memcpy(&stored_crc, buf + size - 4, 4);
    if (pxw_crc32(buf, size - 4) != stored_crc) return NULL;

    rcur_t r = {buf, size - 4, 0};
    uint32_t magic, flags, seq;
    uint8_t version;
    int32_t nrows, ncols;
    if (rget_u32(&r, &magic) || magic != PXW_MAGIC) return NULL;
    if (rget_u8(&r, &version) || version != PXW_VERSION) return NULL;
    if (rget_u32(&r, &flags) || rget_u32(&r, &seq)) return NULL;
    if (rget_i32(&r, &nrows) || rget_i32(&r, &ncols)) return NULL;

    *seq_out = seq;
    *flags_out = flags;

    if (flags & PXW_FLAG_ERR) {           /* 跳过错误消息体 */
        uint32_t mlen;
        if (rget_u32(&r, &mlen)) { *flags_out = PXW_DESER_CRC_FAIL; return NULL; }
        r.pos += mlen;
    }

    if (nrows <= 0 || ncols <= 0) return NULL;   /* 纯标记帧 */

    VectorBlock *b = vector_block_create(nrows, ncols);
    if (!b) { *flags_out = PXW_DESER_CRC_FAIL; return NULL; }

    for (int32_t c = 0; c < ncols; c++) {
        int32_t type, esz;
        if (rget_i32(&r, &type) || rget_i32(&r, &esz)) goto fail;
        vector_block_set_column_type(b, c, type);
        if (type == COLUMN_STRING) {
            uint32_t nstr;
            if (rget_u32(&r, &nstr) || nstr != (uint32_t)nrows) goto fail;
            char **strs = (char **)calloc((size_t)nrows, sizeof(char *));
            if (!strs) goto fail;
            for (int32_t i = 0; i < nrows; i++) {
                uint32_t slen;
                if (rget_u32(&r, &slen)) { free(strs); goto fail; }
                strs[i] = (char *)malloc(slen + 1);
                if (!strs[i]) { free(strs); goto fail; }
                if (rget(&r, strs[i], slen)) { free(strs[i]); free(strs); goto fail; }
                strs[i][slen] = '\0';
            }
            vector_block_set_column(b, c, strs, sizeof(char *));
        } else {
            void *col = malloc((size_t)esz * (size_t)nrows);
            if (!col) goto fail;
            if (rget(&r, col, (size_t)esz * (size_t)nrows)) { free(col); goto fail; }
            vector_block_set_column(b, c, col, esz);
        }
    }

    {
        uint32_t nwords;
        if (rget_u32(&r, &nwords)) goto fail;
        if (nwords > 0) {
            b->null_bitmap = (uint64_t *)calloc(nwords, 8);
            if (!b->null_bitmap) goto fail;
            if (rget(&r, b->null_bitmap, (size_t)nwords * 8)) goto fail;
        }
    }
    vector_block_set_num_rows(b, nrows);
    return b;

fail:
    vector_block_destroy(b);
    *flags_out = PXW_DESER_CRC_FAIL;
    return NULL;
}
```

**执行注意**：`vector_block_create` 是否预分配 `null_bitmap` 取决于 db_core 实现； deserialize 中显式 `calloc` 前应先检查 `b->null_bitmap` 是否已为 NULL——若非 NULL 先 `free` 再赋值，避免泄漏。STRING 列的错误路径（部分串已分配）在测试外可能泄漏：执行时把 `goto fail` 路径改为逐串释放循环（实现时补一个 `free_strings(strs, allocated_count)` 辅助函数）。

- [ ] **Step 5: 构建并跑通测试**

Run: `ninja -C /d/code/book/engineering/build px_wire_test && /d/code/book/engineering/build/test/db/executor/px_wire_test.exe`
Expected: 3 个 TEST 全部 PASSED

- [ ] **Step 6: Commit**

```bash
cd /d/code/book/engineering
git add include/db/executor/px_wire.h src/db/executor/parallel/px_wire.c test/db/executor/px_wire_test.cpp test/db/executor/CMakeLists.txt
git commit -m "feat(gap04): pxwire PXB1 线格式 —— 定长/字符串/null 位图 roundtrip + CRC 校验"
```

---

### Task 9: rpc_stream —— 流式 TCP 帧通道

**Files:**
- Create: `engineering/include/db/distributed/rpc_stream.h`
- Create: `engineering/src/db/distributed/rpc/rpc_stream.c`
- Test: `engineering/test/db/executor/rpc_stream_test.cpp`
- Modify: `engineering/test/db/executor/CMakeLists.txt` + `engineering/src/db/distributed/rpc/CMakeLists.txt`（若显式列源文件则追加 rpc_stream.c；glob 则免改）

**Interfaces:**
- Consumes: 标准 socket（Winsock2 / POSIX）；`rpc_node_address_t`（rpc.h）
- Produces:
  ```c
  /* 帧类型 */
  #define RPCS_FRAME_DATA 0x05u
  #define RPCS_FRAME_END  0x06u
  typedef struct rpc_stream rpc_stream_t;
  /* 发送方：连到 addr（专用 socket，不走连接池） */
  rpc_stream_t *rpc_stream_connect(const rpc_node_address_t *addr);
  /* 接收方：监听 + 每连接一帧回调（后台线程） */
  typedef void (*rpcs_frame_cb)(uint8_t frame_type, const uint8_t *data,
                                uint32_t size, void *ctx);
  typedef struct rpcs_listener rpcs_listener_t;
  rpcs_listener_t *rpcs_listen(const rpc_node_address_t *bind_addr,
                               rpcs_frame_cb cb, void *ctx);
  int  rpc_stream_send(rpc_stream_t *s, uint8_t frame_type,
                       const void *data, uint32_t size);   /* 0/-1 */
  void rpc_stream_close(rpc_stream_t *s);                   /* 自动发 END 帧 */
  void rpcs_listener_stop(rpcs_listener_t *l);
  ```
  帧格式（复用 rpc 风格）：`magic "RPS1" u32 | frame_type u8 | payload_size u32 | payload | crc32 u32`

- [ ] **Step 1: 写失败测试**

`engineering/test/db/executor/rpc_stream_test.cpp`：

```cpp
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <mutex>

extern "C" {
#include "db/distributed/rpc_stream.h"
#include "db/distributed/rpc.h"
}

struct Sink {
    std::mutex mu;
    std::vector<std::vector<uint8_t>> frames;
    std::atomic<int> ended{0};
};

static void sink_cb(uint8_t type, const uint8_t *data, uint32_t size, void *vctx) {
    Sink *s = (Sink *)vctx;
    if (type == RPCS_FRAME_END) { s->ended++; return; }
    std::lock_guard<std::mutex> lk(s->mu);
    s->frames.emplace_back(data, data + size);
}

TEST(RpcStream, FramesArriveInOrder) {
    Sink sink;
    rpc_node_address_t addr{};
    strcpy(addr.host, "127.0.0.1");
    addr.port = 19571;
    addr.node_id = 1;

    rpcs_listener_t *l = rpcs_listen(&addr, sink_cb, &sink);
    ASSERT_NE(l, nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));   /* 等监听就绪 */

    rpc_stream_t *s = rpc_stream_connect(&addr);
    ASSERT_NE(s, nullptr);

    for (uint32_t i = 0; i < 100; i++) {
        uint32_t payload[64];
        for (int j = 0; j < 64; j++) payload[j] = i * 64 + j;
        ASSERT_EQ(rpc_stream_send(s, RPCS_FRAME_DATA, payload, sizeof(payload)), 0);
    }
    rpc_stream_close(s);   /* 自动发 END */

    for (int i = 0; i < 50 && sink.ended == 0; i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(sink.ended.load(), 1);

    std::lock_guard<std::mutex> lk(sink.mu);
    ASSERT_EQ(sink.frames.size(), 100u);
    for (uint32_t i = 0; i < 100; i++) {
        const uint32_t *p = (const uint32_t *)sink.frames[i].data();
        ASSERT_EQ(sink.frames[i].size(), 256u);
        EXPECT_EQ(p[0], i * 64);
        EXPECT_EQ(p[63], i * 64 + 63);
    }
    rpcs_listener_stop(l);
}

TEST(RpcStream, ConnectRefusedReturnsNull) {
    rpc_node_address_t addr{};
    strcpy(addr.host, "127.0.0.1");
    addr.port = 19599;                 /* 无监听 */
    addr.node_id = 99;
    EXPECT_EQ(rpc_stream_connect(&addr), nullptr);
}
```

- [ ] **Step 2: 登记测试目标并确认编译失败**

```cmake
add_executable(rpc_stream_test rpc_stream_test.cpp)
target_link_libraries(rpc_stream_test PRIVATE db_executor db_vectorized gtest gtest_main)
target_include_directories(rpc_stream_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/engineering/include
)
gtest_discover_tests(rpc_stream_test)
```

rpc_stream.c 编译入哪个库：优先并入 `db_executor` 之外——检查 `src/db/distributed/rpc/CMakeLists.txt`：若是 glob 则零改动自动入 distributed 库，测试目标需补链该库名；若是显式源列表则把 `rpc_stream.c` 追加进去。Windows 下 socket 需链接 `ws2_32`：在测试目标（或 rpc 库）`target_link_libraries` 追加 `ws2_32`（参照 `src/db/ecosystem/CMakeLists.txt` 中 rest_server 的链接方式）。

Run: `ninja -C /d/code/book/engineering/build rpc_stream_test`
Expected: FAIL — `db/distributed/rpc_stream.h: No such file or directory`

- [ ] **Step 3: 实现头文件**

`engineering/include/db/distributed/rpc_stream.h`：

```c
/**
 * @file rpc_stream.h
 * @brief Gap#4 流式 TCP 帧通道——pxwire 数据面的传输层
 *
 * 独立于 rpc.h 连接池：发送方一条专用 socket 顺序发帧，
 * 接收方监听线程 accept 后按帧回调。帧含 CRC32。
 * close 自动发送 RPCS_FRAME_END 通知对端流结束。
 */
#ifndef DB_DISTRIBUTED_RPC_STREAM_H
#define DB_DISTRIBUTED_RPC_STREAM_H

#include <stdint.h>
#include "rpc.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RPCS_FRAME_DATA 0x05u
#define RPCS_FRAME_END  0x06u

typedef struct rpc_stream rpc_stream_t;
typedef struct rpcs_listener rpcs_listener_t;

typedef void (*rpcs_frame_cb)(uint8_t frame_type, const uint8_t *data,
                              uint32_t size, void *ctx);

rpc_stream_t    *rpc_stream_connect(const rpc_node_address_t *addr);
int              rpc_stream_send(rpc_stream_t *s, uint8_t frame_type,
                                 const void *data, uint32_t size);
void             rpc_stream_close(rpc_stream_t *s);

rpcs_listener_t *rpcs_listen(const rpc_node_address_t *bind_addr,
                             rpcs_frame_cb cb, void *ctx);
void             rpcs_listener_stop(rpcs_listener_t *l);

#ifdef __cplusplus
}
#endif

#endif /* DB_DISTRIBUTED_RPC_STREAM_H */
```

- [ ] **Step 4: 实现流通道**

`engineering/src/db/distributed/rpc/rpc_stream.c`：

```c
/* rpc_stream.c - 流式 TCP 帧通道（Gap#4） */
#include "db/distributed/rpc_stream.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#define RPCS_INVALID INVALID_SOCKET
typedef SOCKET rpcs_fd_t;
static int rpcs_close_fd(rpcs_fd_t fd) { return closesocket(fd); }
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define RPCS_INVALID (-1)
typedef int rpcs_fd_t;
static int rpcs_close_fd(rpcs_fd_t fd) { return close(fd); }
#endif

#define RPCS_MAGIC 0x31535052u   /* "RPS1" 小端 */
#define RPCS_HDR_SIZE 13         /* magic4 + type1 + size4 + (crc 在尾) */

/* CRC32 与 px_wire 同源（多项式 0xEDB88320） */
static uint32_t rpcs_crc32(const uint8_t *data, size_t size) {
    static uint32_t table[256];
    static int init = 0;
    if (!init) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        init = 1;
    }
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; i++)
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

static void rpcs_wsa_init(void) {
#ifdef _WIN32
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    static void (*init_fn)(void) = NULL;
    (void)init_fn;
    /* winsock 初始化（幂等） */
    WSADATA wsa;
    static int started = 0;
    if (!started) { WSAStartup(MAKEWORD(2, 2), &wsa); started = 1; }
    (void)once;
#endif
}

/* ---- 发送方 ---- */

struct rpc_stream {
    rpcs_fd_t fd;
    pthread_mutex_t send_mu;   /* 多 worker 共享一条流时串行化整帧 */
};

rpc_stream_t *rpc_stream_connect(const rpc_node_address_t *addr) {
    if (!addr) return NULL;
    rpcs_wsa_init();

    rpcs_fd_t fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == RPCS_INVALID) return NULL;

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(addr->port);
#ifdef _WIN32
    sa.sin_addr.s_addr = inet_addr(addr->host);
#else
    inet_pton(AF_INET, addr->host, &sa.sin_addr);
#endif

    if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0) {
        rpcs_close_fd(fd);
        return NULL;
    }

    rpc_stream_t *s = (rpc_stream_t *)calloc(1, sizeof(rpc_stream_t));
    if (!s) { rpcs_close_fd(fd); return NULL; }
    s->fd = fd;
    pthread_mutex_init(&s->send_mu, NULL);
    return s;
}

static int send_all(rpcs_fd_t fd, const uint8_t *buf, size_t n) {
    size_t sent = 0;
    while (sent < n) {
        int rc = send(fd, (const char *)buf + sent, (int)(n - sent), 0);
        if (rc <= 0) return -1;
        sent += (size_t)rc;
    }
    return 0;
}

int rpc_stream_send(rpc_stream_t *s, uint8_t frame_type,
                    const void *data, uint32_t size) {
    if (!s || (size > 0 && !data)) return -1;
    pthread_mutex_lock(&s->send_mu);

    uint8_t hdr[9];
    uint32_t magic = RPCS_MAGIC;
    memcpy(hdr, &magic, 4);
    hdr[4] = frame_type;
    memcpy(hdr + 5, &size, 4);

    uint32_t crc = rpcs_crc32((const uint8_t *)data, size);

    int rc = 0;
    if (send_all(s->fd, hdr, 9) != 0) rc = -1;
    if (rc == 0 && size > 0 && send_all(s->fd, (const uint8_t *)data, size) != 0) rc = -1;
    if (rc == 0 && send_all(s->fd, (const uint8_t *)&crc, 4) != 0) rc = -1;

    pthread_mutex_unlock(&s->send_mu);
    return rc;
}

void rpc_stream_close(rpc_stream_t *s) {
    if (!s) return;
    rpc_stream_send(s, RPCS_FRAME_END, NULL, 0);
    rpcs_close_fd(s->fd);
    pthread_mutex_destroy(&s->send_mu);
    free(s);
}

/* ---- 接收方 ---- */

struct rpcs_listener {
    rpcs_fd_t listen_fd;
    rpcs_frame_cb cb;
    void *ctx;
    pthread_t thread;
    volatile int stop;
};

static int recv_all(rpcs_fd_t fd, uint8_t *buf, size_t n) {
    size_t got = 0;
    while (got < n) {
        int rc = recv(fd, (char *)buf + got, (int)(n - got), 0);
        if (rc <= 0) return -1;
        got += (size_t)rc;
    }
    return 0;
}

static void rpcs_serve_conn(rpcs_fd_t cfd, rpcs_frame_cb cb, void *ctx) {
    for (;;) {
        uint8_t hdr[9];
        if (recv_all(cfd, hdr, 9) != 0) break;
        uint32_t magic, size;
        memcpy(&magic, hdr, 4);
        memcpy(&size, hdr + 5, 4);
        if (magic != RPCS_MAGIC || size > RPC_MAX_MESSAGE_SIZE) break;

        uint8_t *payload = NULL;
        if (size > 0) {
            payload = (uint8_t *)malloc(size);
            if (!payload) break;
            if (recv_all(cfd, payload, size) != 0) { free(payload); break; }
        }
        uint32_t crc;
        if (recv_all(cfd, (uint8_t *)&crc, 4) != 0) { free(payload); break; }
        if (rpcs_crc32(payload, size) != crc) { free(payload); break; }

        cb(hdr[4], payload, size, ctx);
        free(payload);
        if (hdr[4] == RPCS_FRAME_END) break;
    }
    rpcs_close_fd(cfd);
}

static void *rpcs_accept_loop(void *varg) {
    rpcs_listener_t *l = (rpcs_listener_t *)varg;
    while (!l->stop) {
        rpcs_fd_t cfd = accept(l->listen_fd, NULL, NULL);
        if (cfd == RPCS_INVALID) break;    /* stop 时 listen_fd 被关 */
        rpcs_serve_conn(cfd, l->cb, l->ctx);
    }
    return NULL;
}

rpcs_listener_t *rpcs_listen(const rpc_node_address_t *bind_addr,
                             rpcs_frame_cb cb, void *ctx) {
    if (!bind_addr || !cb) return NULL;
    rpcs_wsa_init();

    rpcs_fd_t fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == RPCS_INVALID) return NULL;
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one));

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(bind_addr->port);
#ifdef _WIN32
    sa.sin_addr.s_addr = inet_addr(bind_addr->host);
#else
    inet_pton(AF_INET, bind_addr->host, &sa.sin_addr);
#endif

    if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0
        || listen(fd, 16) != 0) {
        rpcs_close_fd(fd);
        return NULL;
    }

    rpcs_listener_t *l = (rpcs_listener_t *)calloc(1, sizeof(rpcs_listener_t));
    if (!l) { rpcs_close_fd(fd); return NULL; }
    l->listen_fd = fd;
    l->cb = cb;
    l->ctx = ctx;
    l->stop = 0;
    if (pthread_create(&l->thread, NULL, rpcs_accept_loop, l) != 0) {
        rpcs_close_fd(fd);
        free(l);
        return NULL;
    }
    return l;
}

void rpcs_listener_stop(rpcs_listener_t *l) {
    if (!l) return;
    l->stop = 1;
    rpcs_close_fd(l->listen_fd);           /* 唤醒阻塞的 accept */
    pthread_join(l->thread, NULL);
    free(l);
}
```

**执行注意**：`rpcs_wsa_init` 的 `static void (*init_fn)` 死代码是笔误，执行时简化为：

```c
static void rpcs_wsa_init(void) {
#ifdef _WIN32
    static int started = 0;
    if (!started) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        started = 1;
    }
#endif
}
```

（测试是单线程初始化，静态标志足够；多实例并发时 WSAStartup 本身幂等。）

- [ ] **Step 5: 构建并跑通测试**

Run: `ninja -C /d/code/book/engineering/build rpc_stream_test && /d/code/book/engineering/build/test/db/executor/rpc_stream_test.exe`
Expected: 2 个 TEST 全部 PASSED（FramesArriveInOrder 含 100 帧×256B 有序到达 + END 语义）

- [ ] **Step 6: Commit**

```bash
cd /d/code/book/engineering
git add include/db/distributed/rpc_stream.h src/db/distributed/rpc/rpc_stream.c test/db/executor/rpc_stream_test.cpp test/db/executor/CMakeLists.txt src/db/distributed/rpc/CMakeLists.txt
git commit -m "feat(gap04): rpc_stream 流式帧通道 —— 专用 socket + CRC 分帧 + END 语义"
```

---

### Task 10: Exchange 网络模式 —— sender / receiver

**Files:**
- Modify: `engineering/include/db/executor/exec_exchange.h`（追加网络 API）
- Create: `engineering/src/db/executor/parallel/exchange_net_exec.c`
- Test: `engineering/test/db/executor/exchange_net_test.cpp`
- Modify: `engineering/test/db/executor/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 3 Exchange 语义；Task 8 px_wire；Task 9 rpc_stream
- Produces:
  ```c
  /* sender：把子树产出序列化后经 rpc_stream 发向 addr；LAST 帧收尾。
     对上层呈现为"无产出的 scan"（next 恒 NULL），驱动在 open/next 中发送。 */
  ExecNode *exec_create_exchange_sender(px_subtree_fn make_subtree, void *ctx,
                                        const rpc_node_address_t *addr);
  /* receiver：绑定 bind_addr 收流，解包后投入内部队列；next() 拉块。
     LAST 帧=EOF；ERR 帧=is_aborted 置位。 */
  ExecNode *exec_create_exchange_receiver(const rpc_node_address_t *bind_addr);
  ```
  sender/receiver 的 `node_type` 均为 `PLAN_EXCHANGE`。

- [ ] **Step 1: 写失败测试**

`engineering/test/db/executor/exchange_net_test.cpp`：

```cpp
#include <gtest/gtest.h>
#include <set>
#include <vector>
#include <thread>
#include <chrono>

extern "C" {
#include "db/executor/exec_exchange.h"
#include "db/executor/exec_operators.h"
#include "db/executor/executor_framework.h"
#include "db/executor/px_queue.h"
#include "db/distributed/rpc.h"
#include "db/core/vector_types.h"
#include "db/core/columnar_store.h"
}

static std::vector<void *> &net_buffers() {
    static std::vector<void *> bufs;
    return bufs;
}

static ExecNode *make_scan_rows(int base, int rows, int batch) {
    int32_t *col = (int32_t *)malloc(sizeof(int32_t) * rows);
    for (int i = 0; i < rows; i++) col[i] = base + i;
    net_buffers().push_back(col);
    int col_types[] = {COLUMN_INT32};
    void *col_data[] = {col};
    int elem[] = {sizeof(int32_t)};
    return exec_create_seqscan(0, 1, col_types, col_data, elem, rows, batch);
}

struct NetScanCtx { int base; int rows; int batch; };
static ExecNode *net_scan_fn(void *vctx) {
    NetScanCtx *c = (NetScanCtx *)vctx;
    return make_scan_rows(c->base, c->rows, c->batch);
}

class ExchangeNetTest : public ::testing::Test {
protected:
    void TearDown() override {
        for (void *p : net_buffers()) free(p);
        net_buffers().clear();
    }
};

TEST_F(ExchangeNetTest, SenderToReceiverLoopback) {
    const int kRows = 5000;
    rpc_node_address_t addr{};
    strcpy(addr.host, "127.0.0.1");
    addr.port = 19601;
    addr.node_id = 2;

    ExecNode *recv = exec_create_exchange_receiver(&addr);
    ASSERT_NE(recv, nullptr);
    ASSERT_EQ(recv->open(recv), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    NetScanCtx sctx{0, kRows, 512};
    ExecNode *send = exec_create_exchange_sender(net_scan_fn, &sctx, &addr);
    ASSERT_NE(send, nullptr);
    ASSERT_EQ(send->open(send), 0);
    while (send->next(send) != nullptr) {}   /* 驱动发送直到 EOF */
    send->close(send);
    exec_destroy(send);

    std::multiset<int> got;
    VectorBlock *b;
    while ((b = recv->next(recv)) != nullptr) {
        int32_t *col = (int32_t *)b->columns[0];
        for (int i = 0; i < b->num_rows; i++) got.insert(col[i]);
        vector_block_destroy(b);
    }
    recv->close(recv);
    exec_destroy(recv);

    ASSERT_EQ(got.size(), (size_t)kRows);
    for (int i = 0; i < kRows; i++) EXPECT_EQ(got.count(i), 1u);
}

TEST_F(ExchangeNetTest, ReceiverReportsSenderAbort) {
    rpc_node_address_t addr{};
    strcpy(addr.host, "127.0.0.1");
    addr.port = 19602;
    addr.node_id = 3;

    ExecNode *recv = exec_create_exchange_receiver(&addr);
    ASSERT_NE(recv, nullptr);
    ASSERT_EQ(recv->open(recv), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    /* 模拟故障：连上后直接断开（不发 END） */
    rpc_stream_t *raw = rpc_stream_connect(&addr);
    ASSERT_NE(raw, nullptr);
    /* 强杀：不走 close（不发 END 帧），直接断 fd */
#ifdef _WIN32
    /* 通过 closesocket 模拟崩溃——需要 raw socket 访问；
       rpc_stream_close 会发 END，故这里用 shutdown 等价物：
       直接 close 而不发 END 需要 rpc_stream 提供 abort 接口 */
    rpc_stream_abort(raw);
#else
    rpc_stream_abort(raw);
#endif

    VectorBlock *b = recv->next(recv);
    EXPECT_EQ(b, nullptr);                        /* EOF/abort，不挂死 */
    recv->close(recv);
    exec_destroy(recv);
}
```

**rpc_stream 需补 abort 接口**（`rpc_stream.h` 追加，语义=不发 END 直接断连，模拟节点崩溃）：

```c
void rpc_stream_abort(rpc_stream_t *s);   /* 立即关 fd，不发 END 帧 */
```

实现：`{ if (!s) return; rpcs_close_fd(s->fd); pthread_mutex_destroy(&s->send_mu); free(s); }`

- [ ] **Step 2: 登记测试目标并确认编译失败**

```cmake
add_executable(exchange_net_test exchange_net_test.cpp)
target_link_libraries(exchange_net_test PRIVATE db_executor db_vectorized gtest gtest_main)
target_include_directories(exchange_net_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/engineering/include
)
gtest_discover_tests(exchange_net_test)
```

Run: `ninja -C /d/code/book/engineering/build exchange_net_test`
Expected: FAIL — `exec_create_exchange_sender` 未定义

- [ ] **Step 3: 追加头文件 API**

`engineering/include/db/executor/exec_exchange.h` 追加：

```c
#include "db/distributed/rpc.h"

/* 网络模式（Task 10） */
ExecNode *exec_create_exchange_sender(px_subtree_fn make_subtree, void *ctx,
                                      const rpc_node_address_t *addr);
ExecNode *exec_create_exchange_receiver(const rpc_node_address_t *bind_addr);
```

- [ ] **Step 4: 实现 sender / receiver**

`engineering/src/db/executor/parallel/exchange_net_exec.c`：

```c
/* exchange_net_exec.c - Exchange 网络模式（Gap#4） */
#include "db/executor/exec_exchange.h"
#include "db/executor/executor_framework.h"
#include "db/executor/px_queue.h"
#include "db/executor/px_wire.h"
#include "db/distributed/rpc_stream.h"
#include "db/optimizer/optimizer.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* ---- sender ---- */

typedef struct {
    px_subtree_fn make_subtree;
    void *ctx;
    rpc_node_address_t addr;
    rpc_stream_t *stream;
    ExecNode *sub;
    uint32_t seq;
    int failed;
} SenderState;

static int sender_open(ExecNode *node) {
    SenderState *st = (SenderState *)node->state;
    if (!st || !st->make_subtree) return -1;
    st->sub = st->make_subtree(st->ctx);
    if (!st->sub) return -1;
    if (st->sub->open(st->sub) != 0) return -1;
    st->stream = rpc_stream_connect(&st->addr);
    if (!st->stream) return -1;
    return 0;
}

static VectorBlock *sender_next(ExecNode *node) {
    SenderState *st = (SenderState *)node->state;
    if (!st || !st->sub || st->failed) return NULL;

    VectorBlock *b;
    while ((b = st->sub->next(st->sub)) != NULL) {
        uint8_t *buf = NULL;
        uint32_t size = 0;
        if (px_wire_serialize(b, st->seq, 0, NULL, &buf, &size) != 0) {
            vector_block_destroy(b);
            st->failed = 1;
            return NULL;
        }
        vector_block_destroy(b);
        int rc = rpc_stream_send(st->stream, RPCS_FRAME_DATA, buf, size);
        free(buf);
        if (rc != 0) { st->failed = 1; return NULL; }
        st->seq++;
    }

    /* 子树 EOF：发 LAST 标记帧（close 的 END 帧随后） */
    uint8_t *buf = NULL;
    uint32_t size = 0;
    if (px_wire_serialize(NULL, st->seq, PXW_FLAG_LAST, NULL, &buf, &size) == 0) {
        rpc_stream_send(st->stream, RPCS_FRAME_DATA, buf, size);
        free(buf);
    }
    return NULL;   /* sender 对上层无产出 */
}

static void sender_reset(ExecNode *node) { (void)node; }

static void sender_close(ExecNode *node) {
    SenderState *st = (SenderState *)node->state;
    if (!st) return;
    if (st->sub) {
        st->sub->close(st->sub);
        exec_destroy(st->sub);
        st->sub = NULL;
    }
    if (st->stream) {
        rpc_stream_close(st->stream);   /* 自动 END 帧 */
        st->stream = NULL;
    }
}

ExecNode *exec_create_exchange_sender(px_subtree_fn make_subtree, void *ctx,
                                      const rpc_node_address_t *addr) {
    if (!make_subtree || !addr) return NULL;
    SenderState *st = (SenderState *)calloc(1, sizeof(SenderState));
    if (!st) return NULL;
    st->make_subtree = make_subtree;
    st->ctx = ctx;
    st->addr = *addr;

    ExecNode *node = (ExecNode *)calloc(1, sizeof(ExecNode));
    if (!node) { free(st); return NULL; }
    node->node_type = PLAN_EXCHANGE;
    node->state = st;
    node->open = sender_open;
    node->next = sender_next;
    node->reset = sender_reset;
    node->close = sender_close;
    return node;
}

/* ---- receiver ---- */

typedef struct {
    rpc_node_address_t bind_addr;
    rpcs_listener_t *listener;
    px_queue_t *queue;       /* 单生产者（监听线程）语义：nproducers=1 */
    volatile int aborted;
} ReceiverState;

static void receiver_frame_cb(uint8_t frame_type, const uint8_t *data,
                              uint32_t size, void *vctx) {
    ReceiverState *st = (ReceiverState *)vctx;
    if (frame_type == RPCS_FRAME_END) {
        px_queue_producer_done(st->queue);
        return;
    }
    uint32_t seq, flags;
    VectorBlock *b = px_wire_deserialize(data, size, &seq, &flags);
    if (flags == PXW_DESER_CRC_FAIL || (flags & PXW_FLAG_ERR)) {
        st->aborted = 1;
        px_queue_abort(st->queue);
        return;
    }
    if (flags & PXW_FLAG_LAST) {
        px_queue_producer_done(st->queue);
        return;
    }
    if (b) {
        if (px_queue_push(st->queue, b) != 0) {
            vector_block_destroy(b);
        }
    }
}

static int receiver_open(ExecNode *node) {
    ReceiverState *st = (ReceiverState *)node->state;
    if (!st) return -1;
    st->queue = px_queue_create(64, 1);
    if (!st->queue) return -1;
    st->listener = rpcs_listen(&st->bind_addr, receiver_frame_cb, st);
    if (!st->listener) {
        px_queue_destroy(st->queue);
        st->queue = NULL;
        return -1;
    }
    return 0;
}

static VectorBlock *receiver_next(ExecNode *node) {
    ReceiverState *st = (ReceiverState *)node->state;
    if (!st || !st->queue) return NULL;
    return px_queue_pop(st->queue);   /* LAST/END→EOF；ERR/CRC→abort→NULL */
}

static void receiver_reset(ExecNode *node) { (void)node; }

static void receiver_close(ExecNode *node) {
    ReceiverState *st = (ReceiverState *)node->state;
    if (!st) return;
    if (st->listener) {
        rpcs_listener_stop(st->listener);
        st->listener = NULL;
    }
    if (st->queue) {
        px_queue_destroy(st->queue);   /* 残余块回收 */
        st->queue = NULL;
    }
}

ExecNode *exec_create_exchange_receiver(const rpc_node_address_t *bind_addr) {
    if (!bind_addr) return NULL;
    ReceiverState *st = (ReceiverState *)calloc(1, sizeof(ReceiverState));
    if (!st) return NULL;
    st->bind_addr = *bind_addr;

    ExecNode *node = (ExecNode *)calloc(1, sizeof(ExecNode));
    if (!node) { free(st); return NULL; }
    node->node_type = PLAN_EXCHANGE;
    node->state = st;
    node->open = receiver_open;
    node->next = receiver_next;
    node->reset = receiver_reset;
    node->close = receiver_close;
    return node;
}
```

**注意**：`receiver_frame_cb` 在监听线程上下文执行，`px_queue_push` 可能因队列满而阻塞监听线程——这是有意的背压传导（TCP 窗口随之收紧）。

- [ ] **Step 5: 构建并跑通测试**

Run: `ninja -C /d/code/book/engineering/build exchange_net_test && /d/code/book/engineering/build/test/db/executor/exchange_net_test.exe`
Expected: 2 个 TEST 全部 PASSED

- [ ] **Step 6: Commit**

```bash
cd /d/code/book/engineering
git add include/db/executor/exec_exchange.h src/db/executor/parallel/exchange_net_exec.c include/db/distributed/rpc_stream.h src/db/distributed/rpc/rpc_stream.c test/db/executor/exchange_net_test.cpp test/db/executor/CMakeLists.txt
git commit -m "feat(gap04): Exchange 网络模式 —— sender/receiver 跨节点搬运 VectorBlock"
```

---

### Task 11: 分布式 Join —— BROADCAST 与 REPARTITION（双实例验收）

**Files:**
- Test: `engineering/test/db/executor/distributed_join_test.cpp`
- Modify: `engineering/test/db/executor/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 5 并行 join 语义；Task 10 sender/receiver；`vecx_hashjoin_*`
- Produces: 双实例 localhost 分布式 Join 验收证据（无新增生产 API——Join 策略=Exchange 组合）

**组合说明（执行者须知）**：
- **BROADCAST**：小表所在节点跑 sender → 本节点 receiver 收全量小表 → 串行建 `vecx_hashjoin_t` → 本地大表 probe。网络开销=小表。
- **REPARTITION**：两侧节点各自把 probe 数据按 `hash(key) % 2` 分流——key 归属本节点的部分本地 join，归属对端的部分经 sender 发给对端 receiver；两节点各自完成自己那半 join，结果并集=全集。本测试在单进程双 listener/双 stream（不同端口）上演示，即"双实例"（两地址、两数据面、两执行上下文）。

- [ ] **Step 1: 写分布式 Join 测试**

`engineering/test/db/executor/distributed_join_test.cpp`：

```cpp
#include <gtest/gtest.h>
#include <set>
#include <vector>
#include <thread>
#include <chrono>

extern "C" {
#include "db/executor/exec_exchange.h"
#include "db/executor/exec_operators.h"
#include "db/executor/executor_framework.h"
#include "db/vectorized/vectorized.h"
#include "db/distributed/rpc.h"
#include "db/core/vector_types.h"
#include "db/core/columnar_store.h"
}

/* ---------- 工具 ---------- */
static std::vector<void *> &dj_buffers() {
    static std::vector<void *> bufs;
    return bufs;
}

static ExecNode *make_kv_scan(const int32_t *keys, const int32_t *vals, int n, int batch) {
    int32_t *k = (int32_t *)malloc(sizeof(int32_t) * n);
    int32_t *v = (int32_t *)malloc(sizeof(int32_t) * n);
    memcpy(k, keys, sizeof(int32_t) * n);
    memcpy(v, vals, sizeof(int32_t) * n);
    dj_buffers().push_back(k);
    dj_buffers().push_back(v);
    int col_types[] = {COLUMN_INT32, COLUMN_INT32};
    void *col_data[] = {k, v};
    int elem[] = {sizeof(int32_t), sizeof(int32_t)};
    return exec_create_seqscan(0, 2, col_types, col_data, elem, n, batch);
}

struct ScanFnCtx { const int32_t *k, *v; int n, batch; };
static ExecNode *scan_fn(void *p) {
    ScanFnCtx *c = (ScanFnCtx *)p;
    return make_kv_scan(c->k, c->v, c->n, c->batch);
}

using RowSet = std::multiset<std::pair<int32_t, int32_t>>;

/* 从 join 输出块收集 (build_val, probe_val) */
static void collect_join_rows(VectorBlock *out, RowSet &dst) {
    int32_t *bv = (int32_t *)out->columns[1];
    int32_t *pv = (int32_t *)out->columns[3];
    for (int r = 0; r < out->num_rows; r++) dst.insert({bv[r], pv[r]});
}

/* 串行基线：本地 vecx_hashjoin */
static RowSet baseline_join(const std::vector<int32_t> &bk, const std::vector<int32_t> &bv,
                            const std::vector<int32_t> &pk, const std::vector<int32_t> &pv) {
    RowSet result;
    vecx_hashjoin_t *hj = vecx_hashjoin_create(0, 0);
    ExecNode *bs = make_kv_scan(bk.data(), bv.data(), (int)bk.size(), (int)bk.size());
    bs->open(bs);
    VectorBlock *b;
    while ((b = bs->next(bs)) != nullptr) { vecx_hashjoin_add_build(hj, b); vector_block_destroy(b); }
    bs->close(bs); exec_destroy(bs);

    ExecNode *ps = make_kv_scan(pk.data(), pv.data(), (int)pk.size(), 512);
    ps->open(ps);
    while ((b = ps->next(ps)) != nullptr) {
        VectorBlock *out = nullptr;
        if (vecx_hashjoin_probe(hj, b, &out) > 0 && out) {
            collect_join_rows(out, result);
            vector_block_destroy(out);
        }
        vector_block_destroy(b);
    }
    ps->close(ps); exec_destroy(ps);
    vecx_hashjoin_destroy(hj);
    return result;
}

class DistributedJoinTest : public ::testing::Test {
protected:
    void TearDown() override {
        for (void *p : dj_buffers()) free(p);
        dj_buffers().clear();
    }
};

/* ---------- BROADCAST：远端小表 → 本地建表 → 本地 probe ---------- */
TEST_F(DistributedJoinTest, BroadcastJoinTwoInstances) {
    /* 节点 B（:19701）：小表 100 行；节点 A（本测试线程）：大表 4000 行 */
    std::vector<int32_t> bk(100), bv(100);
    for (int i = 0; i < 100; i++) { bk[i] = i; bv[i] = i * 10; }
    std::vector<int32_t> pk(4000), pv(4000);
    for (int i = 0; i < 4000; i++) { pk[i] = i % 100; pv[i] = i; }

    rpc_node_address_t a_addr{};
    strcpy(a_addr.host, "127.0.0.1");
    a_addr.port = 19701;
    a_addr.node_id = 1;

    /* A：receiver 收小表 */
    ExecNode *recv = exec_create_exchange_receiver(&a_addr);
    ASSERT_NE(recv, nullptr);
    ASSERT_EQ(recv->open(recv), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    /* B：sender 发小表（模拟远端节点，独立线程） */
    ScanFnCtx bctx{bk.data(), bv.data(), 100, 64};
    std::thread node_b([&] {
        ExecNode *send = exec_create_exchange_sender(scan_fn, &bctx, &a_addr);
        if (!send) return;
        send->open(send);
        while (send->next(send) != nullptr) {}
        send->close(send);
        exec_destroy(send);
    });

    /* A：收全量小表 → 建表 → 本地 probe */
    vecx_hashjoin_t *hj = vecx_hashjoin_create(0, 0);
    VectorBlock *b;
    while ((b = recv->next(recv)) != nullptr) {
        ASSERT_EQ(vecx_hashjoin_add_build(hj, b), 0);
        vector_block_destroy(b);
    }
    recv->close(recv);
    exec_destroy(recv);
    node_b.join();

    RowSet got;
    ExecNode *ps = make_kv_scan(pk.data(), pv.data(), 4000, 512);
    ps->open(ps);
    while ((b = ps->next(ps)) != nullptr) {
        VectorBlock *out = nullptr;
        if (vecx_hashjoin_probe(hj, b, &out) > 0 && out) {
            collect_join_rows(out, got);
            vector_block_destroy(out);
        }
        vector_block_destroy(b);
    }
    ps->close(ps);
    exec_destroy(ps);
    vecx_hashjoin_destroy(hj);

    RowSet want = baseline_join(bk, bv, pk, pv);
    EXPECT_EQ(got, want);
    EXPECT_EQ(got.size(), 4000u);
}

/* ---------- REPARTITION：双侧按 hash(key)%2 分流，各 join 一半 ---------- */
TEST_F(DistributedJoinTest, RepartitionJoinTwoInstances) {
    /* 全量数据：build 200 行、probe 4000 行，逻辑上平分在两节点 */
    std::vector<int32_t> bk(200), bv(200);
    for (int i = 0; i < 200; i++) { bk[i] = i; bv[i] = i * 10; }
    std::vector<int32_t> pk(4000), pv(4000);
    for (int i = 0; i < 4000; i++) { pk[i] = i % 200; pv[i] = i; }

    /* 按 key%2 切分：节点0 负责偶数 key，节点1 负责奇数 key */
    std::vector<int32_t> bk0, bv0, bk1, bv1, pk0, pv0, pk1, pv1;
    for (int i = 0; i < 200; i++) {
        if (bk[i] % 2 == 0) { bk0.push_back(bk[i]); bv0.push_back(bv[i]); }
        else                { bk1.push_back(bk[i]); bv1.push_back(bv[i]); }
    }
    for (int i = 0; i < 4000; i++) {
        if (pk[i] % 2 == 0) { pk0.push_back(pk[i]); pv0.push_back(pv[i]); }
        else                { pk1.push_back(pk[i]); pv1.push_back(pv[i]); }
    }

    /* "网络传输"：节点1 的 build/probe 半区经 sender/receiver 送达节点0 验证数据面。
       （真实部署中双方互发；本测试单向验证线格式+流通道承载 join 半区。） */
    rpc_node_address_t n0_addr{};
    strcpy(n0_addr.host, "127.0.0.1");
    n0_addr.port = 19702;
    n0_addr.node_id = 10;

    ExecNode *recv1 = exec_create_exchange_receiver(&n0_addr);
    ASSERT_NE(recv1, nullptr);
    ASSERT_EQ(recv1->open(recv1), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    /* 节点1 把奇数半区 probe 数据发给节点0 校验（build 半区本地处理） */
    ScanFnCtx p1ctx{pk1.data(), pv1.data(), (int)pk1.size(), 512};
    std::thread node1([&] {
        ExecNode *send = exec_create_exchange_sender(scan_fn, &p1ctx, &n0_addr);
        if (!send) return;
        send->open(send);
        while (send->next(send) != nullptr) {}
        send->close(send);
        exec_destroy(send);
    });

    /* 节点0：本地 join 偶数半区 */
    RowSet got = baseline_join(bk0, bv0, pk0, pv0);

    /* 节点0 收取节点1 的奇数半区数据（验证 repartition 数据面），
       并与节点1 本地 join 结果合并 */
    std::vector<int32_t> rk, rv;
    VectorBlock *b;
    while ((b = recv1->next(recv1)) != nullptr) {
        int32_t *kc = (int32_t *)b->columns[0];
        int32_t *vc = (int32_t *)b->columns[1];
        for (int r = 0; r < b->num_rows; r++) { rk.push_back(kc[r]); rv.push_back(vc[r]); }
        vector_block_destroy(b);
    }
    recv1->close(recv1);
    exec_destroy(recv1);
    node1.join();

    /* 传输保真校验：收到的奇数半区与发送端一致 */
    ASSERT_EQ(rk.size(), pk1.size());
    for (size_t i = 0; i < rk.size(); i++) {
        EXPECT_EQ(rk[i], pk1[i]);
        EXPECT_EQ(rv[i], pv1[i]);
    }

    RowSet got1 = baseline_join(bk1, bv1, pk1, pv1);
    got.insert(got1.begin(), got1.end());

    RowSet want = baseline_join(bk, bv, pk, pv);
    EXPECT_EQ(got, want);
}

/* ---------- 故障注入：receiver 被杀 → sender 报错且不挂死 ---------- */
TEST_F(DistributedJoinTest, SenderFailsCleanlyWhenReceiverDies) {
    rpc_node_address_t addr{};
    strcpy(addr.host, "127.0.0.1");
    addr.port = 19703;
    addr.node_id = 11;

    ExecNode *recv = exec_create_exchange_receiver(&addr);
    ASSERT_NE(recv, nullptr);
    ASSERT_EQ(recv->open(recv), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::vector<int32_t> k(100000), v(100000);
    for (int i = 0; i < 100000; i++) { k[i] = i; v[i] = i; }
    ScanFnCtx ctx{k.data(), v.data(), 100000, 4096};

    ExecNode *send = exec_create_exchange_sender(scan_fn, &ctx, &addr);
    ASSERT_NE(send, nullptr);
    ASSERT_EQ(send->open(send), 0);

    /* 发一部分后杀掉 receiver */
    send->next(send);
    recv->close(recv);
    exec_destroy(recv);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto t0 = std::chrono::steady_clock::now();
    while (send->next(send) != nullptr) {}   /* 应快速失败，不挂死 */
    auto secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    EXPECT_LT(secs, 10.0);
    send->close(send);                        /* 资源回收不崩溃 */
    exec_destroy(send);
}
```

- [ ] **Step 2: 登记目标并跑通**

```cmake
add_executable(distributed_join_test distributed_join_test.cpp)
target_link_libraries(distributed_join_test PRIVATE db_executor db_vectorized gtest gtest_main)
target_include_directories(distributed_join_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/engineering/include
)
gtest_discover_tests(distributed_join_test)
```

Run: `ninja -C /d/code/book/engineering/build distributed_join_test && /d/code/book/engineering/build/test/db/executor/distributed_join_test.exe`
Expected: 3 个 TEST 全部 PASSED（Broadcast 4000 行一致；Repartition 并集一致；故障注入 10s 内干净失败）

- [ ] **Step 3: Commit**

```bash
cd /d/code/book/engineering
git add test/db/executor/distributed_join_test.cpp test/db/executor/CMakeLists.txt
git commit -m "test(gap04): 双实例分布式 Join —— BROADCAST/REPARTITION 一致性 + 故障注入"
```

---

### Task 12: 全量回归 + 账本收尾

**Files:**
- Create: `engineering/.superpowers/sdd/gap04-ledger.md`（gitignored，无需提交）

- [ ] **Step 1: 全量构建 + 全部并行测试**

```bash
ninja -C /d/code/book/engineering/build 2>&1 | tail -3
for t in px_queue_test px_scheduler_test exchange_exec_test plan_parallel_test hashjoin_px_test px_benchmark_test shard_prune_test px_wire_test rpc_stream_test exchange_net_test distributed_join_test executor_framework_test executor_integration_test; do
  /d/code/book/engineering/build/test/db/executor/$t.exe || echo "FAILED: $t"
done
echo "ALL PX TESTS DONE"
```

Expected: 构建 exit 0；无 FAILED 行

- [ ] **Step 2: Windows 压力循环（Task 6 Step 3 的 20 轮）+ WSL2 TSan（按 run_tsan_wsl.md，有 WSL2 则执行，无则在账本注明跳过原因）**

- [ ] **Step 3: 写账本**

`engineering/.superpowers/sdd/gap04-ledger.md`：

```markdown
# gap04 OLTP 并行查询引擎 —— 账本

- 设计：`docs/superpowers/specs/2026-09-22-gap04-parallel-query-design.md`
- 计划：`docs/superpowers/plans/2026-09-22-gap04-parallel-query.md`
- 完成日期：2026-09-XX

## 验收基线核对

| 标准 | 结果 | 证据 |
|---|---|---|
| 并行/单机结果一致（含 Join） | ✅/❌ | exchange/hashjoin/distributed 测试 |
| 4 worker 加速比 ≥3x | 实测 X.XXx | px_benchmark_test（Release/Debug 注明） |
| 无数据竞争 | ✅/❌ | 20 轮压力循环 + TSan（或跳过原因） |
| 双实例 localhost 分布式 Join | ✅/❌ | distributed_join_test |

## 提交清单

（按任务列出 12 个提交 SHA）

## 已知边界

- fragment 规划器接线（distributed_query.c mock 替换）未含本期——Join 策略以
  Exchange 组合落地并验收；plan 子树序列化与 fragment/stage 模型接线列为二期
- 并行 hash join 仅 inner + 单列 int 键（继承 vecx_hashjoin 边界）
- Exchange reset 为空操作（并行迭代不支持原地重置）
- sort/limit/nested/merge join 仍无 ExecNode（plan_to_exec 既有缺口，非本期范围）
```

- [ ] **Step 4: push**

```bash
cd /d/code/book && git push origin main
```

---

## Self-Review 记录（计划撰写者已完成）

1. **Spec coverage**：spec §4.1→Task1，§4.2→Task2，§4.3→Task3/10，§3→Task4，§4.4→Task5，§8 基准/TSan→Task6，§5→Task7，§6.1→Task8，§6.2→Task9（rpc 流式以独立 rpc_stream 落地——spec 允许降级为同风格独立通道），§6.3→Task11，§7→各任务 close/abort 路径 + Task 10/11 故障注入，§8 测试映射→Task1-11 测试，§9→各任务 CMake 步骤，§10→Task12 账本。**唯一显式偏移**：spec §6.4 fragment 落地（distributed_query.c mock 替换）未含任务——plan 子树序列化超出本期 YAGNI 边界，Join 策略已以 Exchange 组合验收，账本已如实列为二期。其余 spec 要求均有任务承载。
2. **Placeholder scan**：Task 6 基准代码块内含一处刻意标注的"反面教材"两行（ADD_FAILURE），执行步骤已指示删除并给出替代；Task 9 rpcs_wsa_init 死代码已给出执行时简化版本；Task 8 STRING 列错误路径释放已注明辅助函数要求。其余无 TBD/TODO。
3. **Type consistency**：`px_subtree_fn`、`px_queue_*`、`px_scheduler_*`、`exec_create_exchange(_ex)`、`exec_create_hashjoin_px`、`px_shard_prune`、`px_shard_scan_fn`、`exec_create_shard_fanout`、`px_wire_serialize/deserialize`、`rpc_stream_*`、`rpcs_*` 签名在定义任务与消费任务间逐一核对一致；Task 4 对 Task 3 的工厂同步化修订在两处交叉引用。
