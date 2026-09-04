/**
 * @file server.cpp
 * @brief Modular RAG REST API 服务器实现
 */

#include "rag/modular/api/server.h"
#include "rag/logger.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>
#include <atomic>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
#endif

namespace rag::modular {

// ========== 常量定义 ==========

static const int BUFFER_SIZE = 8192;
static const int BACKLOG = 10;

// ========== JSON 工具 ==========

std::string ApiServer::json_escape(const std::string& str) {
    std::string result;
    result.reserve(str.size() + 16);
    for (char c : str) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    result += buf;
                } else {
                    result += c;
                }
        }
    }
    return result;
}

// ========== ApiServer 实现 ==========

struct ApiServer::Impl {
    Impl() : server_socket(-1) {}

    ~Impl() {
        // 仅清理 socket，不调用外部 stop()
        if (server_socket >= 0) {
#ifdef _WIN32
            closesocket(server_socket);
            WSACleanup();
#else
            close(server_socket);
#endif
            server_socket = -1;
        }
    }

    bool start_server(int port) {
#ifdef _WIN32
        WSADATA wsa_data;
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
            RAG_ERROR("WSAStartup 失败");
            return false;
        }
#endif

        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket < 0) {
            RAG_ERROR("创建 socket 失败");
            return false;
        }

        // 设置 SO_REUSEADDR
        int opt = 1;
#ifdef _WIN32
        setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
        setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(static_cast<uint16_t>(port));

        if (bind(server_socket, reinterpret_cast<struct sockaddr*>(&addr),
                 sizeof(addr)) < 0) {
            RAG_ERROR("绑定端口 " + std::to_string(port) + " 失败");
#ifdef _WIN32
            closesocket(server_socket);
            WSACleanup();
#else
            close(server_socket);
#endif
            return false;
        }

        if (listen(server_socket, BACKLOG) < 0) {
            RAG_ERROR("监听失败");
#ifdef _WIN32
            closesocket(server_socket);
            WSACleanup();
#else
            close(server_socket);
#endif
            return false;
        }

        RAG_INFO("API 服务器启动于端口 " + std::to_string(port));
        return true;
    }

    void stop_server() {
        if (server_socket >= 0) {
#ifdef _WIN32
            closesocket(server_socket);
            WSACleanup();
#else
            close(server_socket);
#endif
            server_socket = -1;
        }
    }

    bool accept_client(int& client_socket, std::string& client_ip) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        std::memset(&client_addr, 0, sizeof(client_addr));

        client_socket = accept(server_socket,
                               reinterpret_cast<struct sockaddr*>(&client_addr),
                               &client_len);

        if (client_socket < 0) {
            return false;
        }

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
        client_ip = ip_str;

        return true;
    }

    std::string read_request(int client_socket) {
        std::string request;
        char buffer[BUFFER_SIZE];

        // 设置超时
#ifdef _WIN32
        DWORD timeout = 5000;  // 5 秒
        setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif

        ssize_t bytes_read;
        while ((bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
            buffer[bytes_read] = '\0';
            request += buffer;

            // 检查是否读完（简单的启发式判断）
            if (bytes_read < BUFFER_SIZE - 1) {
                break;
            }
        }

        return request;
    }

    bool send_response(int client_socket, const std::string& response) {
        ssize_t bytes_sent = send(client_socket, response.c_str(),
                                  static_cast<ssize_t>(response.size()), 0);
        return bytes_sent == static_cast<ssize_t>(response.size());
    }

    void close_client(int client_socket) {
#ifdef _WIN32
        closesocket(client_socket);
#else
        close(client_socket);
#endif
    }

    int server_socket;
};

// ========== ApiServer 构造函数/析构函数 ==========

ApiServer::ApiServer()
    : port_(8080), running_(false), initialized_(false), start_time_ms_(0),
      active_requests_(0), impl_(std::make_unique<Impl>()) {
}

ApiServer::~ApiServer() {
    stop();
}

bool ApiServer::init(const ServerConfig& config) {
    config_ = config;

    // 初始化 Modular 配置（使用默认配置）
    // 实际使用时可以通过配置传入更完整的配置
    modular_config_.default_pipeline = PipelineType::ADVANCED;
    modular_config_.llm.model_path = "";
    modular_config_.llm.model_type = "qwen2.5-7b";
    modular_config_.embedding.model_path = "";
    modular_config_.data_dir = "./rag_data";
    modular_config_.index_dir = "./rag_data/index";

    // 创建所有 Pipeline
    pipelines_ = PipelineFactory::create_all(modular_config_);

    // 设置默认 Pipeline
    default_pipeline_ = PipelineFactory::create(
        modular_config_.default_pipeline, modular_config_);

    initialized_ = true;
    RAG_INFO("Modular API 服务器初始化完成");
    return true;
}

bool ApiServer::start(int port) {
    if (!initialized_) {
        RAG_ERROR("服务器未初始化");
        return false;
    }

    if (running_) {
        RAG_WARN("服务器已在运行");
        return true;
    }

    port_ = port;

    if (!impl_->start_server(port_)) {
        return false;
    }

    running_ = true;
    start_time_ms_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    // 在后台线程运行主循环
    std::thread([this]() {
        while (running_) {
            int client_socket;
            std::string client_ip;

            if (impl_->accept_client(client_socket, client_ip)) {
                // 处理请求
                std::string request = impl_->read_request(client_socket);

                if (!request.empty()) {
                    // 解析 HTTP 请求
                    std::istringstream iss(request);
                    std::string method, path, version;
                    iss >> method >> path >> version;

                    std::string response;
                    std::string body;

                    // 根据路径和方法处理请求
                    if (method == "POST" && path == "/api/v1/query") {
                        // 找到请求体（空行之后）
                        auto body_pos = request.find("\r\n\r\n");
                        if (body_pos != std::string::npos) {
                            body = request.substr(body_pos + 4);
                        }
                        body = handle_query(body);
                        response = create_json_response(body);
                    }
                    else if (method == "GET" && path == "/api/v1/status") {
                        body = handle_status();
                        response = create_json_response(body);
                    }
                    else if (method == "GET" && path == "/api/v1/pipelines") {
                        body = handle_pipelines();
                        response = create_json_response(body);
                    }
                    else if (method == "GET" && path == "/api/v1/health") {
                        body = handle_health();
                        response = create_json_response(body);
                    }
                    else {
                        // 404 Not Found
                        body = "{\"error\":\"Not Found\"}";
                        response = create_json_response(body, 404);
                    }

                    impl_->send_response(client_socket, response);
                }

                impl_->close_client(client_socket);
            }
        }
    }).detach();

    return true;
}

void ApiServer::stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    impl_->stop_server();

    RAG_INFO("Modular API 服务器已停止");
}

std::string ApiServer::handle_query(const std::string& body) {
    active_requests_++;

    ApiQueryRequest request;
    try {
        request = parse_query_request(body);
    } catch (const std::exception& e) {
        active_requests_--;
        return "{\"success\":false,\"error\":\"" +
               json_escape(std::string(e.what())) + "\"}";
    }

    ApiQueryResponse response = execute_query(request);

    active_requests_--;

    // 构建响应 JSON
    std::ostringstream oss;
    oss << "{";
    oss << "\"success\":" << (response.success ? "true" : "false") << ",";
    oss << "\"answer\":\"" << json_escape(response.answer) << "\",";
    oss << "\"chunks\":[";
    for (size_t i = 0; i < response.chunks.size(); i++) {
        if (i > 0) oss << ",";
        oss << "{";
        oss << "\"id\":\"" << json_escape(response.chunks[i].id) << "\",";
        oss << "\"content\":\"" << json_escape(response.chunks[i].content) << "\",";
        oss << "\"file_path\":\"" << json_escape(response.chunks[i].file_path) << "\",";
        oss << "\"score\":" << response.chunks[i].score;
        oss << "}";
    }
    oss << "],";
    oss << "\"retrieval_time_ms\":" << response.retrieval_time_ms << ",";
    oss << "\"generation_time_ms\":" << response.generation_time_ms << ",";
    oss << "\"total_time_ms\":" << response.total_time_ms << ",";
    oss << "\"total_tokens\":" << response.total_tokens;
    if (!response.error_message.empty()) {
        oss << ",\"error\":\"" << json_escape(response.error_message) << "\"";
    }
    oss << "}";

    return oss.str();
}

std::string ApiServer::handle_status() {
    int64_t current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    std::ostringstream oss;
    oss << "{";
    oss << "\"is_running\":" << (running_ ? "true" : "false") << ",";
    oss << "\"active_requests\":" << active_requests_ << ",";
    oss << "\"uptime_ms\":" << (running_ ? (current_time - start_time_ms_) : 0) << ",";
    oss << "\"version\":\"1.0.0\"";
    oss << "}";

    return oss.str();
}

std::string ApiServer::handle_pipelines() {
    std::ostringstream oss;
    oss << "[";

    bool first = true;
    for (const auto& kv : pipelines_) {
        if (!first) oss << ",";
        first = false;

        auto& pipeline = kv.second;
        oss << "{";
        oss << "\"name\":\"" << json_escape(pipeline->name()) << "\",";
        oss << "\"type\":\"" << json_escape(
            pipeline_type_to_string(pipeline->type())) << "\",";
        oss << "\"description\":\""
            << json_escape(PipelineFactory::get_description(pipeline->type()))
            << "\",";
        oss << "\"is_ready\":" << (pipeline->is_ready() ? "true" : "false");
        oss << "}";
    }

    oss << "]";
    return oss.str();
}

std::string ApiServer::handle_health() {
    std::ostringstream oss;
    oss << "{";
    oss << "\"healthy\":" << (running_ && initialized_ ? "true" : "false") << ",";
    oss << "\"status_message\":\"" << (running_ ? "running" : "stopped") << "\",";
    oss << "\"timestamp\":"
        << std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    oss << "}";

    return oss.str();
}

ApiQueryRequest ApiServer::parse_query_request(const std::string& body) {
    ApiQueryRequest request;

    // 简单的 JSON 解析（避免引入外部依赖）
    // 格式: {"query":"...", "pipeline":"...", "top_k":5}

    // 提取 query
    auto query_pos = body.find("\"query\"");
    if (query_pos != std::string::npos) {
        auto colon_pos = body.find(':', query_pos);
        auto start_quote = body.find('"', colon_pos);
        auto end_quote = body.find('"', start_quote + 1);
        if (start_quote != std::string::npos && end_quote != std::string::npos) {
            request.query = body.substr(start_quote + 1, end_quote - start_quote - 1);
        }
    }

    // 提取 pipeline
    auto pipeline_pos = body.find("\"pipeline\"");
    if (pipeline_pos != std::string::npos) {
        auto colon_pos = body.find(':', pipeline_pos);
        auto start_quote = body.find('"', colon_pos);
        auto end_quote = body.find('"', start_quote + 1);
        if (start_quote != std::string::npos && end_quote != std::string::npos) {
            request.pipeline = body.substr(start_quote + 1, end_quote - start_quote - 1);
        }
    }

    // 提取 top_k
    auto topk_pos = body.find("\"top_k\"");
    if (topk_pos != std::string::npos) {
        auto colon_pos = body.find(':', topk_pos);
        request.top_k = std::stoi(body.substr(colon_pos + 1));
    }

    return request;
}

ApiQueryResponse ApiServer::execute_query(const ApiQueryRequest& request) {
    ApiQueryResponse response;

    auto start_time = std::chrono::steady_clock::now();

    try {
        // 选择 Pipeline
        ModularPipeline* pipeline = default_pipeline_.get();

        if (!request.pipeline.empty()) {
            auto type = string_to_pipeline_type(request.pipeline);
            auto it = pipelines_.find(type);
            if (it != pipelines_.end()) {
                pipeline = it->second.get();
            }
        }

        if (!pipeline) {
            response.error_message = "未找到指定的 Pipeline";
            return response;
        }

        // 构建查询
        ModularQuery query;
        query.text = request.query;
        query.top_k = request.top_k;
        for (const auto& opt : request.options) {
            query.options[opt.first] = opt.second;
        }

        // 执行查询
        auto result = pipeline->query(query);

        response.success = result.success;
        response.answer = result.answer;
        response.total_tokens = result.total_tokens;
        response.retrieval_time_ms = result.retrieval_time_ms;
        response.generation_time_ms = result.generation_time_ms;
        response.total_time_ms = result.total_time_ms;
        response.error_message = result.error_message;

        // 转换上下文 - RetrievalResult 包含 Chunk 结构
        for (const auto& ctx : result.context) {
            ApiChunk chunk;
            chunk.id = ctx.chunk.id;
            chunk.content = ctx.chunk.content;
            chunk.file_path = ctx.chunk.metadata.file_path;
            chunk.score = ctx.score;
            response.chunks.push_back(chunk);
        }

    } catch (const std::exception& e) {
        response.success = false;
        response.error_message = e.what();
    }

    return response;
}

std::string ApiServer::create_json_response(const std::string& data, int status) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status << " "
        << (status == 200 ? "OK" : status == 404 ? "Not Found" : "Bad Request")
        << "\r\n";
    oss << "Content-Type: application/json\r\n";
    oss << "Content-Length: " << data.size() << "\r\n";
    oss << "Access-Control-Allow-Origin: *\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << data;

    return oss.str();
}

std::string ApiServer::create_error_response(const std::string& error, int status) {
    std::string body = "{\"error\":\"" + json_escape(error) + "\"}";
    return create_json_response(body, status);
}

// ========== 工厂函数 ==========

std::unique_ptr<ApiServer> create_api_server() {
    return std::make_unique<ApiServer>();
}

}  // namespace rag::modular
