/**
 * @file server.cpp
 * @brief REST API 服务器实现
 *
 * 简化版 HTTP 服务器实现
 * 生产环境建议使用 httplib 或其他成熟的 HTTP 库
 */

#include "rag/server.h"
#include "rag/logger.h"
#include "rag/llm_service.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <atomic>
#include <regex>

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

namespace rag {

// ========== HTTP 工具 ==========

static std::string url_decode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%' && i + 2 < str.size()) {
            int value;
            std::istringstream iss(str.substr(i + 1, 2));
            iss >> std::hex >> value;
            result += static_cast<char>(value);
            i += 2;
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

static std::map<std::string, std::string> parse_query_params(const std::string& query) {
    std::map<std::string, std::string> params;
    std::istringstream iss(query);
    std::string pair;

    while (std::getline(iss, pair, '&')) {
        auto pos = pair.find('=');
        if (pos != std::string::npos) {
            std::string key = url_decode(pair.substr(0, pos));
            std::string value = url_decode(pair.substr(pos + 1));
            params[key] = value;
        }
    }

    return params;
}

static std::string read_body(std::istream& input, size_t content_length) {
    std::string body;
    body.resize(content_length);
    input.read(&body[0], content_length);
    return body;
}

// ========== Server 实现 ==========

struct Server::Impl {
    std::atomic<bool> running{false};
    int server_socket = -1;
    std::thread accept_thread;
};

Server::Server() : impl_(std::make_unique<Impl>()) {}

Server::~Server() {
    stop();
}

void Server::set_engine(std::shared_ptr<RAGEngine> engine) {
    engine_ = std::move(engine);
}

void Server::set_metrics(std::shared_ptr<MetricsCollector> metrics) {
    metrics_ = std::move(metrics);
}

void Server::set_health_checker(std::shared_ptr<HealthChecker> health_checker) {
    health_checker_ = std::move(health_checker);
}

bool Server::start(const ServerConfig& config) {
    if (impl_->running.load()) {
        RAG_WARN("Server is already running");
        return false;
    }

    config_ = config;

#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        RAG_ERROR("WSAStartup failed");
        return false;
    }
#endif

    impl_->server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (impl_->server_socket < 0) {
        RAG_ERROR("Failed to create socket");
        return false;
    }

    int opt = 1;
#ifdef _WIN32
    setsockopt(impl_->server_socket, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
    setsockopt(impl_->server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(config.port));

    if (bind(impl_->server_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        RAG_ERROR("Failed to bind to port " + std::to_string(config.port));
#ifdef _WIN32
        closesocket(impl_->server_socket);
#else
        close(impl_->server_socket);
#endif
        return false;
    }

    if (listen(impl_->server_socket, 10) < 0) {
        RAG_ERROR("Failed to listen");
#ifdef _WIN32
        closesocket(impl_->server_socket);
#else
        close(impl_->server_socket);
#endif
        return false;
    }

    impl_->running.store(true);

    // 启动接受线程
    impl_->accept_thread = std::thread([this]() {
        RAG_INFO("Server listening on " + config_.host + ":" + std::to_string(config_.port));

        while (impl_->running.load()) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_socket = accept(impl_->server_socket,
                                       (struct sockaddr*)&client_addr, &client_len);

            if (client_socket < 0) {
                if (impl_->running.load()) {
                    RAG_ERROR("Failed to accept connection");
                }
                continue;
            }

            // 处理请求
            std::thread([this, client_socket]() {
                this->handle_connection(client_socket);
            }).detach();
        }
    });

    RAG_INFO("Server started successfully");
    return true;
}

void Server::stop() {
    if (!impl_->running.load()) {
        return;
    }

    impl_->running.store(false);

#ifdef _WIN32
    if (impl_->server_socket >= 0) {
        closesocket(impl_->server_socket);
    }
    WSACleanup();
#else
    if (impl_->server_socket >= 0) {
        close(impl_->server_socket);
    }
#endif

    if (impl_->accept_thread.joinable()) {
        impl_->accept_thread.join();
    }

    RAG_INFO("Server stopped");
}

void Server::handle_connection(int client_socket) {
    try {
        // 读取请求
        char buffer[8192];
        std::string request;
        int n = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

        if (n <= 0) {
#ifdef _WIN32
            closesocket(client_socket);
#else
            close(client_socket);
#endif
            return;
        }

        buffer[n] = '\0';
        request = buffer;

        // 解析请求行
        std::istringstream iss(request);
        std::string method, path, version;
        iss >> method >> path >> version;

        // 提取查询参数
        std::string route, query;
        auto query_pos = path.find('?');
        if (query_pos != std::string::npos) {
            route = path.substr(0, query_pos);
            query = path.substr(query_pos + 1);
        } else {
            route = path;
        }

        // 读取请求体
        std::string body;
        size_t content_length = 0;
        auto header_end = request.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            auto headers = request.substr(0, header_end);
            auto body_start = request.substr(header_end + 4);

            // 查找 Content-Length
            std::regex length_regex("Content-Length:\\s*(\\d+)");
            std::smatch match;
            if (std::regex_search(headers, match, length_regex)) {
                content_length = std::stoul(match[1].str());
            }

            body = body_start;
            // 如果需要更多数据
            while (body.size() < content_length) {
                n = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
                if (n <= 0) break;
                buffer[n] = '\0';
                body += buffer;
            }
        }

        // 路由处理
        std::string response;
        int status = 200;

        if (route == "/api/v1/query" && method == "POST") {
            response = handle_query(body);
        } else if (route == "/api/v1/retrieve" && method == "POST") {
            response = handle_retrieve(body);
        } else if (route == "/api/v1/documents" && method == "GET") {
            response = handle_documents();
        } else if (route == "/api/v1/index/status" && method == "GET") {
            response = handle_index_status();
        } else if (route == "/health" && method == "GET") {
            response = handle_health();
        } else if (route == "/metrics" && method == "GET") {
            response = handle_metrics();
        } else if (route == "/" || route.empty()) {
            response = handle_root();
        } else {
            response = create_error_response("Not Found", 404);
            status = 404;
        }

        // 添加 CORS 头
        if (config_.cors_enabled) {
            response = add_cors_headers(response);
        }

        // 发送响应
        std::string http_response = "HTTP/1.1 " + std::to_string(status) + " OK\r\n";
        http_response += "Content-Type: application/json\r\n";
        http_response += "Content-Length: " + std::to_string(response.size()) + "\r\n";
        http_response += "Connection: close\r\n";
        http_response += "\r\n";
        http_response += response;

        send(client_socket, http_response.c_str(), http_response.size(), 0);

    } catch (const std::exception& e) {
        RAG_ERROR("Request handling error: " + std::string(e.what()));
    }

#ifdef _WIN32
    closesocket(client_socket);
#else
    close(client_socket);
#endif
}

std::string Server::add_cors_headers(const std::string& response) {
    // 响应已经包含 JSON 内容，这里返回原始响应
    return response;
}

std::string Server::create_json_response(const std::string& body, int status) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status << " OK\r\n";
    oss << "Content-Type: application/json\r\n";
    oss << "Content-Length: " << body.size() << "\r\n";
    if (config_.cors_enabled) {
        oss << "Access-Control-Allow-Origin: " << config_.cors_origin << "\r\n";
        oss << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
        oss << "Access-Control-Allow-Headers: Content-Type\r\n";
    }
    oss << "\r\n";
    oss << body;
    return oss.str();
}

std::string Server::create_error_response(const std::string& error, int status) {
    std::ostringstream oss;
    oss << "{\"error\": \"" << error << "\", \"status\": " << status << "}";
    return create_json_response(oss.str(), status);
}

std::string Server::handle_query(const std::string& body) {
    if (!engine_) {
        return create_error_response("Engine not initialized", 500);
    }

    // 简单解析 JSON（实际应使用 JSON 库）
    std::string query = "unknown";
    int top_k = 5;

    auto query_pos = body.find("\"query\"");
    if (query_pos != std::string::npos) {
        auto start = body.find(':', query_pos);
        auto end = body.find('"', start + 1);
        if (start != std::string::npos && end != std::string::npos) {
            query = body.substr(start + 2, end - start - 3);
        }
    }

    auto topk_pos = body.find("\"top_k\"");
    if (topk_pos != std::string::npos) {
        auto start = body.find(':', topk_pos);
        auto end = body.find_first_of(",}", start);
        if (start != std::string::npos && end != std::string::npos) {
            top_k = std::stoi(body.substr(start + 1, end - start - 1));
        }
    }

    auto result = engine_->query(query, top_k);

    std::ostringstream oss;
    oss << "{";
    oss << "\"answer\": \"" << result.answer << "\",";
    oss << "\"confidence\": " << result.confidence << ",";
    oss << "\"query_time_ms\": " << result.query_time_ms << ",";
    oss << "\"request_id\": \"" << result.request_id << "\",";
    oss << "\"chunks\": [";

    for (size_t i = 0; i < result.chunks.size(); ++i) {
        if (i > 0) oss << ",";
        const auto& chunk = result.chunks[i];
        oss << "{";
        oss << "\"id\": \"" << chunk.chunk.id << "\",";
        oss << "\"content\": \"" << chunk.chunk.content << "\",";
        oss << "\"file_path\": \"" << chunk.chunk.metadata.file_path << "\",";
        oss << "\"score\": " << chunk.score;
        oss << "}";
    }

    oss << "]}";
    return create_json_response(oss.str());
}

std::string Server::handle_retrieve(const std::string& body) {
    if (!engine_) {
        return create_error_response("Engine not initialized", 500);
    }

    std::string query;
    auto query_pos = body.find("\"query\"");
    if (query_pos != std::string::npos) {
        auto start = body.find(':', query_pos);
        auto end = body.find('"', start + 1);
        if (start != std::string::npos && end != std::string::npos) {
            query = body.substr(start + 2, end - start - 3);
        }
    }

    auto results = engine_->retrieve(query, 5);

    std::ostringstream oss;
    oss << "{\"results\": [";

    for (size_t i = 0; i < results.size(); ++i) {
        if (i > 0) oss << ",";
        const auto& result = results[i];
        oss << "{";
        oss << "\"id\": \"" << result.chunk.id << "\",";
        oss << "\"content\": \"" << result.chunk.content << "\",";
        oss << "\"score\": " << result.score;
        oss << "}";
    }

    oss << "]}";
    return create_json_response(oss.str());
}

std::string Server::handle_documents() {
    if (!engine_) {
        return create_error_response("Engine not initialized", 500);
    }

    auto docs = engine_->list_documents();

    std::ostringstream oss;
    oss << "{\"documents\": [";

    for (size_t i = 0; i < docs.size(); ++i) {
        if (i > 0) oss << ",";
        const auto& doc = docs[i];
        oss << "{";
        oss << "\"id\": \"" << doc.id << "\",";
        oss << "\"file_name\": \"" << doc.metadata.file_name << "\",";
        oss << "\"file_path\": \"" << doc.metadata.file_path << "\",";
        oss << "\"status\": " << static_cast<int>(doc.status);
        oss << "}";
    }

    oss << "], \"total\": " << docs.size() << "}";
    return create_json_response(oss.str());
}

std::string Server::handle_index_status() {
    if (!engine_) {
        return create_error_response("Engine not initialized", 500);
    }

    auto status = engine_->get_index_status();

    std::ostringstream oss;
    oss << "{";
    oss << "\"index_name\": \"" << status.index_name << "\",";
    oss << "\"document_count\": " << status.document_count << ",";
    oss << "\"chunk_count\": " << status.chunk_count << ",";
    oss << "\"vector_count\": " << status.vector_count << ",";
    oss << "\"status\": " << static_cast<int>(status.status);
    oss << "}";
    return create_json_response(oss.str());
}

std::string Server::handle_health() {
    if (health_checker_) {
        return health_checker_->to_json();
    }

    std::ostringstream oss;
    oss << "{\"status\": \"healthy\", \"checks\": []}";
    return create_json_response(oss.str());
}

std::string Server::handle_metrics() {
    if (metrics_) {
        return metrics_->to_prometheus_format();
    }

    return "# No metrics available\n";
}

std::string Server::handle_root() {
    std::ostringstream oss;
    oss << "{";
    oss << "\"name\": \"RAG API\",";
    oss << "\"version\": \"1.0.0\",";
    oss << "\"endpoints\": [";
    oss << "\"/api/v1/query\",";
    oss << "\"/api/v1/retrieve\",";
    oss << "\"/api/v1/documents\",";
    oss << "\"/api/v1/index/status\",";
    oss << "\"/health\",";
    oss << "\"/metrics\"";
    oss << "]";
    oss << "}";
    return create_json_response(oss.str());
}

// ========== 工厂函数 ==========

std::unique_ptr<Server> create_server() {
    return std::make_unique<Server>();
}

}  // namespace rag
