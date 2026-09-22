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
