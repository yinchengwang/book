/**
 * @file pipeline_base.h
 * @brief Modular RAG Pipeline 基类（模板方法：意图门 → do_query → 证据门）
 */
#pragma once

#include "mmrag/modular/types.h"
#include "mmrag/modular/config.h"
#include "mmrag/intent_gate.h"
#include "mmrag/llm_service.h"
#include "mmrag/retriever.h"
#include <memory>
#include <mutex>
#include <string>

namespace mmrag::modular {

/**
 * @brief Modular RAG Pipeline 基类
 *
 * query() 为模板方法：先过意图门（闲聊/越界短路），PROCEED 才调用
 * 子类的 do_query()。子类在 do_query() 内、调 LLM 前用
 * has_sufficient_evidence() 做证据门检查。
 */
class ModularPipeline {
public:
    virtual ~ModularPipeline() = default;

    virtual PipelineType type() const = 0;
    virtual std::string name() const = 0;
    virtual bool init(const ModularConfig& config) = 0;

    /**
     * @brief 执行查询（模板方法：意图门 → do_query）
     */
    ModularQueryResult query(const ModularQuery& query);

    virtual bool is_ready() const = 0;

protected:
    /**
     * @brief 子类实现：检索 + 生成（原 query() 逻辑）
     */
    virtual ModularQueryResult do_query(const ModularQuery& query) = 0;

    /**
     * @brief 证据门：检索结果为空或最高分 < evidence_threshold → false
     */
    bool has_sufficient_evidence(const std::vector<RetrievalResult>& results) const;

    /**
     * @brief 构造"知识库未覆盖"结果（routed_by = "evidence_gate"）
     */
    ModularQueryResult make_no_evidence_result(const ModularQuery& query) const;

    std::string build_context(const std::string& query,
                              const std::vector<RetrievalResult>& results);
    std::string generate_with_llm(const std::string& prompt,
                                  const GenerateOptions& options = {});

    std::shared_ptr<LLMService> llm_;
    std::shared_ptr<Retriever> retriever_;
    ModularConfig config_;

private:
    void ensure_intent_gate();   // 惰性构造（std::call_once）

    std::once_flag gate_once_flag_;
    std::unique_ptr<mmrag::IntentGate> intent_gate_;
};

} // namespace mmrag::modular
