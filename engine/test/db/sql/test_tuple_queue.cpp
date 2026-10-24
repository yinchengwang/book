/**
 * @file test_tuple_queue.cpp
 * @brief TupleQueue 线程安全队列单元测试
 */

#include <gtest/gtest.h>

extern "C" {
#include "db/sql/tuple_queue.h"
}

#include <thread>
#include <vector>
#include <atomic>

namespace {

/**
 * @brief 测试基本创建和销毁
 */
TEST(TupleQueueTest, CreateDestroy) {
    TupleQueue *queue = TupleQueueCreate(10);
    ASSERT_NE(queue, nullptr);
    EXPECT_EQ(TupleQueueSize(queue), 0);
    EXPECT_FALSE(TupleQueueIsClosed(queue));

    TupleQueueDestroy(queue);
}

/**
 * @brief 测试默认容量
 */
TEST(TupleQueueTest, DefaultCapacity) {
    TupleQueue *queue = TupleQueueCreate(0);
    ASSERT_NE(queue, nullptr);

    /* 验证队列创建成功 */
    EXPECT_EQ(TupleQueueSize(queue), 0);

    TupleQueueDestroy(queue);
}

/**
 * @brief 测试基本入队出队
 */
TEST(TupleQueueTest, BasicSendReceive) {
    TupleQueue *queue = TupleQueueCreate(10);
    ASSERT_NE(queue, nullptr);

    int value1 = 100;
    int value2 = 200;

    /* 入队 */
    EXPECT_EQ(TupleQueueSend(queue, &value1), 0);
    EXPECT_EQ(TupleQueueSend(queue, &value2), 0);
    EXPECT_EQ(TupleQueueSize(queue), 2);

    /* 出队 */
    void *result1 = TupleQueueReceive(queue);
    void *result2 = TupleQueueReceive(queue);

    EXPECT_EQ(result1, &value1);
    EXPECT_EQ(result2, &value2);
    EXPECT_EQ(TupleQueueSize(queue), 0);

    TupleQueueDestroy(queue);
}

/**
 * @brief 测试队列关闭
 */
TEST(TupleQueueTest, BlockOnEmpty) {
    TupleQueue *queue = TupleQueueCreate(10);
    ASSERT_NE(queue, nullptr);

    /* 关闭空队列 */
    TupleQueueClose(queue);
    EXPECT_TRUE(TupleQueueIsClosed(queue));

    /* 从关闭的空队列接收应返回 NULL */
    void *result = TupleQueueReceive(queue);
    EXPECT_EQ(result, nullptr);

    TupleQueueDestroy(queue);
}

/**
 * @brief 测试关闭后发送失败
 */
TEST(TupleQueueTest, SendAfterClose) {
    TupleQueue *queue = TupleQueueCreate(10);
    ASSERT_NE(queue, nullptr);

    int value = 100;

    /* 关闭队列 */
    TupleQueueClose(queue);

    /* 发送应失败 */
    EXPECT_EQ(TupleQueueSend(queue, &value), -1);

    TupleQueueDestroy(queue);
}

/**
 * @brief 测试多生产者
 */
TEST(TupleQueueTest, MultiProducer) {
    TupleQueue *queue = TupleQueueCreate(100);
    ASSERT_NE(queue, nullptr);

    const int num_producers = 4;
    const int items_per_producer = 25;
    std::vector<std::thread> producers;
    std::atomic<int> total_sent{0};

    /* 启动生产者线程 */
    for (int i = 0; i < num_producers; i++) {
        producers.emplace_back([&, i]() {
            for (int j = 0; j < items_per_producer; j++) {
                int *value = new int(i * items_per_producer + j);
                if (TupleQueueSend(queue, value) == 0) {
                    total_sent++;
                }
            }
        });
    }

    /* 等待所有生产者完成 */
    for (auto &t : producers) {
        t.join();
    }

    EXPECT_EQ(total_sent, num_producers * items_per_producer);

    /* 关闭队列 */
    TupleQueueClose(queue);

    /* 接收所有数据 */
    int received = 0;
    void *slot;
    while ((slot = TupleQueueReceive(queue)) != nullptr) {
        int *value = static_cast<int *>(slot);
        delete value;
        received++;
    }

    EXPECT_EQ(received, num_producers * items_per_producer);

    TupleQueueDestroy(queue);
}

/**
 * @brief 测试多消费者
 */
TEST(TupleQueueTest, MultiConsumer) {
    TupleQueue *queue = TupleQueueCreate(100);
    ASSERT_NE(queue, nullptr);

    const int num_items = 100;
    const int num_consumers = 4;
    std::atomic<int> total_received{0};
    std::atomic<int> items_consumed{0};

    /* 生产者：发送数据后关闭队列 */
    std::thread producer([&]() {
        for (int i = 0; i < num_items; i++) {
            int *value = new int(i);
            TupleQueueSend(queue, value);
        }
        TupleQueueClose(queue);
    });

    /* 启动消费者线程 */
    std::vector<std::thread> consumers;
    for (int i = 0; i < num_consumers; i++) {
        consumers.emplace_back([&]() {
            void *slot;
            while ((slot = TupleQueueReceive(queue)) != nullptr) {
                int *value = static_cast<int *>(slot);
                delete value;
                items_consumed++;
            }
        });
    }

    /* 等待所有线程完成 */
    producer.join();
    for (auto &t : consumers) {
        t.join();
    }

    EXPECT_EQ(items_consumed, num_items);

    TupleQueueDestroy(queue);
}

/**
 * @brief 测试生产者消费者同时运行
 */
TEST(TupleQueueTest, ProducerConsumerParallel) {
    TupleQueue *queue = TupleQueueCreate(50);
    ASSERT_NE(queue, nullptr);

    const int num_items = 200;
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};

    /* 生产者线程 */
    std::thread producer([&]() {
        for (int i = 0; i < num_items; i++) {
            int *value = new int(i);
            if (TupleQueueSend(queue, value) == 0) {
                produced++;
            }
        }
        TupleQueueClose(queue);
    });

    /* 消费者线程 */
    std::thread consumer([&]() {
        void *slot;
        while ((slot = TupleQueueReceive(queue)) != nullptr) {
            int *value = static_cast<int *>(slot);
            delete value;
            consumed++;
        }
    });

    /* 等待完成 */
    producer.join();
    consumer.join();

    EXPECT_EQ(produced, num_items);
    EXPECT_EQ(consumed, num_items);

    TupleQueueDestroy(queue);
}

}  // namespace
