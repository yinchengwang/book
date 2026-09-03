/**
 * @file server.cpp
 * @brief REST API 服务器实现
 *
 * 简化版 HTTP 服务器实现
 * 生产环境建议使用 httplib 或其他成熟的 HTTP 库
 */

#include "rag/llm_service.h"
#include "rag/server.h"
#include "rag/logger.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <atomic>
#include <regex>
#include <cstdio>

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

// ========== JSON 工具（最小实现，仅满足本服务器需求） ==========

// JSON 字符串转义
static std::string json_escape(const std::string& str) {
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

// 从 JSON body 中提取字符串字段（处理转义字符）
// 找到 "key" 后提取其字符串值；未找到返回 default_value
static std::string extract_json_string(const std::string& body, const std::string& key,
                                       const std::string& default_value = "") {
    std::string pattern = "\"" + key + "\"";
    auto pos = body.find(pattern);
    if (pos == std::string::npos) return default_value;

    // 跳过 key、冒号和空白
    pos = body.find(':', pos + pattern.size());
    if (pos == std::string::npos) return default_value;
    ++pos;
    while (pos < body.size() && (body[pos] == ' ' || body[pos] == '\t')) ++pos;

    // 值必须是字符串
    if (pos >= body.size() || body[pos] != '"') return default_value;
    ++pos;

    // 提取到未转义的结束引号
    std::string result;
    while (pos < body.size()) {
        char c = body[pos];
        if (c == '\\' && pos + 1 < body.size()) {
            char next = body[pos + 1];
            switch (next) {
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case 'r': result += '\r'; break;
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case '/': result += '/'; break;
                default: result += next; break;
            }
            pos += 2;
        } else if (c == '"') {
            return result;  // 结束引号
        } else {
            result += c;
            ++pos;
        }
    }
    return result;  // 未闭合，返回已提取部分
}

// 从 JSON body 中提取整数字段
static int extract_json_int(const std::string& body, const std::string& key, int default_value) {
    std::string pattern = "\"" + key + "\"";
    auto pos = body.find(pattern);
    if (pos == std::string::npos) return default_value;

    pos = body.find(':', pos + pattern.size());
    if (pos == std::string::npos) return default_value;
    ++pos;
    while (pos < body.size() && (body[pos] == ' ' || body[pos] == '\t')) ++pos;

    try {
        return std::stoi(body.substr(pos));
    } catch (...) {
        return default_value;
    }
}

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

        // 路由处理（处理器返回完整 HTTP 响应）
        std::string response;

        if (route == "/api/v1/query" && method == "POST") {
            response = handle_query(body);
        } else if (route == "/api/v1/retrieve" && method == "POST") {
            response = handle_retrieve(body);
        } else if (route == "/api/v1/documents" && method == "GET") {
            response = handle_documents();
        } else if (route.rfind("/api/v1/documents/", 0) == 0 && method == "GET") {
            // /api/v1/documents/{id}/content
            std::string rest = route.substr(std::string("/api/v1/documents/").size());
            if (rest.size() > 8 && rest.compare(rest.size() - 8, 8, "/content") == 0) {
                std::string doc_id = url_decode(rest.substr(0, rest.size() - 8));
                response = handle_document_content(doc_id);
            } else {
                response = handle_document(url_decode(rest));
            }
        } else if (route == "/api/v1/index/status" && method == "GET") {
            response = handle_index_status();
        } else if (route == "/health" && method == "GET") {
            response = handle_health();
        } else if (route == "/metrics" && method == "GET") {
            response = handle_metrics();
        } else if (method == "OPTIONS") {
            // CORS 预检
            response = "HTTP/1.1 204 No Content\r\n"
                       "Access-Control-Allow-Origin: *\r\n"
                       "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                       "Access-Control-Allow-Headers: Content-Type\r\n"
                       "Content-Length: 0\r\n\r\n";
        } else if (method == "GET") {
            // 静态文件服务（Task 3 实现 serve_static；未命中时返回 404）
            response = serve_static(route);
        } else {
            response = create_error_response("Not Found", 404);
        }

        // 发送响应
        send(client_socket, response.c_str(), response.size(), 0);

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

    std::string query = extract_json_string(body, "query");
    int top_k = extract_json_int(body, "top_k", 5);

    if (query.empty()) {
        return create_error_response("Missing query parameter", 400);
    }

    auto result = engine_->query(query, top_k);

    std::ostringstream oss;
    oss << "{";
    oss << "\"answer\": \"" << json_escape(result.answer) << "\",";
    oss << "\"confidence\": " << result.confidence << ",";
    oss << "\"query_time_ms\": " << result.query_time_ms << ",";
    oss << "\"request_id\": \"" << json_escape(result.request_id) << "\",";
    oss << "\"chunks\": [";

    for (size_t i = 0; i < result.chunks.size(); ++i) {
        if (i > 0) oss << ",";
        const auto& chunk = result.chunks[i];
        oss << "{";
        oss << "\"id\": \"" << json_escape(chunk.chunk.id) << "\",";
        oss << "\"document_id\": \"" << json_escape(chunk.chunk.document_id) << "\",";
        oss << "\"content\": \"" << json_escape(chunk.chunk.content) << "\",";
        oss << "\"file_path\": \"" << json_escape(chunk.chunk.metadata.file_path) << "\",";
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

    std::string query = extract_json_string(body, "query");
    int top_k = extract_json_int(body, "top_k", 5);

    if (query.empty()) {
        return create_error_response("Missing query parameter", 400);
    }

    auto results = engine_->retrieve(query, top_k);

    std::ostringstream oss;
    oss << "{\"results\": [";

    for (size_t i = 0; i < results.size(); ++i) {
        if (i > 0) oss << ",";
        const auto& result = results[i];
        oss << "{";
        oss << "\"id\": \"" << json_escape(result.chunk.id) << "\",";
        oss << "\"document_id\": \"" << json_escape(result.chunk.document_id) << "\",";
        oss << "\"content\": \"" << json_escape(result.chunk.content) << "\",";
        oss << "\"file_path\": \"" << json_escape(result.chunk.metadata.file_path) << "\",";
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
        oss << "\"id\": \"" << json_escape(doc.id) << "\",";
        oss << "\"file_name\": \"" << json_escape(doc.metadata.file_name) << "\",";
        oss << "\"file_path\": \"" << json_escape(doc.metadata.file_path) << "\",";
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

std::string Server::handle_document(const std::string& id) {
    if (!engine_) {
        return create_error_response("Engine not initialized", 500);
    }
    auto docs = engine_->list_documents();
    for (const auto& doc : docs) {
        if (doc.id == id) {
            std::ostringstream oss;
            oss << "{";
            oss << "\"id\": \"" << json_escape(doc.id) << "\",";
            oss << "\"file_name\": \"" << json_escape(doc.metadata.file_name) << "\",";
            oss << "\"file_path\": \"" << json_escape(doc.metadata.file_path) << "\",";
            oss << "\"status\": " << static_cast<int>(doc.status);
            oss << "}";
            return create_json_response(oss.str());
        }
    }
    return create_error_response("Document not found", 404);
}

std::string Server::handle_document_content(const std::string& id) {
    if (!engine_) {
        return create_error_response("Engine not initialized", 500);
    }

    auto docs = engine_->list_documents();
    for (const auto& doc : docs) {
        // 按文档 ID 匹配；同时允许按 file_path 匹配（前端只有 chunk 的 file_path 时也能用）
        if (doc.id == id || doc.metadata.file_path == id) {
            std::ostringstream oss;
            oss << "{";
            oss << "\"id\": \"" << json_escape(doc.id) << "\",";
            oss << "\"file_path\": \"" << json_escape(doc.metadata.file_path) << "\",";
            oss << "\"title\": \"" << json_escape(doc.metadata.title.empty()
                                                 ? doc.metadata.file_name
                                                 : doc.metadata.title) << "\",";
            oss << "\"content\": \"" << json_escape(doc.content) << "\"";
            oss << "}";
            return create_json_response(oss.str());
        }
    }

    return create_error_response("Document not found", 404);
}

std::string Server::serve_static(const std::string& route) {
    if (config_.static_dir.empty()) {
        return create_error_response("Not Found", 404);
    }

    // 路径穿越防护：拒绝包含 .. 的路径
    std::string clean = route;
    if (clean.empty() || clean == "/") {
        clean = "/index.html";
    }
    if (clean.find("..") != std::string::npos) {
        return create_error_response("Forbidden", 403);
    }

    namespace fs = std::filesystem;
    fs::path file_path = fs::path(config_.static_dir) / clean.substr(1);

    // SPA 回退：文件不存在且非资源文件时返回 index.html
    std::error_code ec;
    if (!fs::exists(file_path, ec) || fs::is_directory(file_path, ec)) {
        if (clean.rfind("/assets/", 0) == 0) {
            return create_error_response("Not Found", 404);
        }
        file_path = fs::path(config_.static_dir) / "index.html";
        if (!fs::exists(file_path, ec)) {
            return create_error_response("Not Found", 404);
        }
    }

    // 读取文件（二进制模式，支持图片/字体）
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        return create_error_response("Not Found", 404);
    }
    std::ostringstream content;
    content << file.rdbuf();
    std::string body = content.str();

    // MIME 类型映射
    std::string ext = file_path.extension().string();
    std::string mime = "application/octet-stream";
    if (ext == ".html") mime = "text/html; charset=utf-8";
    else if (ext == ".js")   mime = "application/javascript";
    else if (ext == ".css")  mime = "text/css";
    else if (ext == ".json") mime = "application/json";
    else if (ext == ".svg")  mime = "image/svg+xml";
    else if (ext == ".png")  mime = "image/png";
    else if (ext == ".jpg" || ext == ".jpeg") mime = "image/jpeg";
    else if (ext == ".ico")  mime = "image/x-icon";
    else if (ext == ".woff" || ext == ".woff2") mime = "font/woff2";

    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n";
    oss << "Content-Type: " << mime << "\r\n";
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "Cache-Control: no-cache\r\n";
    oss << "\r\n";
    oss << body;
    return oss.str();
}

// ========== 工厂函数 ==========

std::unique_ptr<Server> create_server() {
    return std::make_unique<Server>();
}

}  // namespace rag
