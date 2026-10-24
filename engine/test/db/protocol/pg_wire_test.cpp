/**
 * @file pg_wire_test.cpp
 * @brief PostgreSQL Wire Protocol 兼容层单元测试
 *
 * 测试范围：
 *   - 连接创建与释放
 *   - 消息类型枚举验证
 *   - 启动消息解析
 *   - 认证消息处理
 *   - 简单查询解析与处理
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
#include "db/pg_wire_protocol.h"
}

namespace {

/* ============================================================
 * 辅助函数：构造网络字节序整数
 * ============================================================ */

static void write_be32(char *buf, int32_t val) {
    uint32_t uval = (uint32_t)val;
    buf[0] = (char)((uval >> 24) & 0xFF);
    buf[1] = (char)((uval >> 16) & 0xFF);
    buf[2] = (char)((uval >> 8) & 0xFF);
    buf[3] = (char)(uval & 0xFF);
}

/* ============================================================
 * 测试 fixture，提供 socketpair 用于模拟客户端/服务端通信
 * ============================================================ */

class PGWireTest : public ::testing::Test {
protected:
    int server_sock = -1;
    int client_sock = -1;
    pg_connection_t *conn = nullptr;

    void SetUp() override {
#ifdef _WIN32
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
        int fds[2];
        ASSERT_NE(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), -1);
        server_sock = fds[0];
        client_sock = fds[1];
#endif

        conn = pg_connection_create(server_sock);
        ASSERT_NE(conn, nullptr);
    }

    void TearDown() override {
        if (conn) {
            pg_connection_free(conn);
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
     * @brief 从客户端 socket 读取一个完整的 PG 消息
     *
     * PG 消息格式：1 字节类型 + 4 字节长度（含长度自身）+ 载荷
     * 启动消息无类型字节，直接 4 字节长度 + 4 字节版本 + 键值对
     *
     * @param[out] msg_type 消息类型字节（启动消息返回 0）
     * @return 消息载荷（不含类型字节和 4 字节长度头）
     */
    std::vector<uint8_t> read_message(int sock, char &msg_type) {
        /* 读取第一个字节，判断是否为启动消息 */
        char peek_buf[1];
        ssize_t n = recv(sock, peek_buf, 1, MSG_PEEK);
        EXPECT_GT(n, 0);

        uint8_t first_byte = (uint8_t)peek_buf[0];

        /* 启动消息：前 4 字节为长度（>=8），首字节可能是版本高字节（0x00） */
        /* 正常消息：首字节为 ASCII 字母（消息类型） */
        bool is_startup = (first_byte < 'A' || first_byte > 'z');

        if (is_startup) {
            /* 启动消息：直接读取 4 字节长度 + 载荷 */
            msg_type = 0;
            char len_buf[4];
            ssize_t total = 0;
            while (total < 4) {
                n = recv(sock, len_buf + total, 4 - (int)total, 0);
                EXPECT_GT(n, 0);
                total += n;
            }
            uint32_t msg_len = ((uint32_t)(unsigned char)len_buf[0] << 24)
                             | ((uint32_t)(unsigned char)len_buf[1] << 16)
                             | ((uint32_t)(unsigned char)len_buf[2] << 8)
                             | (uint32_t)(unsigned char)len_buf[3];
            uint32_t payload_len = msg_len - 4;  /* 长度包含自身 4 字节 */

            std::vector<uint8_t> payload(payload_len);
            total = 0;
            while (total < (ssize_t)payload_len) {
                n = recv(sock, (char *)(payload.data() + total), payload_len - (int)total, 0);
                EXPECT_GT(n, 0);
                total += n;
            }
            return payload;
        } else {
            /* 普通消息：1 字节类型 + 4 字节长度 + 载荷 */
            msg_type = (char)first_byte;
            recv(sock, peek_buf, 1, 0);  /* 消费类型字节 */

            char len_buf[4];
            ssize_t total = 0;
            while (total < 4) {
                n = recv(sock, len_buf + total, 4 - (int)total, 0);
                EXPECT_GT(n, 0);
                total += n;
            }
            uint32_t msg_len = ((uint32_t)(unsigned char)len_buf[0] << 24)
                             | ((uint32_t)(unsigned char)len_buf[1] << 16)
                             | ((uint32_t)(unsigned char)len_buf[2] << 8)
                             | (uint32_t)(unsigned char)len_buf[3];
            uint32_t payload_len = msg_len - 4;  /* 长度包含自身 4 字节 */

            std::vector<uint8_t> payload(payload_len);
            total = 0;
            while (total < (ssize_t)payload_len) {
                n = recv(sock, (char *)(payload.data() + total), payload_len - (int)total, 0);
                EXPECT_GT(n, 0);
                total += n;
            }
            return payload;
        }
    }

    /**
     * @brief 向服务端发送启动消息
     */
    void send_startup_message(int sock, const char *user, const char *database) {
        /* 构建键值对 */
        std::vector<uint8_t> body;

        /* 版本号 3.0（网络字节序） */
        body.push_back(0x00);
        body.push_back(0x03);
        body.push_back(0x00);
        body.push_back(0x00);

        /* user\0value\0 */
        const char *key = "user";
        body.insert(body.end(), key, key + strlen(key));
        body.push_back(0);
        body.insert(body.end(), user, user + strlen(user));
        body.push_back(0);

        if (database) {
            key = "database";
            body.insert(body.end(), key, key + strlen(key));
            body.push_back(0);
            body.insert(body.end(), database, database + strlen(database));
            body.push_back(0);
        }

        /* 终止符 */
        body.push_back(0);

        /* 长度 = 4（长度自身） + body.size() */
        uint32_t total_len = 4 + (uint32_t)body.size();
        char len_buf[4];
        len_buf[0] = (char)((total_len >> 24) & 0xFF);
        len_buf[1] = (char)((total_len >> 16) & 0xFF);
        len_buf[2] = (char)((total_len >> 8) & 0xFF);
        len_buf[3] = (char)(total_len & 0xFF);

        send(sock, len_buf, 4, 0);
        send(sock, (const char *)body.data(), (int)body.size(), 0);
    }

    /**
     * @brief 向服务端发送普通消息（类型 + 长度 + 载荷）
     */
    void send_message(int sock, char type, const std::vector<uint8_t> &payload) {
        uint32_t total_len = 4 + (uint32_t)payload.size();
        char header[5];
        header[0] = type;
        header[1] = (char)((total_len >> 24) & 0xFF);
        header[2] = (char)((total_len >> 16) & 0xFF);
        header[3] = (char)((total_len >> 8) & 0xFF);
        header[4] = (char)(total_len & 0xFF);

        send(sock, header, 5, 0);
        if (!payload.empty()) {
            send(sock, (const char *)payload.data(), (int)payload.size(), 0);
        }
    }
};

/* ============================================================
 * 消息类型枚举测试
 * ============================================================ */

TEST(PGMessageTypeTest, FrontendMessageTypes) {
    EXPECT_EQ(PG_MSG_STARTUP, 0);
    EXPECT_EQ(PG_MSG_PASSWORD, 'p');
    EXPECT_EQ(PG_MSG_QUERY, 'Q');
    EXPECT_EQ(PG_MSG_PARSE, 'P');
    EXPECT_EQ(PG_MSG_BIND, 'B');
    EXPECT_EQ(PG_MSG_EXECUTE, 'E');
    EXPECT_EQ(PG_MSG_DESCRIBE, 'D');
    EXPECT_EQ(PG_MSG_SYNC, 'S');
    EXPECT_EQ(PG_MSG_FLUSH, 'H');
    EXPECT_EQ(PG_MSG_CLOSE, 'C');
    EXPECT_EQ(PG_MSG_TERMINATE, 'X');
}

TEST(PGMessageTypeTest, BackendMessageTypes) {
    EXPECT_EQ(PG_MSG_AUTH_REQUEST, 'R');
    EXPECT_EQ(PG_MSG_PARAMETER_STATUS, 'K');
    EXPECT_EQ(PG_MSG_BACKEND_KEY_DATA, 'k');
    EXPECT_EQ(PG_MSG_READY_FOR_QUERY, 'Z');
    EXPECT_EQ(PG_MSG_ROW_DESCRIPTION, 'T');
    EXPECT_EQ(PG_MSG_DATA_ROW, 'D');
    EXPECT_EQ(PG_MSG_COMMAND_COMPLETE, 'C');
    EXPECT_EQ(PG_MSG_EMPTY_QUERY_RESP, 'I');
    EXPECT_EQ(PG_MSG_ERROR_RESPONSE, 'E');
    EXPECT_EQ(PG_MSG_NOTICE_RESPONSE, 'N');
    EXPECT_EQ(PG_MSG_PARSE_COMPLETE, '1');
    EXPECT_EQ(PG_MSG_BIND_COMPLETE, '2');
    EXPECT_EQ(PG_MSG_CLOSE_COMPLETE, '3');
    EXPECT_EQ(PG_MSG_NO_DATA, 'n');
    EXPECT_EQ(PG_MSG_PARAMETER_DESCRIPTION, 't');
}

TEST(PGMessageTypeTest, AuthSubTypes) {
    EXPECT_EQ(PG_AUTH_OK, 0);
    EXPECT_EQ(PG_AUTH_CLEARTEXT_PASSWORD, 3);
    EXPECT_EQ(PG_AUTH_MD5_PASSWORD, 5);
    EXPECT_EQ(PG_AUTH_SASL, 10);
}

TEST(PGMessageTypeTest, TransactionStatus) {
    EXPECT_EQ(PG_TXN_IDLE, 'I');
    EXPECT_EQ(PG_TXN_IN_BLOCK, 'T');
    EXPECT_EQ(PG_TXN_FAILED, 'E');
}

TEST(PGMessageTypeTest, FormatCodes) {
    EXPECT_EQ(PG_FORMAT_TEXT, 0);
    EXPECT_EQ(PG_FORMAT_BINARY, 1);
}

TEST(PGMessageTypeTest, ProtocolVersion) {
    EXPECT_EQ(PG_PROTOCOL_VERSION_MAJOR, 3);
    EXPECT_EQ(PG_PROTOCOL_VERSION_MINOR, 0);
    EXPECT_EQ(PG_PROTOCOL_VERSION, (3 << 16) | 0);
}

/* ============================================================
 * 连接管理测试
 * ============================================================ */

TEST_F(PGWireTest, ConnectionCreate) {
    EXPECT_NE(conn, nullptr);
    EXPECT_EQ(conn->sock, server_sock);
    EXPECT_EQ(conn->state, PG_CONN_STATE_STARTUP);
    EXPECT_FALSE(conn->authenticated);
    EXPECT_EQ(conn->protocol_version, 0);
    EXPECT_EQ(conn->query_count, (uint64_t)0);
}

TEST_F(PGWireTest, ConnectionFree) {
    pg_connection_t *c = pg_connection_create(server_sock);
    ASSERT_NE(c, nullptr);
    pg_connection_free(c);
    /* 重复释放不应崩溃 */
    pg_connection_free(nullptr);
}

TEST_F(PGWireTest, ConnectionReset) {
    conn->authenticated = true;
    conn->state = PG_CONN_STATE_READY;
    conn->query_count = 42;
    conn->user = strdup("testuser");
    conn->database = strdup("testdb");

    int ret = pg_connection_reset(conn);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(conn->state, PG_CONN_STATE_STARTUP);
    EXPECT_FALSE(conn->authenticated);
    EXPECT_EQ(conn->query_count, (uint64_t)0);
    EXPECT_EQ(conn->user, nullptr);
    EXPECT_EQ(conn->database, nullptr);
}

TEST_F(PGWireTest, ConnectionResetNull) {
    EXPECT_EQ(pg_connection_reset(nullptr), -1);
}

/* ============================================================
 * 默认配置测试
 * ============================================================ */

TEST_F(PGWireTest, DefaultConfig) {
    EXPECT_STREQ(conn->config.server_encoding, "UTF8");
    EXPECT_EQ(conn->config.integer_datetimes, 1);
    EXPECT_STREQ(conn->config.client_encoding, "UTF8");
    EXPECT_STREQ(conn->config.database_encoding, "UTF8");
    EXPECT_STREQ(conn->config.date_style, "ISO, MDY");
    EXPECT_STREQ(conn->config.interval_style, "postgres");
    EXPECT_STREQ(conn->config.timezone, "UTC");
    EXPECT_STREQ(conn->config.application_name, "unknown");
}

/* ============================================================
 * 读写缓冲区测试
 * ============================================================ */

TEST_F(PGWireTest, ConnectionWrite) {
    const char data[] = "hello";
    int ret = pg_connection_write(conn, data, 5);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(conn->write_buf_len, 0);
}

TEST_F(PGWireTest, ConnectionWriteNull) {
    EXPECT_EQ(pg_connection_write(nullptr, "x", 1), -1);
}

TEST_F(PGWireTest, ConnectionWriteLargeData) {
    /* 测试缓冲区自动扩展 */
    std::vector<char> big_data(16384, 'A');
    int ret = pg_connection_write(conn, big_data.data(), (int)big_data.size());
    EXPECT_EQ(ret, 0);
    EXPECT_GE(conn->write_buf_size, 16384);
}

TEST_F(PGWireTest, ConnectionFlushEmpty) {
    EXPECT_EQ(pg_connection_flush(conn), 0);
}

/* ============================================================
 * 启动消息测试
 * ============================================================ */

TEST_F(PGWireTest, HandleStartup) {
    /* 构造启动消息体（不含长度前缀）：版本 3.0 + user=test + database=mydb + \0 */
    char body[64];
    int pos = 0;
    write_be32(body + pos, PG_PROTOCOL_VERSION); pos += 4;
    strcpy(body + pos, "user"); pos += 5;  /* 含 \0 */
    strcpy(body + pos, "test"); pos += 5;
    strcpy(body + pos, "database"); pos += 9;
    strcpy(body + pos, "mydb"); pos += 5;
    body[pos++] = '\0';  /* 终止符 */

    int ret = pg_handle_startup(conn, body, pos);
    EXPECT_EQ(ret, 0);
    EXPECT_STREQ(conn->user, "test");
    EXPECT_STREQ(conn->database, "mydb");
    EXPECT_EQ(conn->state, PG_CONN_STATE_READY);
}

TEST_F(PGWireTest, HandleStartupInvalidVersion) {
    char body[64];
    int pos = 0;
    write_be32(body + pos, (2 << 16) | 0); pos += 4;  /* 版本 2.0（不支持） */
    strcpy(body + pos, "user"); pos += 5;
    strcpy(body + pos, "test"); pos += 5;
    body[pos++] = '\0';

    int ret = pg_handle_startup(conn, body, pos);
    EXPECT_EQ(ret, -1);
}

TEST_F(PGWireTest, HandleStartupNull) {
    EXPECT_EQ(pg_handle_startup(nullptr, "x", 1), -1);
    EXPECT_EQ(pg_handle_startup(conn, "x", 1), -1);  /* 长度不足 */
}

TEST_F(PGWireTest, HandleStartupDefaultDatabase) {
    /* 不指定 database 时，使用用户名作为数据库名 */
    char body[64];
    int pos = 0;
    write_be32(body + pos, PG_PROTOCOL_VERSION); pos += 4;
    strcpy(body + pos, "user"); pos += 5;
    strcpy(body + pos, "alice"); pos += 6;
    body[pos++] = '\0';

    int ret = pg_handle_startup(conn, body, pos);
    EXPECT_EQ(ret, 0);
    EXPECT_STREQ(conn->user, "alice");
    EXPECT_STREQ(conn->database, "alice");
}

/* ============================================================
 * 认证消息测试
 * ============================================================ */

TEST_F(PGWireTest, HandlePassword) {
    conn->state = PG_CONN_STATE_AUTH;

    int ret = pg_handle_password(conn, "mypass", 6);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(conn->authenticated);
    EXPECT_EQ(conn->state, PG_CONN_STATE_READY);
}

TEST_F(PGWireTest, HandlePasswordNull) {
    EXPECT_EQ(pg_handle_password(nullptr, "x", 1), -1);
}

TEST_F(PGWireTest, SendAuthOk) {
    char recv_type = 0;
    std::vector<uint8_t> payload;

    std::thread reader([this, &recv_type, &payload]() {
        payload = read_message(client_sock, recv_type);
    });

    int ret = pg_send_auth_ok(conn);
    EXPECT_EQ(ret, 0);
    pg_connection_flush(conn);

    reader.join();

    EXPECT_EQ(recv_type, 'R');  /* AuthenticationOk */
    ASSERT_GE(payload.size(), 4u);
    int32_t auth_type = ((int32_t)payload[0] << 24) | ((int32_t)payload[1] << 16)
                       | ((int32_t)payload[2] << 8) | (int32_t)payload[3];
    EXPECT_EQ(auth_type, PG_AUTH_OK);
}

/* ============================================================
 * 参数状态消息测试
 * ============================================================ */

TEST_F(PGWireTest, SendParameterStatus) {
    char recv_type = 0;
    std::vector<uint8_t> payload;

    std::thread reader([this, &recv_type, &payload]() {
        payload = read_message(client_sock, recv_type);
    });

    int ret = pg_send_parameter_status(conn, "server_version", "17.0");
    EXPECT_EQ(ret, 0);
    pg_connection_flush(conn);

    reader.join();

    EXPECT_EQ(recv_type, 'K');  /* ParameterStatus */

    /* 验证键值对：server_version\017.0\0 */
    std::string msg((const char *)payload.data(), payload.size());
    EXPECT_NE(msg.find("server_version"), std::string::npos);
    EXPECT_NE(msg.find("17.0"), std::string::npos);
}

TEST_F(PGWireTest, SendParameterStatusNull) {
    EXPECT_EQ(pg_send_parameter_status(nullptr, "x", "y"), -1);
    EXPECT_EQ(pg_send_parameter_status(conn, nullptr, "y"), -1);
    EXPECT_EQ(pg_send_parameter_status(conn, "x", nullptr), -1);
}

/* ============================================================
 * 后端密钥数据测试
 * ============================================================ */

TEST_F(PGWireTest, SendBackendKeyData) {
    char recv_type = 0;
    std::vector<uint8_t> payload;

    std::thread reader([this, &recv_type, &payload]() {
        payload = read_message(client_sock, recv_type);
    });

    int ret = pg_send_backend_key_data(conn, 12345, 67890);
    EXPECT_EQ(ret, 0);
    pg_connection_flush(conn);

    reader.join();

    EXPECT_EQ(recv_type, 'k');  /* BackendKeyData */
    ASSERT_GE(payload.size(), 8u);

    int32_t pid = ((int32_t)payload[0] << 24) | ((int32_t)payload[1] << 16)
                 | ((int32_t)payload[2] << 8) | (int32_t)payload[3];
    int32_t key = ((int32_t)payload[4] << 24) | ((int32_t)payload[5] << 16)
                 | ((int32_t)payload[6] << 8) | (int32_t)payload[7];
    EXPECT_EQ(pid, 12345);
    EXPECT_EQ(key, 67890);
}

/* ============================================================
 * ReadyForQuery 测试
 * ============================================================ */

TEST_F(PGWireTest, SendReadyForQueryIdle) {
    conn->state = PG_CONN_STATE_READY;

    char recv_type = 0;
    std::vector<uint8_t> payload;

    std::thread reader([this, &recv_type, &payload]() {
        payload = read_message(client_sock, recv_type);
    });

    int ret = pg_send_ready_for_query(conn);
    EXPECT_EQ(ret, 0);
    pg_connection_flush(conn);

    reader.join();

    EXPECT_EQ(recv_type, 'Z');  /* ReadyForQuery */
    ASSERT_GE(payload.size(), 1u);
    EXPECT_EQ(payload[0], (uint8_t)PG_TXN_IDLE);  /* 'I' */
}

TEST_F(PGWireTest, SendReadyForQueryInTransaction) {
    conn->state = PG_CONN_STATE_QUERY;

    char recv_type = 0;
    std::vector<uint8_t> payload;

    std::thread reader([this, &recv_type, &payload]() {
        payload = read_message(client_sock, recv_type);
    });

    pg_send_ready_for_query(conn);
    pg_connection_flush(conn);

    reader.join();

    EXPECT_EQ(recv_type, 'Z');
    ASSERT_GE(payload.size(), 1u);
    EXPECT_EQ(payload[0], (uint8_t)PG_TXN_IN_BLOCK);  /* 'T' */
}

/* ============================================================
 * 错误响应测试
 * ============================================================ */

TEST_F(PGWireTest, SendErrorResponse) {
    char recv_type = 0;
    std::vector<uint8_t> payload;

    std::thread reader([this, &recv_type, &payload]() {
        payload = read_message(client_sock, recv_type);
    });

    int ret = pg_send_error_response(conn, "ERROR", "42P01", "relation does not exist");
    EXPECT_EQ(ret, 0);
    pg_connection_flush(conn);

    reader.join();

    EXPECT_EQ(recv_type, 'E');  /* ErrorResponse */

    /* 验证字段：S\0ERROR\0C\042P01\0M\0relation does not exist\0\0 */
    std::string msg((const char *)payload.data(), payload.size());
    EXPECT_NE(msg.find("ERROR"), std::string::npos);
    EXPECT_NE(msg.find("42P01"), std::string::npos);
    EXPECT_NE(msg.find("relation does not exist"), std::string::npos);
}

TEST_F(PGWireTest, SendErrorResponseNull) {
    EXPECT_EQ(pg_send_error_response(nullptr, "E", "XX000", "msg"), -1);
    EXPECT_EQ(pg_send_error_response(conn, nullptr, "XX000", "msg"), -1);
    EXPECT_EQ(pg_send_error_response(conn, "E", nullptr, "msg"), -1);
    EXPECT_EQ(pg_send_error_response(conn, "E", "XX000", nullptr), -1);
}

/* ============================================================
 * 简单查询测试
 * ============================================================ */

TEST_F(PGWireTest, HandleQuery) {
    conn->state = PG_CONN_STATE_READY;

    int ret = pg_handle_query(conn, "SELECT 1");
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(conn->query_count, (uint64_t)1);
    EXPECT_EQ(conn->state, PG_CONN_STATE_READY);
}

TEST_F(PGWireTest, HandleQueryNull) {
    EXPECT_EQ(pg_handle_query(nullptr, "SELECT 1"), -1);
    EXPECT_EQ(pg_handle_query(conn, nullptr), -1);
}

TEST_F(PGWireTest, HandleQueryIncrementsCounter) {
    conn->state = PG_CONN_STATE_READY;
    conn->query_count = 0;

    pg_handle_query(conn, "SELECT 1");
    EXPECT_EQ(conn->query_count, (uint64_t)1);

    pg_handle_query(conn, "SELECT 2");
    EXPECT_EQ(conn->query_count, (uint64_t)2);
}

TEST_F(PGWireTest, HandleQuerySendsResponse) {
    conn->state = PG_CONN_STATE_READY;

    char recv_type1 = 0, recv_type2 = 0;
    std::vector<uint8_t> payload1, payload2;

    std::thread reader([this, &recv_type1, &payload1, &recv_type2, &payload2]() {
        payload1 = read_message(client_sock, recv_type1);
        payload2 = read_message(client_sock, recv_type2);
    });

    pg_handle_query(conn, "SELECT 1");
    pg_connection_flush(conn);

    reader.join();

    /* 应收到 EmptyQueryResponse + ReadyForQuery */
    EXPECT_EQ(recv_type1, 'I');  /* EmptyQueryResponse */
    EXPECT_EQ(recv_type2, 'Z');  /* ReadyForQuery */
}

/* ============================================================
 * 终止消息测试
 * ============================================================ */

TEST_F(PGWireTest, HandleTerminate) {
    int ret = pg_handle_terminate(conn);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(conn->state, PG_CONN_STATE_TERMINATED);
}

TEST_F(PGWireTest, HandleTerminateNull) {
    EXPECT_EQ(pg_handle_terminate(nullptr), 0);
}

/* ============================================================
 * 扩展查询测试
 * ============================================================ */

TEST_F(PGWireTest, HandleParse) {
    conn->state = PG_CONN_STATE_READY;

    int32_t param_oids[] = {PG_TYPE_OID_INT4};
    int ret = pg_handle_parse(conn, "stmt1", "SELECT $1", 1, param_oids);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(conn->query_count, (uint64_t)1);
}

TEST_F(PGWireTest, HandleParseNull) {
    EXPECT_EQ(pg_handle_parse(nullptr, nullptr, "SELECT 1", 0, nullptr), -1);
}

TEST_F(PGWireTest, HandleBind) {
    conn->state = PG_CONN_STATE_READY;

    int ret = pg_handle_bind(conn, "portal1", "stmt1", 0, nullptr, nullptr, nullptr, PG_FORMAT_TEXT);
    EXPECT_EQ(ret, 0);
    EXPECT_STREQ(conn->portal_name, "portal1");
}

TEST_F(PGWireTest, HandleBindNull) {
    EXPECT_EQ(pg_handle_bind(nullptr, nullptr, nullptr, 0, nullptr, nullptr, nullptr, 0), -1);
}

TEST_F(PGWireTest, HandleExecute) {
    conn->state = PG_CONN_STATE_READY;

    int ret = pg_handle_execute(conn, "portal1", 0);
    EXPECT_EQ(ret, 0);
}

TEST_F(PGWireTest, HandleExecuteNull) {
    EXPECT_EQ(pg_handle_execute(nullptr, nullptr, 0), -1);
}

TEST_F(PGWireTest, HandleDescribe) {
    int ret = pg_handle_describe(conn, 'S', "stmt1");
    EXPECT_EQ(ret, 0);
}

TEST_F(PGWireTest, HandleDescribeNull) {
    EXPECT_EQ(pg_handle_describe(nullptr, 'S', "x"), -1);
}

TEST_F(PGWireTest, HandleSync) {
    int ret = pg_handle_sync(conn);
    EXPECT_EQ(ret, 0);
}

TEST_F(PGWireTest, HandleSyncNull) {
    EXPECT_EQ(pg_handle_sync(nullptr), -1);
}

TEST_F(PGWireTest, HandleClose) {
    int ret = pg_handle_close(conn, 'S', "stmt1");
    EXPECT_EQ(ret, 0);
}

TEST_F(PGWireTest, HandleCloseNull) {
    EXPECT_EQ(pg_handle_close(nullptr, 'S', "x"), -1);
}

/* ============================================================
 * 结果集测试
 * ============================================================ */

TEST_F(PGWireTest, ResultCreate) {
    pg_result_t *result = pg_result_create();
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result->num_columns, 0);
    EXPECT_EQ(result->num_rows, 0);
    EXPECT_FALSE(result->is_error);
    pg_result_free(result);
}

TEST_F(PGWireTest, ResultFreeNull) {
    pg_result_free(nullptr);  /* 释放 NULL 不崩溃 */
}

TEST_F(PGWireTest, ResultSetColumns) {
    pg_result_t *result = pg_result_create();
    ASSERT_NE(result, nullptr);

    pg_column_desc_t cols[2];
    memset(cols, 0, sizeof(cols));
    cols[0].name = strdup("id");
    cols[0].type_oid = PG_TYPE_OID_INT4;
    cols[0].format_code = PG_FORMAT_TEXT;
    cols[1].name = strdup("name");
    cols[1].type_oid = PG_TYPE_OID_TEXT;
    cols[1].format_code = PG_FORMAT_TEXT;

    int ret = pg_result_set_columns(result, cols, 2);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result->num_columns, 2);
    EXPECT_STREQ(result->columns[0].name, "id");
    EXPECT_STREQ(result->columns[1].name, "name");

    free(cols[0].name);
    free(cols[1].name);
    pg_result_free(result);
}

TEST_F(PGWireTest, ResultSetColumnsNull) {
    pg_result_t *result = pg_result_create();
    EXPECT_EQ(pg_result_set_columns(result, nullptr, 0), -1);
    pg_result_free(result);
}

TEST_F(PGWireTest, ResultAddRow) {
    pg_result_t *result = pg_result_create();
    ASSERT_NE(result, nullptr);

    pg_column_desc_t cols[2];
    memset(cols, 0, sizeof(cols));
    cols[0].name = strdup("id");
    cols[1].name = strdup("name");
    pg_result_set_columns(result, cols, 2);

    const char *values[] = {"1", "Alice"};
    int32_t lens[] = {1, 5};
    int ret = pg_result_add_row(result, values, lens, 2);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result->num_rows, 1);

    free(cols[0].name);
    free(cols[1].name);
    pg_result_free(result);
}

TEST_F(PGWireTest, ResultAddRowNull) {
    pg_result_t *result = pg_result_create();
    EXPECT_EQ(pg_result_add_row(result, nullptr, nullptr, 0), -1);
    pg_result_free(result);
}

TEST_F(PGWireTest, ResultSetCommand) {
    pg_result_t *result = pg_result_create();
    ASSERT_NE(result, nullptr);

    pg_result_set_command(result, "INSERT 0 1", 1, 1);
    EXPECT_STREQ(result->cmd_status, "INSERT 0 1");
    EXPECT_EQ(result->cmd_affected, (int64_t)1);
    EXPECT_EQ(result->cmd_oid, (int64_t)1);

    pg_result_free(result);
}

TEST_F(PGWireTest, ResultSetError) {
    pg_result_t *result = pg_result_create();
    ASSERT_NE(result, nullptr);

    pg_result_set_error(result, "ERROR", "42P01", "table not found");
    EXPECT_TRUE(result->is_error);
    EXPECT_STREQ(result->error_severity, "ERROR");
    EXPECT_STREQ(result->error_sqlstate, "42P01");
    EXPECT_STREQ(result->error_message, "table not found");

    pg_result_free(result);
}

/* ============================================================
 * 错误响应字段标识符测试
 * ============================================================ */

TEST(PGErrFieldTest, FieldIdentifiers) {
    EXPECT_EQ(PG_ERR_SEVERITY, 'S');
    EXPECT_EQ(PG_ERR_SQLSTATE, 'C');
    EXPECT_EQ(PG_ERR_MESSAGE, 'M');
    EXPECT_EQ(PG_ERR_DETAIL, 'D');
    EXPECT_EQ(PG_ERR_HINT, 'H');
    EXPECT_EQ(PG_ERR_END, '\0');
}

/* ============================================================
 * 工具函数测试
 * ============================================================ */

TEST(PGUtilsTest, BuildCommandTag) {
    char buf[64];

    pg_build_command_tag(buf, sizeof(buf), "SELECT", 0, 5);
    EXPECT_STREQ(buf, "SELECT 5");

    pg_build_command_tag(buf, sizeof(buf), "INSERT", 1, 1);
    EXPECT_STREQ(buf, "INSERT 1 1");

    pg_build_command_tag(buf, sizeof(buf), "UPDATE", 0, 3);
    EXPECT_STREQ(buf, "UPDATE 3");

    pg_build_command_tag(buf, sizeof(buf), "DELETE", 0, 2);
    EXPECT_STREQ(buf, "DELETE 2");
}

TEST(PGUtilsTest, BuildCommandTagSmallBuffer) {
    char buf[3];
    pg_build_command_tag(buf, sizeof(buf), "SELECT", 0, 100);
    /* 应截断，不溢出 */
    EXPECT_LT(strlen(buf), sizeof(buf));
}

TEST(PGUtilsTest, TypeOidFromCType) {
    EXPECT_EQ(pg_type_oid_from_c_type("bool"), PG_TYPE_OID_BOOL);
    EXPECT_EQ(pg_type_oid_from_c_type("int4"), PG_TYPE_OID_INT4);
    EXPECT_EQ(pg_type_oid_from_c_type("int8"), PG_TYPE_OID_INT8);
    EXPECT_EQ(pg_type_oid_from_c_type("int2"), PG_TYPE_OID_INT2);
    EXPECT_EQ(pg_type_oid_from_c_type("float4"), PG_TYPE_OID_FLOAT4);
    EXPECT_EQ(pg_type_oid_from_c_type("float8"), PG_TYPE_OID_FLOAT8);
    EXPECT_EQ(pg_type_oid_from_c_type("text"), PG_TYPE_OID_TEXT);
    EXPECT_EQ(pg_type_oid_from_c_type("bytea"), PG_TYPE_OID_BYTEA);
    EXPECT_EQ(pg_type_oid_from_c_type("string"), PG_TYPE_OID_TEXT);
    EXPECT_EQ(pg_type_oid_from_c_type("double"), PG_TYPE_OID_FLOAT8);
    EXPECT_EQ(pg_type_oid_from_c_type("unknown"), PG_TYPE_OID_UNKNOWN);
    EXPECT_EQ(pg_type_oid_from_c_type(nullptr), PG_TYPE_OID_UNKNOWN);
}

/* ============================================================
 * 数据类型 OID 常量测试
 * ============================================================ */

TEST(PGTypeOIDTest, OIDs) {
    EXPECT_EQ(PG_TYPE_OID_BOOL, 16);
    EXPECT_EQ(PG_TYPE_OID_BYTEA, 17);
    EXPECT_EQ(PG_TYPE_OID_INT8, 20);
    EXPECT_EQ(PG_TYPE_OID_INT2, 21);
    EXPECT_EQ(PG_TYPE_OID_INT4, 23);
    EXPECT_EQ(PG_TYPE_OID_TEXT, 25);
    EXPECT_EQ(PG_TYPE_OID_FLOAT4, 700);
    EXPECT_EQ(PG_TYPE_OID_FLOAT8, 701);
    EXPECT_EQ(PG_TYPE_OID_VARCHAR, 1043);
    EXPECT_EQ(PG_TYPE_OID_DATE, 1082);
    EXPECT_EQ(PG_TYPE_OID_TIMESTAMP, 1114);
    EXPECT_EQ(PG_TYPE_OID_JSON, 114);
    EXPECT_EQ(PG_TYPE_OID_UUID, 2950);
}

}  // namespace
