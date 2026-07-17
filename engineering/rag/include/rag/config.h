/**
 * @file config.h
 * @brief RAG 系统配置 - YAML 配置解析
 */
#pragma once

#include "rag/error.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <filesystem>
#include <sstream>

namespace rag {

// ========== 前向声明 ==========

class Config;
class ConfigLoader;

// ========== 数据源配置 ==========

/**
 * @brief 数据源配置
 */
struct DataSourceConfig {
    std::string path;                           // 数据路径
    std::vector<std::string> patterns = {"*.md"};  // 文件匹配模式
    std::string file_type;                      // 文件类型过滤
    bool recursive = true;                      // 递归扫描
};

// ========== Embedding 模型配置 ==========

/**
 * @brief Embedding 模型配置
 */
struct EmbeddingConfig {
    std::string model_path;                     // 模型路径
    std::string model_type = "bge-base-zh-v1.5"; // 模型类型
    int batch_size = 32;                        // 批处理大小
    int max_length = 512;                       // 最大序列长度
    std::string device = "cpu";                 // 设备 (cpu/cuda)
    int num_threads = 4;                        // CPU 线程数
};

// ========== LLM 配置 ==========

/**
 * @brief LLM 配置
 */
struct LLMConfig {
    std::string model_path;                     // 模型路径 (GGUF)
    std::string model_type = "qwen2.5-7b";      // 模型类型
    int n_ctx = 4096;                           // 上下文窗口
    int n_threads = 4;                          // CPU 线程数
    int max_tokens = 1024;                      // 最大生成长度
    float temperature = 0.7f;                   // 温度参数
    float top_p = 0.9f;                         // Top-p 采样
    bool stream = false;                        // 流式输出
};

// ========== 索引配置 ==========

/**
 * @brief HNSW 索引配置
 */
struct HNSWConfig {
    int dim = 768;                              // 向量维度
    int max_elements = 1000000;                 // 最大元素数
    int m = 16;                                 // 连接数
    int ef_construction = 200;                  // 构建精度
    int ef_search = 100;                        // 搜索精度
    std::string space = "l2";                   // 距离度量 (l2/ip/cosine)
};

/**
 * @brief BM25 配置
 */
struct BM25Config {
    float k1 = 1.5f;                              // BM25 k1 参数
    float b = 0.75f;                              // BM25 b 参数
    int avg_doc_len = 256;                        // 平均文档长度
};

// ========== 检索配置 ==========

/**
 * @brief 检索配置
 */
struct RetrievalConfig {
    int top_k = 5;                              // 返回结果数
    float min_score = 0.0f;                     // 最小分数阈值
    float rrf_k = 60;                           // RRF 融合参数
    bool hybrid = true;                         // 是否混合检索
    float hybrid_weight = 0.7f;                 // 向量检索权重
};

// ========== 分块配置 ==========

/**
 * @brief 分块配置
 */
struct ChunkingConfig {
    std::string strategy = "recursive";         // 分块策略
    int chunk_size = 500;                       // 块大小（字符）
    int chunk_overlap = 50;                     // 块重叠
    int min_chunk_size = 50;                    // 最小块大小
    bool preserve_structure = true;             // 保留结构
};

// ========== 日志配置 ==========

/**
 * @brief 日志配置
 */
struct LoggingConfig {
    std::string level = "info";                 // 日志级别
    std::string file = "./rag_data/logs/rag.log";
    size_t max_size = 100 * 1024 * 1024;        // 100MB
    int max_backups = 5;
    bool console = true;
};

// ========== 服务器配置 ==========

/**
 * @brief 服务器配置
 */
struct ServerConfig {
    std::string host = "0.0.0.0";               // 监听地址
    int port = 8080;                            // 端口
    int num_threads = 4;                        // 工作线程数
    bool cors_enabled = true;                   // 启用 CORS
    std::string cors_origin = "*";              // CORS 允许的源
};

// ========== Graph 配置 ==========

/**
 * @brief Graph RAG 配置
 */
struct GraphConfig {
    bool enabled = false;                      // 是否启用 Graph RAG
    std::string extractor_type = "rule";       // 提取器类型: rule, llm
    int max_hops = 2;                          // 子图扩展最大跳数
    bool incremental_update = true;            // 增量更新
    std::string kg_storage_path = "kg.json";   // 图谱存储路径
    float entity_confidence_threshold = 0.5f;  // 实体置信度阈值
    std::string retriever_type = "hybrid";     // 检索器类型: base, hybrid
};

// ========== 主配置类 ==========

/**
 * @brief RAG 系统配置
 */
class Config {
public:
    Config() = default;

    // 版本
    std::string version = "1.0.0";

    // 数据源
    std::vector<DataSourceConfig> data_sources;

    // Embedding
    EmbeddingConfig embedding;

    // LLM
    LLMConfig llm;

    // 索引
    HNSWConfig hnsw;
    BM25Config bm25;

    // 检索
    RetrievalConfig retrieval;

    // 分块
    ChunkingConfig chunking;

    // Graph RAG
    GraphConfig graph;

    // 日志
    LoggingConfig logging;

    // 服务器
    ServerConfig server;

    // 数据目录
    std::string data_dir = "./rag_data";
    std::string index_dir = "./rag_data/index";
    std::string cache_dir = "./rag_data/cache";

    // 验证配置
    bool validate() const;

    // 获取默认值
    static Config default_config();

    // 转换为字符串
    std::string to_string() const;
};

// ========== 配置加载器 ==========

/**
 * @brief 配置加载器
 */
class ConfigLoader {
public:
    ConfigLoader() = default;

    // 从文件加载
    Config load(const std::filesystem::path& path);

    // 从字符串加载
    Config load_from_string(const std::string& yaml_content);

    // 从环境变量加载
    Config load_from_env();

    // 保存到文件
    void save(const Config& config, const std::filesystem::path& path);

private:
    void apply_env_overrides(Config& config);
};

}  // namespace rag
