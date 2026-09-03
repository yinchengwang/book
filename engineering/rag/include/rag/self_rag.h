/**
 * @file self_rag.h
 * @brief Self-RAG 和 Corrective-RAG 实现
 *
 * Self-RAG: 通过自我反思 token 评估检索结果质量
 * Corrective-RAG: 根据评估结果决定是否需要修正检索策略
 */
#pragma once

#include "rag/pipeline.h"
#include "rag/reranker.h"
#include "rag/llm_service.h"
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

namespace rag {

// ========== Self-RAG Reflection Tokens ==========

/**
 * @brief 自我反思 Token
 *
 * LLM 生成时输出这些特殊 token 来评估检索内容
 */
enum class ReflectionToken : uint8_t {
    // 基础标记
    START_REFLECTION = 0x01,   // 开始反思
    END_REFLECTION = 0x02,     // 结束反思

    // 相关性评估
    IS_RELEVANT = 0x10,        // 检索内容与查询相关
    IS_NOT_RELEVANT = 0x11,    // 检索内容不相关

    // 完整性评估
    IS_COMPLETE = 0x20,        // 检索内容完整回答了问题
    IS_PARTIAL = 0x21,         // 检索内容只部分回答了问题
    IS_SUPPORTED = 0x22,       // 检索内容支持生成

    // 有用性评估
    IS_USEFUL = 0x30,          // 检索内容对回答有用
    IS_NOT_USEFUL = 0x31,      // 检索内容对回答无用

    // 组合标记
    FULLY_USEFUL = 0x32,       // 完全有用: IS_RELEVANT | IS_SUPPORTED | IS_USEFUL
};

/**
 * @brief 反思评估结果
 */
struct ReflectionResult {
    bool is_relevant = false;      // 相关性
    bool is_supported = false;     // 支持性
    bool is_complete = false;      // 完整性
    bool is_useful = false;        // 有用性

    float relevance_score = 0.0f;
    float support_score = 0.0f;
    float completeness_score = 0.0f;
    float usefulness_score = 0.0f;

    // 组合得分
    float overall_score() const {
        return (relevance_score + support_score + completeness_score + usefulness_score) / 4.0f;
    }

    std::string to_string() const;
};

/**
 * @brief 解析 LLM 输出的反思 token
 */
ReflectionResult parse_reflection_tokens(const std::string& llm_output);

// ========== Self-RAG 配置 ==========

struct SelfRAGConfig {
    // 启用自我检查
    bool enable_self_check = true;

    // 最大检索轮次
    int max_retrieval_turns = 3;

    // 阈值
    float relevance_threshold = 0.5f;
    float support_threshold = 0.3f;
    float completeness_threshold = 0.3f;
    float usefulness_threshold = 0.5f;

    // 综合阈值 (用于判断是否接受当前结果)
    float acceptance_threshold = 0.4f;

    // 是否使用 LLM 进行评估
    bool use_llm_evaluation = true;

    // LLM 配置
    std::string llm_model = "gpt-4";
    std::string llm_endpoint;

    // 评估模式
    enum class EvaluationMode {
        TOKEN_BASED,    // 基于 token 匹配
        LLM_JUDGE,      // LLM 判断
        HYBRID          // 混合
    };
    EvaluationMode evaluation_mode = EvaluationMode::HYBRID;

    // 日志级别
    int verbosity = 1;
};

// ========== Corrective-RAG 动作 ==========

/**
 * @brief 修正动作类型
 */
enum class CorrectiveAction {
    PASS,           // 直接使用当前结果
    REWRITE,        // 重写查询后检索
    REPEAT,         // 重新检索
    EXPAND,         // 扩展检索 (增加 top_k)
    WEB_FALLBACK,   // 回退到 Web 搜索
    DIRECT_GENERATE // 直接生成 (无足够上下文)
};

/**
 * @brief 修正决策结果
 */
struct CorrectiveDecision {
    CorrectiveAction action;
    float confidence;
    std::string reason;
    std::string new_query;  // 如果需要重写查询
};

// ========== Self-RAG Stage ==========

/**
 * @brief Self-RAG Stage
 *
 * 作为 Pipeline 的一个 Stage 集成
 */
class SelfRAGStage : public RetrievalStage {
public:
    /**
     * @brief 构造函数
     * @param config 配置
     * @param llm_service LLM 服务 (可选)
     */
    explicit SelfRAGStage(
        const SelfRAGConfig& config,
        std::shared_ptr<LLMService> llm_service = nullptr);

    ~SelfRAGStage() override = default;

    // ========== RetrievalStage 接口 ==========

    std::string name() const override { return "self_rag"; }

    StageType type() const override { return StageType::SELF_RAG; }

    bool supports(QueryType query_type) const override {
        return true;  // 所有查询类型都支持
    }

    bool is_ready() const override { return true; }

    StageOutput process(const StageInput& input) override;

    // ========== 评估方法 ==========

    /**
     * @brief 评估 chunk 的相关性
     */
    ReflectionResult evaluate_chunk(
        const std::string& query,
        const Chunk& chunk);

    /**
     * @brief 批量评估 chunks
     */
    std::vector<ReflectionResult> evaluate_chunks(
        const std::string& query,
        const std::vector<Chunk>& chunks);

    /**
     * @brief 判断是否需要重新检索
     */
    bool should_rewrite(
        const std::vector<ReflectionResult>& evaluations);

    /**
     * @brief 根据评估过滤 chunks
     */
    std::vector<Chunk> filter_by_threshold(
        const std::vector<Chunk>& chunks,
        const std::vector<ReflectionResult>& evaluations);

    // ========== 配置 ==========

    void update_config(const SelfRAGConfig& config);
    const SelfRAGConfig& config() const { return config_; }

private:
    // 禁用拷贝
    SelfRAGStage(const SelfRAGStage&) = delete;
    SelfRAGStage& operator=(const SelfRAGStage&) = delete;

    // LLM 评估
    ReflectionResult llm_evaluate(
        const std::string& query,
        const Chunk& chunk);

    // Token 评估
    ReflectionResult token_evaluate(
        const std::string& llm_output);

    // 构建评估 prompt
    std::string build_evaluation_prompt(
        const std::string& query,
        const Chunk& chunk);

    // 解析 LLM 评估结果
    ReflectionResult parse_llm_evaluation(const std::string& response);

    SelfRAGConfig config_;
    std::shared_ptr<LLMService> llm_service_;
};

// ========== Corrective-RAG ==========

/**
 * @brief Corrective-RAG 决策器
 *
 * 根据评估结果决定下一步动作
 */
class CorrectiveRAG {
public:
    explicit CorrectiveRAG(const SelfRAGConfig& config);
    ~CorrectiveRAG() = default;

    /**
     * @brief 决定修正动作
     * @param chunks 当前检索到的 chunks
     * @param evaluations 评估结果
     * @param avg_score 平均得分
     * @return 修正决策
     */
    CorrectiveDecision decide_action(
        const std::vector<Chunk>& chunks,
        const std::vector<ReflectionResult>& evaluations,
        float avg_score);

    /**
     * @brief 根据动作更新查询
     */
    std::string rewrite_query(
        const std::string& original_query,
        const CorrectiveAction& action,
        const std::vector<Chunk>& chunks);

    /**
     * @brief 评估是否需要 web 搜索回退
     */
    bool should_use_web_fallback(float avg_score, int chunk_count);

private:
    // 评估检索质量
    float calculate_quality_score(
        const std::vector<ReflectionResult>& evaluations);

    // 决定检索策略
    CorrectiveAction decide_by_quality(float quality_score);

    SelfRAGConfig config_;

    // 质量历史
    std::vector<float> quality_history_;
};

// ========== Self-RAG Pipeline ==========

/**
 * @brief Self-RAG 包装器
 *
 * 包装整个检索流程，实现自我修正
 */
class SelfRAGPipeline {
public:
    /**
     * @brief 构造函数
     * @param base_pipeline 基础 Pipeline
     * @param config 配置
     * @param llm_service LLM 服务
     */
    SelfRAGPipeline(
        std::unique_ptr<RetrievalPipeline> base_pipeline,
        const SelfRAGConfig& config,
        std::shared_ptr<LLMService> llm_service = nullptr);

    ~SelfRAGPipeline() = default;

    /**
     * @brief 执行带自我修正的检索
     * @param query 查询
     * @param top_k 返回结果数
     * @return 最终结果
     */
    PipelineResult execute(const std::string& query, int top_k);

    /**
     * @brief 异步执行
     */
    std::future<PipelineResult> execute_async(const std::string& query, int top_k);

    // 统计信息
    struct Stats {
        int total_turns = 0;
        int successful_turns = 0;
        int rewrite_count = 0;
        int repeat_count = 0;
        int web_fallback_count = 0;
        float avg_quality_score = 0.0f;
    };

    Stats get_stats() const { return stats_; }
    void reset_stats() { stats_ = Stats(); }

private:
    // 执行一轮检索
    PipelineResult execute_turn(
        const std::string& query,
        int top_k,
        int turn_number);

    // 处理修正决策
    std::pair<CorrectiveAction, std::string> handle_corrective(
        const CorrectiveDecision& decision,
        const std::string& current_query);

    std::unique_ptr<RetrievalPipeline> base_pipeline_;
    SelfRAGConfig config_;
    std::shared_ptr<LLMService> llm_service_;
    CorrectiveRAG corrective_;

    Stats stats_;
};

// ========== Factory ==========

/**
 * @brief 创建 Self-RAG Stage
 */
std::shared_ptr<SelfRAGStage> create_self_rag_stage(
    const SelfRAGConfig& config,
    std::shared_ptr<LLMService> llm_service = nullptr);

/**
 * @brief 创建 Corrective-RAG
 */
std::unique_ptr<CorrectiveRAG> create_corrective_rag(
    const SelfRAGConfig& config);

/**
 * @brief 创建 Self-RAG Pipeline
 */
std::unique_ptr<SelfRAGPipeline> create_self_rag_pipeline(
    std::unique_ptr<RetrievalPipeline> base_pipeline,
    const SelfRAGConfig& config,
    std::shared_ptr<LLMService> llm_service = nullptr);

}  // namespace rag
