/**
 * @file cli.h
 * @brief RAG CLI 命令行工具
 */
#pragma once

#include "rag/engine.h"
#include <string>
#include <vector>
#include <functional>

namespace rag {

// ========== CLI 命令 ==========

/**
 * @brief CLI 命令类型
 */
enum class CLICommand {
    INDEX,
    QUERY,
    DOC,
    CONFIG,
    SERVER,
    HELP,
    VERSION
};

// ========== CLI 选项 ==========

/**
 * @brief CLI 选项
 */
struct CLIOptions {
    CLICommand command = CLICommand::HELP;
    std::vector<std::string> args;
    std::string config_path;
    bool verbose = false;
    int top_k = 5;
};

// ========== CLI 应用 ==========

/**
 * @brief CLI 应用
 */
class CLIApp {
public:
    CLIApp();
    ~CLIApp();

    // 解析命令行参数
    bool parse_args(int argc, char* argv[]);

    // 执行命令
    int run();

    // 获取选项
    const CLIOptions& options() const { return options_; }

private:
    // 帮助信息
    void print_help();
    void print_version();

    // 命令处理
    int handle_index();
    int handle_query();
    int handle_doc();
    int handle_config();
    int handle_server();

    // 工具函数
    std::string read_line(const std::string& prompt);

    CLIOptions options_;
    std::unique_ptr<RAGEngine> engine_;
};

// ========== 命令行解析 ==========

/**
 * @brief 解析命令行参数
 */
CLIOptions parse_arguments(int argc, char* argv[]);

/**
 * @brief 显示用法
 */
void show_usage(const char* program_name);

}  // namespace rag
