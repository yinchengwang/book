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
