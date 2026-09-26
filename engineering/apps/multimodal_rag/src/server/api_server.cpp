/**
 * @file server.cpp
 * @brief REST API 服务器实现
 *
 * 简化版 HTTP 服务器实现
 * 生产环境建议使用 httplib 或其他成熟的 HTTP 库
 */

#include "mmrag/llm_service.h"
#include "mmrag/server.h"
#include "mmrag/thread_pool.h"
#include "mmrag/logger.h"
#include "mmrag/metrics.h"
#include "mmrag/eval/eval_store.h"

// Modular Pipeline
#include "mmrag/modular/pipeline/pipeline_factory.h"
#include "mmrag/modular/pipeline/naive_pipeline.h"
#include "mmrag/modular/pipeline/advanced_pipeline.h"
#include "mmrag/modular/pipeline/hybrid_pipeline.h"
#include "mmrag/modular/pipeline/hyde_pipeline.h"
#include "mmrag/modular/pipeline/graph_pipeline.h"
#include "mmrag/modular/pipeline/corrective_pipeline.h"
#include "mmrag/modular/pipeline/react_pipeline.h"
#include "mmrag/modular/pipeline/iterative_pipeline.h"
#include "mmrag/modular/pipeline/recursive_pipeline.h"
#include "mmrag/modular/types.h"
#include <cstdlib>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <cstdio>
#include <cctype>
#include <random>
#include <queue>
#include <mutex>

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
    #include <poll.h>  // for poll(), pollfd, POLLIN used in body-read loop
#endif

namespace mmrag {
namespace api {

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

// 从原始请求中提取指定 HTTP header
static std::string extract_header(const std::string& raw_request,
                                  const std::string& header_name) {
    std::string pattern = header_name + ": ";
    auto pos = raw_request.find(pattern);
    if (pos == std::string::npos) {
        // 大小写不敏感重试
        std::string upper_name = header_name;
        for (auto& c : upper_name) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
        pattern = upper_name + ": ";
        pos = raw_request.find(pattern);
    }
    if (pos == std::string::npos) return "";
    pos += pattern.size();
    auto end = raw_request.find("\r\n", pos);
    if (end == std::string::npos) return "";
    return raw_request.substr(pos, end - pos);
}

static std::string read_body(std::istream& input, size_t content_length) {
    (void)input; (void)content_length;
    // Not used - body is read directly from socket
    return "";
}

// ========== Server 实现 ==========

struct Server::Impl {
    std::atomic<bool> running{false};
    int server_socket = -1;
    std::thread accept_thread;
    // 工作线程池：替代原来每个请求一个 detached thread 的设计，
    // 避免突发流量耗尽线程资源。
    std::unique_ptr<ThreadPool> worker_pool;

    // ========== Modular Pipelines Cache ==========
    std::mutex pipelines_mutex;
    std::unordered_map<mmrag::modular::PipelineType, std::shared_ptr<mmrag::modular::ModularPipeline>> pipelines;
    mmrag::modular::ModularConfig modular_config;

    // ========== Eval Store ==========
    std::unique_ptr<mmrag::eval::EvalRunStore> eval_store;

    // ========== 异步上传任务 ==========
    struct UploadTask {
        std::string id;
        std::string filename;
        std::string file_path;
        size_t file_size = 0;

        // 阶段定义
        enum class Stage {
            QUEUED,        // 已入队
            PARSING,       // 解析文件
            CHUNKING,      // 文本分块
            EMBEDDING,     // 生成向量
            INDEXING,      // 索引
            COMPLETED,     // 完成
            FAILED         // 失败
        };
        std::atomic<Stage> stage{Stage::QUEUED};

        // 进度
        std::atomic<int> progress{0};   // 0-100
        std::atomic<int> total{0};     // 总数（chunk 数等）
        std::atomic<int> processed{0}; // 已处理数
        std::string error_message;

        // 结果
        std::string doc_id;
        int chunk_count = 0;
        std::chrono::steady_clock::time_point created_at;
        std::chrono::steady_clock::time_point updated_at;

        UploadTask() : created_at(std::chrono::steady_clock::now()),
                       updated_at(std::chrono::steady_clock::now()) {}
    };

    std::mutex tasks_mutex;
    std::unordered_map<std::string, std::shared_ptr<UploadTask>> tasks_;

    // 生成新 task_id
    std::string generate_task_id() {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(1000, 9999);
        return "upload-" + std::to_string(ms) + "-" + std::to_string(dist(gen));
    }
};

Server::Server() : impl_(std::make_unique<Impl>()) {
    // 默认 8 个工作线程。可通过环境变量 RAG_WORKER_THREADS 调整。
    size_t worker_count = 8;
    const char* env = std::getenv("RAG_WORKER_THREADS");
    if (env) {
        int n = std::atoi(env);
        if (n > 0 && n <= 64) worker_count = static_cast<size_t>(n);
    }
    impl_->worker_pool = std::make_unique<ThreadPool>(worker_count);
    impl_->eval_store = std::make_unique<mmrag::eval::EvalRunStore>("eval_data/runs");
}

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

    // Initialize Modular Pipelines if not already initialized
    if (impl_->pipelines.empty()) {
        RAG_INFO("Initializing Modular Pipelines...");
        try {
            // Use default config for now.
            // Later we can load this from the main config file.
            mmrag::modular::ModularConfig m_config;

            // Configure LLM for Ollama — model_path triggers AdvancedPipeline LLM creation
            m_config.llm.model_path = "qwen2.5:3b";
            m_config.llm.model_type = "qwen2.5:3b";
            m_config.llm.max_tokens = 1024;
            m_config.llm.temperature = 0.7f;

            // Create all pipelines
            auto created_pipelines = mmrag::modular::PipelineFactory::create_all(m_config);

            // Move them into the map
            for (auto& pair : created_pipelines) {
                auto pipeline = std::shared_ptr<mmrag::modular::ModularPipeline>(std::move(pair.second));

                // Inject dependencies based on pipeline type
                if (pipeline->type() == mmrag::modular::PipelineType::NAIVE) {
                    auto naive = std::dynamic_pointer_cast<mmrag::modular::NaivePipeline>(pipeline);
                    if (naive && engine_) {
                        auto ret = engine_->retriever();
                        if (ret) {
                            auto hnsw = std::dynamic_pointer_cast<mmrag::HNSWRetriever>(ret);
                            if (hnsw) naive->set_hnsw_retriever(hnsw);
                        }
                    }
                } else if (pipeline->type() == mmrag::modular::PipelineType::ADVANCED ||
                           pipeline->type() == mmrag::modular::PipelineType::HYBRID) {
                    auto advanced = std::dynamic_pointer_cast<mmrag::modular::AdvancedPipeline>(pipeline);
                    if (advanced && engine_) {
                         auto ret = engine_->retriever();
                         if (ret) {
                             auto hnsw = std::dynamic_pointer_cast<mmrag::HNSWRetriever>(ret);
                             if (hnsw) advanced->set_hnsw_retriever(hnsw);
                         }
                         // Note: BM25Retriever requires separate instantiation or exposure from engine
                         // For now, skipping BM25 injection if not available
                    }
                    // For Hybrid, we also need Graph retriever
                    if (pipeline->type() == mmrag::modular::PipelineType::HYBRID) {
                        auto hybrid = std::dynamic_pointer_cast<mmrag::modular::HybridPipeline>(pipeline);
                        if (hybrid && engine_) {
                             // Graph retriever dependency injection
                             // TODO: Expose Graph Retriever from RAGEngine
                        }
                    }
                }
                // TODO: 其他 pipeline 类型的依赖注入

                // Initialize pipeline (triggers LLM creation for advanced pipelines)
                RAG_INFO("Initializing pipeline: " + pipeline->name());
                bool init_ok = pipeline->init(m_config);
                RAG_INFO("Pipeline " + pipeline->name() + " init=" + std::to_string(init_ok));

                impl_->pipelines[pipeline->type()] = pipeline;
            }
            impl_->modular_config = m_config;

            RAG_INFO("Initialized " + std::to_string(impl_->pipelines.size()) + " modular pipelines.");
        } catch (const std::exception& e) {
            RAG_ERROR("Failed to initialize modular pipelines: " + std::string(e.what()));
        }
    }

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

            // 处理请求（通过线程池提交，避免每个连接都创建新线程）
            auto pool = impl_->worker_pool.get();
            if (!pool->submit([this, client_socket]() {
                this->handle_connection(client_socket);
            })) {
                RAG_WARN("Worker pool queue full, rejecting connection");
#ifdef _WIN32
                closesocket(client_socket);
#else
                close(client_socket);
#endif
            }
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

    // 等待工作线程池排空已提交的任务（最多等 5 秒）
    if (impl_->worker_pool) {
        impl_->worker_pool->shutdown();
    }

    RAG_INFO("Server stopped");
}

void Server::handle_connection(int client_socket) {
    try {
        // 设置接收超时，避免恶意/慢速 client 导致 worker 永久阻塞。
        // 60s 足以覆盖正常的大文件上传，慢速网络场景可重新协商。
#ifdef _WIN32
        DWORD rcv_timeout = 60000;  // 60 秒（毫秒）
        setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&rcv_timeout), sizeof(rcv_timeout));
        // 同时设置发送超时，避免响应时阻塞
        setsockopt(client_socket, SOL_SOCKET, SO_SNDTIMEO,
                   reinterpret_cast<const char*>(&rcv_timeout), sizeof(rcv_timeout));
#else
        struct timeval rcv_timeout = {60, 0};  // 60 秒
        setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &rcv_timeout, sizeof(rcv_timeout));
        setsockopt(client_socket, SOL_SOCKET, SO_SNDTIMEO, &rcv_timeout, sizeof(rcv_timeout));
#endif

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
            auto pos = headers.find("Content-Length:");
            if (pos != std::string::npos) {
                pos += 15; // Skip "Content-Length:"
                while (pos < headers.size() && headers[pos] == ' ') ++pos;
                std::string num_str;
                while (pos < headers.size() && std::isdigit(headers[pos])) {
                    num_str += headers[pos];
                    ++pos;
                }
                if (!num_str.empty()) {
                    content_length = std::stoul(num_str);
                }
            }

            body = body_start;
            // 如果需要更多数据：每次 recv 前用 select() 短超时探测可读性。
            // 这样能快速识别"client 已发完 body 但保持 keep-alive 不关闭"的情况，
            // 避免 worker thread 在 recv() 上永久阻塞。
            // 注意：单次 select 超时并不代表 client 发完数据 — 可能是网络慢，
            // 但 content_length 已经收齐时就跳出循环。
            while (body.size() < content_length) {
#ifdef _WIN32
                fd_set readfds;
                FD_ZERO(&readfds);
                FD_SET(client_socket, &readfds);
                struct timeval tv;
                tv.tv_sec = 5;   // 5 秒探测间隔（覆盖慢速网络）
                tv.tv_usec = 0;
                int ready = select(0, &readfds, NULL, NULL, &tv);
                if (ready <= 0) {
                    // 超时或错误：可能是 client 已发完 body 等待响应（HTTP/1.1 keep-alive）
                    // 或 socket 出错。无论如何跳出循环，避免永久阻塞
                    RAG_WARN("Body recv timeout/error at " + std::to_string(body.size()) +
                             "/" + std::to_string(content_length) + " bytes");
                    break;
                }
#else
                struct pollfd pfd;
                pfd.fd = client_socket;
                pfd.events = POLLIN;
                int ready = poll(&pfd, 1, 5000);  // 5 秒超时
                if (ready <= 0) {
                    RAG_WARN("Body recv timeout/error at " + std::to_string(body.size()) +
                             "/" + std::to_string(content_length) + " bytes");
                    break;
                }
#endif
                n = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
                if (n <= 0) {
                    if (n < 0) {
                        RAG_WARN("recv() failed at " + std::to_string(body.size()) +
                                 "/" + std::to_string(content_length) + " bytes");
                    }
                    break;
                }
                // 用 append(buffer, n) 而非 += buffer：二进制 body 可能含 \0，
                // operator+= 会按 strlen 截断，导致大文件 body 永远凑不齐 content_length
                body.append(buffer, n);
            }
            // 如果 body 仍然不够完整（超时退出），记录警告
            if (body.size() < content_length) {
                RAG_WARN("Incomplete body received: got " + std::to_string(body.size()) +
                         " of " + std::to_string(content_length) + " bytes");
            }
        }

        // 路由处理（处理器返回完整 HTTP 响应）
        std::string response;

        if (route == "/api/v1/query" && method == "POST") {
            response = handle_query(body);
        } else if (route == "/api/v1/retrieve" && method == "POST") {
            response = handle_retrieve(body);
        } else if (route == "/api/v1/documents" && method == "GET") {
            response = handle_documents(query);
        } else if (route == "/api/v1/documents" && method == "POST") {
            std::string ct = extract_header(request, "Content-Type");
            response = handle_document_upload(body, ct);
        } else if (route.rfind("/api/v1/documents/", 0) == 0) {
            // /api/v1/documents/{id} 或 /api/v1/documents/{id}/content
            std::string rest = route.substr(std::string("/api/v1/documents/").size());
            std::string doc_id_or_path;
            std::string subpath;
            if (rest.size() > 8 && rest.compare(rest.size() - 8, 8, "/content") == 0) {
                doc_id_or_path = url_decode(rest.substr(0, rest.size() - 8));
                subpath = "/content";
            } else {
                doc_id_or_path = url_decode(rest);
            }
            if (!subpath.empty()) {
                response = handle_document_content(doc_id_or_path);
            } else {
                response = handle_document(doc_id_or_path, method);
            }
        } else if (route == "/api/v1/knowledge/documents" && method == "GET") {
            response = handle_documents(query);
        } else if (route == "/api/v1/knowledge/upload" && method == "POST") {
            std::string ct = extract_header(request, "Content-Type");
            response = handle_document_upload(body, ct);
        } else if (method == "OPTIONS") {
            // CORS 预检 (必须在所有路径匹配之前先处理 OPTIONS)
            response = "HTTP/1.1 204 No Content\r\n"
                       "Access-Control-Allow-Origin: *\r\n"
                       "Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n"
                       "Access-Control-Allow-Headers: Content-Type\r\n"
                       "Content-Length: 0\r\n\r\n";
        } else if (route.rfind("/api/v1/knowledge/upload/", 0) == 0 && method == "GET") {
            // /api/v1/knowledge/upload/{task_id} — 进度查询
            std::string task_id = route.substr(std::string("/api/v1/knowledge/upload/").size());
            response = handle_upload_progress(task_id);
        } else if (route.rfind("/api/v1/knowledge/documents/", 0) == 0) {
            std::string rest = route.substr(std::string("/api/v1/knowledge/documents/").size());
            std::string doc_id = url_decode(rest);
            response = handle_document(doc_id, method);
        } else if (route == "/api/v1/documents/dirs") {
            std::string ct = extract_header(request, "Content-Type");
            response = handle_dirs(body, method, query);
        } else if (route == "/api/v1/index/status" && method == "GET") {
            response = handle_index_status();
        } else if (route == "/api/v1/index/rebuild" && method == "POST") {
            response = handle_rebuild(body);
        } else if (route == "/api/v1/status" && method == "GET") {
            response = handle_system_status();
        } else if (route == "/api/v1/pipelines" && method == "GET") {
            response = handle_pipelines_list();
        } else if (route.rfind("/api/v1/pipelines/", 0) == 0) {
            std::string rest = route.substr(std::string("/api/v1/pipelines/").size());
            if (method == "GET") {
                response = handle_pipeline_config(rest);
            } else if (method == "PUT") {
                response = handle_pipeline_update(rest, body);
            } else {
                response = create_error_response("Method not allowed", 405);
            }
        } else if (route.rfind("/api/v1/migration/", 0) == 0) {
            response = handle_migration(route, method, body);
        } else if (route == "/api/v1/eval/datasets" && method == "GET") {
            response = handle_eval_datasets_list();
        } else if (route == "/api/v1/eval/datasets" && method == "POST") {
            response = handle_eval_datasets_create(body);
        } else if (route == "/api/v1/eval/runs" && method == "GET") {
            response = handle_eval_runs_list(query);
        } else if (route == "/api/v1/eval/runs" && method == "POST") {
            response = handle_eval_runs_create(body);
        } else if (route == "/api/v1/eval/compare" && method == "GET") {
            response = handle_eval_compare(query);
        } else if (route == "/api/v1/eval/trends" && method == "GET") {
            response = handle_eval_trends(query);
        } else if (route == "/api/v1/eval/optimization-suggestions" && method == "GET") {
            response = handle_eval_optimization_suggestions(query);
        } else if (route == "/api/v1/query/stream" && method == "POST") {
            response = handle_query_stream(body);
        } else if (route == "/health" && method == "GET") {
            response = handle_health();
        } else if (route == "/metrics" && method == "GET") {
            response = handle_metrics();
        } else if (method == "OPTIONS") {
            // CORS 预检
            response = "HTTP/1.1 204 No Content\r\n"
                       "Access-Control-Allow-Origin: *\r\n"
                       "Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n"
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
    } catch (...) {
        RAG_ERROR("Request handling error: unknown exception");
        try {
            std::string err = create_error_response("Internal Server Error", 500);
            send(client_socket, err.c_str(), err.size(), 0);
        } catch (...) {}
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
    std::string pipeline_type_str = extract_json_string(body, "pipeline_type", "naive");

    if (query.empty()) {
        return create_error_response("Missing query parameter", 400);
    }

    // 如果请求了非 naive 的 pipeline，尝试使用 modular pipeline 系统
    // naive pipeline 也可以走 modular 系统（更一致），但我们保留 engine_->query 作为
    // 传统 fallback，以确保现有的“naive”调用完全向后兼容。
    if (pipeline_type_str != "naive" || !engine_) {
        // Modular pipeline path
        try {
            auto type = mmrag::modular::string_to_pipeline_type(pipeline_type_str);
            std::shared_ptr<mmrag::modular::ModularPipeline> pipeline;

            {
                std::lock_guard<std::mutex> lock(impl_->pipelines_mutex);
                auto it = impl_->pipelines.find(type);
                if (it != impl_->pipelines.end()) {
                    pipeline = it->second;
                }
            }

            if (!pipeline) {
                return create_error_response("Pipeline not found or initialized: " + pipeline_type_str, 404);
            }

            if (!pipeline->is_ready()) {
                // Pipeline not ready (no LLM loaded) — fall through to naive retrieval
                RAG_WARN("Pipeline " + pipeline_type_str + " not ready, falling back to naive");
            } else {
                // Prepare modular query
                mmrag::modular::ModularQuery m_query;
                m_query.text = query;
                m_query.pipeline_type = type;
                m_query.top_k = top_k;

                // Execute
                auto m_result = pipeline->query(m_query);

                // Convert ModularQueryResult to JSON
                std::ostringstream oss;
                oss << "{";
                oss << "\"answer\": \"" << json_escape(m_result.answer) << "\",";
                oss << "\"context\": [";

                for (size_t i = 0; i < m_result.context.size(); ++i) {
                    if (i > 0) oss << ",";
                    const auto& chunk_ref = m_result.context[i];
                    oss << "{";
                    oss << "\"chunk_id\": \"" << json_escape(chunk_ref.chunk.id) << "\",";
                    oss << "\"content\": \"" << json_escape(chunk_ref.chunk.content) << "\",";
                    oss << "\"score\": " << chunk_ref.score << ",";
                    oss << "\"source\": \"" << json_escape(chunk_ref.source) << "\",";
                    oss << "\"metadata\": {";
                    oss << "\"file_path\": \"" << json_escape(chunk_ref.chunk.metadata.file_path) << "\",";
                    oss << "\"document_id\": \"" << json_escape(chunk_ref.chunk.document_id) << "\"";
                    oss << "}";
                    oss << "}";
                }

                oss << "],";
                oss << "\"retrieval_time_ms\": " << m_result.retrieval_time_ms << ",";
                oss << "\"generation_time_ms\": " << m_result.generation_time_ms << ",";
                oss << "\"total_time_ms\": " << m_result.total_time_ms << ",";
                oss << "\"total_tokens\": " << m_result.total_tokens;
                oss << "}";
                return create_json_response(oss.str());
            }
        } catch (const std::exception& e) {
            // 回退到 engine_->query 如果 pipeline 解析失败
            RAG_WARN("Modular pipeline failed, falling back to engine query: " + std::string(e.what()));
        }
    }

    // 默认/向后兼容路径：使用内置 RAGEngine::query
    auto result = engine_->query(query, top_k);

    std::ostringstream oss;
    oss << "{";
    oss << "\"answer\": \"" << json_escape(result.answer) << "\",";
    oss << "\"context\": [";

    for (size_t i = 0; i < result.chunks.size(); ++i) {
        if (i > 0) oss << ",";
        const auto& chunk = result.chunks[i];
        oss << "{";
        oss << "\"chunk_id\": \"" << json_escape(chunk.chunk.id) << "\",";
        oss << "\"content\": \"" << json_escape(chunk.chunk.content) << "\",";
        oss << "\"score\": " << chunk.score << ",";
        oss << "\"source\": \"" << json_escape(chunk.source) << "\",";
        oss << "\"metadata\": {";
        oss << "\"file_path\": \"" << json_escape(chunk.chunk.metadata.file_path) << "\",";
        oss << "\"document_id\": \"" << json_escape(chunk.chunk.document_id) << "\"";
        oss << "}";
        oss << "}";
    }

    oss << "],";
    oss << "\"retrieval_time_ms\": " << result.query_time_ms << ",";
    oss << "\"generation_time_ms\": 0,";
    oss << "\"total_time_ms\": " << result.query_time_ms << ",";
    oss << "\"total_tokens\": 0";
    oss << "}";
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

std::string Server::handle_documents(const std::string& query) {
    if (!engine_) {
        return create_error_response("Engine not initialized", 500);
    }

    auto params = parse_query_params(query);
    std::string dir_filter = params.count("dir") ? params["dir"] : "";
    auto docs = engine_->list_documents();

    std::ostringstream oss;
    oss << "{\"documents\": [";
    bool first = true;
    int count = 0;
    for (size_t i = 0; i < docs.size(); ++i) {
        const auto& doc = docs[i];
        // 按目录过滤
        if (!dir_filter.empty()) {
            if (doc.metadata.file_path.find(dir_filter) != 0) continue;
        }
        if (first) first = false; else oss << ",";
        oss << "{";
        oss << "\"id\": \"" << json_escape(doc.id) << "\",";
        oss << "\"file_name\": \"" << json_escape(doc.metadata.file_name) << "\",";
        oss << "\"file_path\": \"" << json_escape(doc.metadata.file_path) << "\",";
        oss << "\"file_type\": \"" << json_escape(doc.metadata.file_type) << "\",";
        oss << "\"file_size\": " << doc.metadata.file_size << ",";
        oss << "\"status\": " << static_cast<int>(doc.status) << ",";
        oss << "\"indexed_at\": " << doc.indexed_at;
        oss << "}";
        ++count;
    }
    oss << "], \"total\": " << count << "}";
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

std::string Server::handle_system_status() {
    if (!engine_) {
        return create_error_response("Engine not initialized", 500);
    }

    auto status = engine_->get_index_status();
    auto docs = engine_->list_documents();

    std::ostringstream oss;
    oss << "{";
    oss << "\"status\": \"healthy\",";
    oss << "\"uptime_seconds\": 0,";
    oss << "\"total_queries\": 0,";
    oss << "\"total_documents\": " << docs.size() << ",";
    oss << "\"total_chunks\": " << status.chunk_count << ",";
    oss << "\"active_pipelines\": [\"naive\", \"advanced\", \"hybrid\", \"hyde\", \"graph\", \"corrective\", \"react\", \"iterative\", \"recursive\"],";
    oss << "\"models_loaded\": []";
    oss << "}";
    return create_json_response(oss.str());
}

std::string Server::handle_pipelines_list() {
    std::ostringstream oss;
    oss << "[\"naive\", \"advanced\", \"hybrid\", \"hyde\", \"graph\", \"corrective\", \"react\", \"iterative\", \"recursive\"]";
    return create_json_response(oss.str());
}

std::string Server::handle_pipeline_config(const std::string& type) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"type\": \"" << json_escape(type) << "\",";
    oss << "\"retrieval\": {";
    oss << "\"top_k\": 5,";
    oss << "\"rrf_k\": 60,";
    oss << "\"enable_reranker\": false";
    oss << "},";
    oss << "\"llm\": {";
    oss << "\"model\": \"qwen2.5-7b\",";
    oss << "\"temperature\": 0.7,";
    oss << "\"max_tokens\": 1024";
    oss << "}";
    oss << "}";
    return create_json_response(oss.str());
}

std::string Server::handle_pipeline_update(const std::string& type, const std::string& body) {
    // 实际应用中应更新配置
    (void)type; (void)body;
    return create_json_response("{\"success\": true}");
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

std::string Server::handle_document(const std::string& id_or_path, const std::string& method) {
    if (!engine_) {
        return create_error_response("Engine not initialized", 500);
    }

    auto docs = engine_->list_documents();
    Document* target = nullptr;
    for (auto& doc : docs) {
        if (doc.id == id_or_path || doc.metadata.file_path == id_or_path) {
            target = &doc;
            break;
        }
    }

    if (!target) {
        return create_error_response("Document not found", 404);
    }

    // DELETE: 删除文档文件 + 从索引移除
    if (method == "DELETE") {
        // 删除文件系统上的文件
        namespace fs = std::filesystem;
        fs::path data_root(config_.data_dir.empty() ? "./rag_data" : config_.data_dir);
        fs::path file_path = data_root / target->metadata.file_path;
        std::error_code ec;
        if (fs::exists(file_path, ec)) {
            fs::remove(file_path, ec);
        }
        // 从索引移除
        engine_->remove_document(target->id);

        std::ostringstream oss;
        oss << "{\"deleted\": true, \"id\": \"" << json_escape(target->id) << "\"}";
        return create_json_response(oss.str());
    }

    // GET: 返回文档元数据
    std::ostringstream oss;
    oss << "{";
    oss << "\"id\": \"" << json_escape(target->id) << "\",";
    oss << "\"file_name\": \"" << json_escape(target->metadata.file_name) << "\",";
    oss << "\"file_path\": \"" << json_escape(target->metadata.file_path) << "\",";
    oss << "\"file_type\": \"" << json_escape(target->metadata.file_type) << "\",";
    oss << "\"file_size\": " << target->metadata.file_size << ",";
    oss << "\"status\": " << static_cast<int>(target->status);
    oss << "}";
    return create_json_response(oss.str());
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

std::string Server::handle_document_upload(const std::string& body,
                                          const std::string& content_type) {
    namespace fs = std::filesystem;

    // 确保数据目录存在
    fs::path data_dir(config_.data_dir.empty() ? "./rag_data" : config_.data_dir);
    std::error_code ec;
    fs::create_directories(data_dir, ec);

    // 简单 multipart 解析：查找 filename="..." 和 Content-Type
    std::string boundary;
    if (content_type.find("multipart/form-data") != std::string::npos) {
        auto bp = content_type.find("boundary=");
        if (bp != std::string::npos) {
            boundary = "--" + content_type.substr(bp + 9);
        }
    }

    if (boundary.empty()) {
        return create_error_response("Invalid Content-Type: expected multipart/form-data", 400);
    }

    // 解析 multipart body — 只提取第一个文件用于异步处理
    size_t pos = 0;
    std::string filename;
    std::string file_data;

    while (true) {
        auto part_start = body.find(boundary, pos);
        if (part_start == std::string::npos) break;
        part_start += boundary.size();
        if (body.compare(part_start, 2, "--") == 0) break;
        part_start += 2; // skip \r\n

        auto header_end = body.find("\r\n\r\n", part_start);
        if (header_end == std::string::npos) break;
        auto part_headers = body.substr(part_start, header_end - part_start);
        auto data_start = header_end + 4;
        auto part_end = body.find(boundary, data_start);
        if (part_end == std::string::npos) break;

        // 提取 filename
        auto fn_start = part_headers.find("filename=\"");
        if (fn_start != std::string::npos) {
            fn_start += 10;
            auto fn_end = part_headers.find("\"", fn_start);
            if (fn_end != part_headers.npos) {
                filename = part_headers.substr(fn_start, fn_end - fn_start);
            }
            file_data = body.substr(data_start, part_end - data_start - 2);
            break; // 只处理第一个文件
        }
        pos = part_end;
    }

    if (filename.empty() || file_data.empty()) {
        return create_error_response("No file found in upload", 400);
    }
    if (filename.find("..") != std::string::npos) {
        return create_error_response("Invalid filename", 400);
    }

    // 保存文件到磁盘
    fs::path file_path = fs::absolute(data_dir / filename);
    {
        std::ofstream out(file_path.string(), std::ios::binary);
        if (!out) {
            return create_error_response("Failed to save file", 500);
        }
        out.write(file_data.data(), file_data.size());
    }

    // 创建上传任务
    auto task = std::make_shared<Impl::UploadTask>();
    task->id = impl_->generate_task_id();
    task->filename = filename;
    task->file_path = file_path.string();
    task->file_size = file_data.size();

    {
        std::lock_guard<std::mutex> lock(impl_->tasks_mutex);
        impl_->tasks_[task->id] = task;
    }

    // 提交到线程池异步处理（lambda 捕获所有依赖，避免修改头文件）
    impl_->worker_pool->submit([this, task]() {
        process_upload_task_impl(std::static_pointer_cast<void>(task));
    });

    // 立即返回 task_id
    std::ostringstream oss;
    oss << "{";
    oss << "\"task_id\": \"" << task->id << "\",";
    oss << "\"filename\": \"" << json_escape(filename) << "\",";
    oss << "\"size\": " << task->file_size << ",";
    oss << "\"status\": \"queued\"";
    oss << "}";
    return create_json_response(oss.str(), 202);  // 202 Accepted
}

// 后台异步处理上传任务 — 按阶段更新进度
void Server::process_upload_task_impl(std::shared_ptr<void> task_ptr) {
    auto task = std::static_pointer_cast<Impl::UploadTask>(task_ptr);
    using Stage = Impl::UploadTask::Stage;
    namespace fs = std::filesystem;

    auto t_now = []() {
        return std::chrono::steady_clock::now();
    };
    auto t_start = t_now();
    auto last_phase = [t_start, t_now](const std::string& name) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            t_now() - t_start).count();
        RAG_INFO("[TIMING] " + name + " at +" + std::to_string(ms) + "ms");
    };

    auto update_progress = [task](Stage stage, int processed, int total, int percent) {
        task->stage.store(stage);
        task->processed.store(processed);
        task->total.store(total);
        task->progress.store(percent);
        task->updated_at = std::chrono::steady_clock::now();
    };

    try {
        // ========== 阶段1：解析文件 ==========
        update_progress(Stage::PARSING, 0, 1, 5);
        std::string parsed_content;

        // 提取文件扩展名
        std::string ext = fs::path(task->filename).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        // 使用 ParserRegistry 进行真正的文档解析
        mmrag::ParserRegistry parser_registry;
        auto* parser = parser_registry.get_parser(task->file_path);

        try {
            auto parse_result = parser->parse(task->file_path);
            parsed_content = parse_result.content;
            RAG_INFO("Parsed file: " + task->filename + " -> " +
                     std::to_string(parsed_content.size()) + " chars");
        } catch (const std::exception& e) {
            RAG_WARN("Parser failed for " + task->filename + ": " + e.what() +
                     ", falling back to raw read");
            // 解析失败时回退到原始读取
            std::ifstream in(task->file_path, std::ios::binary);
            std::ostringstream ss;
            ss << in.rdbuf();
            parsed_content = ss.str();
        }

        update_progress(Stage::PARSING, 1, 1, 15);
        last_phase("PARSING done");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));  // 让前端看到 parsing 阶段

        // ========== 阶段2：分块 ==========
        // 使用真正的 ChunkerFactory 进行语义分块
        mmrag::ChunkingConfig chunk_config;
        chunk_config.strategy = "recursive";  // 使用递归分块策略
        chunk_config.chunk_size = 500;        // 每块最大 500 字符
        chunk_config.chunk_overlap = 50;      // 块间重叠 50 字符
        chunk_config.min_chunk_size = 100;    // 最小块大小 100 字符

        auto chunker = mmrag::ChunkerFactory::create(chunk_config);
        mmrag::DocumentMetadata metadata;
        metadata.file_path = task->file_path;
        metadata.file_name = task->filename;

        auto chunk_result = chunker->chunk(parsed_content, "temp-doc-id", metadata);
        std::vector<std::string> chunks;
        for (const auto& chunk : chunk_result.chunks) {
            chunks.push_back(chunk.content);
        }

        if (chunks.empty()) chunks.push_back(parsed_content);

        task->total.store(static_cast<int>(chunks.size()));
        task->chunk_count = static_cast<int>(chunks.size());
        update_progress(Stage::CHUNKING, 0, static_cast<int>(chunks.size()), 20);
        last_phase("CHUNKING done (" + std::to_string(chunks.size()) + " chunks)");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));  // 让前端看到 chunking 阶段

        // ========== 阶段3+4：生成向量 + 索引（合并为单阶段，按 chunk 推进）============
        // 创建 Document 对象
        mmrag::Document doc;
        // 使用 task_id + 原子计数器确保唯一性（同一毫秒内的并发上传也不会冲突）
        static std::atomic<int> doc_seq{0};
        doc.id = "doc-" + task->id + "-" + std::to_string(doc_seq.fetch_add(1));
        doc.content = parsed_content;
        doc.metadata.file_path = task->file_path;
        doc.metadata.file_name = task->filename;
        doc.metadata.file_type = ext;
        doc.metadata.file_size = task->file_size;
        doc.status = mmrag::Document::Status::PENDING;
        doc.created_at = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        // 提交到引擎 — add_document 内部完成 embedding + indexing。
        update_progress(Stage::EMBEDDING, 0, static_cast<int>(chunks.size()), 20);
        last_phase("EMBEDDING begin");

        // 实际调用引擎处理 — 这一步同时做 embedding + 建 HNSW 索引 + 建 BM25 索引
        update_progress(Stage::INDEXING, 0, static_cast<int>(chunks.size()), 90);

        // 并行索引构建：底层索引已实现线程安全（FlatArrayIndex 和 BM25Index
        // 都有内部 mutex），因此可以移除全局锁，允许多个上传任务并行处理。
        {
            auto add_t0 = std::chrono::steady_clock::now();

            // 使用带进度回调的 add_document，实现 chunk 级别进度更新
            engine_->add_document(doc,
                [&](int processed, int total, const std::string& stage) {
                    // 根据引擎内部阶段映射到前端阶段
                    Stage frontend_stage;
                    int percent;
                    if (stage == "embedding") {
                        frontend_stage = Stage::EMBEDDING;
                        percent = 20 + (processed * 70 / total);  // 20%-90%
                    } else {
                        frontend_stage = Stage::INDEXING;
                        percent = 20 + (processed * 70 / total);  // 20%-90%
                    }
                    update_progress(frontend_stage, processed, total, percent);
                    last_phase(stage + " " + std::to_string(processed) + "/" + std::to_string(total));
                });

            auto add_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - add_t0).count();
            double per_chunk = chunks.size() > 0 ? double(add_ms) / chunks.size() : 0.0;
            RAG_INFO("[TIMING] add_document: " + std::to_string(add_ms) + "ms total, " +
                     std::to_string(chunks.size()) + " chunks, " +
                     std::to_string(per_chunk).substr(0, 5) + "ms/chunk");
        }
        last_phase("add_document (embedding + HNSW + BM25) done");

        // ========== 阶段5：完成 ==========
        update_progress(Stage::COMPLETED, static_cast<int>(chunks.size()),
                       static_cast<int>(chunks.size()), 100);
        task->doc_id = doc.id;

        RAG_INFO("Upload task " + task->id + " completed: " + task->filename +
                 " (" + std::to_string(chunks.size()) + " chunks)");

    } catch (const std::exception& e) {
        task->stage.store(Stage::FAILED);
        task->error_message = e.what();
        task->updated_at = std::chrono::steady_clock::now();
        RAG_ERROR("Upload task " + task->id + " failed: " + std::string(e.what()));
    } catch (...) {
        task->stage.store(Stage::FAILED);
        task->error_message = "Unknown error";
        task->updated_at = std::chrono::steady_clock::now();
        RAG_ERROR("Upload task " + task->id + " failed: unknown error");
    }
}

// 获取上传任务进度
std::string Server::handle_upload_progress(const std::string& task_id) {
    std::shared_ptr<Impl::UploadTask> task;
    {
        std::lock_guard<std::mutex> lock(impl_->tasks_mutex);
        auto it = impl_->tasks_.find(task_id);
        if (it != impl_->tasks_.end()) {
            task = it->second;
        }
    }

    if (!task) {
        return create_error_response("Task not found", 404);
    }

    static const char* stage_names[] = {
        "queued", "parsing", "chunking", "embedding", "indexing", "completed", "failed"
    };
    int stage_idx = static_cast<int>(task->stage.load());
    const char* stage_name = (stage_idx >= 0 && stage_idx < 7) ? stage_names[stage_idx] : "unknown";

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - task->created_at).count();

    std::ostringstream oss;
    oss << "{";
    oss << "\"task_id\": \"" << task->id << "\",";
    oss << "\"filename\": \"" << json_escape(task->filename) << "\",";
    oss << "\"size\": " << task->file_size << ",";
    oss << "\"stage\": \"" << stage_name << "\",";
    oss << "\"progress\": " << task->progress.load() << ",";
    oss << "\"processed\": " << task->processed.load() << ",";
    oss << "\"total\": " << task->total.load() << ",";
    oss << "\"elapsed_ms\": " << elapsed << ",";
    oss << "\"chunk_count\": " << task->chunk_count << ",";
    if (!task->doc_id.empty()) {
        oss << "\"doc_id\": \"" << task->doc_id << "\",";
    }
    if (task->stage.load() == Impl::UploadTask::Stage::FAILED && !task->error_message.empty()) {
        oss << "\"error\": \"" << json_escape(task->error_message) << "\",";
    }
    oss << "\"completed\": " << (task->stage.load() == Impl::UploadTask::Stage::COMPLETED ? "true" : "false");
    oss << "}";
    return create_json_response(oss.str(), 200);
}

// ========== 重建索引 Job 管理 ==========

struct IndexJob {
    std::string job_id;
    std::atomic<float> progress{0.0f};
    std::atomic<int> processed{0};
    std::atomic<int> total{0};
    std::atomic<bool> cancelled{false};
    std::atomic<bool> completed{false};
    std::string current_file;
    std::string message;
    std::chrono::steady_clock::time_point started_at;
    std::thread worker;
};

static std::mutex g_jobs_mutex;
static std::unordered_map<std::string, std::shared_ptr<IndexJob>> g_jobs;

static std::string generate_job_id() {
    static std::mt19937 rng((unsigned)std::chrono::steady_clock::now().time_since_epoch().count());
    std::string id = "job-" + std::to_string(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    id += "-" + std::to_string(rng() % 10000);
    return id;
}

// ========== 迁移 Job 管理 ==========

struct MigrationJob {
    std::string job_id;
    std::atomic<float> progress{0.0f};
    std::string status; // "exporting" / "importing" / "completed" / "failed"
    std::string file_path;
    std::string error;
    std::thread worker;
};

static std::mutex g_mig_mutex;
static std::unordered_map<std::string, std::shared_ptr<MigrationJob>> g_migrations;

std::string Server::handle_dirs(const std::string& body,
                                const std::string& method,
                                const std::string& query) {
    namespace fs = std::filesystem;

    fs::path data_dir(config_.data_dir.empty() ? "./rag_data" : config_.data_dir);
    std::error_code ec;

    // GET: 列出所有子目录
    if (method == "GET") {
        std::vector<std::string> dirs;
        if (fs::exists(data_dir, ec)) {
            for (const auto& entry : fs::recursive_directory_iterator(data_dir, ec)) {
                if (fs::is_directory(entry, ec)) {
                    std::string rel = entry.path().lexically_relative(data_dir).string();
                    if (!rel.empty()) {
                        std::replace(rel.begin(), rel.end(), '\\', '/');
                        dirs.push_back(rel);
                    }
                }
            }
        }

        std::ostringstream oss;
        oss << "{\"dirs\": [";
        for (size_t i = 0; i < dirs.size(); ++i) {
            if (i > 0) oss << ",";
            oss << "\"" << json_escape(dirs[i]) << "\"";
        }
        oss << "], \"total\": " << dirs.size() << "}";
        return create_json_response(oss.str());
    }

    // POST: 创建目录
    if (method == "POST") {
        std::string path = extract_json_string(body, "path");
        if (path.empty()) {
            return create_error_response("Missing 'path' field", 400);
        }
        // 路径清理：移除 leading/trailing /
        while (!path.empty() && (path.front() == '/' || path.front() == '\\')) path.erase(path.begin());
        while (!path.empty() && (path.back() == '/' || path.back() == '\\')) path.pop_back();

        fs::path new_dir = data_dir / path;
        bool created = fs::create_directories(new_dir, ec);

        std::ostringstream oss;
        oss << "{\"created\": " << (created ? "true" : "false") << ",";
        oss << "\"path\": \"" << json_escape(path) << "\"}";
        return create_json_response(oss.str());
    }

    // DELETE: 删除目录
    if (method == "DELETE") {
        auto params = parse_query_params(query);
        if (!params.count("path")) {
            return create_error_response("Missing 'path' query parameter", 400);
        }
        std::string rel_path = params["path"];
        // 清理
        while (!rel_path.empty() && (rel_path.front() == '/' || rel_path.front() == '\\')) rel_path.erase(rel_path.begin());
        while (!rel_path.empty() && (rel_path.back() == '/' || rel_path.back() == '\\')) rel_path.pop_back();

        fs::path dir_path = data_dir / rel_path;
        std::error_code ec2;
        bool removed = fs::remove(dir_path, ec2);

        std::ostringstream oss;
        oss << "{\"deleted\": " << (removed ? "true" : "false");
        if (!removed && ec2) {
            oss << ", \"error\": \"" << json_escape(ec2.message()) << "\"";
        }
        oss << "}";
        return create_json_response(oss.str());
    }

    return create_error_response("Method not allowed", 405);
}

// ========== 重建索引 API ==========

std::string Server::handle_rebuild(const std::string& body) {
    std::string rebuild_dir = extract_json_string(body, "dir");
    bool incremental = true;
    {
        std::istringstream iss(body);
        std::string token;
        while (std::getline(iss, token, ',')) {
            auto pos = token.find("\"incremental\"");
            if (pos != std::string::npos) {
                auto colon = token.find(':', pos);
                if (colon != std::string::npos) {
                    auto val_start = token.find_first_not_of(" \t\r\n", colon + 1);
                    if (val_start != std::string::npos && token[val_start] == 'f') {
                        incremental = false;
                    }
                }
            }
        }
    }

    std::string job_id = generate_job_id();
    auto job = std::make_shared<IndexJob>();
    job->job_id = job_id;
    job->started_at = std::chrono::steady_clock::now();

    job->worker = std::thread([this, job, rebuild_dir, incremental]() {
        try {
            // 1. Scan files
            std::vector<std::string> files;
            namespace fs = std::filesystem;
            fs::path data_path(rebuild_dir.empty()
                ? (config_.data_dir.empty() ? "./rag_data" : config_.data_dir)
                : rebuild_dir);

            if (!fs::exists(data_path)) {
                job->message = "Directory not found: " + data_path.string();
                job->completed = true;
                return;
            }

            for (const auto& entry : fs::recursive_directory_iterator(data_path)) {
                if (fs::is_regular_file(entry)) {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".txt" || ext == ".md" || ext == ".pdf" || ext == ".json") {
                        files.push_back(entry.path().string());
                    }
                }
            }

            job->total = files.size();
            if (files.empty()) {
                job->message = "No files found to index";
                job->completed = true;
                return;
            }

            // 2. Index files (engine_ must be thread-safe for add_document)
            for (size_t i = 0; i < files.size(); ++i) {
                if (job->cancelled) break;
                job->current_file = files[i];
                job->processed = i + 1;
                job->progress = static_cast<float>(i + 1) / files.size();

                // Read file content
                std::ifstream ifs(files[i], std::ios::binary);
                std::string content((std::istreambuf_iterator<char>(ifs)),
                                     std::istreambuf_iterator<char>());

                // Create a Document struct from file path
                mmrag::Document doc;
                doc.id = "doc-" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
                doc.content = content;
                doc.metadata.file_path = files[i];
                doc.metadata.file_name = fs::path(files[i]).filename().string();
                doc.metadata.file_type = fs::path(files[i]).extension().string();
                doc.metadata.file_size = content.size();
                doc.status = mmrag::Document::Status::PENDING;
                doc.created_at = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                engine_->add_document(doc);
            }

            job->completed = true;
            job->message = incremental ? "Incremental indexing completed" : "Full rebuild completed";
        } catch (const std::exception& e) {
            job->message = "Error: " + std::string(e.what());
            job->completed = true;
        } catch (...) {
            job->message = "Error: unknown exception in rebuild thread";
            job->completed = true;
        }
    });

    job->worker.detach();

    {
        std::lock_guard<std::mutex> lock(g_jobs_mutex);
        g_jobs[job_id] = job;
    }

    std::ostringstream oss;
    oss << "{\"job_id\": \"" << job_id << "\", \"status\": \"started\"}";
    return create_json_response(oss.str());
}

// ========== 迁移 API ==========

std::string Server::handle_migration(const std::string& route, const std::string& method, const std::string& body) {
    // POST /api/v1/migration/export
    if (route == "/api/v1/migration/export" && method == "POST") {
        std::string target_db = extract_json_string(body, "target_db");

        std::string job_id = generate_job_id();
        auto job = std::make_shared<MigrationJob>();
        job->job_id = job_id;
        job->status = "exporting";

        job->worker = std::thread([this, job, target_db]() {
            try {
                // Export data to JSONL
                std::string export_dir = config_.data_dir.empty() ? "./rag_data" : config_.data_dir;
                std::string export_path = export_dir + "/migration_export.jsonl";

                auto docs = engine_->list_documents();
                std::ofstream ofs(export_path);
                for (const auto& doc : docs) {
                    // Write document content to JSONL
                    ofs << "{\"id\":\"" << json_escape(doc.id) << "\",";
                    ofs << "\"file\":\"" << json_escape(doc.metadata.file_path) << "\"}\n";
                }
                ofs.close();

                job->file_path = export_path;
                job->progress = 1.0f;
                job->status = "completed";
            } catch (const std::exception& e) {
                job->status = "failed";
                job->error = e.what();
            }
        });

        job->worker.detach();

        {
            std::lock_guard<std::mutex> lock(g_mig_mutex);
            g_migrations[job_id] = job;
        }

        std::ostringstream oss;
        oss << "{\"job_id\": \"" << job_id << "\", \"status\": \"exporting\"}";
        return create_json_response(oss.str());
    }

    // POST /api/v1/migration/import
    if (route == "/api/v1/migration/import" && method == "POST") {
        std::string file_path = extract_json_string(body, "file_path");

        std::string job_id = generate_job_id();
        auto job = std::make_shared<MigrationJob>();
        job->job_id = job_id;
        job->status = "importing";

        job->worker = std::thread([this, job, file_path]() {
            try {
                std::ifstream ifs(file_path);
                std::string line;
                while (std::getline(ifs, line)) {
                    if (line.empty()) continue;
                    // Simple JSONL parsing
                    auto id_pos = line.find("\"id\":\"");
                    auto file_pos = line.find("\"file\":\"");
                    if (id_pos != std::string::npos && file_pos != std::string::npos) {
                        auto id_end = line.find("\"", id_pos + 6);
                        auto file_end = line.find("\"", file_pos + 8);
                        if (id_end != std::string::npos && file_end != std::string::npos) {
                            std::string file = line.substr(file_pos + 8, file_end - file_pos - 8);
                            // Create a Document struct from file path
                            mmrag::Document doc;
                            doc.id = "doc-" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch()).count());
                            doc.metadata.file_path = file;
                            doc.metadata.file_name = std::filesystem::path(file).filename().string();
                            doc.metadata.file_type = std::filesystem::path(file).extension().string();
                            doc.status = mmrag::Document::Status::PENDING;
                            doc.created_at = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch()).count();
                            engine_->add_document(doc);
                        }
                    }
                }
                job->progress = 1.0f;
                job->status = "completed";
            } catch (const std::exception& e) {
                job->status = "failed";
                job->error = e.what();
            }
        });

        job->worker.detach();

        {
            std::lock_guard<std::mutex> lock(g_mig_mutex);
            g_migrations[job_id] = job;
        }

        std::ostringstream oss;
        oss << "{\"job_id\": \"" << job_id << "\", \"status\": \"importing\"}";
        return create_json_response(oss.str());
    }

    // GET /api/v1/migration/status/{job_id}
    if (route.rfind("/api/v1/migration/status/", 0) == 0 && method == "GET") {
        std::string job_id = route.substr(std::string("/api/v1/migration/status/").size());
        std::lock_guard<std::mutex> lock(g_mig_mutex);
        auto it = g_migrations.find(job_id);
        if (it == g_migrations.end()) {
            return create_error_response("Migration job not found", 404);
        }
        auto& job = it->second;
        std::ostringstream oss;
        oss << "{\"job_id\": \"" << job_id << "\",";
        oss << "\"status\": \"" << job->status << "\",";
        oss << "\"progress\": " << job->progress << ",";
        if (!job->file_path.empty()) oss << "\"file_path\": \"" << json_escape(job->file_path) << "\",";
        if (!job->error.empty()) oss << "\"error\": \"" << json_escape(job->error) << "\",";
        oss << "\"completed\": " << (job->status == "completed" || job->status == "failed" ? "true" : "false");
        oss << "}";
        return create_json_response(oss.str());
    }

    return create_error_response("Not Found", 404);
}

// ========== 评估 API ==========

std::string Server::handle_eval_datasets_list() {
    std::ostringstream oss;
    oss << "{\"datasets\": [], \"total\": 0}";
    return create_json_response(oss.str());
}

std::string Server::handle_eval_datasets_create(const std::string& body) {
    std::string name = extract_json_string(body, "name");
    if (name.empty()) {
        return create_error_response("Missing 'name' field", 400);
    }

    std::string dataset_id = "ds_" + std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    std::ostringstream oss;
    oss << "{";
    oss << "\"id\": \"" << dataset_id << "\",";
    oss << "\"name\": \"" << json_escape(name) << "\",";
    oss << "\"created_at\": \"" << std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() << "\"";
    oss << "}";
    return create_json_response(oss.str(), 201);
}

std::string Server::handle_eval_runs_list(const std::string& query) {
    auto params = parse_query_params(query);
    int limit = params.count("limit") ? std::stoi(params["limit"]) : 100;

    auto runs = impl_->eval_store->list_runs(limit);

    std::ostringstream oss;
    oss << "{\"runs\": [";
    for (size_t i = 0; i < runs.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "{";
        oss << "\"run_id\": \"" << json_escape(runs[i].run_id) << "\",";
        oss << "\"dataset_id\": \"" << json_escape(runs[i].dataset_id) << "\",";
        oss << "\"pipeline_type\": \"" << json_escape(runs[i].pipeline_type) << "\",";
        oss << "\"timestamp\": " << runs[i].timestamp << ",";
        oss << "\"author\": \"" << json_escape(runs[i].author) << "\"";
        oss << "}";
    }
    oss << "], \"total\": " << runs.size() << "}";
    return create_json_response(oss.str());
}

std::string Server::handle_eval_runs_create(const std::string& body) {
    std::string dataset_id = extract_json_string(body, "dataset_id");
    std::string pipeline_type = extract_json_string(body, "pipeline_type", "naive");
    std::string author = extract_json_string(body, "author", "system");

    if (dataset_id.empty()) {
        return create_error_response("Missing 'dataset_id' field", 400);
    }

    mmrag::eval::EvalRunRecord record;
    record.run_id = mmrag::eval::EvalRunStore::generate_run_id();
    record.dataset_id = dataset_id;
    record.pipeline_type = pipeline_type;
    record.author = author;
    record.timestamp = mmrag::current_timestamp_ms();
    record.description = "Evaluation run for " + pipeline_type;

    // 设置默认 summary
    record.summary.run_id = record.run_id;
    record.summary.dataset_id = dataset_id;
    record.summary.pipeline_type = pipeline_type;
    record.summary.timestamp = record.timestamp;

    bool saved = impl_->eval_store->save_run(record);

    if (!saved) {
        return create_error_response("Failed to save evaluation run", 500);
    }

    std::ostringstream oss;
    oss << "{";
    oss << "\"run_id\": \"" << record.run_id << "\",";
    oss << "\"status\": \"completed\",";
    oss << "\"pipeline_type\": \"" << pipeline_type << "\"";
    oss << "}";
    return create_json_response(oss.str(), 201);
}

std::string Server::handle_eval_compare(const std::string& query) {
    auto params = parse_query_params(query);
    if (!params.count("run_a") || !params.count("run_b")) {
        return create_error_response("Missing 'run_a' or 'run_b' query parameters", 400);
    }

    std::string run_a = params["run_a"];
    std::string run_b = params["run_b"];

    auto result = impl_->eval_store->compare_runs(run_a, run_b);

    std::ostringstream oss;
    oss << "{";
    oss << "\"comparison_id\": \"" << result.comparison_id << "\",";
    oss << "\"run_ids\": [\"" << run_a << "\", \"" << run_b << "\"],";
    oss << "\"metric_diffs\": [";

    for (size_t i = 0; i < result.metric_diffs.size(); ++i) {
        if (i > 0) oss << ",";
        const auto& diff = result.metric_diffs[i];
        oss << "{";
        oss << "\"metric_name\": \"" << diff.metric_name << "\",";
        oss << "\"value_a\": " << diff.value_a << ",";
        oss << "\"value_b\": " << diff.value_b << ",";
        oss << "\"delta\": " << diff.delta << ",";
        oss << "\"percent_change\": " << diff.percent_change << ",";
        oss << "\"direction\": \"" << diff.direction << "\"";
        oss << "}";
    }

    oss << "],";
    oss << "\"timestamp\": " << result.timestamp;
    oss << "}";
    return create_json_response(oss.str());
}

std::string Server::handle_eval_trends(const std::string& query) {
    auto params = parse_query_params(query);
    std::string pipeline_type = params.count("pipeline_type") ? params["pipeline_type"] : "naive";
    std::string metric_name = params.count("metric") ? params["metric"] : "mrr";
    int limit = params.count("limit") ? std::stoi(params["limit"]) : 30;

    auto trend = impl_->eval_store->get_metric_trend(pipeline_type, metric_name, limit);

    std::ostringstream oss;
    oss << "{";
    oss << "\"pipeline_type\": \"" << pipeline_type << "\",";
    oss << "\"metric_name\": \"" << metric_name << "\",";
    oss << "\"data\": [";

    for (size_t i = 0; i < trend.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "{\"timestamp\": " << trend[i].first << ", \"value\": " << trend[i].second << "}";
    }

    oss << "],";
    oss << "\"total\": " << trend.size();
    oss << "}";
    return create_json_response(oss.str());
}

std::string Server::handle_eval_optimization_suggestions(const std::string& query) {
    auto params = parse_query_params(query);
    std::string pipeline_type = params.count("pipeline_type") ? params["pipeline_type"] : "naive";

    auto runs = impl_->eval_store->list_runs_by_pipeline(pipeline_type, 10);

    std::ostringstream oss;
    oss << "{";
    oss << "\"pipeline_type\": \"" << pipeline_type << "\",";
    oss << "\"suggestions\": [";

    if (runs.empty()) {
        oss << "\"No evaluation data available. Run evaluations first.\"";
    } else {
        const auto& latest = runs[0];
        if (latest.summary.avg_precision_at_5 < 0.5) {
            oss << "\"Consider increasing top_k to improve recall\",";
        }
        if (latest.summary.avg_hallucination_rate > 0.2) {
            oss << "\"High hallucination rate detected. Review retrieval quality.\",";
        }
        if (latest.summary.avg_latency_ms > 1000) {
            oss << "\"High latency detected. Consider optimizing embedding or retrieval.\",";
        }
        // Remove trailing comma
        std::string s = oss.str();
        if (s.back() == ',') s.pop_back();
        oss.str("");
        oss << s;
    }

    oss << "]";
    oss << "}";
    return create_json_response(oss.str());
}

std::string Server::handle_query_stream(const std::string& body) {
    if (!engine_) {
        return create_error_response("Engine not initialized", 500);
    }

    std::string query = extract_json_string(body, "query");
    int top_k = extract_json_int(body, "top_k", 5);

    if (query.empty()) {
        return create_error_response("Missing query parameter", 400);
    }

    // Simulate streaming by returning a complete response with SSE headers
    auto result = engine_->query(query, top_k);

    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n";
    oss << "Content-Type: text/event-stream\r\n";
    oss << "Cache-Control: no-cache\r\n";
    oss << "Connection: keep-alive\r\n";
    oss << "Access-Control-Allow-Origin: *\r\n";
    oss << "\r\n";

    // Send the answer as SSE events
    std::string answer = result.answer;
    size_t chunk_size = 20;  // Characters per chunk

    for (size_t i = 0; i < answer.size(); i += chunk_size) {
        std::string chunk = answer.substr(i, std::min(chunk_size, answer.size() - i));
        oss << "data: " << json_escape(chunk) << "\n\n";
    }

    // Send completion event
    oss << "data: [DONE]\n\n";

    return oss.str();
}

// ========== 工厂函数 ==========

std::unique_ptr<Server> create_server() {
    return std::make_unique<Server>();
}

}  // namespace api
}  // namespace mmrag
