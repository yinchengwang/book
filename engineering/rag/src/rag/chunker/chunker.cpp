/**
 * @file chunker.cpp
 * @brief 分块器实现
 */

#include "rag/chunker.h"
#include "rag/logger.h"
#include <algorithm>
#include <cctype>
#include <chrono>

namespace rag {

// ========== 辅助函数 ==========

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

static std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::string current;
    int line_num = 1;
    int char_pos = 0;

    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n') {
            lines.push_back(current);
            current.clear();
            ++line_num;
            char_pos = i + 1;
        } else {
            current += text[i];
        }
    }
    if (!current.empty()) {
        lines.push_back(current);
    }
    return lines;
}

// ========== Chunker 实现 ==========

// ========== FixedChunker 实现 ==========

FixedChunker::FixedChunker(const ChunkingConfig& config) : config_(config) {}

ChunkResult FixedChunker::chunk(const std::string& text,
                                const std::string& document_id,
                                const DocumentMetadata& metadata) {
    auto start = std::chrono::steady_clock::now();

    std::vector<Chunk> chunks;
    int pos = 0;
    int chunk_index = 0;

    while (pos < static_cast<int>(text.size())) {
        int end = std::min(pos + config_.chunk_size, static_cast<int>(text.size()));

        // 尝试在单词边界结束
        if (end < static_cast<int>(text.size())) {
            while (end > pos && !std::isspace(text[end - 1])) {
                --end;
            }
            if (end == pos) {
                end = std::min(pos + config_.chunk_size, static_cast<int>(text.size()));
            }
        }

        std::string content = text.substr(pos, end - pos);
        content = trim(content);

        if (!content.empty()) {
            Chunk chunk;
            chunk.id = generate_uuid();
            chunk.document_id = document_id;
            chunk.content = content;
            chunk.chunk_index = chunk_index++;
            chunk.start_char = pos;
            chunk.end_char = end;
            chunk.metadata = metadata;

            // 估算行号
            int line_count = std::count(text.begin() + pos, text.begin() + end, '\n');
            chunk.start_line = chunk.metadata.created_time;  // 需要计算
            chunk.end_line = chunk.start_line + line_count;

            chunks.push_back(chunk);
        }

        pos = end - config_.chunk_overlap;
        if (pos <= 0) pos = end;
        if (pos <= 0) break;
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    ChunkResult result;
    result.chunks = std::move(chunks);
    result.processing_time_ms = duration.count();
    result.total_tokens = static_cast<int>(result.chunks.size()) * 100;  // 简化估算

    RAG_DEBUG("FixedChunker: created " + std::to_string(result.chunks.size()) + " chunks");
    return result;
}

std::vector<Chunk> FixedChunker::chunk(const std::string& text,
                                       const std::string& document_id) {
    return chunk(text, document_id, DocumentMetadata{}).chunks;
}

// ========== RecursiveChunker 实现 ==========

const std::vector<std::string> RecursiveChunker::separators_ = {
    "\n\n",   // 段落
    "\n",     // 行
    ". ",     // 句子
    ", ",     // 短语
    " ",      // 单词
    ""        // 字符
};

// CodeChunker 默认分隔符
const std::vector<std::string> CodeChunker::default_separators_ = {
    "\n\n", "\n", ". ", ", ", " ", ""
};

RecursiveChunker::RecursiveChunker(const ChunkingConfig& config) : config_(config) {}

std::vector<std::string> RecursiveChunker::split_text(const std::string& text, int depth) {
    if (depth >= static_cast<int>(separators_.size())) {
        return {text};
    }

    if (text.size() <= static_cast<size_t>(config_.chunk_size)) {
        return {text};
    }

    std::string sep = get_separator(depth);
    std::vector<std::string> parts;

    if (sep.empty()) {
        // 字符级分割
        for (size_t i = 0; i < text.size(); i += config_.chunk_size) {
            parts.push_back(text.substr(i, config_.chunk_size));
        }
    } else {
        size_t pos = 0;
        while (pos < text.size()) {
            size_t next = text.find(sep, pos);
            if (next == std::string::npos) {
                next = text.size();
            }

            std::string part = text.substr(pos, next - pos);
            if (!part.empty()) {
                if (static_cast<int>(part.size()) > config_.chunk_size) {
                    // 递归分割
                    auto sub_parts = split_text(part, depth + 1);
                    parts.insert(parts.end(), sub_parts.begin(), sub_parts.end());
                } else {
                    parts.push_back(part);
                }
            }

            pos = next + sep.size();
        }
    }

    // 合并小片段
    std::vector<std::string> merged;
    std::string current;

    for (const auto& part : parts) {
        if (current.empty()) {
            current = part;
        } else if (static_cast<int>(current.size() + sep.size() + part.size()) <= config_.chunk_size) {
            current += sep + part;
        } else {
            if (!current.empty()) {
                merged.push_back(trim(current));
            }
            current = part;
        }
    }

    if (!current.empty()) {
        merged.push_back(trim(current));
    }

    return merged;
}

bool RecursiveChunker::should_split(const std::string& text) const {
    return static_cast<int>(text.size()) > config_.chunk_size;
}

std::string RecursiveChunker::get_separator(int depth) const {
    if (depth < static_cast<int>(separators_.size())) {
        return separators_[depth];
    }
    return "";
}

ChunkResult RecursiveChunker::chunk(const std::string& text,
                                    const std::string& document_id,
                                    const DocumentMetadata& metadata) {
    auto start = std::chrono::steady_clock::now();

    auto parts = split_text(text);
    std::vector<Chunk> chunks;

    int pos = 0;
    int chunk_index = 0;

    for (const auto& part : parts) {
        std::string content = trim(part);
        if (content.empty()) continue;

        if (static_cast<int>(content.size()) < config_.min_chunk_size && chunk_index > 0 && !chunks.empty()) {
            // 合并到前一个块
            chunks.back().content += " " + content;
            chunks.back().end_char = pos + static_cast<int>(content.size());
        } else {
            Chunk chunk;
            chunk.id = generate_uuid();
            chunk.document_id = document_id;
            chunk.content = content;
            chunk.chunk_index = chunk_index++;
            chunk.start_char = pos;
            chunk.end_char = pos + static_cast<int>(content.size());
            chunk.metadata = metadata;
            chunks.push_back(chunk);
        }

        pos += static_cast<int>(content.size()) + 1;  // +1 for separator
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    ChunkResult result;
    result.chunks = std::move(chunks);
    result.processing_time_ms = duration.count();
    result.total_tokens = static_cast<int>(result.chunks.size()) * 100;

    RAG_DEBUG("RecursiveChunker: created " + std::to_string(result.chunks.size()) + " chunks");
    return result;
}

std::vector<Chunk> RecursiveChunker::chunk(const std::string& text,
                                           const std::string& document_id) {
    return chunk(text, document_id, DocumentMetadata{}).chunks;
}

// ========== CodeChunker 实现 ==========

CodeChunker::CodeChunker(const ChunkingConfig& config) : config_(config) {}

void CodeChunker::set_language(const std::string& lang) {
    language_ = lang;
}

std::vector<std::string> CodeChunker::get_language_separators(const std::string& lang) {
    if (lang == "cpp" || lang == "c" || lang == "h") {
        return {"}\n\n", "\n\n", "\n", ";\n"};
    } else if (lang == "python" || lang == "py") {
        return {"\n\n\n", "\n\n", "\n", "\n\nclass ", "\n\ndef "};
    } else if (lang == "java") {
        return {"}\n\n", "\n\n", "\n", ";\n"};
    } else if (lang == "javascript" || lang == "js" || lang == "typescript" || lang == "ts") {
        return {"}\n\n", "\n\n", "\n", ";\n"};
    }
    return default_separators_;
}

ChunkResult CodeChunker::chunk(const std::string& text,
                               const std::string& document_id,
                               const DocumentMetadata& metadata) {
    auto start = std::chrono::steady_clock::now();

    std::vector<std::string> parts;
    std::vector<std::string> separators;

    if (language_.empty()) {
        separators = default_separators_;
    } else {
        separators = get_language_separators(language_);
    }

    // 简单的代码结构分割
    std::string current;
    int brace_count = 0;

    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        current += c;

        // 跟踪大括号
        if (c == '{') brace_count++;
        else if (c == '}') brace_count--;

        // 找到分隔符
        for (const auto& sep : separators) {
            if (current.size() >= sep.size() &&
                current.substr(current.size() - sep.size()) == sep) {

                // 检查是否在括号内
                if (brace_count == 0 && current.size() > sep.size()) {
                    parts.push_back(current.substr(0, current.size() - sep.size()));
                    current.clear();
                }
                break;
            }
        }

        // 检查大小
        if (static_cast<int>(current.size()) > config_.chunk_size * 2) {
            // 强制分割
            parts.push_back(current);
            current.clear();
            brace_count = 0;
        }
    }

    if (!current.empty()) {
        parts.push_back(current);
    }

    // 创建 Chunk
    std::vector<Chunk> chunks;
    int chunk_index = 0;
    int pos = 0;

    for (const auto& part : parts) {
        std::string content = trim(part);
        if (content.empty()) continue;

        Chunk chunk;
        chunk.id = generate_uuid();
        chunk.document_id = document_id;
        chunk.content = content;
        chunk.chunk_index = chunk_index++;
        chunk.start_char = pos;
        chunk.end_char = pos + static_cast<int>(content.size());
        chunk.metadata = metadata;
        chunks.push_back(chunk);

        pos += static_cast<int>(content.size());
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    ChunkResult result;
    result.chunks = std::move(chunks);
    result.processing_time_ms = duration.count();
    result.total_tokens = static_cast<int>(result.chunks.size()) * 100;

    RAG_DEBUG("CodeChunker: created " + std::to_string(result.chunks.size()) + " chunks");
    return result;
}

std::vector<Chunk> CodeChunker::chunk(const std::string& text,
                                      const std::string& document_id) {
    return chunk(text, document_id, DocumentMetadata{}).chunks;
}

// ========== ChunkerFactory 实现 ==========

std::unique_ptr<Chunker> ChunkerFactory::create(const ChunkingConfig& config) {
    return create(config.strategy, config);
}

std::unique_ptr<Chunker> ChunkerFactory::create(const std::string& type,
                                                const ChunkingConfig& config) {
    if (type == "fixed") {
        return std::make_unique<FixedChunker>(config);
    } else if (type == "code") {
        return std::make_unique<CodeChunker>(config);
    }
    // 默认使用递归分块器
    return std::make_unique<RecursiveChunker>(config);
}

}  // namespace rag
