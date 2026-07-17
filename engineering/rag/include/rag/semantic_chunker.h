/**
 * @file semantic_chunker.h
 * @brief 语义分块器 - 基于句子边界和语义相似度分块
 */
#pragma once

#include "rag/chunker.h"
#include "rag/embedding.h"
#include <memory>
#include <vector>
#include <string>

namespace rag {

// ========== 语义分块器配置 ==========

/**
 * @brief 语义分块器配置
 */
struct SemanticChunkingConfig {
    // 基础配置
    int chunk_size = 512;           // 目标块大小（字符数）
    int overlap = 50;               // 重叠大小

    // 句子级别配置
    int min_sentence_len = 10;      // 最小句子长度
    int max_sentence_len = 500;    // 最大句子长度

    // 合并阈值
    float merge_similarity_threshold = 0.7f;  // 相似度阈值
    int merge_min_chunks = 2;       // 最少合并块数
    int merge_max_chunks = 5;       // 最多合并块数

    // Embedding 配置 (用于语义相似度)
    std::string embedding_type = "simple";
    int embedding_dimension = 384;

    // 句子分隔符
    std::vector<std::string> sentence_separators = {
        "。", "！", "？", ".", "!", "?",
        "\n", "\r\n", "\n\n"
    };
};

// ========== 句子信息 ==========

/**
 * @brief 句子信息
 */
struct Sentence {
    std::string text;       // 句子文本
    int start_pos;          // 起始位置
    int end_pos;            // 结束位置
    int sentence_num;       // 句子序号

    // Embedding 用于相似度计算
    std::vector<float> embedding;
};

// ========== 语义分块器 ==========

/**
 * @brief 语义分块器
 *
 * 基于句子边界分块，并使用语义相似度合并相关句子
 *
 * 工作流程:
 * 1. 句子分割: 将文本分割成句子
 * 2. 向量化: 为每个句子生成 embedding
 * 3. 语义合并: 合并相似度高的连续句子
 * 4. 块生成: 生成最终的分块
 */
class SemanticChunker : public Chunker {
public:
    /**
     * @brief 构造函数
     * @param config 配置
     * @param embed_service Embedding 服务
     */
    explicit SemanticChunker(const SemanticChunkingConfig& config,
                             std::shared_ptr<EmbeddingService> embed_service = nullptr);

    // Chunker 接口
    ChunkResult chunk(const std::string& text,
                      const std::string& document_id,
                      const DocumentMetadata& metadata) override;

    std::vector<Chunk> chunk(const std::string& text,
                             const std::string& document_id) override;

    std::string name() const override { return "semantic"; }
    const ChunkingConfig& config() const override { return base_config_; }

    // 配置方法
    void set_config(const SemanticChunkingConfig& config);
    const SemanticChunkingConfig& semantic_config() const { return config_; }

    // Embedding 服务设置
    void set_embedding_service(std::shared_ptr<EmbeddingService> service);

private:
    // 1. 句子分割
    std::vector<Sentence> split_sentences(const std::string& text);

    // 2. 向量化句子
    void embed_sentences(std::vector<Sentence>& sentences);

    // 3. 语义合并
    std::vector<std::vector<Sentence>> merge_semantic_chunks(
        const std::vector<Sentence>& sentences);

    // 4. 生成最终块
    std::vector<Chunk> generate_chunks(
        const std::vector<std::vector<Sentence>>& merged_chunks,
        const std::string& document_id);

    // 计算句子相似度
    float compute_similarity(const Sentence& a, const Sentence& b);

    // 检查是否应该合并
    bool should_merge(const Sentence& current, const Sentence& next,
                     float avg_similarity);

    // 简化版: 使用关键词重叠
    float keyword_overlap(const std::string& a, const std::string& b);

    // 基础配置 (兼容 Chunker 接口)
    ChunkingConfig base_config_;

    // 语义分块配置
    SemanticChunkingConfig config_;

    // Embedding 服务
    std::shared_ptr<EmbeddingService> embed_service_;
};

// ========== 混合分块器 ==========

/**
 * @brief 混合分块器
 *
 * 结合多种分块策略:
 * - 首选语义分块
 * - 如果句子太短或太长，使用递归分块
 */
class HybridChunker : public Chunker {
public:
    HybridChunker(const ChunkingConfig& base_config,
                  std::shared_ptr<EmbeddingService> embed_service = nullptr);

    ChunkResult chunk(const std::string& text,
                      const std::string& document_id,
                      const DocumentMetadata& metadata) override;

    std::vector<Chunk> chunk(const std::string& text,
                             const std::string& document_id) override;

    std::string name() const override { return "hybrid"; }
    const ChunkingConfig& config() const override { return base_config_; }

private:
    ChunkingConfig base_config_;
    std::shared_ptr<EmbeddingService> embed_service_;
    SemanticChunkingConfig semantic_config_;
    RecursiveChunker recursive_chunker_;
};

// ========== 工厂函数 ==========

std::unique_ptr<Chunker> create_semantic_chunker(
    const SemanticChunkingConfig& config,
    std::shared_ptr<EmbeddingService> embed_service = nullptr);

std::unique_ptr<Chunker> create_hybrid_chunker(
    const ChunkingConfig& config,
    std::shared_ptr<EmbeddingService> embed_service = nullptr);

}  // namespace rag
