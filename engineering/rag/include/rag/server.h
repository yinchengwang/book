/**
 * @file server.h
 * @brief REST API 服务器
 */
#pragma once

#include "rag/engine.h"
#include "rag/metrics.h"
#include <string>
#include <memory>
#include <functional>
#include <vector>

namespace rag {

// ========== 前向声明 ==========

class Server;

// ========== 块引用（前置定义） ==========

/**
 * @brief 块引用
 */
struct ChunkReference {
    std::string id;
    std::string content;
    std::string file_path;
    float score = 0.0f;
};

// ========== 请求/响应 ==========

/**
 * @brief 查询请求
 */
struct QueryRequest {
    std::string query;
    int top_k = 5;
    bool stream = false;
};

/**
 * @brief 查询响应
 */
struct QueryResponse {
    std::string answer;
    std::vector<ChunkReference> chunks;
    float confidence = 0.0f;
    int64_t query_time_ms = 0;
    std::string request_id;
};

// ========== 服务器配置 ==========
// 注意：ServerConfig 定义在 config.h 中，这里复用同一类型

// ========== REST API 服务器 ==========

/**
 * @brief REST API 服务器
 */
class Server {
public:
    Server();
    ~Server();

    // 设置引擎
    void set_engine(std::shared_ptr<RAGEngine> engine);

    // 设置指标收集器
    void set_metrics(std::shared_ptr<MetricsCollector> metrics);

    // 设置健康检查器
    void set_health_checker(std::shared_ptr<HealthChecker> health_checker);

    // 启动服务器
    bool start(const ServerConfig& config);

    // 停止服务器
    void stop();

    // 是否运行中
    bool is_running() const { return running_; }

    // 获取配置
    const ServerConfig& config() const { return config_; }

private:
    // 注册路由
    void register_routes();

    // 中间件
    std::string add_cors_headers(const std::string& response);
    std::string create_json_response(const std::string& body, int status = 200);
    std::string create_error_response(const std::string& error, int status = 400);

    // 处理器
    std::string handle_query(const std::string& body);
    std::string handle_retrieve(const std::string& body);
    std::string handle_documents();
    std::string handle_document(const std::string& id);
    std::string handle_index_status();
    std::string handle_health();
    std::string handle_metrics();
    std::string handle_root();

    // 连接处理
    void handle_connection(int client_socket);

    ServerConfig config_;
    bool running_ = false;

    // 组件
    std::shared_ptr<RAGEngine> engine_;
    std::shared_ptr<MetricsCollector> metrics_;
    std::shared_ptr<HealthChecker> health_checker_;

    // 内部实现
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ========== 工厂函数 ==========

/**
 * @brief 创建服务器
 */
std::unique_ptr<Server> create_server();

}  // namespace rag
