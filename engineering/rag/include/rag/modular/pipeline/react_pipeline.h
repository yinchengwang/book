/**
 * @file react_pipeline.h
 * @brief ReActPipeline - ReAct 推理+行动 RAG Pipeline
 *
 * 流程: Query → Agent循环(Thought→Action→Observation) → LLM
 *
 * ReActPipeline 结合推理与行动:
 * - Thought: 分析当前状态，决定下一步行动
 * - Action: 执行检索或其他操作
 * - Observation: 观察行动结果
 * - 循环直到获得足够信息
 * - 最终使用 LLM 生成回答
 *
 * 注意: 此实现依赖 Task 8 的 Agent 实现，
 * 当前使用占位符/条件编译，待 Agent 实现后可启用完整功能
 */
#pragma once

#include "rag/modular/pipeline/pipeline_base.h"
#include "rag/modular/agent/agent.h"
#include "rag/retriever.h"
#include "rag/types.h"
#include <memory>
#include <vector>
#include <string>
#include <deque>

namespace rag::modular {

// ========== ReAct 组件 ==========

/**
 * @brief ReAct 动作类型
 */
enum class ReActAction {
    RETRIEVE,          // 执行检索
    GENERATE,          // 生成回答
    REFINE_QUERY,      // 优化查询
    EXPAND_CONTEXT,    // 扩展上下文
    FINISH             // 结束
};

/**
 * @brief ReAct 步骤
 */
struct ReActStep {
    int step_number = 0;                    // 步骤编号
    ReActAction action = ReActAction::RETRIEVE;  // 执行的动作
    std::string thought;                    // 思考过程
    std::string action_input;               // 动作输入
    std::string observation;                // 观察结果
    bool is_final = false;                  // 是否为最终步骤
};

/**
 * @brief ReAct 状态
 */
struct ReActState {
    std::string original_query;             // 原始查询
    std::string current_query;              // 当前查询
    std::vector<ReActStep> steps;           // 执行步骤
    std::vector<RetrievalResult> all_results;  // 所有检索结果
    std::string final_response;             // 最终回答
};

/**
 * @brief ReActPipeline - ReAct RAG Pipeline
 */
class ReActPipeline : public ModularPipeline {
public:
    ReActPipeline();
    ~ReActPipeline() override;

    /**
     * @brief 获取 Pipeline 类型
     */
    PipelineType type() const override { return PipelineType::REACT; }

    /**
     * @brief 获取 Pipeline 名称
     */
    std::string name() const override { return "ReActPipeline"; }

    /**
     * @brief 初始化 Pipeline
     * @param config 配置信息
     * @return 初始化是否成功
     */
    bool init(const ModularConfig& config) override;

    /**
     * @brief 执行查询
     * @param query 查询信息
     * @return 查询结果
     */
    ModularQueryResult query(const ModularQuery& query) override;

    /**
     * @brief 检查 Pipeline 是否就绪
     */
    bool is_ready() const override;

    /**
     * @brief 设置 HNSW 检索器
     */
    void set_hnsw_retriever(std::shared_ptr<rag::HNSWRetriever> retriever);

    /**
     * @brief 设置 BM25 检索器
     */
    void set_bm25_retriever(std::shared_ptr<rag::BM25Retriever> retriever);

    /**
     * @brief 获取检索器
     */
    std::shared_ptr<rag::HNSWRetriever> hnsw_retriever() const { return hnsw_retriever_; }
    std::shared_ptr<rag::BM25Retriever> bm25_retriever() const { return bm25_retriever_; }

    /**
     * @brief 设置最大循环次数
     */
    void set_max_iterations(int max_iterations) { max_iterations_ = max_iterations; }

    /**
     * @brief 获取最大循环次数
     */
    int max_iterations() const { return max_iterations_; }

    /**
     * @brief 获取 ReAct 状态
     */
    const ReActState& get_state() const { return state_; }

    /**
     * @brief 重置状态
     */
    void reset();

private:
    /**
     * @brief 执行 ReAct 循环
     */
    void run_react_loop();

    /**
     * @brief 决定下一步动作
     */
    ReActAction decide_next_action();

    /**
     * @brief 执行动作
     */
    std::string execute_action(ReActAction action);

    /**
     * @brief 生成思考
     */
    std::string generate_thought(ReActAction action);

    /**
     * @brief 执行检索
     */
    std::vector<rag::RetrievalResult> retrieve(const std::string& query, int top_k);

    /**
     * @brief 评估当前状态是否足够生成回答
     */
    bool is_satisfied() const;

    /**
     * @brief 构建最终回答
     */
    std::string build_final_answer();

    /**
     * @brief 构建思考提示词
     */
    std::string build_thought_prompt(const ReActState& state, ReActAction next_action);

    /**
     * @brief 解析 LLM 的动作决定
     */
    ReActAction parse_action_decision(const std::string& response);

    std::shared_ptr<rag::HNSWRetriever> hnsw_retriever_;     // HNSW 向量检索器
    std::shared_ptr<rag::BM25Retriever> bm25_retriever_;     // BM25 全文检索器

    std::unique_ptr<rag::modular::agent::Agent> agent_;     // Agent (Task 8)

    ReActState state_;                                        // ReAct 状态
    bool initialized_ = false;                                // 初始化标志
    int max_iterations_ = 5;                                  // 最大循环次数
    int current_iteration_ = 0;                               // 当前迭代
    float satisfaction_threshold_ = 0.6f;                     // 满足阈值

    // 统计信息
    struct Stats {
        int total_iterations = 0;
        int retrieve_count = 0;
        int refine_count = 0;
        float avg_confidence = 0.0f;
    };
    Stats stats_;
};

}  // namespace rag::modular
