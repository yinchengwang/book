/**
 * @file server.h
 * @brief REST API 服务器
 */
#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <atomic>

// Forward declarations
// Note: We include engine.h to get full RAGEngine definition for shared_ptr usage
#include "mmrag/engine.h"

namespace mmrag {

// ========== 前向声明 ==========

class MetricsCollector;
class HealthChecker;
struct ServerConfig;

namespace api {

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
    std::string handle_documents(const std::string& query);
    std::string handle_document(const std::string& id, const std::string& method);
    std::string handle_document_content(const std::string& id);   // 新增：文档内容
    std::string handle_document_upload(const std::string& body,
                                      const std::string& content_type);
    std::string handle_upload_progress(const std::string& task_id);   // 新增：上传进度查询
    std::string handle_dirs(const std::string& body,
                           const std::string& method,
                           const std::string& query);
    std::string serve_static(const std::string& route);            // 新增：静态文件
    std::string handle_index_status();
    std::string handle_rebuild(const std::string& body);
    std::string handle_migration(const std::string& route,
                                const std::string& method,
                                const std::string& body);
    std::string handle_eval_datasets_list();
    std::string handle_eval_datasets_create(const std::string& body);
    std::string handle_eval_runs_list(const std::string& query);
    std::string handle_eval_runs_create(const std::string& body);
    std::string handle_eval_compare(const std::string& query);
    std::string handle_eval_trends(const std::string& query);
    std::string handle_eval_optimization_suggestions(const std::string& query);
    std::string handle_query_stream(const std::string& body);
    std::string handle_health();
    std::string handle_metrics();
    std::string handle_root();
    std::string handle_system_status();
    std::string handle_pipelines_list();
    std::string handle_pipeline_config(const std::string& type);
    std::string handle_pipeline_update(const std::string& type, const std::string& body);

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

    // 异步上传任务处理（lambda 内部调用）
    void process_upload_task_impl(std::shared_ptr<void> task_ptr);
};

// ========== 工厂函数 ==========

/**
 * @brief 创建服务器
 */
std::unique_ptr<Server> create_server();

}  // namespace api
}  // namespace mmrag
