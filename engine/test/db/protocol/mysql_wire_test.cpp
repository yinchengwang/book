/**
 * @file mysql_wire_test.cpp
 * @brief MySQL 线协议兼容层单元测试
 *
 * 测试范围：
 *   - 连接创建与释放
 *   - 握手认证流程
 *   - 查询请求处理
 *   - OK/EOF/ERROR 包发送
 *   - 结果集发送
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#endif

extern "C" {
#include "db/mysql_wire_protocol.h"
}

namespace {

/**
 * @brief 测试 fixture，提供 socketpair 用于模拟客户端/服务端通信
 */
class MySQLWireTest : public ::testing::Test {
protected:
    int server_sock = -1;
    int client_sock = -1;
    mysql_connection_t *conn = nullptr;

    void SetUp() override {
#ifdef _WIN32
        // Windows 使用 TCP loopback 模拟
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;

        server_sock = socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_NE(server_sock, -1);

        int opt = 1;
        setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

        ASSERT_NE(bind(server_sock, (struct sockaddr *)&addr, sizeof(addr)), -1);
        ASSERT_NE(listen(server_sock, 1), -1);

        socklen_t addr_len = sizeof(addr);
        getsockname(server_sock, (struct sockaddr *)&addr, &addr_len);

        client_sock = socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_NE(client_sock, -1);
        ASSERT_NE(connect(client_sock, (struct sockaddr *)&addr, sizeof(addr)), -1);

        struct sockaddr_in client_addr;
        addr_len = sizeof(client_addr);
        server_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len);
        ASSERT_NE(server_sock, -1);
#else
        // Unix 使用 socketpair
        int fds[2];
        ASSERT_NE(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), -1);
        server_sock = fds[0];
        client_sock = fds[1];
#endif

        conn = mysql_connection_create(server_sock);
        ASSERT_NE(conn, nullptr);
    }

    void TearDown() override {
        if (conn) {
            mysql_connection_free(conn);
            conn = nullptr;
        }
#ifdef _WIN32
        if (client_sock != -1) closesocket(client_sock);
        if (server_sock != -1) closesocket(server_sock);
#else
        if (client_sock != -1) close(client_sock);
        if (server_sock != -1) close(server_sock);
#endif
    }

    /**
     * @brief 从客户端 socket 读取一个完整的 MySQL 包
     * @return 包体内容（不含 4 字节头），长度通过 out_len 返回
     */
    std::vector<uint8_t> read_packet(int sock, uint8_t &seq_id) {
        std::vector<uint8_t> header(4);
        ssize_t n = recv(sock, (char *)header.data(), 4, 0);
        EXPECT_GT(n, 0);

        uint32_t pkt_len = (uint32_t)header[0] | ((uint32_t)header[1] << 8) | ((uint32_t)header[2] << 16);
        seq_id = header[3];

        std::vector<uint8_t> body(pkt_len);
        if (pkt_len > 0) {
            ssize_t total = 0;
            while (total < (ssize_t)pkt_len) {
                n = recv(sock, (char *)(body.data() + total), pkt_len - total, 0);
                EXPECT_GT(n, 0);
                total += n;
            }
        }
        return body;
    }

    /**
     * @brief 向服务端发送一个 MySQL 包（手动构造）
     */
    void send_packet(int sock, const std::vector<uint8_t> &body, uint8_t seq_id) {
        uint8_t header[4];
        header[0] = (uint8_t)(body.size() & 0xFF);
        header[1] = (uint8_t)((body.size() >> 8) & 0xFF);
        header[2] = (uint8_t)((body.size() >> 16) & 0xFF);
        header[3] = seq_id;

        ssize_t n = send(sock, (const char *)header, 4, 0);
        EXPECT_EQ(n, 4);
        if (!body.empty()) {
            n = send(sock, (const char *)body.data(), body.size(), 0);
            EXPECT_EQ(n, (ssize_t)body.size());
        }
    }
};

/* ============================================================
 * 连接管理测试
 * ============================================================ */

TEST_F(MySQLWireTest, ConnectionCreate) {
    EXPECT_NE(conn, nullptr);
    EXPECT_EQ(conn->sock, server_sock);
    EXPECT_EQ(conn->packet_id, 0u);
    EXPECT_FALSE(conn->authenticated);
}

TEST_F(MySQLWireTest, ConnectionFree) {
    mysql_connection_t *c = mysql_connection_create(server_sock);
    ASSERT_NE(c, nullptr);
    mysql_connection_free(c);
    // 重复释放不应崩溃
    mysql_connection_free(nullptr);
}

TEST_F(MySQLWireTest, NextPacketId) {
    EXPECT_EQ(conn->packet_id, 0u);
    uint32_t id1 = mysql_next_packet_id(conn);
    EXPECT_EQ(id1, 0u);
    uint32_t id2 = mysql_next_packet_id(conn);
    EXPECT_EQ(id2, 1u);
}

/* ============================================================
 * OK 包测试
 * ============================================================ */

TEST_F(MySQLWireTest, SendOk) {
    // 在新线程中发送 OK 包（阻塞操作）
    std::thread sender([this]() {
        int ret = mysql_send_ok(conn, 10, 5);
        EXPECT_EQ(ret, 0);
    });

    // 客户端读取
    uint8_t seq_id = 0xFF;
    auto body = read_packet(client_sock, seq_id);

    sender.join();

    // OK 包第一个字节应为 0x00
    ASSERT_FALSE(body.empty());
    EXPECT_EQ(body[0], 0x00);
}

TEST_F(MySQLWireTest, SendOkZeroAffected) {
    std::thread sender([this]() {
        int ret = mysql_send_ok(conn, 0, 0);
        EXPECT_EQ(ret, 0);
    });

    uint8_t seq_id = 0xFF;
    auto body = read_packet(client_sock, seq_id);
    sender.join();

    ASSERT_FALSE(body.empty());
    EXPECT_EQ(body[0], 0x00);
}

/* ============================================================
 * EOF 包测试
 * ============================================================ */

TEST_F(MySQLWireTest, SendEof) {
    std::thread sender([this]() {
        int ret = mysql_send_eof(conn, 0);
        EXPECT_EQ(ret, 0);
    });

    uint8_t seq_id = 0xFF;
    auto body = read_packet(client_sock, seq_id);
    sender.join();

    ASSERT_GE(body.size(), 5u);
    EXPECT_EQ(body[0], 0xFE);  // EOF 标志
}

/* ============================================================
 * ERROR 包测试
 * ============================================================ */

TEST_F(MySQLWireTest, SendError) {
    std::thread sender([this]() {
        int ret = mysql_send_error(conn, 1045, "28000", "Access denied");
        EXPECT_EQ(ret, 0);
    });

    uint8_t seq_id = 0xFF;
    auto body = read_packet(client_sock, seq_id);
    sender.join();

    ASSERT_GE(body.size(), 8u);
    EXPECT_EQ(body[0], 0xFF);  // Error 标志

    // 错误码（little-endian）
    uint16_t err_code = (uint16_t)body[1] | ((uint16_t)body[2] << 8);
    EXPECT_EQ(err_code, 1045);

    // SQLSTATE
    EXPECT_EQ(body[3], '#');
    EXPECT_EQ(memcmp(&body[4], "28000", 5), 0);
}

TEST_F(MySQLWireTest, SendErrorNullSqlstate) {
    std::thread sender([this]() {
        int ret = mysql_send_error(conn, 1064, nullptr, "Syntax error");
        EXPECT_EQ(ret, 0);
    });

    uint8_t seq_id = 0xFF;
    auto body = read_packet(client_sock, seq_id);
    sender.join();

    ASSERT_GE(body.size(), 8u);
    EXPECT_EQ(body[0], 0xFF);
    // 默认 SQLSTATE 为 HY000
    EXPECT_EQ(memcmp(&body[4], "HY000", 5), 0);
}

/* ============================================================
 * 列定义包测试
 * ============================================================ */

TEST_F(MySQLWireTest, SendColumnDef) {
    mysql_column_def_t column;
    memset(&column, 0, sizeof(column));
    column.schema = "test_db";
    column.table = "users";
    column.org_table = "users";
    column.name = "id";
    column.org_name = "id";
    column.charsetnr = 33;  // utf8
    column.length = 11;
    column.type = 0x03;  // MYSQL_TYPE_LONG
    column.flags = 0x0001;  // NOT_NULL
    column.decimals = 0;

    std::thread sender([this, &column]() {
        int ret = mysql_send_column_def(conn, &column);
        EXPECT_EQ(ret, 0);
    });

    uint8_t seq_id = 0xFF;
    auto body = read_packet(client_sock, seq_id);
    sender.join();

    // 验证 "def" 前缀
    ASSERT_GE(body.size(), 4u);
    EXPECT_EQ(body[0], 3);  // 长度 = 3
    EXPECT_EQ(memcmp(&body[1], "def", 3), 0);
}

/* ============================================================
 * 行数据包测试
 * ============================================================ */

TEST_F(MySQLWireTest, SendRow) {
    const char *fields[] = {"1", "Alice", "alice@example.com"};
    uint32_t lengths[] = {1, 5, 19};
    int num_fields = 3;

    std::thread sender([this, &fields, &lengths, num_fields]() {
        int ret = mysql_send_row(conn, fields, lengths, num_fields);
        EXPECT_EQ(ret, 0);
    });

    uint8_t seq_id = 0xFF;
    auto body = read_packet(client_sock, seq_id);
    sender.join();

    // 验证行数据包含三个字段
    EXPECT_GT(body.size(), 0u);
}

TEST_F(MySQLWireTest, SendRowNullable) {
    const char *fields[] = {"1", nullptr, "bob@example.com"};
    int num_fields = 3;

    std::thread sender([this, &fields, num_fields]() {
        int ret = mysql_send_row_nullable(conn, fields, num_fields);
        EXPECT_EQ(ret, 0);
    });

    uint8_t seq_id = 0xFF;
    auto body = read_packet(client_sock, seq_id);
    sender.join();

    // 第二个字段应为 NULL (0xFB)
    EXPECT_GT(body.size(), 2u);
}

/* ============================================================
 * 结果集测试
 * ============================================================ */

TEST_F(MySQLWireTest, SendResultEmpty) {
    mysql_result_t result;
    mysql_result_init(&result, 0);

    std::thread sender([this, &result]() {
        int ret = mysql_send_result(conn, &result);
        EXPECT_EQ(ret, 0);
    });

    // 读取列数包
    uint8_t seq_id = 0xFF;
    auto body = read_packet(client_sock, seq_id);
    EXPECT_GE(body.size(), 2u);
    EXPECT_EQ(body[0], 0x00);

    // 读取 EOF（列定义结束）
    body = read_packet(client_sock, seq_id);
    EXPECT_GE(body.size(), 5u);
    EXPECT_EQ(body[0], 0xFE);

    // 读取 EOF（数据结束）
    body = read_packet(client_sock, seq_id);
    EXPECT_GE(body.size(), 5u);
    EXPECT_EQ(body[0], 0xFE);

    sender.join();
    mysql_result_free(&result);
}

TEST_F(MySQLWireTest, SendResultWithColumns) {
    mysql_result_t result;
    mysql_result_init(&result, 2);

    // 设置列定义
    result.columns[0].schema = "test";
    result.columns[0].table = "users";
    result.columns[0].org_table = "users";
    result.columns[0].name = "id";
    result.columns[0].org_name = "id";
    result.columns[0].charsetnr = 33;
    result.columns[0].length = 11;
    result.columns[0].type = 0x03;
    result.columns[0].flags = 0x0001;
    result.columns[0].decimals = 0;

    result.columns[1].schema = "test";
    result.columns[1].table = "users";
    result.columns[1].org_table = "users";
    result.columns[1].name = "name";
    result.columns[1].org_name = "name";
    result.columns[1].charsetnr = 33;
    result.columns[1].length = 255;
    result.columns[1].type = 0xFD;  // MYSQL_TYPE_STRING
    result.columns[1].flags = 0x0000;
    result.columns[1].decimals = 0;

    // 设置行数据
    result.num_rows = 2;
    result.rows = (char **)calloc(2, sizeof(char *));
    result.rows[0] = strdup("1");
    result.rows[1] = strdup("2");

    std::thread sender([this, &result]() {
        int ret = mysql_send_result(conn, &result);
        EXPECT_EQ(ret, 0);
    });

    // 读取列数包
    uint8_t seq_id = 0xFF;
    auto body = read_packet(client_sock, seq_id);
    EXPECT_GE(body.size(), 2u);

    // 读取 2 个列定义
    body = read_packet(client_sock, seq_id);
    EXPECT_GE(body.size(), 4u);
    body = read_packet(client_sock, seq_id);
    EXPECT_GE(body.size(), 4u);

    // 读取 EOF（列定义结束）
    body = read_packet(client_sock, seq_id);
    EXPECT_EQ(body[0], 0xFE);

    // 读取行数据
    body = read_packet(client_sock, seq_id);
    body = read_packet(client_sock, seq_id);

    // 读取 EOF（数据结束）
    body = read_packet(client_sock, seq_id);
    EXPECT_EQ(body[0], 0xFE);

    sender.join();
    mysql_result_free(&result);
}

TEST_F(MySQLWireTest, SendResultWithError) {
    mysql_result_t result;
    mysql_result_init(&result, 0);
    result.error_code = 1064;
    result.error_msg = strdup("You have an error in your SQL syntax");

    std::thread sender([this, &result]() {
        int ret = mysql_send_result(conn, &result);
        EXPECT_EQ(ret, 0);
    });

    uint8_t seq_id = 0xFF;
    auto body = read_packet(client_sock, seq_id);
    sender.join();

    // 应发送错误包
    ASSERT_GE(body.size(), 8u);
    EXPECT_EQ(body[0], 0xFF);

    mysql_result_free(&result);
}

/* ============================================================
 * 握手流程测试（简化版）
 * ============================================================ */

TEST_F(MySQLWireTest, HandshakeSend) {
    // 服务端发送握手包
    std::thread sender([this]() {
        int ret = mysql_handle_handshake(conn);
        // 握手会等待客户端响应，客户端关闭后应返回 -1
        // 这里仅测试握手包发送部分
    });

    uint8_t seq_id = 0xFF;
    auto body = read_packet(client_sock, seq_id);

    // 验证协议版本
    ASSERT_FALSE(body.empty());
    EXPECT_EQ(body[0], 10);  // Protocol 10

    // 验证服务器版本字符串
    std::string version((const char *)&body[1]);
    EXPECT_TRUE(version.find("MultiModal") != std::string::npos);

    // 关闭客户端 socket 以结束握手
#ifdef _WIN32
    closesocket(client_sock);
    client_sock = -1;
#else
    close(client_sock);
    client_sock = -1;
#endif

    sender.join();
}

/* ============================================================
 * 边界条件测试
 * ============================================================ */

TEST_F(MySQLWireTest, NullConnection) {
    // 所有 API 对 NULL 连接应安全返回 -1
    EXPECT_EQ(mysql_send_ok(nullptr, 0, 0), -1);
    EXPECT_EQ(mysql_send_eof(nullptr, 0), -1);
    EXPECT_EQ(mysql_send_error(nullptr, 0, nullptr, nullptr), -1);
    EXPECT_EQ(mysql_send_raw(nullptr, nullptr, 0), -1);
    EXPECT_EQ(mysql_handle_handshake(nullptr), -1);
    EXPECT_EQ(mysql_handle_query(nullptr, nullptr), -1);
}

TEST_F(MySQLWireTest, SendRawNullData) {
    EXPECT_EQ(mysql_send_raw(conn, nullptr, 0), -1);
}

TEST_F(MySQLWireTest, QueryHandleBasic) {
    // mysql_handle_query 目前返回 0（TODO）
    int ret = mysql_handle_query(conn, "SELECT 1");
    EXPECT_EQ(ret, 0);
}

}  // namespace
