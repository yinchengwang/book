/**
 * @file test_worker_pool.cpp
 * @brief WorkerPool 线程池单元测试
 */

#include <gtest/gtest.h>

extern "C" {
#include "db/sql/worker_pool.h"
#include "db/sql/tuple_queue.h"
}

#include <thread>
#include <vector>
#include <atomic>

namespace {

/**
 * @brief 简单任务函数：无参数
 */
static void simple_task(int worker_id, void *arg, TupleQueue *output_q) {
    (void)worker_id;
    (void)arg;
    (void)output_q;
    /* 空任务 */
}

/**
 * @brief 测试创建和销毁
 */
TEST(WorkerPoolTest, CreateDestroy) {
    WorkerPool *pool = WorkerPoolCreate(4);
    ASSERT_NE(pool, nullptr);
    EXPECT_EQ(WorkerPoolGetNWorkers(pool), 4);
    EXPECT_FALSE(WorkerPoolIsShutdown(pool));

    WorkerPoolDestroy(pool);
}

/**
 * @brief 测试无效参数
 */
TEST(WorkerPoolTest, InvalidParams) {
    /* Worker 数量为 0 */
    WorkerPool *pool = WorkerPoolCreate(0);
    EXPECT_EQ(pool, nullptr);

    WorkerPoolDestroy(pool);
}

/**
 * @brief 测试启动和等待
 */
TEST(WorkerPoolTest, StartAndWait) {
    WorkerPool *pool = WorkerPoolCreate(4);
    ASSERT_NE(pool, nullptr);

    std::atomic<int> counter{0};

    /* 任务函数：递增计数器 */
    auto task_fn = [](int worker_id, void *arg, TupleQueue *output_q) {
        auto *counter_ptr = static_cast<std::atomic<int> *>(arg);
        (*counter_ptr)++;
        (void)worker_id;
        (void)output_q;
    };

    /* 创建任务数组 */
    WorkerTask tasks[4];
    for (int i = 0; i < 4; i++) {
        tasks[i].func = task_fn;
        tasks[i].arg = &counter;
        tasks[i].output_q = nullptr;
    }

    /* 启动 worker */
    int nstarted = WorkerPoolStart(pool, tasks);
    EXPECT_EQ(nstarted, 4);

    /* 等待完成 */
    WorkerPoolWait(pool);
    EXPECT_TRUE(WorkerPoolIsShutdown(pool));

    /* 验证计数器 */
    EXPECT_EQ(counter, 4);

    WorkerPoolDestroy(pool);
}

/**
 * @brief 测试重复启动失败
 */
TEST(WorkerPoolTest, DoubleStart) {
    WorkerPool *pool = WorkerPoolCreate(2);
    ASSERT_NE(pool, nullptr);

    WorkerTask tasks[2];
    for (int i = 0; i < 2; i++) {
        tasks[i].func = simple_task;
        tasks[i].arg = nullptr;
        tasks[i].output_q = nullptr;
    }

    /* 第一次启动 */
    int nstarted = WorkerPoolStart(pool, tasks);
    EXPECT_GT(nstarted, 0);

    /* 第二次启动应失败 */
    int nstarted2 = WorkerPoolStart(pool, tasks);
    EXPECT_EQ(nstarted2, -1);

    WorkerPoolWait(pool);
    WorkerPoolDestroy(pool);
}

/**
 * @brief 测试带输出队列的任务
 */
TEST(WorkerPoolTest, TaskWithOutputQueue) {
    WorkerPool *pool = WorkerPoolCreate(2);
    ASSERT_NE(pool, nullptr);

    TupleQueue *queue = TupleQueueCreate(100);
    ASSERT_NE(queue, nullptr);

    std::atomic<int> values_sent{0};

    /* 任务函数：发送值到队列 */
    auto task_fn = [](int worker_id, void *arg, TupleQueue *output_q) {
        auto *sent_ptr = static_cast<std::atomic<int> *>(arg);
        for (int i = 0; i < 10; i++) {
            int *value = new int(worker_id * 10 + i);
            if (TupleQueueSend(output_q, value) == 0) {
                (*sent_ptr)++;
            }
        }
    };

    WorkerTask tasks[2];
    for (int i = 0; i < 2; i++) {
        tasks[i].func = task_fn;
        tasks[i].arg = &values_sent;
        tasks[i].output_q = queue;
    }

    /* 启动 worker */
    int nstarted = WorkerPoolStart(pool, tasks);
    EXPECT_EQ(nstarted, 2);

    /* 启动消费者线程 */
    std::atomic<int> values_received{0};
    std::thread consumer([&]() {
        void *slot;
        while ((slot = TupleQueueReceive(queue)) != nullptr) {
            int *value = static_cast<int *>(slot);
            delete value;
            values_received++;
        }
    });

    /* 等待 worker 完成 */
    WorkerPoolWait(pool);

    /* 消费者线程应该收到通知并退出 */
    consumer.join();

    EXPECT_EQ(values_sent, 20);
    EXPECT_EQ(values_received, 20);

    TupleQueueDestroy(queue);
    WorkerPoolDestroy(pool);
}

/**
 * @brief 测试 worker 数量
 */
TEST(WorkerPoolTest, WorkerCount) {
    WorkerPool *pool1 = WorkerPoolCreate(1);
    EXPECT_EQ(WorkerPoolGetNWorkers(pool1), 1);
    WorkerPoolDestroy(pool1);

    WorkerPool *pool8 = WorkerPoolCreate(8);
    EXPECT_EQ(WorkerPoolGetNWorkers(pool8), 8);
    WorkerPoolDestroy(pool8);
}

/**
 * @brief 测试并发执行
 */
TEST(WorkerPoolTest, ConcurrentExecution) {
    WorkerPool *pool = WorkerPoolCreate(4);
    ASSERT_NE(pool, nullptr);

    std::atomic<int> sum{0};

    /* 任务函数：原子递增 */
    auto task_fn = [](int worker_id, void *arg, TupleQueue *output_q) {
        auto *sum_ptr = static_cast<std::atomic<int> *>(arg);
        for (int i = 0; i < 1000; i++) {
            (*sum_ptr) += i;
        }
        (void)worker_id;
        (void)output_q;
    };

    WorkerTask tasks[4];
    for (int i = 0; i < 4; i++) {
        tasks[i].func = task_fn;
        tasks[i].arg = &sum;
        tasks[i].output_q = nullptr;
    }

    int nstarted = WorkerPoolStart(pool, tasks);
    EXPECT_EQ(nstarted, 4);

    WorkerPoolWait(pool);

    /* 每个线程累加 0+1+...+999 = 499500 */
    EXPECT_EQ(sum, 4 * 499500);

    WorkerPoolDestroy(pool);
}

}  // namespace