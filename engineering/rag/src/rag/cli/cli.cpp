/**
 * @file cli.cpp
 * @brief CLI 命令行工具实现
 */

#include "rag/cli.h"
#include "rag/logger.h"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace rag {

// ========== CLIApp 实现 ==========

CLIApp::CLIApp() = default;

CLIApp::~CLIApp() = default;

bool CLIApp::parse_args(int argc, char* argv[]) {
    options_ = parse_arguments(argc, argv);
    return true;
}

int CLIApp::run() {
    switch (options_.command) {
        case CLICommand::INDEX:
            return handle_index();

        case CLICommand::QUERY:
            return handle_query();

        case CLICommand::DOC:
            return handle_doc();

        case CLICommand::CONFIG:
            return handle_config();

        case CLICommand::SERVER:
            return handle_server();

        case CLICommand::VERSION:
            print_version();
            return 0;

        case CLICommand::HELP:
        default:
            print_help();
            return 0;
    }
}

void CLIApp::print_help() {
    std::cout << R"(
RAG 命令行工具 v1.0.0

用法: rag_cli <命令> [选项]

命令:
  index [路径...]    构建索引
  query [问题]       查询
  doc                文档管理
  config             配置管理
  server             启动 REST API 服务器
  help               显示帮助
  version            显示版本

索引命令选项:
  rag_cli index                  构建所有配置的路径
  rag_cli index ./docs           构建指定路径
  rag_cli index --rebuild        重建索引

查询命令选项:
  rag_cli query                  交互式查询
  rag_cli query "问题"           直接查询
  rag_cli query "问题" --top-k 3 设置返回数量

文档命令:
  rag_cli doc list               列出所有文档
  rag_cli doc info <id>          显示文档详情
  rag_cli doc delete <id>        删除文档

配置命令:
  rag_cli config show            显示当前配置
  rag_cli config save <path>     保存配置

示例:
  rag_cli index ./docs
  rag_cli query "RAG 系统如何构建索引？"
  rag_cli doc list
  rag_cli server --port 8080
)";
}

void CLIApp::print_version() {
    std::cout << "RAG CLI v1.0.0\n";
    std::cout << "Build: " << __DATE__ << " " << __TIME__ << "\n";
}

int CLIApp::handle_index() {
    std::cout << "Initializing RAG Engine...\n";

    EngineConfig config;
    if (!options_.config_path.empty()) {
        ConfigLoader loader;
        config.config = loader.load(options_.config_path);
    } else {
        config.config = Config::default_config();
    }

    engine_ = create_engine(config);

    std::cout << "Building index...\n";
    auto result = engine_->build_index(options_.args);

    std::cout << "\n=== Build Complete ===\n";
    std::cout << "Documents processed: " << result.documents_processed << "\n";
    std::cout << "Documents failed: " << result.documents_failed << "\n";
    std::cout << "Chunks created: " << result.chunks_created << "\n";
    std::cout << "Duration: " << result.duration_ms << "ms\n";

    return result.success ? 0 : 1;
}

int CLIApp::handle_query() {
    std::cout << "Initializing RAG Engine...\n";

    EngineConfig config;
    if (!options_.config_path.empty()) {
        ConfigLoader loader;
        config.config = loader.load(options_.config_path);
    } else {
        config.config = Config::default_config();
    }

    engine_ = create_engine(config);

    // 检查是否有直接的问题参数
    if (!options_.args.empty()) {
        std::string query = options_.args[0];
        std::cout << "\nQuery: " << query << "\n\n";

        auto result = engine_->query(query, options_.top_k);

        std::cout << "=== Answer ===\n";
        std::cout << result.answer << "\n\n";

        std::cout << "=== Sources ===\n";
        for (size_t i = 0; i < result.chunks.size(); ++i) {
            const auto& chunk = result.chunks[i].chunk;
            std::cout << "[" << (i + 1) << "] " << chunk.metadata.file_name;
            std::cout << " (score: " << std::fixed << std::setprecision(3)
                      << result.chunks[i].score << ")\n";
            std::cout << "    " << chunk.content.substr(0, 100) << "...\n\n";
        }

        std::cout << "Query time: " << result.query_time_ms << "ms\n";
        return 0;
    }

    // 交互式查询
    std::cout << "\nEnter your question (or 'quit' to exit):\n\n";

    while (true) {
        std::cout << "> ";
        std::string query;
        std::getline(std::cin, query);

        if (query == "quit" || query == "exit" || query.empty()) {
            std::cout << "Goodbye!\n";
            break;
        }

        auto result = engine_->query(query, options_.top_k);

        std::cout << "\n=== Answer ===\n";
        std::cout << result.answer << "\n\n";

        std::cout << "=== Sources ===\n";
        for (size_t i = 0; i < result.chunks.size(); ++i) {
            const auto& chunk = result.chunks[i].chunk;
            std::cout << "[" << (i + 1) << "] " << chunk.metadata.file_name << "\n";
        }

        std::cout << "\nQuery time: " << result.query_time_ms << "ms\n\n";
    }

    return 0;
}

int CLIApp::handle_doc() {
    if (options_.args.empty() || options_.args[0] == "list") {
        EngineConfig config;
        config.config = Config::default_config();
        engine_ = create_engine(config);

        auto docs = engine_->list_documents();

        std::cout << "\n=== Documents (" << docs.size() << ") ===\n";
        for (const auto& doc : docs) {
            std::cout << doc.id << " | " << doc.metadata.file_name;
            std::cout << " | " << (doc.status == Document::Status::INDEXED ? "indexed" : "pending");
            std::cout << "\n";
        }
        return 0;
    }

    if (options_.args[0] == "info" && options_.args.size() > 1) {
        EngineConfig config;
        config.config = Config::default_config();
        engine_ = create_engine(config);

        auto doc = engine_->get_document(options_.args[1]);
        if (doc) {
            std::cout << "\n=== Document Info ===\n";
            std::cout << "ID: " << doc->id << "\n";
            std::cout << "File: " << doc->metadata.file_path << "\n";
            std::cout << "Size: " << doc->metadata.file_size << " bytes\n";
            std::cout << "Status: " << (doc->status == Document::Status::INDEXED ? "indexed" : "pending") << "\n";
            std::cout << "Content preview: " << doc->content.substr(0, 200) << "...\n";
        } else {
            std::cout << "Document not found: " << options_.args[1] << "\n";
            return 1;
        }
        return 0;
    }

    if (options_.args[0] == "delete" && options_.args.size() > 1) {
        EngineConfig config;
        config.config = Config::default_config();
        engine_ = create_engine(config);

        engine_->remove_document(options_.args[1]);
        std::cout << "Document deleted: " << options_.args[1] << "\n";
        return 0;
    }

    std::cout << "Unknown doc command. Use 'rag_cli doc list|info|delete'\n";
    return 1;
}

int CLIApp::handle_config() {
    if (options_.args.empty() || options_.args[0] == "show") {
        auto config = Config::default_config();
        std::cout << config.to_string();
        return 0;
    }

    if (options_.args[0] == "save" && options_.args.size() > 1) {
        ConfigLoader loader;
        auto config = Config::default_config();
        loader.save(config, options_.args[1]);
        std::cout << "Config saved to: " << options_.args[1] << "\n";
        return 0;
    }

    std::cout << "Unknown config command. Use 'rag_cli config show|save'\n";
    return 1;
}

int CLIApp::handle_server() {
    std::cout << "Starting REST API server...\n";
    std::cout << "(Server implementation not included in this version)\n";
    return 0;
}

// ========== 命令行解析 ==========

static std::string get_option(int& i, int argc, char* argv[]) {
    if (i + 1 < argc && argv[i + 1][0] != '-') {
        return argv[++i];
    }
    return "";
}

CLIOptions parse_arguments(int argc, char* argv[]) {
    CLIOptions options;

    if (argc < 2) {
        options.command = CLICommand::HELP;
        return options;
    }

    std::string cmd = argv[1];

    if (cmd == "index") {
        options.command = CLICommand::INDEX;
        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg[0] == '-') {
                if (arg == "--rebuild" || arg == "-r") {
                    // 重建标志
                } else if (arg == "--config" || arg == "-c") {
                    options.config_path = get_option(i, argc, argv);
                }
            } else {
                options.args.push_back(arg);
            }
        }
    } else if (cmd == "query") {
        options.command = CLICommand::QUERY;
        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg[0] == '-') {
                if (arg == "--top-k" || arg == "-k") {
                    options.top_k = std::stoi(get_option(i, argc, argv));
                } else if (arg == "--config" || arg == "-c") {
                    options.config_path = get_option(i, argc, argv);
                }
            } else {
                options.args.push_back(arg);
            }
        }
    } else if (cmd == "doc") {
        options.command = CLICommand::DOC;
        for (int i = 2; i < argc; ++i) {
            options.args.push_back(argv[i]);
        }
    } else if (cmd == "config") {
        options.command = CLICommand::CONFIG;
        for (int i = 2; i < argc; ++i) {
            options.args.push_back(argv[i]);
        }
    } else if (cmd == "server") {
        options.command = CLICommand::SERVER;
    } else if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        options.command = CLICommand::HELP;
    } else if (cmd == "version" || cmd == "--version" || cmd == "-v") {
        options.command = CLICommand::VERSION;
    } else {
        std::cerr << "Unknown command: " << cmd << "\n";
        options.command = CLICommand::HELP;
    }

    return options;
}

void show_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " <command> [options]\n";
    std::cerr << "Run '" << program_name << " help' for more information.\n";
}

}  // namespace rag

// ========== 主函数 ==========

int main(int argc, char* argv[]) {
    rag::CLIApp app;

    if (!app.parse_args(argc, argv)) {
        rag::show_usage(argv[0]);
        return 1;
    }

    return app.run();
}
