/**
 * @file main.cpp
 * @brief Modular RAG CLI 工具
 *
 * 命令行接口，用于:
 * - RAG 查询
 * - Pipeline 列表
 * - 启动 API 服务器
 */
#include "rag/modular/api/server.h"
#include "rag/modular/types.h"
#include "rag/modular/config.h"
#include "rag/modular/pipeline/pipeline_factory.h"
#include "rag/logger.h"
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <cstring>

namespace rag::modular {

// ========== CLI 配置 ==========

/**
 * @brief CLI 配置
 */
struct CLIConfig {
    std::string command;                        // 命令
    std::string query;                          // 查询文本
    std::string pipeline;                       // Pipeline 类型
    int port = 8080;                            // 服务器端口
    bool verbose = false;                       // 详细输出
};

// ========== 命令处理 ==========

/**
 * @brief 解析命令行参数
 * @param argc 参数数量
 * @param argv 参数列表
 * @return CLI 配置
 */
CLIConfig parse_arguments(int argc, char* argv[]) {
    CLIConfig config;

    if (argc < 2) {
        config.command = "help";
        return config;
    }

    config.command = argv[1];

    // 解析子命令参数
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];

        // 查询文本
        if (arg[0] != '-' && config.query.empty()) {
            config.query = arg;
        }
        // Pipeline 选项
        else if (arg.find("--pipeline=") == 0) {
            config.pipeline = arg.substr(11);
        }
        // 端口选项
        else if (arg.find("--port=") == 0) {
            config.port = std::stoi(arg.substr(7));
        }
        // 详细输出
        else if (arg == "-v" || arg == "--verbose") {
            config.verbose = true;
        }
        // 帮助
        else if (arg == "-h" || arg == "--help") {
            config.command = "help";
        }
    }

    return config;
}

/**
 * @brief 打印帮助信息
 */
void print_help() {
    std::cout << R"(
Modular RAG CLI 工具
====================

用法: modular_rag <命令> [选项]

命令:
  query <问题> [--pipeline=xxx]    执行 RAG 查询
  list-pipelines                   列出所有可用的 Pipeline
  server [--port=8080]             启动 API 服务器
  help                             显示帮助信息

选项:
  --pipeline=xxx    指定使用的 Pipeline 类型 (默认: advanced)
                    可选值: naive, advanced, hybrid, hyde, graph,
                            corrective, react, iterative, recursive
  --port=8080       指定服务器端口 (默认: 8080)
  -v, --verbose     详细输出模式

示例:
  modular_rag query "什么是机器学习"
  modular_rag query "什么是深度学习" --pipeline=hyde
  modular_rag list-pipelines
  modular_rag server --port=8080
)";
}

/**
 * @brief 处理 query 命令
 */
int handle_query(const CLIConfig& config) {
    if (config.query.empty()) {
        std::cerr << "错误: 请提供查询文本" << std::endl;
        std::cout << "用法: modular_rag query <问题> [--pipeline=xxx]" << std::endl;
        return 1;
    }

    // 初始化配置
    ModularConfig modular_config;
    modular_config.default_pipeline = PipelineType::ADVANCED;

    // 根据命令行参数选择 Pipeline
    PipelineType pipeline_type = modular_config.default_pipeline;
    if (!config.pipeline.empty()) {
        pipeline_type = string_to_pipeline_type(config.pipeline);
    }

    // 创建 Pipeline
    auto pipeline = PipelineFactory::create(pipeline_type, modular_config);
    if (!pipeline) {
        std::cerr << "错误: 无法创建 Pipeline: "
                  << (config.pipeline.empty() ? "advanced" : config.pipeline)
                  << std::endl;
        return 1;
    }

    // 初始化 Pipeline
    if (!pipeline->init(modular_config)) {
        std::cerr << "错误: Pipeline 初始化失败" << std::endl;
        return 1;
    }

    if (config.verbose) {
        std::cout << "使用 Pipeline: " << pipeline->name() << std::endl;
        std::cout << "查询: " << config.query << std::endl;
        std::cout << "---" << std::endl;
    }

    // 执行查询
    ModularQuery query;
    query.text = config.query;
    query.pipeline_type = pipeline_type;

    auto result = pipeline->query(query);

    if (result.success) {
        std::cout << "\n回答:\n" << result.answer << std::endl;

        if (config.verbose) {
            std::cout << "\n--- 统计信息 ---" << std::endl;
            std::cout << "检索耗时: " << result.retrieval_time_ms << " ms" << std::endl;
            std::cout << "生成耗时: " << result.generation_time_ms << " ms" << std::endl;
            std::cout << "总耗时: " << result.total_time_ms << " ms" << std::endl;
            std::cout << "Token 数: " << result.total_tokens << std::endl;

            if (!result.context.empty()) {
                std::cout << "\n--- 检索到的上下文 ---" << std::endl;
                for (size_t i = 0; i < result.context.size(); i++) {
                    const auto& ctx = result.context[i];
                    std::cout << "[" << (i + 1) << "] "
                              << ctx.chunk.metadata.file_path
                              << " (分数: " << ctx.score << ")"
                              << std::endl;
                    // 截断显示
                    std::string content = ctx.chunk.content;
                    if (content.length() > 200) {
                        content = content.substr(0, 200) + "...";
                    }
                    std::cout << content << std::endl;
                }
            }
        }
    } else {
        std::cerr << "查询失败: " << result.error_message << std::endl;
        return 1;
    }

    return 0;
}

/**
 * @brief 处理 list-pipelines 命令
 */
int handle_list_pipelines() {
    std::cout << "可用 Pipeline 列表:\n" << std::endl;

    auto types = list_pipeline_types();
    for (const auto& type_name : types) {
        auto type = string_to_pipeline_type(type_name);
        std::cout << "  " << type_name << std::endl;
        std::cout << "    " << PipelineFactory::get_description(type) << std::endl;
        std::cout << std::endl;
    }

    return 0;
}

/**
 * @brief 处理 server 命令
 */
int handle_server(const CLIConfig& config) {
    std::cout << "启动 Modular RAG API 服务器..." << std::endl;
    std::cout << "端口: " << config.port << std::endl;
    std::cout << "端点:" << std::endl;
    std::cout << "  POST /api/v1/query     - RAG 查询" << std::endl;
    std::cout << "  GET  /api/v1/status    - 系统状态" << std::endl;
    std::cout << "  GET  /api/v1/pipelines - Pipeline 列表" << std::endl;
    std::cout << "  GET  /api/v1/health    - 健康检查" << std::endl;
    std::cout << "\n按 Ctrl+C 停止服务器\n" << std::endl;

    // 创建并初始化服务器
    auto server = create_api_server();
    if (!server) {
        std::cerr << "错误: 无法创建服务器" << std::endl;
        return 1;
    }

    ServerConfig server_config;
    server_config.port = config.port;

    if (!server->init(server_config)) {
        std::cerr << "错误: 服务器初始化失败" << std::endl;
        return 1;
    }

    if (!server->start(config.port)) {
        std::cerr << "错误: 服务器启动失败" << std::endl;
        return 1;
    }

    // 等待服务器停止
    while (server->is_running()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}

}  // namespace rag::modular

// ========== 主函数 ==========

int main(int argc, char* argv[]) {
    using namespace rag::modular;

    // 记录日志
    RAG_INFO("Modular RAG CLI 启动");

    // 解析参数
    auto config = parse_arguments(argc, argv);

    // 执行命令
    int result = 0;

    if (config.command == "query") {
        result = handle_query(config);
    }
    else if (config.command == "list-pipelines") {
        result = handle_list_pipelines();
    }
    else if (config.command == "server") {
        result = handle_server(config);
    }
    else if (config.command == "help") {
        print_help();
    }
    else {
        std::cerr << "未知命令: " << config.command << std::endl;
        print_help();
        result = 1;
    }

    RAG_INFO("Modular RAG CLI 退出");
    return result;
}
