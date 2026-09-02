/**
 * @file stream_engine_test.cpp
 * @brief 流式存储引擎测试
 *
 * 测试流式引擎的创建、打开、生产、消费、偏移量获取等功能。
 */
#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

extern "C" {
#include "db/stream_engine.h"
#include "db/log.h"
}

#ifdef _WIN32
#define mkdir(path) _mkdir(path)
#endif

/**
 * @brief 流式引擎测试夹具
 */
class StreamEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 初始化日志 */
        log_config_t log_config;
        memset(&log_config, 0, sizeof(log_config));
        log_config.level = LOG_LEVEL_ERROR;
        log_config.target = LOG_TARGET_CONSOLE;
        log_config.enable_colors = false;
        log_init(&log_config);

        /* 确保测试目录存在 */
#ifdef _WIN32
        mkdir("./test_data_stream");
#else
        mkdir("./test_data_stream", 0755);
#endif
    }

    void TearDown() override {
        log_shutdown();

        /* 清理测试数据目录 */
#ifdef _WIN32
        system("rmdir /s /q ./test_data_stream");
#else
        system("rm -rf ./test_data_stream");
#endif
    }
};

/**
 * @brief 测试流引擎初始化和关闭
 */
TEST_F(StreamEngineTest, InitShutdown) {
    /* 获取流引擎操作表 */
    const storage_ops_t *ops = stream_engine_get_ops();
    if (ops == nullptr) {
        GTEST_SKIP() << "Stream engine not available";
    }

    EXPECT_STREQ("stream_engine", ops->name);
    EXPECT_EQ(MODEL_STREAM, ops->model);

    /* 初始化引擎 */
    if (ops->init) {
        int ret = ops->init("./test_data_stream");
        EXPECT_EQ(0, ret);

        /* 关闭引擎 */
        if (ops->shutdown) {
            ops->shutdown();
        }
    }
}

/**
 * @brief 测试流创建和存在性检查
 */
TEST_F(StreamEngineTest, CreateAndExists) {
    const storage_ops_t *ops = stream_engine_get_ops();
    if (ops == nullptr) {
        GTEST_SKIP() << "Stream engine not available";
    }

    if (!ops->init || ops->init("./test_data_stream") != 0) {
        GTEST_SKIP() << "Cannot initialize stream engine";
    }

    /* 创建流 */
    stream_config_t config = {
        .name = "test_stream",
        .partition_count = 3,
        .retention_ms = 3600000,
        .replication_factor = 1,
        .compression = STREAM_COMPRESS_NONE
    };

    if (ops->stream_create) {
        int ret = ops->stream_create("test_stream", &config);
        EXPECT_EQ(0, ret);

        /* 检查流是否存在 */
        if (ops->stream_exists) {
            EXPECT_TRUE(ops->stream_exists("test_stream"));
            EXPECT_FALSE(ops->stream_exists("nonexistent_stream"));
        }
    }

    if (ops->shutdown) {
        ops->shutdown();
    }
}

/**
 * @brief 测试流的打开和关闭
 */
TEST_F(StreamEngineTest, OpenClose) {
    const storage_ops_t *ops = stream_engine_get_ops();
    if (ops == nullptr) {
        GTEST_SKIP() << "Stream engine not available";
    }

    if (!ops->init || ops->init("./test_data_stream") != 0) {
        GTEST_SKIP() << "Cannot initialize stream engine";
    }

    /* 创建流 */
    stream_config_t config = {
        .name = "test_stream",
        .partition_count = 1,
        .retention_ms = 3600000,
        .replication_factor = 1,
        .compression = STREAM_COMPRESS_NONE
    };

    if (ops->stream_create) {
        ops->stream_create("test_stream", &config);
    }

    /* 打开流 */
    if (ops->stream_open) {
        void *stream = ops->stream_open("test_stream");
        ASSERT_NE(nullptr, stream);

        /* 关闭流 */
        if (ops->stream_close) {
            EXPECT_EQ(0, ops->stream_close(stream));
        }
    }

    if (ops->shutdown) {
        ops->shutdown();
    }
}

/**
 * @brief 测试生产消息
 */
TEST_F(StreamEngineTest, Produce) {
    const storage_ops_t *ops = stream_engine_get_ops();
    if (ops == nullptr) {
        GTEST_SKIP() << "Stream engine not available";
    }

    if (!ops->init || ops->init("./test_data_stream") != 0) {
        GTEST_SKIP() << "Cannot initialize stream engine";
    }

    /* 创建并打开流 */
    stream_config_t config = {
        .name = "test_stream",
        .partition_count = 1,
        .retention_ms = 3600000,
        .replication_factor = 1,
        .compression = STREAM_COMPRESS_NONE
    };

    if (ops->stream_create) {
        ops->stream_create("test_stream", &config);
    }

    void *stream = nullptr;
    if (ops->stream_open) {
        stream = ops->stream_open("test_stream");
    }

    if (stream && ops->produce) {
        /* 生产消息 */
        const char *msg = "Hello, Stream!";
        int ret = ops->produce(stream, msg, strlen(msg));
        EXPECT_EQ(0, ret);

        /* 再次生产 */
        const char *msg2 = "Second message";
        ret = ops->produce(stream, msg2, strlen(msg2));
        EXPECT_EQ(0, ret);
    }

    if (stream && ops->stream_close) {
        ops->stream_close(stream);
    }

    if (ops->shutdown) {
        ops->shutdown();
    }
}

/**
 * @brief 测试获取偏移量
 */
TEST_F(StreamEngineTest, GetOffset) {
    const storage_ops_t *ops = stream_engine_get_ops();
    if (ops == nullptr) {
        GTEST_SKIP() << "Stream engine not available";
    }

    if (!ops->init || ops->init("./test_data_stream") != 0) {
        GTEST_SKIP() << "Cannot initialize stream engine";
    }

    /* 创建并打开流 */
    stream_config_t config = {
        .name = "test_stream",
        .partition_count = 1,
        .retention_ms = 3600000,
        .replication_factor = 1,
        .compression = STREAM_COMPRESS_NONE
    };

    if (ops->stream_create) {
        ops->stream_create("test_stream", &config);
    }

    void *stream = nullptr;
    if (ops->stream_open) {
        stream = ops->stream_open("test_stream");
    }

    if (stream && ops->produce && ops->get_offset) {
        /* 初始偏移量 */
        int64_t offset = ops->get_offset(stream);
        EXPECT_GE(offset, 0);

        /* 生产消息后偏移量应该增加 */
        const char *msg = "Test message";
        ops->produce(stream, msg, strlen(msg));

        offset = ops->get_offset(stream);
        EXPECT_GT(offset, 0);
    }

    if (stream && ops->stream_close) {
        ops->stream_close(stream);
    }

    if (ops->shutdown) {
        ops->shutdown();
    }
}

/**
 * @brief 测试消费者订阅和消费
 */
TEST_F(StreamEngineTest, ConsumerSubscribe) {
    const storage_ops_t *ops = stream_engine_get_ops();
    if (ops == nullptr) {
        GTEST_SKIP() << "Stream engine not available";
    }

    if (!ops->init || ops->init("./test_data_stream") != 0) {
        GTEST_SKIP() << "Cannot initialize stream engine";
    }

    /* 创建并打开流 */
    stream_config_t config = {
        .name = "test_stream",
        .partition_count = 1,
        .retention_ms = 3600000,
        .replication_factor = 1,
        .compression = STREAM_COMPRESS_NONE
    };

    if (ops->stream_create) {
        ops->stream_create("test_stream", &config);
    }

    void *stream = nullptr;
    if (ops->stream_open) {
        stream = ops->stream_open("test_stream");
    }

    /* 生产一些消息 */
    if (stream && ops->produce) {
        for (int i = 0; i < 5; i++) {
            char msg[64];
            snprintf(msg, sizeof(msg), "Message %d", i);
            ops->produce(stream, msg, strlen(msg));
        }
    }

    /* 订阅流 */
    if (stream && ops->subscribe) {
        stream_consumer_t *consumer = ops->subscribe(stream, 0);
        if (consumer) {
            /* 消费消息 */
            char buffer[256];
            size_t len = sizeof(buffer);

            int ret = ops->consume(consumer, buffer, &len, sizeof(buffer));
            EXPECT_GE(ret, 0);

            /* 提交偏移量 */
            if (ops->commit_offset) {
                ops->commit_offset(consumer, consumer->current_offset);
            }

            /* 关闭消费者 */
            if (ops->consumer_close) {
                ops->consumer_close(consumer);
            }
        }
    }

    if (stream && ops->stream_close) {
        ops->stream_close(stream);
    }

    if (ops->shutdown) {
        ops->shutdown();
    }
}

/**
 * @brief 测试流的删除
 */
TEST_F(StreamEngineTest, Drop) {
    const storage_ops_t *ops = stream_engine_get_ops();
    if (ops == nullptr) {
        GTEST_SKIP() << "Stream engine not available";
    }

    if (!ops->init || ops->init("./test_data_stream") != 0) {
        GTEST_SKIP() << "Cannot initialize stream engine";
    }

    /* 创建流 */
    stream_config_t config = {
        .name = "test_stream",
        .partition_count = 1,
        .retention_ms = 3600000,
        .replication_factor = 1,
        .compression = STREAM_COMPRESS_NONE
    };

    if (ops->stream_create) {
        ops->stream_create("test_stream", &config);
    }

    /* 删除流 */
    if (ops->stream_drop) {
        int ret = ops->stream_drop("test_stream");
        EXPECT_EQ(0, ret);

        /* 确认流不存在 */
        if (ops->stream_exists) {
            EXPECT_FALSE(ops->stream_exists("test_stream"));
        }
    }

    if (ops->shutdown) {
        ops->shutdown();
    }
}

/**
 * @brief 测试分区消息生产
 */
TEST_F(StreamEngineTest, ProducePartition) {
    const storage_ops_t *ops = stream_engine_get_ops();
    if (ops == nullptr) {
        GTEST_SKIP() << "Stream engine not available";
    }

    if (!ops->init || ops->init("./test_data_stream") != 0) {
        GTEST_SKIP() << "Cannot initialize stream engine";
    }

    /* 创建多分区流 */
    stream_config_t config = {
        .name = "test_stream",
        .partition_count = 4,
        .retention_ms = 3600000,
        .replication_factor = 1,
        .compression = STREAM_COMPRESS_NONE
    };

    if (ops->stream_create) {
        ops->stream_create("test_stream", &config);
    }

    void *stream = nullptr;
    if (ops->stream_open) {
        stream = ops->stream_open("test_stream");
    }

    if (stream && ops->produce_partition) {
        /* 向不同分区发送消息 */
        const char *msg = "Test message";
        for (int i = 0; i < 4; i++) {
            int ret = ops->produce_partition(stream, msg, strlen(msg), i);
            EXPECT_EQ(0, ret);
        }
    }

    if (stream && ops->stream_close) {
        ops->stream_close(stream);
    }

    if (ops->shutdown) {
        ops->shutdown();
    }
}

/**
 * @brief 测试流统计信息
 */
TEST_F(StreamEngineTest, StreamStats) {
    const storage_ops_t *ops = stream_engine_get_ops();
    if (ops == nullptr) {
        GTEST_SKIP() << "Stream engine not available";
    }

    if (!ops->init || ops->init("./test_data_stream") != 0) {
        GTEST_SKIP() << "Cannot initialize stream engine";
    }

    /* 创建并打开流 */
    stream_config_t config = {
        .name = "test_stream",
        .partition_count = 1,
        .retention_ms = 3600000,
        .replication_factor = 1,
        .compression = STREAM_COMPRESS_NONE
    };

    if (ops->stream_create) {
        ops->stream_create("test_stream", &config);
    }

    void *stream = nullptr;
    if (ops->stream_open) {
        stream = ops->stream_open("test_stream");
    }

    /* 生产消息 */
    if (stream && ops->produce) {
        const char *msg = "Test message";
        ops->produce(stream, msg, strlen(msg));
    }

    /* 获取统计信息 */
    if (ops->get_stream_stats) {
        storage_stats_t stats;
        int ret = ops->get_stream_stats("test_stream", &stats);
        EXPECT_GE(ret, 0);
    }

    /* 获取分区数 */
    if (ops->get_partition_count) {
        int count = 0;
        int ret = ops->get_partition_count("test_stream", &count);
        EXPECT_GE(ret, 0);
        EXPECT_GT(count, 0);
    }

    if (stream && ops->stream_close) {
        ops->stream_close(stream);
    }

    if (ops->shutdown) {
        ops->shutdown();
    }
}

/**
 * @brief 主函数
 */
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
