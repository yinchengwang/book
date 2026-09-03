/**
 * @file code_chunker.cpp
 * @brief 代码感知分块器实现
 */

#include "rag/semantic_chunker.h"
#include "rag/logger.h"
#include <algorithm>
#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <cctype>

namespace rag {

// ========== 工具函数 ==========

static std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::string current;

    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n') {
            lines.push_back(current);
            current.clear();
        } else {
            current += text[i];
        }
    }
    if (!current.empty()) {
        lines.push_back(current);
    }
    return lines;
}

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r");
    return s.substr(start, end - start + 1);
}

// ========== CodeAwareChunker 实现 ==========

CodeAwareChunker::CodeAwareChunker(const Config& config)
    : config_(config), base_config_() {
    base_config_.chunk_size = config.merge_threshold * 10;
    base_config_.chunk_overlap = 0;
    base_config_.min_chunk_size = config.min_chunk_lines * 10;
}

CodeAwareChunker::CodeAwareChunker()
    : config_(), base_config_() {
    base_config_.chunk_size = 500;
    base_config_.min_chunk_size = 50;
}

std::string CodeAwareChunker::detect_language(const std::string& text) {
    auto info = detect_language_info(text);
    return info.language;
}

CodeAwareChunker::LanguageInfo CodeAwareChunker::detect_language_info(const std::string& text) {
    LanguageInfo info;
    info.language = "unknown";

    // C++ 检测
    if (text.find("#include") != std::string::npos ||
        text.find("namespace ") != std::string::npos ||
        text.find("std::") != std::string::npos ||
        std::regex_search(text, std::regex(R"(\b\w+\s+\w+\s*\([^)]*\)\s*\{)"))) {
        info.language = "cpp";
        info.import_patterns = {"#include", "using namespace"};
        info.function_patterns = {
            R"((\w+)\s+(\w+)\s*\([^)]*\)\s*\{)",  // 返回类型 函数名(参数) {
            R"((\w+)\s+(\w+)\s*\([^)]*\)\s*const\s*\{)",  // const 函数
            R"(::(\w+)\s*\([^)]*\)\s*\{)"  // 类名::函数
        };
        info.class_patterns = {
            R"(class\s+(\w+)(?:\s*:\s*\w+)?\s*\{)",
            R"(struct\s+(\w+)\s*\{)"
        };
        info.block_end = "}";
    }
    // Python 检测
    else if (text.find("def ") != std::string::npos ||
             text.find("import ") != std::string::npos ||
             text.find("class ") != std::string::npos ||
             text.find("if __name__") != std::string::npos) {
        info.language = "python";
        info.import_patterns = {"import ", "from "};
        info.function_patterns = {
            R"(def\s+(\w+)\s*\([^)]*\)\s*:)"
        };
        info.class_patterns = {
            R"(class\s+(\w+)(?:\s*\([^)]*\))?\s*:)"
        };
        info.block_end = "";
    }
    // JavaScript 检测
    else if (text.find("function ") != std::string::npos ||
             text.find("const ") != std::string::npos ||
             text.find("let ") != std::string::npos ||
             text.find("=>") != std::string::npos ||
             text.find("require(") != std::string::npos) {
        info.language = "javascript";
        info.import_patterns = {"require(", "import ", "from "};
        info.function_patterns = {
            R"(function\s+(\w+)\s*\([^)]*\)\s*\{)",
            R"(const\s+(\w+)\s*=\s*(?:async\s+)?\([^)]*\)\s*=>)",
            R"(const\s+(\w+)\s*=\s*(?:async\s+)?function)",
            R"(let\s+(\w+)\s*=\s*(?:async\s+)?\([^)]*\)\s*=>)"
        };
        info.class_patterns = {
            R"(class\s+(\w+)(?:\s+extends\s+\w+)?\s*\{)"
        };
        info.block_end = "}";
    }
    // Java 检测
    else if (text.find("public ") != std::string::npos ||
             text.find("private ") != std::string::npos ||
             text.find("class ") != std::string::npos ||
             text.find("import java.") != std::string::npos) {
        info.language = "java";
        info.import_patterns = {"import ", "package "};
        info.function_patterns = {
            R"((?:public|private|protected)?\s*(?:static)?\s*\w+\s+(\w+)\s*\([^)]*\)\s*(?:throws\s+\w+)?\s*\{)"
        };
        info.class_patterns = {
            R"(class\s+(\w+)(?:\s+extends\s+\w+)?(?:\s+implements\s+\w+)?\s*\{)"
        };
        info.block_end = "}";
    }

    return info;
}

std::vector<CodeAwareChunker::CodeBlock> CodeAwareChunker::parse_code_blocks(
    const std::string& text,
    const LanguageInfo& lang_info) {

    std::vector<CodeBlock> blocks;
    auto lines = split_lines(text);

    int current_line = 0;
    std::string current_block_content;
    std::string current_block_type;
    std::string current_block_name;
    int block_start_line = 0;
    int brace_count = 0;
    bool in_block = false;

    auto flush_block = [&](bool force = false) {
        if (current_block_content.empty()) return;

        std::string trimmed = trim(current_block_content);
        int line_count = static_cast<int>(
            std::count(current_block_content.begin(), current_block_content.end(), '\n')) + 1;

        // 检查是否需要flush（块太小或force）
        if (force || line_count >= config_.min_chunk_lines ||
            current_block_type == "import" || current_block_type == "class") {

            CodeBlock block;
            block.type = current_block_type;
            block.name = current_block_name;
            block.content = trimmed;
            block.start_line = block_start_line;
            block.end_line = block_start_line + line_count - 1;

            // 提取依赖
            for (const auto& line : split_lines(current_block_content)) {
                for (const auto& pattern : lang_info.import_patterns) {
                    if (line.find(pattern) != std::string::npos) {
                        block.dependencies.push_back(trim(line));
                        break;
                    }
                }
            }

            blocks.push_back(block);
        }

        current_block_content.clear();
        current_block_type.clear();
        current_block_name.clear();
        in_block = false;
        brace_count = 0;
    };

    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string& line = lines[i];
        current_block_content += line + "\n";

        if (!in_block) {
            // 检测函数/类定义
            bool found = false;

            // 检测类
            for (const auto& pattern : lang_info.class_patterns) {
                std::smatch match;
                if (std::regex_search(line, match, std::regex(pattern))) {
                    flush_block(true);
                    current_block_type = "class";
                    current_block_name = match[1].str();
                    block_start_line = current_line;
                    in_block = true;
                    found = true;
                    break;
                }
            }

            // 检测函数
            if (!found) {
                for (const auto& pattern : lang_info.function_patterns) {
                    std::smatch match;
                    if (std::regex_search(line, match, std::regex(pattern))) {
                        flush_block(true);
                        current_block_type = "function";
                        // 函数名可能是第1个或第2个捕获组
                        current_block_name = match[match.size() > 1 ? 2 : 1].str();
                        block_start_line = current_line;
                        in_block = true;
                        found = true;
                        break;
                    }
                }
            }

            // 检测导入
            if (!found) {
                for (const auto& pattern : lang_info.import_patterns) {
                    if (line.find(pattern) != std::string::npos) {
                        flush_block(true);
                        current_block_type = "import";
                        current_block_name = trim(line);
                        block_start_line = current_line;
                        in_block = true;
                        found = true;
                        break;
                    }
                }
            }

            // 跟踪大括号
            for (char c : line) {
                if (c == '{') {
                    brace_count++;
                } else if (c == '}') {
                    brace_count--;
                }
            }
        } else {
            // 在块内，跟踪大括号
            for (char c : line) {
                if (c == '{') {
                    brace_count++;
                } else if (c == '}') {
                    brace_count--;
                }
            }

            // Python 块结束检测（缩进减少）
            if (lang_info.language == "python" && !line.empty() && line[0] != ' ' && line[0] != '\t') {
                if (current_block_type == "function" || current_block_type == "class") {
                    flush_block();
                    current_line++;
                    continue;
                }
            }

            // C风格语言块结束
            if (brace_count <= 0 && !lang_info.block_end.empty()) {
                flush_block();
                current_line++;
                continue;
            }
        }

        current_line++;
    }

    flush_block(true);

    return blocks;
}

ChunkResult CodeAwareChunker::chunk(const std::string& text,
                                    const std::string& document_id,
                                    const DocumentMetadata& metadata) {
    auto start = std::chrono::steady_clock::now();

    // 1. 检测语言
    auto lang_info = detect_language_info(text);
    RAG_DEBUG("CodeAwareChunker: detected language = " + lang_info.language);

    // 2. 解析代码块
    auto code_blocks = parse_code_blocks(text, lang_info);
    RAG_DEBUG("CodeAwareChunker: parsed " + std::to_string(code_blocks.size()) + " code blocks");

    // 3. 构建最终块
    std::vector<Chunk> chunks;
    std::vector<CodeBlock> pending_small_blocks;
    int chunk_index = 0;

    // 收集所有导入语句
    std::vector<std::string> all_imports;
    for (const auto& block : code_blocks) {
        for (const auto& dep : block.dependencies) {
            if (std::find(all_imports.begin(), all_imports.end(), dep) == all_imports.end()) {
                all_imports.push_back(dep);
            }
        }
    }

    for (size_t i = 0; i < code_blocks.size(); ++i) {
        const auto& block = code_blocks[i];

        // 收集上下文依赖
        std::string context;
        if (config_.preserve_context) {
            for (const auto& dep : block.dependencies) {
                context += dep + "\n";
            }
        }

        std::string chunk_content = context + block.content;

        // 检查块大小
        int line_count = block.end_line - block.start_line + 1;

        if (line_count < config_.min_chunk_lines) {
            // 小块，暂存待合并
            pending_small_blocks.push_back(block);
        } else {
            // 大块，先合并之前的小块
            if (!pending_small_blocks.empty()) {
                auto merged = merge_small_blocks(pending_small_blocks, all_imports, document_id);
                for (auto& m : merged) {
                    m.chunk_index = chunk_index++;
                    chunks.push_back(m);
                }
                pending_small_blocks.clear();
            }

            // 添加当前块
            Chunk chunk;
            chunk.id = generate_uuid();
            chunk.document_id = document_id;
            chunk.content = chunk_content;
            chunk.chunk_index = chunk_index++;
            chunk.start_line = block.start_line;
            chunk.end_line = block.end_line;
            chunk.metadata = metadata;
            chunk.metadata.extra["code_type"] = block.type;
            chunk.metadata.extra["code_name"] = block.name;

            chunks.push_back(chunk);
        }
    }

    // 处理剩余的小块
    if (!pending_small_blocks.empty()) {
        auto merged = merge_small_blocks(pending_small_blocks, all_imports, document_id);
        for (auto& m : merged) {
            m.chunk_index = chunk_index++;
            chunks.push_back(m);
        }
    }

    auto end = std::chrono::steady_clock::now();
    ChunkResult result;
    result.chunks = std::move(chunks);
    result.processing_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();
    result.total_tokens = static_cast<int>(result.chunks.size()) * 100;

    RAG_DEBUG("CodeAwareChunker: created " + std::to_string(result.chunks.size()) + " chunks");
    return result;
}

std::vector<Chunk> CodeAwareChunker::merge_small_blocks(
    const std::vector<CodeBlock>& small_blocks,
    const std::vector<std::string>& imports,
    const std::string& document_id) {

    std::vector<Chunk> chunks;

    if (small_blocks.empty()) return chunks;

    std::string current_content;
    int start_line = small_blocks.front().start_line;
    int end_line = 0;

    // 构建上下文头
    std::string context_header;
    if (config_.preserve_context) {
        for (const auto& imp : imports) {
            context_header += imp + "\n";
        }
        if (!context_header.empty()) {
            context_header += "\n";
        }
    }

    for (const auto& block : small_blocks) {
        int estimated_lines = block.end_line - block.start_line + 1;
        int current_lines = end_line - start_line + 1;

        // 检查合并后是否超过阈值
        if (current_lines + estimated_lines > config_.merge_threshold && !current_content.empty()) {
            // 创建当前块
            Chunk chunk;
            chunk.id = generate_uuid();
            chunk.document_id = document_id;
            chunk.content = context_header + trim(current_content);
            chunk.start_line = start_line;
            chunk.end_line = end_line;
            chunk.metadata.extra["code_type"] = "merged";
            chunks.push_back(chunk);

            // 重置
            current_content.clear();
            start_line = block.start_line;
        }

        current_content += block.content + "\n\n";
        end_line = block.end_line;
    }

    // 添加最后一块
    if (!current_content.empty()) {
        Chunk chunk;
        chunk.id = generate_uuid();
        chunk.document_id = document_id;
        chunk.content = context_header + trim(current_content);
        chunk.start_line = start_line;
        chunk.end_line = end_line;
        chunk.metadata.extra["code_type"] = "merged";
        chunks.push_back(chunk);
    }

    return chunks;
}

std::string CodeAwareChunker::name() const {
    return "code_aware";
}

const ChunkingConfig& CodeAwareChunker::config() const {
    return base_config_;
}

std::vector<Chunk> CodeAwareChunker::chunk(const std::string& text,
                                          const std::string& document_id) {
    DocumentMetadata metadata;
    metadata.extra["document_id"] = document_id;
    return chunk(text, document_id, metadata).chunks;
}

// ========== 工厂函数 ==========

std::unique_ptr<Chunker> create_code_aware_chunker(
    const CodeAwareChunker::Config& config) {
    return std::make_unique<CodeAwareChunker>(config);
}

}  // namespace rag