/**
 * @file semantic_chunker.cpp
 * @brief 语义分块器实现
 */

#include "rag/semantic_chunker.h"
#include "rag/logger.h"
#include "rag/embedding.h"
#include <algorithm>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <chrono>
#include <cctype>
#include <cmath>

namespace rag {

// ========== 工具函数 ==========

// 字符是否为句子分隔符
static bool is_sentence_separator(char c, const std::string& sep) {
    return sep.find(c) != std::string::npos;
}

// 查找分隔符
static size_t find_separator(const std::string& text, size_t start,
                             const std::vector<std::string>& separators) {
    size_t best_pos = std::string::npos;
    size_t best_len = 0;

    for (const auto& sep : separators) {
        size_t pos = text.find(sep, start);
        if (pos != std::string::npos && (best_pos == std::string::npos || pos < best_pos)) {
            best_pos = pos;
            best_len = sep.length();
        }
    }

    if (best_pos == std::string::npos) {
        return std::string::npos;
    }
    return best_pos + best_len - 1;  // 返回分隔符最后一个字符位置
}

// ========== SemanticChunker 实现 ==========

SemanticChunker::SemanticChunker(const SemanticChunkingConfig& config,
                                std::shared_ptr<EmbeddingService> embed_service)
    : config_(config), embed_service_(std::move(embed_service)) {

    // 初始化基础配置 (用于兼容 Chunker 接口)
    base_config_.chunk_size = config.chunk_size;
    base_config_.chunk_overlap = config.overlap;
    base_config_.min_chunk_size = config.min_sentence_len;

    // 如果没有 embedding 服务，创建一个简单的
    if (!embed_service_) {
        embed_service_ = create_embedding_service("simple", config.embedding_dimension);
    }
}

ChunkResult SemanticChunker::chunk(const std::string& text,
                                   const std::string& document_id,
                                   const DocumentMetadata& metadata) {
    ChunkResult result;
    auto start = std::chrono::steady_clock::now();

    // 1. 分割句子
    auto sentences = split_sentences(text);
    RAG_DEBUG("Split into " + std::to_string(sentences.size()) + " sentences");

    // 2. 向量化句子
    embed_sentences(sentences);

    // 3. 语义合并
    auto merged_chunks = merge_semantic_chunks(sentences);
    RAG_DEBUG("Merged into " + std::to_string(merged_chunks.size()) + " semantic chunks");

    // 4. 生成最终块
    result.chunks = generate_chunks(merged_chunks, document_id);

    // 5. 填充元数据
    for (size_t i = 0; i < result.chunks.size(); ++i) {
        result.chunks[i].metadata = metadata;
        result.chunks[i].metadata.extra["chunk_index"] = std::to_string(i);
    }

    auto end = std::chrono::steady_clock::now();
    result.processing_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    return result;
}

std::vector<Chunk> SemanticChunker::chunk(const std::string& text,
                                          const std::string& document_id) {
    DocumentMetadata metadata;
    metadata.extra["document_id"] = document_id;
    return chunk(text, document_id, metadata).chunks;
}

void SemanticChunker::set_config(const SemanticChunkingConfig& config) {
    config_ = config;
    base_config_.chunk_size = config.chunk_size;
    base_config_.chunk_overlap = config.overlap;
}

void SemanticChunker::set_embedding_service(std::shared_ptr<EmbeddingService> service) {
    embed_service_ = std::move(service);
}

std::vector<Sentence> SemanticChunker::split_sentences(const std::string& text) {
    std::vector<Sentence> sentences;
    std::string current;
    int pos = 0;
    int sentence_num = 0;

    // 清理文本
    std::string clean_text;
    clean_text.reserve(text.size());

    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        clean_text += c;

        // 检查是否是分隔符
        bool is_sep = false;
        for (const auto& sep : config_.sentence_separators) {
            if (sep.find(c) != std::string::npos) {
                is_sep = true;
                break;
            }
        }

        if (is_sep) {
            // 找到句子边界
            if (!current.empty()) {
                std::string sentence_text = current;
                int start = pos - static_cast<int>(sentence_text.length()) + 1;
                int end = pos;

                // 检查句子长度是否合理
                if (static_cast<int>(sentence_text.length()) >= config_.min_sentence_len &&
                    static_cast<int>(sentence_text.length()) <= config_.max_sentence_len) {

                    Sentence s;
                    s.text = sentence_text;
                    s.start_pos = start;
                    s.end_pos = end;
                    s.sentence_num = sentence_num++;
                    sentences.push_back(s);
                }

                current.clear();
            }
        } else {
            current += c;
        }
        pos++;
    }

    // 处理最后一个句子
    if (!current.empty()) {
        std::string sentence_text = current;
        int start = pos - static_cast<int>(sentence_text.length()) + 1;
        int end = pos;

        if (static_cast<int>(sentence_text.length()) >= config_.min_sentence_len) {
            Sentence s;
            s.text = sentence_text;
            s.start_pos = start;
            s.end_pos = end;
            s.sentence_num = sentence_num;
            sentences.push_back(s);
        }
    }

    return sentences;
}

void SemanticChunker::embed_sentences(std::vector<Sentence>& sentences) {
    if (!embed_service_ || !embed_service_->is_ready()) {
        RAG_WARN("Embedding service not ready, skipping sentence embedding");
        return;
    }

    // 批量编码
    std::vector<std::string> texts;
    for (const auto& s : sentences) {
        texts.push_back(s.text);
    }

    try {
        auto embeddings = embed_service_->encode_batch(texts);

        for (size_t i = 0; i < sentences.size() && i < embeddings.size(); ++i) {
            sentences[i].embedding = std::move(embeddings[i]);
        }
    } catch (const std::exception& e) {
        RAG_ERROR("Failed to embed sentences: " + std::string(e.what()));
    }
}

std::vector<std::vector<Sentence>> SemanticChunker::merge_semantic_chunks(
    const std::vector<Sentence>& sentences) {

    if (sentences.empty()) {
        return {};
    }

    std::vector<std::vector<Sentence>> merged;
    std::vector<Sentence> current_group;

    for (size_t i = 0; i < sentences.size(); ++i) {
        current_group.push_back(sentences[i]);

        // 检查是否应该结束当前组
        bool should_end = false;

        // 条件1: 组内句子数达到上限
        if (static_cast<int>(current_group.size()) >= config_.merge_max_chunks) {
            should_end = true;
        }

        // 条件2: 当前组字符数超过目标块大小
        int total_chars = 0;
        for (const auto& s : current_group) {
            total_chars += static_cast<int>(s.text.length());
        }
        if (total_chars >= config_.chunk_size) {
            should_end = true;
        }

        // 条件3: 与下一个句子相似度太低
        if (!should_end && i + 1 < sentences.size()) {
            float sim = compute_similarity(sentences[i], sentences[i + 1]);
            if (sim < config_.merge_similarity_threshold) {
                should_end = true;
            }
        }

        if (should_end && !current_group.empty()) {
            merged.push_back(current_group);
            current_group.clear();
        }
    }

    // 处理最后一组
    if (!current_group.empty()) {
        merged.push_back(current_group);
    }

    return merged;
}

std::vector<Chunk> SemanticChunker::generate_chunks(
    const std::vector<std::vector<Sentence>>& merged_chunks,
    const std::string& document_id) {

    std::vector<Chunk> chunks;

    for (size_t i = 0; i < merged_chunks.size(); ++i) {
        const auto& group = merged_chunks[i];

        // 合并句子文本
        std::string chunk_text;
        int start_pos = group.front().start_pos;
        int end_pos = group.back().end_pos;

        for (size_t j = 0; j < group.size(); ++j) {
            chunk_text += group[j].text;
            if (j < group.size() - 1) {
                chunk_text += " ";  // 句子间添加空格
            }
        }

        Chunk chunk;
        chunk.id = document_id + "_chunk_" + std::to_string(i);
        chunk.content = chunk_text;
        chunk.start_char = start_pos;
        chunk.end_char = end_pos;
        chunk.num_tokens = static_cast<int>(chunk_text.length() / 4);  // 粗略估计

        chunks.push_back(chunk);
    }

    return chunks;
}

float SemanticChunker::compute_similarity(const Sentence& a, const Sentence& b) {
    // 如果有 embedding，使用余弦相似度
    if (!a.embedding.empty() && !b.embedding.empty()) {
        if (a.embedding.size() == b.embedding.size()) {
            float dot = 0, norm_a = 0, norm_b = 0;
            for (size_t i = 0; i < a.embedding.size(); ++i) {
                dot += a.embedding[i] * b.embedding[i];
                norm_a += a.embedding[i] * a.embedding[i];
                norm_b += b.embedding[i] * b.embedding[i];
            }
            float denom = (std::sqrt(norm_a) * std::sqrt(norm_b));
            return denom > 1e-10f ? dot / denom : 0.0f;
        }
    }

    // 回退到关键词重叠
    return keyword_overlap(a.text, b.text);
}

float SemanticChunker::keyword_overlap(const std::string& a, const std::string& b) {
    // 简单分词
    std::unordered_set<std::string> words_a;
    std::unordered_set<std::string> words_b;

    auto add_words = [](const std::string& text, std::unordered_set<std::string>& words) {
        std::string current;
        for (char c : text) {
            if (std::isalnum(static_cast<unsigned char>(c))) {
                current += static_cast<char>(std::tolower(c));
            } else if (!current.empty()) {
                if (current.length() >= 2) {  // 忽略单字符
                    words.insert(current);
                }
                current.clear();
            }
        }
        if (!current.empty() && current.length() >= 2) {
            words.insert(current);
        }
    };

    add_words(a, words_a);
    add_words(b, words_b);

    if (words_a.empty() || words_b.empty()) {
        return 0.0f;
    }

    // Jaccard 相似度
    int intersection = 0;
    for (const auto& w : words_a) {
        if (words_b.find(w) != words_b.end()) {
            intersection++;
        }
    }

    int union_size = static_cast<int>(words_a.size() + words_b.size() - intersection);
    return union_size > 0 ? static_cast<float>(intersection) / union_size : 0.0f;
}

// ========== HybridChunker 实现 ==========

HybridChunker::HybridChunker(const ChunkingConfig& base_config,
                             std::shared_ptr<EmbeddingService> embed_service)
    : base_config_(base_config),
      embed_service_(std::move(embed_service)),
      semantic_config_(),
      recursive_chunker_(base_config) {

    // 初始化语义配置
    semantic_config_.chunk_size = base_config.chunk_size;
    semantic_config_.overlap = base_config.chunk_overlap;
    semantic_config_.min_sentence_len = base_config.min_chunk_size;
}

ChunkResult HybridChunker::chunk(const std::string& text,
                                 const std::string& document_id,
                                 const DocumentMetadata& metadata) {
    // 简单启发式: 如果文本较长，使用语义分块；否则使用递归分块
    if (static_cast<int>(text.length()) > semantic_config_.chunk_size * 2) {
        SemanticChunker semantic(semantic_config_, embed_service_);
        return semantic.chunk(text, document_id, metadata);
    }

    // 使用递归分块器
    return recursive_chunker_.chunk(text, document_id, metadata);
}

std::vector<Chunk> HybridChunker::chunk(const std::string& text,
                                        const std::string& document_id) {
    DocumentMetadata metadata;
    metadata.extra["document_id"] = document_id;
    return chunk(text, document_id, metadata).chunks;
}

// ========== 工厂函数 ==========

std::unique_ptr<Chunker> create_semantic_chunker(
    const SemanticChunkingConfig& config,
    std::shared_ptr<EmbeddingService> embed_service) {

    return std::make_unique<SemanticChunker>(config, std::move(embed_service));
}

std::unique_ptr<Chunker> create_hybrid_chunker(
    const ChunkingConfig& config,
    std::shared_ptr<EmbeddingService> embed_service) {

    return std::make_unique<HybridChunker>(config, std::move(embed_service));
}

}  // namespace rag
