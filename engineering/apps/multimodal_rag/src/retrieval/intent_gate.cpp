/**
 * @file intent_gate.cpp
 * @brief 意图门实现
 */

#include "mmrag/intent_gate.h"
#include "mmrag/logger.h"

namespace mmrag {

IntentGate::IntentGate(const IntentGateConfig& config,
                       std::shared_ptr<MiniLMEmbedder> embedder)
    : config_(config), embedder_(std::move(embedder)) {
    init_defaults();
    init_exemplars();
}

void IntentGate::init_defaults() {
    if (config_.chat_replies.empty()) {
        config_.chat_replies = {
            {"greeting",  "你好！我是知识库问答助手，可以问我关于已上传文档的问题。"},
            {"thanks",    "不客气！有问题随时问我。"},
            {"goodbye",   "再见！有问题随时回来。"},
            {"capability","我是知识库问答助手，可以基于你上传的文档回答问题。试试问我文档相关的问题吧。"}
        };
    }
}

void IntentGate::init_exemplars() {
    // 内置示例句；embedding 预编码在 Task 3 实现，本任务只留空
}

RouteDecision IntentGate::route(const std::string& query) {
    RouteDecision d;

    if (!config_.enabled) {
        d.matched_by = "disabled";
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total++; stats_.proceed++;
        return d;
    }

    // 1. 规则层
    auto [type, conf] = rule_classifier_.classify_with_confidence(query);
    if (type == QueryType::CHAT && conf >= 0.5f) {
        d.action = RouteAction::CHAT_REPLY;
        d.confidence = conf;
        d.matched_by = "rule";
        d.reply = pick_chat_reply(query);
    } else if (type == QueryType::OUT_OF_SCOPE && conf >= 0.5f) {
        d.action = RouteAction::OUT_OF_SCOPE;
        d.confidence = conf;
        d.matched_by = "rule";
        d.reply = config_.out_of_scope_reply;
    } else if (embedder_ && config_.strategy == "rule_then_embedding") {
        // 2. embedding 兜底（Task 3 实现），异常时 fail-open
        try {
            d = decide_by_embedding(query);
        } catch (const std::exception& e) {
            RAG_WARN("IntentGate embedding 异常，放行: " + std::string(e.what()));
            d = RouteDecision{};
            d.matched_by = "error_fallback";
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.errors++;
        }
    } else {
        d.confidence = conf;
        d.matched_by = "rule";
    }

    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.total++;
    switch (d.action) {
        case RouteAction::CHAT_REPLY:   stats_.chat_replies++; break;
        case RouteAction::OUT_OF_SCOPE: stats_.out_of_scope++; break;
        case RouteAction::PROCEED:      stats_.proceed++;      break;
    }
    return d;
}

std::string IntentGate::pick_chat_reply(const std::string& query) const {
    const auto& replies = config_.chat_replies;
    auto get = [&](const char* key) -> std::string {
        auto it = replies.find(key);
        return it != replies.end() ? it->second : replies.at("greeting");
    };
    if (query.find("谢谢") != std::string::npos ||
        query.find("感谢") != std::string::npos ||
        query.find("thank") != std::string::npos) return get("thanks");
    if (query.find("再见") != std::string::npos ||
        query.find("拜拜") != std::string::npos ||
        query.find("bye")  != std::string::npos) return get("goodbye");
    if (query.find("你能") != std::string::npos ||
        query.find("你会") != std::string::npos ||
        query.find("你是谁") != std::string::npos ||
        query.find("who are you") != std::string::npos) return get("capability");
    return get("greeting");
}

RouteDecision IntentGate::decide_by_embedding(const std::string& /*query*/) {
    // Task 3 实现；当前直接放行
    RouteDecision d;
    d.matched_by = "embedding";
    return d;
}

IntentGate::Stats IntentGate::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

}  // namespace mmrag
