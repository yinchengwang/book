/**
 * @file community.h
 * @brief 知识图谱社区检测与摘要
 *
 * 功能:
 * - 社区检测 (Louvain/Label Propagation)
 * - 社区摘要生成 (LLM)
 * - 多层次社区结构
 */
#pragma once

#include "rag/knowledge_graph.h"
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

namespace rag {

// ========== 社区结构 ==========

/**
 * @brief 社区
 */
struct Community {
    std::string id;
    std::string parent_id;           // 父社区
    std::vector<std::string> entity_ids;  // 实体 ID 列表
    int level = 0;                   // 层次 (0 = 最细粒度)
    int size = 0;                    // 社区大小

    // 社区内关系统计
    int internal_edges = 0;
    int external_edges = 0;

    // LLM 生成的摘要
    std::string summary;
    float summary_confidence = 0.0f;

    // 子社区
    std::vector<std::string> sub_community_ids;

    // 关键实体 (中心性最高的几个)
    std::vector<std::string> central_entities;
};

/**
 * @brief 社区检测结果
 */
struct CommunityDetectionResult {
    bool success = false;
    std::vector<Community> communities;
    std::unordered_map<std::string, std::string> entity_to_community;  // 实体 -> 社区
    int total_levels = 0;
    std::string algorithm;
    double modularity = 0.0;  // 模块度
    int64_t processing_time_ms = 0;
};

// ========== 社区摘要请求 ==========

/**
 * @brief 社区摘要请求
 */
struct CommunitySummaryRequest {
    std::string community_id;
    const KnowledgeGraph* graph;  // 指向图的指针

    // 摘要配置
    int max_length = 200;
    bool include_examples = true;
    int max_examples = 3;

    // 上下文
    std::string context;
};

/**
 * @brief 社区摘要结果
 */
struct CommunitySummaryResult {
    std::string community_id;
    std::string summary;
    float confidence = 0.0f;

    // 社区特征
    std::vector<std::string> main_themes;
    std::vector<std::string> key_entities;
    std::vector<std::string> key_relations;

    // 示例实体
    std::vector<std::pair<std::string, std::string>> example_entities;
};

// ========== CommunityDetector ==========

/**
 * @brief 社区检测器
 */
class CommunityDetector {
public:
    /**
     * @brief 构造函数
     * @param config 配置
     */
    explicit CommunityDetector(const Config& config = {});

    /**
     * @brief 析构函数
     */
    ~CommunityDetector();

    // ========== 社区检测 ==========

    /**
     * @brief 检测社区
     * @param graph 知识图谱
     * @param algorithm 算法: "louvain", "label_propagation", "leiden"
     * @return 检测结果
     */
    CommunityDetectionResult detect_communities(
        const KnowledgeGraph& graph,
        const std::string& algorithm = "louvain");

    /**
     * @brief 检测层次化社区
     * @param graph 知识图谱
     * @param levels 层次数
     * @return 检测结果
     */
    CommunityDetectionResult detect_hierarchical_communities(
        const KnowledgeGraph& graph,
        int levels = 3);

    // ========== 社区操作 ==========

    /**
     * @brief 获取实体所属社区
     */
    std::string get_entity_community(const std::string& entity_id) const;

    /**
     * @brief 获取社区内的实体
     */
    std::vector<Entity> get_community_entities(
        const std::string& community_id) const;

    /**
     * @brief 获取社区内的关系
     */
    std::vector<Relation> get_community_relations(
        const std::string& community_id) const;

    /**
     * @brief 获取子社区
     */
    std::vector<std::string> get_sub_communities(
        const std::string& community_id) const;

    /**
     * @brief 计算模块度
     */
    double calculate_modularity(const KnowledgeGraph& graph) const;

    // ========== 配置 ==========

    struct Config {
        // 算法参数
        float resolution = 1.0f;       // Louvain 分辨率参数
        int max_iterations = 100;      // 最大迭代次数
        float tolerance = 1e-5f;       // 收敛容差

        // 层次化
        bool enable_hierarchical = true;
        int min_community_size = 3;    // 最小社区大小
        int max_community_size = 100;  // 最大社区大小

        // 缓存
        bool cache_results = true;
    };

    void update_config(const Config& config);
    const Config& config() const { return config_; }

private:
    // 禁用拷贝
    CommunityDetector(const CommunityDetector&) = delete;
    CommunityDetector& operator=(const CommunityDetector&) = delete;

    // Louvain 算法
    CommunityDetectionResult detect_louvain(const KnowledgeGraph& graph);

    // Label Propagation 算法
    CommunityDetectionResult detect_label_propagation(const KnowledgeGraph& graph);

    // Leiden 算法
    CommunityDetectionResult detect_leiden(const KnowledgeGraph& graph);

    // 构建辅助结构
    void build_adjacency_list(const KnowledgeGraph& graph);
    void compute_centralities(const KnowledgeGraph& graph);

    // 合并小社区
    std::vector<Community> merge_small_communities(
        std::vector<Community>& communities);

    // 提取中心实体
    std::vector<std::string> extract_central_entities(
        const Community& community,
        int top_k = 5);

    Config config_;

    // 缓存
    std::unordered_map<std::string, std::string> entity_community_cache_;
    std::vector<Community> cached_communities_;

    // 辅助结构
    std::unordered_map<std::string, std::vector<std::string>> adjacency_list_;
    std::unordered_map<std::string, float> betweenness_;
    std::unordered_map<std::string, float> degree_centrality_;
};

// ========== CommunitySummarizer ==========

/**
 * @brief 社区摘要生成器
 */
class CommunitySummarizer {
public:
    /**
     * @brief 构造函数
     * @param llm_service LLM 服务
     */
    explicit CommunitySummarizer(std::shared_ptr<LLMService> llm_service);

    /**
     * @brief 析构函数
     */
    ~CommunitySummarizer() = default;

    // ========== 摘要生成 ==========

    /**
     * @brief 生成社区摘要
     * @param request 摘要请求
     * @return 摘要结果
     */
    CommunitySummaryResult generate_summary(const CommunitySummaryRequest& request);

    /**
     * @brief 批量生成摘要
     */
    std::vector<CommunitySummaryResult> generate_summaries_batch(
        const std::vector<CommunitySummaryRequest>& requests,
        int max_parallel = 5);

    // ========== 配置 ==========

    struct Config {
        std::string model = "gpt-4";
        int max_tokens = 500;
        float temperature = 0.3f;
        int max_examples = 3;
        bool include_relations = true;
    };

    void update_config(const Config& config);
    const Config& config() const { return config_; }

private:
    // 构建摘要 prompt
    std::string build_summary_prompt(
        const Community& community,
        const KnowledgeGraph& graph,
        const std::string& context);

    // 解析 LLM 响应
    CommunitySummaryResult parse_summary_response(
        const std::string& response,
        const std::string& community_id);

    // 提取实体信息
    std::vector<std::pair<std::string, std::string>> extract_entity_info(
        const Community& community,
        const KnowledgeGraph& graph,
        int max_count);

    Config config_;
    std::shared_ptr<LLMService> llm_service_;
};

// ========== GraphRAG Enhancer ==========

/**
 * @brief Graph RAG 增强器
 *
 * 集成社区检测和摘要，增强 Graph RAG 能力
 */
class GraphRAGEnhancer {
public:
    /**
     * @brief 构造函数
     * @param detector 社区检测器
     * @param summarizer 摘要生成器
     */
    GraphRAGEnhancer(
        std::shared_ptr<CommunityDetector> detector,
        std::shared_ptr<CommunitySummarizer> summarizer);

    ~GraphRAGEnhancer() = default;

    // ========== 核心功能 ==========

    /**
     * @brief 构建社区索引
     * @param graph 知识图谱
     * @param build_summaries 是否生成摘要
     * @return 是否成功
     */
    bool build_community_index(
        const KnowledgeGraph& graph,
        bool build_summaries = true);

    /**
     * @brief 检索社区
     * @param query 查询
     * @param top_k 返回数量
     * @return 检索到的社区
     */
    std::vector<Community> retrieve_communities(
        const std::string& query,
        int top_k = 5);

    /**
     * @brief 获取查询相关社区的摘要
     * @param query 查询
     * @param top_k 社区数量
     * @return 摘要列表
     */
    std::vector<CommunitySummaryResult> get_relevant_summaries(
        const std::string& query,
        int top_k = 5);

    /**
     * @brief 获取回答查询所需的社区上下文
     * @param query 查询
     * @param graph 知识图谱
     * @return 上下文 chunks
     */
    std::vector<RetrievalChunk> get_query_context(
        const std::string& query,
        const KnowledgeGraph& graph);

    // ========== 状态 ==========

    bool is_ready() const {
        return detector_ != nullptr;
    }

    int community_count() const { return communities_.size(); }
    int levels() const { return max_level_; }

private:
    // 过滤相关社区
    std::vector<Community> filter_relevant_communities(
        const std::string& query,
        const std::vector<Community>& communities);

    // 构建社区上下文
    std::vector<RetrievalChunk> build_context_chunks(
        const std::vector<Community>& communities,
        const KnowledgeGraph& graph);

    std::shared_ptr<CommunityDetector> detector_;
    std::shared_ptr<CommunitySummarizer> summarizer_;

    std::vector<Community> communities_;
    std::unordered_map<std::string, CommunitySummaryResult> summaries_;
    std::unordered_map<std::string, std::vector<std::string>> query_to_communities_;

    int max_level_ = 0;
    int64_t last_update_time_ = 0;
};

// ========== Factory ==========

/**
 * @brief 创建社区检测器
 */
std::shared_ptr<CommunityDetector> create_community_detector(
    const CommunityDetector::Config& config = {});

/**
 * @brief 创建社区摘要生成器
 */
std::shared_ptr<CommunitySummarizer> create_community_summarizer(
    std::shared_ptr<LLMService> llm_service,
    const CommunitySummarizer::Config& config = {});

/**
 * @brief 创建 Graph RAG 增强器
 */
std::shared_ptr<GraphRAGEnhancer> create_graph_rag_enhancer(
    std::shared_ptr<CommunityDetector> detector = nullptr,
    std::shared_ptr<CommunitySummarizer> summarizer = nullptr,
    std::shared_ptr<LLMService> llm_service = nullptr);

}  // namespace rag
