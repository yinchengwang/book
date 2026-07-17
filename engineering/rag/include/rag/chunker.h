/**
 * @file chunker.h
 * @brief 文本分块器 - 接口和实现
 */
#pragma once

#include "rag/types.h"
#include "rag/config.h"
#include <string>
#include <vector>
#include <memory>

namespace rag {

// ========== 前向声明 ==========

class Chunker;

// ========== 分块结果 ==========

/**
 * @brief 分块结果
 */
struct ChunkResult {
    std::vector<Chunk> chunks;      // 分块结果
    int64_t processing_time_ms;     // 处理时间
    int total_tokens;               // 总 token 数
};

// ========== 分块器接口 ==========

/**
 * @brief 分块器接口
 */
class Chunker {
public:
    virtual ~Chunker() = default;

    // 分块
    virtual ChunkResult chunk(const std::string& text,
                              const std::string& document_id,
                              const DocumentMetadata& metadata) = 0;

    // 分块（字符串版本）
    virtual std::vector<Chunk> chunk(const std::string& text,
                                     const std::string& document_id) = 0;

    // 获取分块器名称
    virtual std::string name() const = 0;

    // 获取配置
    virtual const ChunkingConfig& config() const = 0;
};

// ========== 固定分块器 ==========

/**
 * @brief 固定大小分块器
 *
 * 按固定字符数分块，不考虑语义边界
 */
class FixedChunker : public Chunker {
public:
    explicit FixedChunker(const ChunkingConfig& config);

    ChunkResult chunk(const std::string& text,
                      const std::string& document_id,
                      const DocumentMetadata& metadata) override;

    std::vector<Chunk> chunk(const std::string& text,
                             const std::string& document_id) override;

    std::string name() const override { return "fixed"; }
    const ChunkingConfig& config() const override { return config_; }

private:
    ChunkingConfig config_;
};

// ========== 递归分块器 ==========

/**
 * @brief 递归字符分块器
 *
 * 按优先级尝试不同的分隔符，尽可能保持语义完整
 */
class RecursiveChunker : public Chunker {
public:
    explicit RecursiveChunker(const ChunkingConfig& config);

    ChunkResult chunk(const std::string& text,
                      const std::string& document_id,
                      const DocumentMetadata& metadata) override;

    std::vector<Chunk> chunk(const std::string& text,
                             const std::string& document_id) override;

    std::string name() const override { return "recursive"; }
    const ChunkingConfig& config() const override { return config_; }

private:
    // 分隔符优先级
    static const std::vector<std::string> separators_;

    // 递归分块
    std::vector<std::string> split_text(const std::string& text,
                                        int depth = 0);

    // 检查是否应该分割
    bool should_split(const std::string& text) const;

    // 获取分隔符
    std::string get_separator(int depth) const;

    ChunkingConfig config_;
};

// ========== 代码分块器 ==========

/**
 * @brief 代码感知分块器
 *
 * 尝试按代码结构（函数、类、段落）分块
 */
class CodeChunker : public Chunker {
public:
    explicit CodeChunker(const ChunkingConfig& config);

    ChunkResult chunk(const std::string& text,
                      const std::string& document_id,
                      const DocumentMetadata& metadata) override;

    std::vector<Chunk> chunk(const std::string& text,
                             const std::string& document_id) override;

    std::string name() const override { return "code"; }
    const ChunkingConfig& config() const override { return config_; }

    // 设置代码类型
    void set_language(const std::string& lang);

private:
    // 默认分隔符（复制而非直接访问父类私有成员）
    static const std::vector<std::string> default_separators_;

    // 按语言获取分隔符
    std::vector<std::string> get_language_separators(const std::string& lang);

    ChunkingConfig config_;
    std::string language_;
};

// ========== 分块器工厂 ==========

/**
 * @brief 分块器工厂
 */
class ChunkerFactory {
public:
    // 创建分块器
    static std::unique_ptr<Chunker> create(const ChunkingConfig& config);

    // 创建指定类型的分块器
    static std::unique_ptr<Chunker> create(const std::string& type,
                                           const ChunkingConfig& config);
};

}  // namespace rag
