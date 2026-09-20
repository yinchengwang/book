/**
 * @file engine.h
 * @brief RAG 核心引擎
 */
#pragma once

#include <atomic>
#include <mutex>

#include "mmrag/config.h"
#include "mmrag/types.h"
#include "mmrag/database.h"
#include "mmrag/chunker.h"
#include "mmrag/parser.h"
#include "mmrag/retriever.h"
#include "mmrag/vector_index.h"
#include "mmrag/db_bm25_index.h"
#include "mmrag/graph_support.h"
#include "mmrag/evaluator.h"
#include "mmrag/llm_service.h"
#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <functional>

// Note: server.h includes engine.h, so we need to avoid circular dependency
// The friend declaration was removed earlier to fix compilation issues
// If Server needs access to private members, we should add specific getter methods instead

namespace mmrag {

// ========== 索引构建结果 ==========

/**
 * @brief 索引构建结果
 */
struct IndexBuildResult {
    bool success = false;
    int documents_processed = 0;
    int documents_failed = 0;
    int chunks_created = 0;
    int64_t duration_ms = 0;
    std::string error_message;
};

// ========== 引擎状态 ==========

/**
 * @brief 引擎状态
 */
enum class EngineState {
    UNINITIALIZED,  // 未初始化
    INITIALIZING,   // 初始化中
    READY,          // 就绪
    INDEXING,       // 正在索引
    QUERYING,       // 正在查询
    ERROR           // 错误状态
};

// ========== 引擎配置 ==========

/**
 * @brief 引擎初始化配置
 */
struct EngineConfig {
    Config config;
    bool load_existing_index = true;  // 加载已有索引
    bool auto_create_dirs = true;     // 自动创建目录
};

// ========== RAG 引擎 ==========

/**
 * @brief RAG 核心引擎
 *
 * 协调所有组件，提供统一的入库和查询接口
 */
class RAGEngine {
public:
    RAGEngine();
    ~RAGEngine();

    // 禁止拷贝
    RAGEngine(const RAGEngine&) = delete;
    RAGEngine& operator=(const RAGEngine&) = delete;

    // ========== 初始化 ==========

    /**
     * @brief 初始化引擎
     */
    void init(const EngineConfig& config);

    /**
     * @brief 关闭引擎
     */
    void shutdown();

    /**
     * @brief 获取引擎状态
     */
    EngineState state() const { return state_; }

    // ========== 索引构建 ==========

    /**
     * @brief 构建索引
     * @param paths 要索引的路径列表
     * @return 构建结果
     */
    IndexBuildResult build_index(const std::vector<std::string>& paths = {});

    /**
     * @brief 增量更新索引
     * @param path 要更新的路径
     * @return 构建结果
     */
    IndexBuildResult update_index(const std::string& path);

    /**
     * @brief 清空索引
     */
    void clear_index();

    // ========== 查询 ==========

    /**
     * @brief 查询
     * @param query 查询文本
     * @param top_k 返回结果数
     * @return 查询结果
     */
    QueryResult query(const std::string& query, int top_k = 5);

    /**
     * @brief 仅检索（不生成答案）
     */
    std::vector<RetrievalResult> retrieve(const std::string& query, int top_k = 5);

    // ========== 文档管理 ==========

    /**
     * @brief 添加文档（无进度回调）
     */
    void add_document(const Document& doc);

    /**
     * @brief 添加文档（带进度回调）
     * @param doc 文档对象
     * @param progress_callback 进度回调: (processed_chunks, total_chunks, stage_name)
     */
    void add_document(const Document& doc,
                      std::function<void(int, int, const std::string&)> progress_callback);

    /**
     * @brief 使用当前 Embedding 服务编码文本（公开 API，便于测试）
     */
    std::vector<float> encode_text(const std::string& text);

    /**
     * @brief 获取当前 Embedding 服务（只读）
     */
    std::shared_ptr<EmbeddingService> embedding_service() const { return embed_service_; }

    /**
     * @brief 删除文档
     */
    void remove_document(const std::string& doc_id);

    /**
     * @brief 获取文档
     */
    std::optional<Document> get_document(const std::string& doc_id);

    /**
     * @brief 列出所有文档
     */
    std::vector<Document> list_documents();

    // ========== 统计信息 ==========

    /**
     * @brief 获取索引统计
     */
    IndexStatus get_index_status() const;

    /**
     * @brief 获取配置
     */
    const Config& config() const { return config_; }

    // ========== Graph RAG 支持 ==========

    /**
     * @brief 获取 Graph 引擎扩展
     */
    GraphEngineExtension* graph() { return graph_ext_.get(); }

    /**
     * @brief 获取 Retriever（公开接口，用于 pipeline 注入依赖）
     */
    std::shared_ptr<Retriever> retriever() const { return retriever_; }

    /**
     * @brief 构建知识图谱
     */
    GraphBuildResult build_knowledge_graph();

    /**
     * @brief 增量更新知识图谱
     */
    GraphBuildResult update_knowledge_graph(const std::vector<std::string>& chunk_ids);

    /**
     * @brief Graph 检索
     */
    GraphRetrievalResult graph_retrieve(const std::string& query, int top_k = 10);

    /**
     * @brief 检查 Graph 模式是否启用
     */
    bool is_graph_enabled() const;

    // ========== 评估支持 ==========

    /**
     * @brief 获取评估器
     */
    std::shared_ptr<RAGEvaluator> evaluator() { return evaluator_; }

    /**
     * @brief 设置 LLM 服务（用于评估）
     */
    void set_llm_service(std::shared_ptr<LLMService> llm);

    /**
     * @brief 评估查询
     */
    EvaluationResult evaluate(const std::string& query,
                              const std::vector<std::string>& ground_truths);

    // ========== 持久化 ==========

    /**
     * @brief 保存索引
     */
    void save_index(const std::filesystem::path& path = "");

    /**
     * @brief 加载索引
     */
    void load_index(const std::filesystem::path& path = "");

private:
    // 初始化组件
    void init_components();

    // 扫描文件
    std::vector<std::filesystem::path> scan_files(const std::string& path);

    // 处理单个文件
    bool process_file(const std::filesystem::path& file_path);

    // 创建目录
    void create_directories();

    // 验证状态
    void require_state(EngineState expected, const std::string& operation);

    // 引擎配置
    Config config_;

    // 状态（使用 atomic + mutex 双重保护，避免并发请求时的状态竞态）
    std::atomic<EngineState> state_{EngineState::UNINITIALIZED};
    mutable std::mutex state_mutex_;

    // 组件
    std::shared_ptr<Database> db_;
    std::shared_ptr<Chunker> chunker_;
    std::shared_ptr<ParserRegistry> parser_registry_;
    std::shared_ptr<VectorIndex> vector_index_;
    std::shared_ptr<BM25Index> bm25_index_;
    std::shared_ptr<EmbeddingService> embed_service_;
    std::shared_ptr<Retriever> retriever_;

    // Repository
    std::shared_ptr<DocumentRepository> doc_repo_;
    std::shared_ptr<ChunkRepository> chunk_repo_;
    std::shared_ptr<IndexStatusRepository> status_repo_;

    // Graph RAG 扩展
    std::unique_ptr<GraphEngineExtension> graph_ext_;
    GraphConfig graph_config_;

    // 评估器
    std::shared_ptr<RAGEvaluator> evaluator_;
    std::shared_ptr<LLMService> llm_service_;

    // 路径
    std::filesystem::path data_dir_;
    std::filesystem::path index_dir_;
    std::filesystem::path vector_index_path_;
    std::filesystem::path bm25_index_path_;
};

// ========== 工厂函数 ==========

/**
 * @brief 创建 RAG 引擎
 */
std::unique_ptr<RAGEngine> create_engine(const EngineConfig& config);

}  // namespace mmrag
