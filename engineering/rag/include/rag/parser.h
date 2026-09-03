/**
 * @file parser.h
 * @brief 文档解析器 - 接口和实现
 */
#pragma once

#include "rag/types.h"
#include "rag/error.h"
#include <string>
#include <vector>
#include <memory>
#include <filesystem>

namespace rag {

// ========== 前向声明 ==========

class Parser;
class ParsedDocument;

// ========== 解析结果 ==========

/**
 * @brief 解析结果
 */
struct ParseResult {
    std::string content;            // 解析后的文本内容
    std::string title;              // 提取的标题
    std::vector<std::string> metadata_keys;  // 元数据键
    std::vector<std::string> metadata_values;  // 元数据值
    int64_t processing_time_ms;     // 处理时间
};

// ========== 解析器接口 ==========

/**
 * @brief 文档解析器接口
 */
class Parser {
public:
    virtual ~Parser() = default;

    // 解析文件
    virtual ParseResult parse(const std::filesystem::path& file_path) = 0;

    // 解析字符串内容
    virtual ParseResult parse_content(const std::string& content,
                                      const std::string& source = "") = 0;

    // 获取支持的文件扩展名
    virtual std::vector<std::string> supported_extensions() const = 0;

    // 获取解析器名称
    virtual std::string name() const = 0;

    // 是否支持该文件
    bool can_parse(const std::filesystem::path& file_path) const;
};

// ========== Markdown 解析器 ==========

/**
 * @brief Markdown 解析器
 *
 * 提取 Markdown 文本内容，处理代码块、标题等
 */
class MarkdownParser : public Parser {
public:
    MarkdownParser();

    ParseResult parse(const std::filesystem::path& file_path) override;
    ParseResult parse_content(const std::string& content,
                              const std::string& source = "") override;

    std::vector<std::string> supported_extensions() const override {
        return {".md", ".markdown"};
    }

    std::string name() const override { return "markdown"; }

private:
    // 处理标题
    std::string extract_title(const std::string& content);

    // 清理内容
    std::string clean_content(const std::string& content);

    // 移除代码块
    std::string remove_code_blocks(const std::string& content);

    // 提取链接
    std::vector<std::string> extract_links(const std::string& content);
};

// ========== 代码解析器 ==========

/**
 * @brief 代码解析器
 *
 * 支持 C/C++、Python、Java 等语言的简单解析
 */
class CodeParser : public Parser {
public:
    explicit CodeParser(const std::string& language = "");

    ParseResult parse(const std::filesystem::path& file_path) override;
    ParseResult parse_content(const std::string& content,
                              const std::string& source = "") override;

    std::vector<std::string> supported_extensions() const override;
    std::string name() const override { return "code"; }

    // 设置语言
    void set_language(const std::string& language);

private:
    // 检测语言
    std::string detect_language(const std::filesystem::path& file_path);

    // 提取注释
    std::string extract_comments(const std::string& content);

    // 提取函数签名
    std::vector<std::string> extract_functions(const std::string& content);

    // 提取文档字符串
    std::string extract_docstrings(const std::string& content);

    std::string language_;
};

// ========== 纯文本解析器 ==========

/**
 * @brief 纯文本解析器
 */
class TextParser : public Parser {
public:
    TextParser();

    ParseResult parse(const std::filesystem::path& file_path) override;
    ParseResult parse_content(const std::string& content,
                              const std::string& source = "") override;

    std::vector<std::string> supported_extensions() const override {
        return {".txt", ".text"};
    }

    std::string name() const override { return "text"; }
};

// ========== 解析器注册表 ==========

/**
 * @brief 解析器注册表
 */
class ParserRegistry {
public:
    ParserRegistry();

    // 注册解析器
    void register_parser(std::unique_ptr<Parser> parser);

    // 获取解析器
    Parser* get_parser(const std::filesystem::path& file_path);

    // 获取所有解析器
    const std::vector<std::unique_ptr<Parser>>& parsers() const { return parsers_; }

    // 获取默认解析器
    Parser* default_parser() const { return default_parser_.get(); }

private:
    std::vector<std::unique_ptr<Parser>> parsers_;
    std::unique_ptr<Parser> default_parser_;
};

// ========== 工具函数 ==========

/**
 * @brief 从文件路径检测文件类型
 */
std::string detect_file_type(const std::filesystem::path& file_path);

/**
 * @brief 获取文件元数据
 */
DocumentMetadata get_file_metadata(const std::filesystem::path& file_path);

}  // namespace rag
