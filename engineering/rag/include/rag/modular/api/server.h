/**
 * @file server.h
 * @brief Modular RAG REST API 服务器
 *
 * 提供 Modular RAG 的 RESTful API 接口
 */
#pragma once

#include "rag/modular/types.h"
#include "rag/modular/config.h"
#include "rag/modular/pipeline/pipeline_base.h"
#include "rag/modular/pipeline/pipeline_factory.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>

namespace rag::modular {

// ========== API 请求/响应结构 ==========

/**
 * @brief API 查询请求
 */
struct ApiQueryRequest {
    std::string query;                          // 查询文本
    std::string pipeline;                       // Pipeline 类型名称
    int top_k = 5;                              // 返回结果数
    std::unordered_map<std::string, std::string> options;  // 额外选项
};

/**
 * @brief API 块引用
 */
struct ApiChunk {
    std::string id;                             // 块 ID
    std::string content;                        // 块内容
    std::string file_path;                      // 文件路径
    float score = 0.0f;                         // 相关性分数
};

/**
 * @brief API 查询响应
 */
struct ApiQueryResponse {
    bool success = false;                       // 是否成功
    std::string answer;                         // 生成的回答
    std::vector<ApiChunk> chunks;               // 检索到的上下文
    int64_t retrieval_time_ms = 0;              // 检索耗时
    int64_t generation_time_ms = 0;             // 生成耗时
    int64_t total_time_ms = 0;                  // 总耗时
    int total_tokens = 0;                       // token 数
    std::string error_message;                  // 错误信息
};

/**
 * @brief API Pipeline 信息
 */
struct ApiPipelineInfo {
    std::string name;                           // Pipeline 名称
    std::string type;                           // Pipeline 类型
    std::string description;                    // Pipeline 描述
    bool is_ready = false;                      // 是否就绪
};

/**
 * @brief API 系统状态
 */
struct ApiStatus {
    bool is_running = false;                    // 服务是否运行
    int active_requests = 0;                    // 当前活跃请求数
    int64_t uptime_ms = 0;                      // 运行时间
    std::string version = "1.0.0";              // 版本
    std::unordered_map<std::string, int> pipeline_stats;  // Pipeline 统计
};

/**
 * @brief API 健康状态
 */
struct ApiHealth {
    bool healthy = false;                       // 是否健康
    std::string status_message;                 // 状态消息
    int64_t timestamp = 0;                      // 时间戳
};

// ========== REST API 服务器 ==========

/**
 * @brief Modular RAG REST API 服务器
 *
 * 提供 HTTP REST 接口用于:
 * - RAG 查询
 * - 系统状态
 * - Pipeline 管理
 * - 健康检查
 */
class ApiServer {
public:
    ApiServer();
    ~ApiServer();

    /**
     * @brief 初始化服务器
     * @param config 服务器配置
     * @return 初始化是否成功
     */
    bool init(const ServerConfig& config);

    /**
     * @brief 启动服务器
     * @param port 端口号
     * @return 启动是否成功
     */
    bool start(int port = 8080);

    /**
     * @brief 停止服务器
     */
    void stop();

    /**
     * @brief 服务器是否运行中
     */
    bool is_running() const { return running_; }

    /**
     * @brief 获取端口号
     */
    int port() const { return port_; }

    // ========== 路由处理 ==========

    /**
     * @brief 处理查询请求
     * @param body 请求体 (JSON)
     * @return 响应体 (JSON)
     */
    std::string handle_query(const std::string& body);

    /**
     * @brief 处理状态请求
     * @return 响应体 (JSON)
     */
    std::string handle_status();

    /**
     * @brief 处理 Pipeline 列表请求
     * @return 响应体 (JSON)
     */
    std::string handle_pipelines();

    /**
     * @brief 处理健康检查请求
     * @return 响应体 (JSON)
     */
    std::string handle_health();

private:
    // ========== 内部方法 ==========

    /**
     * @brief 解析查询请求
     * @param body 请求体
     * @return 解析后的请求结构
     */
    ApiQueryRequest parse_query_request(const std::string& body);

    /**
     * @brief 创建 JSON 响应
     * @param data 响应数据
     * @param status HTTP 状态码
     * @return JSON 字符串
     */
    std::string create_json_response(const std::string& data, int status = 200);

    /**
     * @brief 创建错误响应
     * @param error 错误信息
     * @param status HTTP 状态码
     * @return JSON 字符串
     */
    std::string create_error_response(const std::string& error, int status = 400);

    /**
     * @brief JSON 转义
     * @param str 输入字符串
     * @return 转义后的字符串
     */
    std::string json_escape(const std::string& str);

    /**
     * @brief 执行 RAG 查询
     * @param request 查询请求
     * @return 查询响应
     */
    ApiQueryResponse execute_query(const ApiQueryRequest& request);

    // ========== 成员变量 ==========

    int port_ = 8080;                           // 端口号
    bool running_ = false;                      // 运行状态
    bool initialized_ = false;                  // 初始化状态
    int64_t start_time_ms_ = 0;                 // 启动时间
    int active_requests_ = 0;                   // 活跃请求数

    ServerConfig config_;                       // 服务器配置
    ModularConfig modular_config_;              // Modular RAG 配置

    // Pipeline 管理
    std::unordered_map<PipelineType, std::unique_ptr<ModularPipeline>> pipelines_;
    std::unique_ptr<ModularPipeline> default_pipeline_;

    // 内部实现
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ========== 工厂函数 ==========

/**
 * @brief 创建 API 服务器
 * @return API 服务器实例
 */
std::unique_ptr<ApiServer> create_api_server();

}  // namespace rag::modular
