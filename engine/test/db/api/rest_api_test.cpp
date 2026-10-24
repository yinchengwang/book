/**
 * @file rest_api_test.cpp
 * @brief REST API 集成测试
 */
#include <gtest/gtest.h>
#include <db/api/rest_api.h>
#include <db/metrics.h>

#include <thread>
#include <chrono>
#include <atomic>

/* Windows 网络库 */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

/**
 * @brief 测试配置
 */
static const int TEST_PORT = 18080;

class RestAPITest : public ::testing::Test {
protected:
    void SetUp() override {
        metrics_init();

        RestServerConfig config = rest_server_default_config();
        config.port = TEST_PORT;
        config.data_dir = "./test_data";

        server_ = rest_server_create(&config);
        ASSERT_NE(server_, nullptr);

        rest_server_start(server_);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override {
        if (server_) {
            rest_server_stop(server_);
            rest_server_destroy(server_);
            server_ = nullptr;
        }
    }

    RestServer *server_ = nullptr;
};

/**
 * @brief 简单的 HTTP 请求工具
 */
static std::string http_get(const std::string &host, int port, const std::string &path) {
#ifdef _WIN32
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return "";

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port);
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        closesocket(sock);
        WSACleanup();
        return "";
    }

    std::string request = "GET " + path + " HTTP/1.1\r\n"
                          "Host: " + host + ":" + std::to_string(port) + "\r\n"
                          "Connection: close\r\n"
                          "\r\n";

    send(sock, request.c_str(), (int)request.length(), 0);

    char buf[4096];
    std::string response;
    while (true) {
        int n = recv(sock, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;
        buf[n] = '\0';
        response += buf;
    }

    closesocket(sock);
    WSACleanup();

    /* 提取 body */
    size_t body_start = response.find("\r\n\r\n");
    if (body_start != std::string::npos) {
        return response.substr(body_start + 4);
    }
    return "";
#else
    return "";
#endif
}

/**
 * @brief 简单的 POST 请求工具
 */
static std::string http_post(const std::string &host, int port, const std::string &path,
                             const std::string &body) {
#ifdef _WIN32
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return "";

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port);
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        closesocket(sock);
        WSACleanup();
        return "";
    }

    std::string request = "POST " + path + " HTTP/1.1\r\n"
                          "Host: " + host + ":" + std::to_string(port) + "\r\n"
                          "Content-Type: application/json\r\n"
                          "Content-Length: " + std::to_string(body.length()) + "\r\n"
                          "Connection: close\r\n"
                          "\r\n" + body;

    send(sock, request.c_str(), (int)request.length(), 0);

    char buf[4096];
    std::string response;
    while (true) {
        int n = recv(sock, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;
        buf[n] = '\0';
        response += buf;
    }

    closesocket(sock);
    WSACleanup();

    /* 提取 body */
    size_t body_start = response.find("\r\n\r\n");
    if (body_start != std::string::npos) {
        return response.substr(body_start + 4);
    }
    return "";
#else
    return "";
#endif
}

/* ========================================================================
 * 健康检查测试
 * ======================================================================== */

TEST_F(RestAPITest, HealthEndpoint) {
    std::string response = http_get("127.0.0.1", TEST_PORT, "/health");
    EXPECT_FALSE(response.empty());

    /* 检查是否包含 status 字段 */
    EXPECT_NE(response.find("\"status\""), std::string::npos);
}

TEST_F(RestAPITest, ReadyEndpoint) {
    std::string response = http_get("127.0.0.1", TEST_PORT, "/ready");
    EXPECT_FALSE(response.empty());
    EXPECT_NE(response.find("\"status\""), std::string::npos);
}

TEST_F(RestAPITest, LiveEndpoint) {
    std::string response = http_get("127.0.0.1", TEST_PORT, "/live");
    EXPECT_FALSE(response.empty());
    EXPECT_NE(response.find("alive"), std::string::npos);
}

TEST_F(RestAPITest, MetricsEndpoint) {
    std::string response = http_get("127.0.0.1", TEST_PORT, "/metrics");
    EXPECT_FALSE(response.empty());
    /* Prometheus 格式应该包含 HELP 和 TYPE */
    EXPECT_NE(response.find("# HELP"), std::string::npos);
}

/* ========================================================================
 * 集合管理测试
 * ======================================================================== */

TEST_F(RestAPITest, CreateCollection) {
    std::string body = R"({"name":"test_collection","dimension":128})";
    std::string response = http_post("127.0.0.1", TEST_PORT, "/collections", body);
    EXPECT_FALSE(response.empty());
    EXPECT_NE(response.find("\"name\""), std::string::npos);
}

TEST_F(RestAPITest, ListCollections) {
    std::string response = http_get("127.0.0.1", TEST_PORT, "/collections");
    EXPECT_FALSE(response.empty());
    /* 应该是数组 */
    EXPECT_EQ(response.front(), '[');
}

TEST_F(RestAPITest, GetCollection) {
    /* 先创建集合 */
    std::string body = R"({"name":"test_get","dimension":64})";
    http_post("127.0.0.1", TEST_PORT, "/collections", body);

    /* 获取集合 */
    std::string response = http_get("127.0.0.1", TEST_PORT, "/collections/test_get");
    EXPECT_FALSE(response.empty());
}

TEST_F(RestAPITest, DeleteCollection) {
    /* 先创建集合 */
    std::string body = R"({"name":"test_delete","dimension":32})";
    http_post("127.0.0.1", TEST_PORT, "/collections", body);

    /* 删除集合 */
    std::string response = http_get("127.0.0.1", TEST_PORT, "/collections/test_delete");
    EXPECT_FALSE(response.empty());
}

/* ========================================================================
 * 向量操作测试
 * ======================================================================== */

TEST_F(RestAPITest, InsertVectors) {
    /* 先创建集合 */
    std::string coll = R"({"name":"test_vectors","dimension":4})";
    http_post("127.0.0.1", TEST_PORT, "/collections", coll);

    /* 插入向量 */
    std::string body = R"({"vectors":[{"id":"v1","data":[0.1,0.2,0.3,0.4]}]})";
    std::string response = http_post("127.0.0.1", TEST_PORT,
                                     "/collections/test_vectors/vectors", body);
    EXPECT_FALSE(response.empty());
}

TEST_F(RestAPITest, SearchVectors) {
    /* 先创建集合 */
    std::string coll = R"({"name":"test_search","dimension":4})";
    http_post("127.0.0.1", TEST_PORT, "/collections", coll);

    /* 搜索 */
    std::string body = R"({"vector":[0.1,0.2,0.3,0.4],"top_k":10})";
    std::string response = http_post("127.0.0.1", TEST_PORT,
                                     "/collections/test_search/search", body);
    EXPECT_FALSE(response.empty());
}

/* ========================================================================
 * 错误处理测试
 * ======================================================================== */

TEST_F(RestAPITest, NotFoundRoute) {
    std::string response = http_get("127.0.0.1", TEST_PORT, "/nonexistent");
    EXPECT_FALSE(response.empty());
    EXPECT_NE(response.find("NOT_FOUND"), std::string::npos);
}

TEST_F(RestAPITest, InvalidJSON) {
    std::string body = "not json at all";
    std::string response = http_post("127.0.0.1", TEST_PORT, "/collections", body);
    EXPECT_FALSE(response.empty());
    EXPECT_NE(response.find("PARSE_ERROR"), std::string::npos);
}

/* ========================================================================
 * 服务器状态测试
 * ======================================================================== */

TEST_F(RestAPITest, ServerUptime) {
    int64_t uptime = rest_server_get_uptime(server_);
    EXPECT_GT(uptime, 0);
}

TEST_F(RestAPITest, ServerIsRunning) {
    EXPECT_TRUE(rest_server_is_running(server_));
}
