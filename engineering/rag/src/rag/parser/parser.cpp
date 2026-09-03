/**
 * @file parser.cpp
 * @brief 解析器实现
 */

#include "rag/parser.h"
#include "rag/logger.h"
#include "rag/error.h"
#include <fstream>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <regex>

namespace rag {

// ========== 辅助函数 ==========

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

static bool starts_with(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() &&
           s.substr(0, prefix.size()) == prefix;
}

// ========== Parser 实现 ==========

bool Parser::can_parse(const std::filesystem::path& file_path) const {
    auto ext = file_path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    auto supported = supported_extensions();
    for (const auto& e : supported) {
        std::string se = e;
        std::transform(se.begin(), se.end(), se.begin(), ::tolower);
        if (ext == se) return true;
    }
    return false;
}

// ========== MarkdownParser 实现 ==========

MarkdownParser::MarkdownParser() = default;

ParseResult MarkdownParser::parse(const std::filesystem::path& file_path) {
    auto start = std::chrono::steady_clock::now();

    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw DocumentException(ErrorCodes::DOCUMENT_NOT_FOUND,
                               "Cannot open file: " + file_path.string());
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    auto result = parse_content(content, file_path.string());

    auto end = std::chrono::steady_clock::now();
    result.processing_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    RAG_DEBUG("MarkdownParser: parsed " + file_path.string());
    return result;
}

ParseResult MarkdownParser::parse_content(const std::string& content,
                                          const std::string& source) {
    ParseResult result;

    // 提取标题
    result.title = extract_title(content);

    // 清理内容
    result.content = clean_content(content);

    // 提取链接作为元数据
    auto links = extract_links(content);
    result.metadata_keys.reserve(links.size());
    result.metadata_values.reserve(links.size());
    for (size_t i = 0; i < links.size() && i < 10; ++i) {  // 限制元数据数量
        result.metadata_keys.push_back("link_" + std::to_string(i));
        result.metadata_values.push_back(links[i]);
    }

    return result;
}

std::string MarkdownParser::extract_title(const std::string& content) {
    std::istringstream iss(content);
    std::string line;

    while (std::getline(iss, line)) {
        line = trim(line);
        if (line.empty()) continue;

        // 第一个标题行
        if (starts_with(line, "# ")) {
            return line.substr(2);
        }

        // 第一个加粗文本
        if (starts_with(line, "**") && line.find("**", 2) != std::string::npos) {
            size_t start = 2;
            size_t end = line.find("**", start);
            return line.substr(start, end - start);
        }

        break;
    }

    return "";
}

std::string MarkdownParser::clean_content(const std::string& content) {
    std::string result;
    result.reserve(content.size());

    bool in_code_block = false;
    std::istringstream iss(content);
    std::string line;

    while (std::getline(iss, line)) {
        // 跟踪代码块状态
        if (starts_with(trim(line), "```")) {
            in_code_block = !in_code_block;
            continue;
        }

        // 移除 HTML 标签
        std::string cleaned = line;
        size_t pos;
        while ((pos = cleaned.find('<')) != std::string::npos) {
            size_t end = cleaned.find('>', pos);
            if (end != std::string::npos) {
                cleaned.erase(pos, end - pos + 1);
            } else {
                break;
            }
        }

        // 移除 Markdown 图片
        while ((pos = cleaned.find("![")) != std::string::npos) {
            size_t end = cleaned.find(']', pos);
            if (end != std::string::npos) {
                cleaned.erase(pos, end - pos + 1);
            } else {
                break;
            }
        }

        // 处理链接 [text](url) -> text
        while ((pos = cleaned.find("](")) != std::string::npos) {
            size_t start = cleaned.rfind('[', pos);
            size_t end = cleaned.find(')', pos);
            if (start != std::string::npos && end != std::string::npos) {
                cleaned = cleaned.substr(0, start) +
                         cleaned.substr(start + 1, pos - start - 1) +
                         cleaned.substr(end + 1);
            } else {
                break;
            }
        }

        // 移除 Markdown 标题标记
        if (starts_with(trim(cleaned), "#")) {
            size_t pos = cleaned.find_first_not_of("# \t");
            if (pos != std::string::npos) {
                cleaned = cleaned.substr(pos);
            }
        }

        // 移除加粗/斜体标记
        while ((pos = cleaned.find("**")) != std::string::npos) {
            cleaned.erase(pos, 2);
        }
        while ((pos = cleaned.find("__")) != std::string::npos) {
            cleaned.erase(pos, 2);
        }

        result += cleaned + "\n";
    }

    return trim(result);
}

std::string MarkdownParser::remove_code_blocks(const std::string& content) {
    std::string result;
    bool in_code_block = false;
    std::istringstream iss(content);
    std::string line;

    while (std::getline(iss, line)) {
        if (starts_with(trim(line), "```")) {
            in_code_block = !in_code_block;
            continue;
        }
        if (!in_code_block) {
            result += line + "\n";
        }
    }

    return result;
}

std::vector<std::string> MarkdownParser::extract_links(const std::string& content) {
    std::vector<std::string> links;
    size_t pos = 0;

    while ((pos = content.find("](", pos)) != std::string::npos) {
        size_t start = pos + 2;
        size_t end = content.find(')', start);
        if (end != std::string::npos) {
            links.push_back(content.substr(start, end - start));
            pos = end;
        } else {
            break;
        }
    }

    return links;
}

// ========== CodeParser 实现 ==========

CodeParser::CodeParser(const std::string& language) : language_(language) {}

ParseResult CodeParser::parse(const std::filesystem::path& file_path) {
    auto start = std::chrono::steady_clock::now();

    if (language_.empty()) {
        language_ = detect_language(file_path);
    }

    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw DocumentException(ErrorCodes::DOCUMENT_NOT_FOUND,
                               "Cannot open file: " + file_path.string());
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    auto result = parse_content(content, file_path.string());

    auto end = std::chrono::steady_clock::now();
    result.processing_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    RAG_DEBUG("CodeParser: parsed " + file_path.string() + " as " + language_);
    return result;
}

ParseResult CodeParser::parse_content(const std::string& content,
                                      const std::string& source) {
    ParseResult result;

    // 提取文档字符串和注释作为可读文本
    result.content = extract_comments(content);
    if (result.content.empty()) {
        result.content = content;
    }

    // 提取函数名作为元数据
    auto functions = extract_functions(content);
    result.metadata_keys.reserve(functions.size());
    result.metadata_values.reserve(functions.size());
    for (size_t i = 0; i < functions.size() && i < 20; ++i) {
        result.metadata_keys.push_back("function_" + std::to_string(i));
        result.metadata_values.push_back(functions[i]);
    }

    // 提取文档字符串
    result.title = extract_docstrings(content);

    return result;
}

std::vector<std::string> CodeParser::supported_extensions() const {
    if (!language_.empty()) {
        if (language_ == "cpp" || language_ == "c") {
            return {".cpp", ".hpp", ".h", ".cxx", ".cc", ".c"};
        } else if (language_ == "python") {
            return {".py", ".pyw"};
        } else if (language_ == "java") {
            return {".java"};
        } else if (language_ == "javascript") {
            return {".js", ".jsx", ".mjs"};
        } else if (language_ == "typescript") {
            return {".ts", ".tsx"};
        }
    }

    // 默认返回 C/C++ 扩展名
    return {".cpp", ".hpp", ".h", ".cxx", ".cc", ".c", ".py", ".java", ".js", ".ts"};
}

std::string CodeParser::detect_language(const std::filesystem::path& file_path) {
    auto ext = file_path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".c" || ext == ".h") return "c";
    if (ext == ".cpp" || ext == ".hpp" || ext == ".cxx" || ext == ".cc") return "cpp";
    if (ext == ".py" || ext == ".pyw") return "python";
    if (ext == ".java") return "java";
    if (ext == ".js" || ext == ".jsx" || ext == ".mjs") return "javascript";
    if (ext == ".ts" || ext == ".tsx") return "typescript";

    return "cpp";  // 默认
}

std::string CodeParser::extract_comments(const std::string& content) {
    std::string result;
    std::istringstream iss(content);
    std::string line;
    bool in_string = false;
    bool in_comment = false;
    bool in_block_comment = false;

    while (std::getline(iss, line)) {
        std::string cleaned = line;

        // 移除字符串字面量（简化处理）
        size_t pos = 0;
        while ((pos = cleaned.find('"', pos)) != std::string::npos) {
            if (pos == 0 || cleaned[pos - 1] != '\\') {
                in_string = !in_string;
            }
            ++pos;
        }

        if (!in_string && !in_block_comment) {
            // 单行注释
            auto comment_pos = cleaned.find("//");
            if (comment_pos != std::string::npos) {
                std::string comment = trim(cleaned.substr(comment_pos + 2));
                if (!comment.empty() && comment != "/") {
                    result += "// " + comment + "\n";
                }
                cleaned = trim(cleaned.substr(0, comment_pos));
            }

            // 块注释开始
            auto block_start = cleaned.find("/*");
            if (block_start != std::string::npos) {
                in_block_comment = true;
                auto block_end = cleaned.find("*/", block_start + 2);
                if (block_end != std::string::npos) {
                    std::string comment = trim(cleaned.substr(block_start + 2, block_end - block_start - 2));
                    if (!comment.empty()) {
                        result += "/* " + comment + " */\n";
                    }
                    in_block_comment = false;
                    cleaned = cleaned.substr(0, block_start) + cleaned.substr(block_end + 2);
                }
            }
        }

        // 块注释结束
        if (in_block_comment) {
            auto block_end = cleaned.find("*/");
            if (block_end != std::string::npos) {
                std::string comment = trim(cleaned.substr(0, block_end));
                if (!comment.empty()) {
                    result += comment + "\n";
                }
                cleaned = cleaned.substr(block_end + 2);
                in_block_comment = false;
            } else {
                result += cleaned + "\n";
                continue;
            }
        }

        // 保留非注释的代码行（可选）
        // cleaned = trim(cleaned);
        // if (!cleaned.empty()) {
        //     result += cleaned + "\n";
        // }
    }

    return trim(result);
}

std::vector<std::string> CodeParser::extract_functions(const std::string& content) {
    std::vector<std::string> functions;
    std::regex pattern;

    if (language_ == "cpp" || language_ == "c") {
        pattern = std::regex(R"(\b(?:void|int|float|double|bool|string|auto|std::\w+)\s+\w+\s*\()");
    } else if (language_ == "python") {
        pattern = std::regex(R"(\bdef\s+(\w+)\s*\()");
    } else if (language_ == "java") {
        pattern = std::regex(R"(\b(?:public|private|protected)?\s*(?:static)?\s*\w+\s+\w+\s*\()");
    } else {
        pattern = std::regex(R"(\b\w+\s+\w+\s*\()");
    }

    std::sregex_iterator it(content.begin(), content.end(), pattern);
    std::sregex_iterator end;

    while (it != end && functions.size() < 50) {
        std::string match = (*it)[0];
        // 提取函数签名
        size_t start = match.find_first_not_of(" \t");
        size_t end_pos = match.find('(');
        if (end_pos != std::string::npos) {
            functions.push_back(trim(match.substr(0, end_pos)));
        }
        ++it;
    }

    return functions;
}

std::string CodeParser::extract_docstrings(const std::string& content) {
    // 查找第一个文档字符串
    std::vector<std::string> patterns;

    if (language_ == "python") {
        patterns = {"\"\"\"", "'''"};
    } else {
        patterns = {"/**"};
    }

    for (const auto& pattern : patterns) {
        size_t pos = content.find(pattern);
        if (pos != std::string::npos) {
            size_t end = content.find(pattern, pos + pattern.size());
            if (end != std::string::npos) {
                return trim(content.substr(pos + pattern.size(), end - pos - pattern.size()));
            }
        }
    }

    return "";
}

// ========== TextParser 实现 ==========

TextParser::TextParser() = default;

ParseResult TextParser::parse(const std::filesystem::path& file_path) {
    auto start = std::chrono::steady_clock::now();

    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw DocumentException(ErrorCodes::DOCUMENT_NOT_FOUND,
                               "Cannot open file: " + file_path.string());
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    auto result = parse_content(content, file_path.string());

    auto end = std::chrono::steady_clock::now();
    result.processing_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    return result;
}

ParseResult TextParser::parse_content(const std::string& content,
                                      const std::string& source) {
    ParseResult result;
    result.content = content;
    return result;
}

// ========== ParserRegistry 实现 ==========

ParserRegistry::ParserRegistry() {
    // 注册内置解析器
    register_parser(std::make_unique<MarkdownParser>());
    register_parser(std::make_unique<CodeParser>());
    register_parser(std::make_unique<TextParser>());

    // 默认使用文本解析器
    default_parser_ = std::make_unique<TextParser>();
}

void ParserRegistry::register_parser(std::unique_ptr<Parser> parser) {
    parsers_.push_back(std::move(parser));
}

Parser* ParserRegistry::get_parser(const std::filesystem::path& file_path) {
    for (auto& parser : parsers_) {
        if (parser->can_parse(file_path)) {
            return parser.get();
        }
    }
    return default_parser_.get();
}

// ========== 工具函数 ==========

std::string detect_file_type(const std::filesystem::path& file_path) {
    auto ext = file_path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".md" || ext == ".markdown") return "markdown";
    if (ext == ".cpp" || ext == ".hpp" || ext == ".h" || ext == ".c" ||
        ext == ".cxx" || ext == ".cc") return "cpp";
    if (ext == ".py" || ext == ".pyw") return "python";
    if (ext == ".java") return "java";
    if (ext == ".js" || ext == ".jsx") return "javascript";
    if (ext == ".ts" || ext == ".tsx") return "typescript";
    if (ext == ".txt") return "text";

    return "unknown";
}

DocumentMetadata get_file_metadata(const std::filesystem::path& file_path) {
    DocumentMetadata metadata;

    metadata.file_path = file_path.string();
    metadata.file_name = file_path.filename().string();
    metadata.file_type = detect_file_type(file_path);

    try {
        metadata.file_size = std::filesystem::file_size(file_path);
        auto ftime = std::filesystem::last_write_time(file_path);
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
            ftime.time_since_epoch()).count();
        metadata.created_time = seconds;
        metadata.modified_time = seconds;
    } catch (const std::exception& e) {
        RAG_WARN("Failed to get file metadata: " + file_path.string());
    }

    return metadata;
}

}  // namespace rag
