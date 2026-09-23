/**
 * @file intent_gate.h
 * @brief 意图门：检索前路由（闲聊/越界短路，知识库问题放行）
 *
 * 规则优先（RuleBasedQueryClassifier），MiniLM embedding 兜底。
 * fail-open：自身任何故障都按 PROCEED 放行。
 */
#pragma once

#include "mmrag/intent_gate_config.h"
#include "mmrag/query_classifier.h"
#include "mmrag/minilm_embedder.h"
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace mmrag {

enum class RouteAction {
    PROCEED,        // 放行，走正常 RAG
    CHAT_REPLY,     // 闲聊，查表返回
    OUT_OF_SCOPE    // 越界/非提问，固定话术
};

struct RouteDecision {
    RouteAction action = RouteAction::PROCEED;
    float confidence = 0.0f;
    std::string matched_by;  // "rule" | "embedding" | "disabled" | "error_fallback"
    std::string reply;       // CHAT_REPLY / OUT_OF_SCOPE 时的响应文本
};

class IntentGate {
public:
    // embedder 允许为空（nullptr → 自动降级 rule_only）
    IntentGate(const IntentGateConfig& config,
               std::shared_ptr<MiniLMEmbedder> embedder);

    RouteDecision route(const std::string& query);

    struct Stats {
        uint64_t total = 0;
        uint64_t chat_replies = 0;
        uint64_t out_of_scope = 0;
        uint64_t proceed = 0;
        uint64_t errors = 0;
    };
    Stats get_stats() const;

private:
    void init_defaults();      // 填充默认 chat_replies
    void init_exemplars();     // 内置示例句 + extra_*，有 embedder 时预编码
    std::string pick_chat_reply(const std::string& query) const;
    RouteDecision decide_by_embedding(const std::string& query);

    IntentGateConfig config_;
    RuleBasedQueryClassifier rule_classifier_;
    std::shared_ptr<MiniLMEmbedder> embedder_;   // 可能为空
    std::vector<std::vector<float>> chat_exemplar_embs_;
    std::vector<std::vector<float>> oos_exemplar_embs_;
    Stats stats_;
    mutable std::mutex stats_mutex_;
};

}  // namespace mmrag
