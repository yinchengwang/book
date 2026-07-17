/**
 * @file types.h
 * @brief RAG 核心数据结构
 */
#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <optional>

namespace rag {

// ========== 文档元数据 ==========

/**
 * @brief 文档元数据
 */
struct DocumentMetadata {
    std::string file_path;          // 文件路径
    std::string file_name;          // 文件名
    std::string file_type;          // 文件类型 (md/c/cpp/h)
    size_t file_size = 0;           // 文件大小
    int64_t created_time = 0;       // 创建时间
    int64_t modified_time = 0;      // 修改时间
    std::string title;              // 标题（从文件名或内容提取）
    std::string author;             // 作者
    std::map<std::string, std::string> extra;  // 额外字段
};

// ========== 文档 ==========

/**
 * @brief 文档结构
 */
struct Document {
    std::string id;                 // 文档唯一ID (UUID)
    std::string content;            // 原始内容
    DocumentMetadata metadata;      // 元数据

    // 时间戳
    int64_t created_at = 0;
    int64_t updated_at = 0;
    int64_t indexed_at = 0;         // 索引时间

    // 状态
    enum class Status {
        PENDING = 0,    // 待处理
        PROCESSING = 1, // 处理中
        INDEXED = 2,    // 已索引
        FAILED = 3      // 失败
    };
    Status status = Status::PENDING;

    std::string error_message;      // 错误信息（如果失败）
};

// ========== 文本块 ==========

/**
 * @brief 文本块结构
 */
struct Chunk {
    std::string id;                 // 块唯一ID
    std::string document_id;        // 所属文档ID
    std::string content;            // 块内容
    int chunk_index = 0;            // 块序号
    int start_char = 0;             // 在文档中的起始字符位置
    int end_char = 0;               // 在文档中的结束字符位置
    int start_line = 0;             // 起始行号
    int end_line = 0;               // 结束行号

    // Token 数量（可选）
    int num_tokens = 0;

    // 继承自文档的元数据
    DocumentMetadata metadata;

    // 分数（检索时填充）
    float score = 0.0f;
};

// ========== 向量 ==========

/**
 * @brief 向量嵌入
 */
struct Embedding {
    std::string id;                 // 嵌入ID
    std::string chunk_id;           // 对应的块ID
    std::vector<float> vector;      // 向量数据
    int dimension = 0;              // 维度

    int64_t created_at = 0;
};

// ========== 检索结果 ==========

/**
 * @brief 检索结果
 */
struct RetrievalResult {
    Chunk chunk;                    // 匹配的块
    float score = 0.0f;             // 相似度分数
    float vector_score = 0.0f;      // 向量检索分数
    float bm25_score = 0.0f;        // BM25 分数
    std::string source;             // 来源 (hnsw/bm25/hybrid)
    int rank = 0;                   // 排名
};

// ========== 查询结果 ==========

/**
 * @brief 查询结果
 */
struct QueryResult {
    std::string answer;             // 生成的回答
    std::vector<RetrievalResult> chunks;  // 引用的块
    float confidence = 0.0f;        // 置信度
    int64_t query_time_ms = 0;      // 查询耗时（毫秒）
    int64_t timestamp = 0;          // 时间戳
    std::string request_id;         // 请求ID
};

// ========== 索引状态 ==========

/**
 * @brief 索引状态
 */
struct IndexStatus {
    std::string index_name;         // 索引名称
    int64_t document_count = 0;     // 文档数
    int64_t chunk_count = 0;        // 块数
    int64_t vector_count = 0;       // 向量数
    int64_t last_update = 0;        // 最后更新时间
    size_t index_size = 0;          // 索引大小（字节）

    // 状态
    enum class Status {
        NOT_EXISTS = 0,
        BUILDING = 1,
        READY = 2,
        CORRUPTED = 3
    };
    Status status = Status::NOT_EXISTS;
};

// ========== 查询历史 ==========

/**
 * @brief 查询历史记录
 */
struct QueryHistory {
    std::string id;                 // 记录ID
    std::string query;              // 查询内容
    std::string answer;             // 生成的回答
    int chunks_used = 0;            // 使用的块数
    int64_t query_time_ms = 0;      // 查询耗时
    int64_t timestamp = 0;          // 时间戳
};

// ========== 工具函数 ==========

/**
 * @brief 生成 UUID
 */
std::string generate_uuid();

/**
 * @brief 获取当前时间戳（毫秒）
 */
int64_t current_timestamp_ms();

/**
 * @brief 获取当前时间戳（秒）
 */
int64_t current_timestamp();

/**
 * @brief 截断字符串（带省略号）
 */
std::string truncate_string(const std::string& str, size_t max_len);

}  // namespace rag
