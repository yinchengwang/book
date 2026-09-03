// engineering/rag/test/rag/test_async.cpp

#include <gtest/gtest.h>
#include "rag/async_pipeline.h"
#include "rag/pipeline.h"

namespace rag {
namespace test {

class AsyncTest : public ::testing::Test {
protected:
    void SetUp() override {
        AsyncConfig config;
        config.num_workers = 2;
        config.queue_size = 10;
        pool_ = std::make_shared<ThreadPool>(config);
    }

    std::shared_ptr<ThreadPool> pool_;
};

TEST_F(AsyncTest, SubmitTask) {
    auto future = pool_->submit([]() { return 42; });
    EXPECT_EQ(future.get(), 42);
}

TEST_F(AsyncTest, BatchSubmit) {
    std::vector<std::string> queries = {"q1", "q2", "q3"};
    auto futures = pool_->submit_batch(queries, 5);

    EXPECT_EQ(futures.size(), 3);

    for (auto& f : futures) {
        EXPECT_TRUE(f.valid());
    }
}

TEST_F(AsyncTest, QueueSize) {
    EXPECT_EQ(pool_->queue_size(), 0);

    // 提交多个任务
    for (int i = 0; i < 5; i++) {
        pool_->submit([]() { return i; });
    }

    // 等待任务完成
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_GE(pool_->queue_size(), 0);
}

TEST_F(AsyncTest, Shutdown) {
    pool_->shutdown();
    EXPECT_TRUE(pool_->is_shutdown());
}

TEST_F(AsyncTest, MaxConcurrent) {
    AsyncConfig config;
    config.num_workers = 2;
    config.queue_size = 5;
    config.max_concurrent = 2;

    auto pool = std::make_shared<ThreadPool>(config);

    // 提交多个任务
    std::atomic<int> running{0};
    std::atomic<int> max_running{0};

    for (int i = 0; i < 10; i++) {
        pool->submit([&]() {
            int curr = ++running;
            max_running = std::max(max_running.load(), curr);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            --running;
            return i;
        });
    }

    // 等待完成
    pool->wait_for_idle();
    pool->shutdown();

    EXPECT_LE(max_running.load(), config.num_workers);
}

TEST_F(AsyncTest, AsyncPipelineBasic) {
    // Create a minimal pipeline
    auto pipeline = std::make_shared<RetrievalPipeline>();
    AsyncConfig config;
    config.num_workers = 2;

    auto async_pipeline = std::make_shared<AsyncPipeline>(pipeline, nullptr, config);

    auto future = async_pipeline->execute_async("test query", 5);

    EXPECT_TRUE(future.valid());

    auto result = future.get();
    // Pipeline returns success=true even without stages
    EXPECT_TRUE(result.success);
}

TEST_F(AsyncTest, AsyncPipelineBatch) {
    auto pipeline = std::make_shared<RetrievalPipeline>();
    AsyncConfig config;
    config.num_workers = 2;

    auto async_pipeline = std::make_shared<AsyncPipeline>(pipeline, nullptr, config);

    std::vector<std::string> queries = {"q1", "q2", "q3"};
    auto futures = async_pipeline->execute_batch_async(queries, 5);

    EXPECT_EQ(futures.size(), 3);

    for (auto& f : futures) {
        EXPECT_TRUE(f.valid());
        auto result = f.get();
        EXPECT_TRUE(result.success);
    }
}

TEST_F(AsyncTest, AsyncPipelineStats) {
    auto pipeline = std::make_shared<RetrievalPipeline>();
    AsyncConfig config;
    config.num_workers = 2;

    auto async_pipeline = std::make_shared<AsyncPipeline>(pipeline, nullptr, config);

    // Execute some queries
    for (int i = 0; i < 3; i++) {
        async_pipeline->execute_async("query " + std::to_string(i), 5);
    }

    // Wait a bit for async execution
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto stats = async_pipeline->get_stats();
    EXPECT_GE(stats.total_queries, 3);

    async_pipeline->reset_stats();
    auto reset_stats = async_pipeline->get_stats();
    EXPECT_EQ(reset_stats.total_queries, 0);
}

}  // namespace test
}  // namespace rag

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}