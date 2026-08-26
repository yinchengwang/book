/**
 * @file rpc_test.cpp
 * @brief RPC 模块单元测试
 *
 * 测试 RPC 客户端/服务端、消息序列化、连接池等功能。
 */
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>

extern "C" {
#include "db/distributed/rpc.h"
}

/* ========================================================================
 * 测试夹具
 * ======================================================================== */

class RPCTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 配置 */
        config_.server_addr.node_id = 1;
        strncpy(config_.server_addr.host, "127.0.0.1", sizeof(config_.server_addr.host));
        config_.server_addr.port = 19876;
        config_.connect_timeout_ms = 1000;
        config_.request_timeout_ms = 5000;
        config_.pool_size = 4;
        config_.max_retry_count = 2;
        config_.retry_base_interval_ms = 50;

        /* 服务端地址 */
        bind_addr_.node_id = 1;
        strncpy(bind_addr_.host, "127.0.0.1", sizeof(bind_addr_.host));
        bind_addr_.port = 19876;
    }

    void TearDown() override {
    }

    rpc_config_t config_;
    rpc_node_address_t bind_addr_;
};

/* ========================================================================
 * 消息操作测试
 * ======================================================================== */

TEST_F(RPCTest, MessageAllocAndFree) {
    rpc_message_t *msg = rpc_message_alloc(128);
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->header.magic, 0x52504331);
    EXPECT_EQ(msg->header.payload_size, 128u);
    EXPECT_NE(msg->payload, nullptr);

    rpc_message_free(msg);
}

TEST_F(RPCTest, MessageAllocZeroSize) {
    rpc_message_t *msg = rpc_message_alloc(0);
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->header.payload_size, 0u);
    EXPECT_EQ(msg->payload, nullptr);

    rpc_message_free(msg);
}

TEST_F(RPCTest, MessageSerializeAndDeserialize) {
    /* 创建消息 */
    rpc_message_t *msg = rpc_message_alloc(64);
    ASSERT_NE(msg, nullptr);
    msg->header.type = RPC_MSG_REQUEST;
    msg->header.request_id = 12345;

    /* 填充测试数据 */
    const char *test_data = "Hello, RPC!";
    memcpy(msg->payload, test_data, strlen(test_data) + 1);

    /* 序列化 */
    void *buffer = nullptr;
    uint32_t buffer_size = 0;
    EXPECT_EQ(rpc_message_serialize(msg, &buffer, &buffer_size), 0);
    EXPECT_GT(buffer_size, sizeof(rpc_message_header_t));

    /* 反序列化 */
    rpc_message_t *msg2 = rpc_message_deserialize(buffer, buffer_size);
    ASSERT_NE(msg2, nullptr);
    EXPECT_EQ(msg2->header.magic, msg->header.magic);
    EXPECT_EQ(msg2->header.type, msg->header.type);
    EXPECT_EQ(msg2->header.request_id, msg->header.request_id);
    EXPECT_EQ(msg2->header.payload_size, msg->header.payload_size);
    EXPECT_STREQ((const char *)msg2->payload, test_data);

    /* 清理 */
    free(buffer);
    rpc_message_free(msg);
    rpc_message_free(msg2);
}

TEST_F(RPCTest, MessageDeserializeInvalidMagic) {
    /* 创建无效缓冲区 */
    char buffer[64];
    memset(buffer, 0, sizeof(buffer));
    rpc_message_header_t *header = (rpc_message_header_t *)buffer;
    header->magic = 0x12345678;  /* 无效魔数 */

    rpc_message_t *msg = rpc_message_deserialize(buffer, sizeof(buffer));
    EXPECT_EQ(msg, nullptr);
}

TEST_F(RPCTest, MessageDeserializeInvalidChecksum) {
    /* 创建消息 */
    rpc_message_t *msg = rpc_message_alloc(32);
    ASSERT_NE(msg, nullptr);

    /* 序列化 */
    void *buffer = nullptr;
    uint32_t buffer_size = 0;
    EXPECT_EQ(rpc_message_serialize(msg, &buffer, &buffer_size), 0);

    /* 篡改校验和 */
    rpc_message_header_t *header = (rpc_message_header_t *)buffer;
    header->checksum ^= 0xFFFFFFFF;

    /* 反序列化应该失败 */
    rpc_message_t *msg2 = rpc_message_deserialize(buffer, buffer_size);
    EXPECT_EQ(msg2, nullptr);

    free(buffer);
    rpc_message_free(msg);
}

/* ========================================================================
 * CRC32 测试
 * ======================================================================== */

TEST_F(RPCTest, CRC32Basic) {
    const char *data = "Hello, World!";
    uint32_t crc1 = rpc_crc32(data, strlen(data));
    uint32_t crc2 = rpc_crc32(data, strlen(data));

    /* 相同数据应该产生相同的 CRC32 */
    EXPECT_EQ(crc1, crc2);

    /* 不同数据应该产生不同的 CRC32 */
    const char *data2 = "Hello, World?";
    uint32_t crc3 = rpc_crc32(data2, strlen(data2));
    EXPECT_NE(crc1, crc3);
}

/* ========================================================================
 * 错误处理测试
 * ======================================================================== */

TEST_F(RPCTest, ErrorString) {
    EXPECT_STREQ(rpc_error_string(RPC_OK), "成功");
    EXPECT_STREQ(rpc_error_string(RPC_ERR_NETWORK), "网络错误");
    EXPECT_STREQ(rpc_error_string(RPC_ERR_TIMEOUT), "超时");
    EXPECT_STREQ(rpc_error_string(RPC_ERR_CONNECTION), "连接失败");
    EXPECT_STREQ(rpc_error_string(RPC_ERR_PROTOCOL), "协议错误");
    EXPECT_STREQ(rpc_error_string(RPC_ERR_MEMORY), "内存错误");
    EXPECT_STREQ(rpc_error_string(RPC_ERR_NOT_FOUND), "节点未找到");
    EXPECT_STREQ(rpc_error_string(RPC_ERR_INVALID_PARAM), "参数无效");
    EXPECT_STREQ(rpc_error_string(RPC_ERR_POOL_EXHAUSTED), "连接池耗尽");
    EXPECT_STREQ(rpc_error_string(RPC_ERR_CLOSED), "连接已关闭");
}

/* ========================================================================
 * 客户端创建/销毁测试
 * ======================================================================== */

TEST_F(RPCTest, ClientCreateAndDestroy) {
    rpc_client_t *client = rpc_client_create(&config_);
    ASSERT_NE(client, nullptr);

    rpc_client_destroy(client);
}

TEST_F(RPCTest, ClientCreateNullConfig) {
    rpc_client_t *client = rpc_client_create(nullptr);
    EXPECT_EQ(client, nullptr);
}

/* ========================================================================
 * 服务端创建/销毁测试
 * ======================================================================== */

TEST_F(RPCTest, ServerCreateAndDestroy) {
    rpc_server_handler_t handler;
    handler.handle_request = nullptr;
    handler.user_data = nullptr;

    rpc_server_t *server = rpc_server_create(&bind_addr_, &handler);
    ASSERT_NE(server, nullptr);

    rpc_server_destroy(server);
}

TEST_F(RPCTest, ServerCreateNullParams) {
    rpc_server_handler_t handler;
    handler.handle_request = nullptr;
    handler.user_data = nullptr;

    rpc_server_t *server1 = rpc_server_create(nullptr, &handler);
    EXPECT_EQ(server1, nullptr);

    rpc_server_t *server2 = rpc_server_create(&bind_addr_, nullptr);
    EXPECT_EQ(server2, nullptr);
}

/* ========================================================================
 * 端到端测试
 * ======================================================================== */

/**
 * @brief 测试请求处理器
 */
static int test_handle_request(const rpc_message_t *request, rpc_message_t *response, void *user_data) {
    /* 简单的回声处理器: 返回请求数据 */
    if (request->header.payload_size > 0 && request->payload != nullptr) {
        response->payload = malloc(request->header.payload_size);
        if (response->payload == nullptr) {
            return -1;
        }
        memcpy(response->payload, request->payload, request->header.payload_size);
        response->header.payload_size = request->header.payload_size;
    }
    return 0;
}

TEST_F(RPCTest, EndToEndEcho) {
    /* 创建服务端 */
    rpc_server_handler_t handler;
    handler.handle_request = test_handle_request;
    handler.user_data = nullptr;

    rpc_server_t *server = rpc_server_create(&bind_addr_, &handler);
    ASSERT_NE(server, nullptr);

    /* 启动服务端 */
    ASSERT_EQ(rpc_server_start(server), 0);

    /* 等待服务端启动 */
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    /* 创建客户端 */
    rpc_client_t *client = rpc_client_create(&config_);
    ASSERT_NE(client, nullptr);

    /* 发送请求 */
    const char *test_data = "Echo test data";
    rpc_message_t response;
    memset(&response, 0, sizeof(response));

    int rc = rpc_client_call(client, test_data, strlen(test_data) + 1, &response);
    EXPECT_EQ(rc, RPC_OK);

    if (rc == RPC_OK) {
        EXPECT_EQ(response.header.type, RPC_MSG_RESPONSE);
        EXPECT_EQ(response.header.payload_size, strlen(test_data) + 1);
        EXPECT_STREQ((const char *)response.payload, test_data);
        rpc_message_free(&response);
    }

    /* 清理 */
    rpc_client_destroy(client);
    rpc_server_stop(server);
    rpc_server_destroy(server);
}

TEST_F(RPCTest, ClientSendWithoutServer) {
    /* 创建客户端，不启动服务端 */
    rpc_client_t *client = rpc_client_create(&config_);
    ASSERT_NE(client, nullptr);

    /* 发送请求应该失败 */
    const char *test_data = "No server";
    rpc_message_t response;
    memset(&response, 0, sizeof(response));

    int rc = rpc_client_call(client, test_data, strlen(test_data) + 1, &response);
    EXPECT_NE(rc, RPC_OK);

    rpc_client_destroy(client);
}

/* ========================================================================
 * 压力测试
 * ======================================================================== */

TEST_F(RPCTest, StressTestMultipleRequests) {
    /* 创建服务端 */
    rpc_server_handler_t handler;
    handler.handle_request = test_handle_request;
    handler.user_data = nullptr;

    rpc_server_t *server = rpc_server_create(&bind_addr_, &handler);
    ASSERT_NE(server, nullptr);
    ASSERT_EQ(rpc_server_start(server), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    /* 创建客户端 */
    rpc_client_t *client = rpc_client_create(&config_);
    ASSERT_NE(client, nullptr);

    /* 发送多个请求 */
    const int num_requests = 10;
    for (int i = 0; i < num_requests; i++) {
        char test_data[64];
        snprintf(test_data, sizeof(test_data), "Request %d", i);

        rpc_message_t response;
        memset(&response, 0, sizeof(response));

        int rc = rpc_client_call(client, test_data, strlen(test_data) + 1, &response);
        EXPECT_EQ(rc, RPC_OK);

        if (rc == RPC_OK) {
            EXPECT_STREQ((const char *)response.payload, test_data);
            rpc_message_free(&response);
        }
    }

    /* 清理 */
    rpc_client_destroy(client);
    rpc_server_stop(server);
    rpc_server_destroy(server);
}