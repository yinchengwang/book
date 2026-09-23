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
    static const std::unordered_map<std::string, std::string> kDefaults = {
        {"greeting",  "你好！我是知识库问答助手，可以问我关于已上传文档的问题。"},
        {"thanks",    "不客气！有问题随时问我。"},
        {"goodbye",   "再见！有问题随时回来。"},
        {"capability","我是知识库问答助手，可以基于你上传的文档回答问题。试试问我文档相关的问题吧。"}
    };
    // 用户部分覆盖时补齐缺失键，保证 pick_chat_reply 的 fallback 永不抛异常（fail-open）
    for (const auto& [key, value] : kDefaults) {
        if (config_.chat_replies.find(key) == config_.chat_replies.end()) {
            config_.chat_replies[key] = value;
        }
    }
}

void IntentGate::init_exemplars() {
    static const std::vector<std::string> kChat = {
        "你好", "您好", "嗨", "哈喽", "早上好", "晚上好", "在吗", "在不在",
        "谢谢", "谢谢你", "非常感谢", "再见", "拜拜", "你能做什么", "你是谁",
        "hello", "hi", "good morning", "thanks", "goodbye", "how are you"
    };
    static const std::vector<std::string> kOos = {
        "帮我写个周报", "帮我写一封邮件", "写一篇作文", "讲个笑话", "讲个故事",
        "今天天气怎么样", "明天会下雨吗", "给我推荐几部电影", "股票会涨吗",
        "陪我聊聊天", "我好无聊", "write an email for me", "tell me a joke",
        "what's the weather today"
    };

    auto encode_all = [&](const std::vector<std::string>& texts,
                          std::vector<std::vector<float>>& out) {
        for (const auto& t : texts) {
            auto v = embedder_->encode(t);
            if (!v.empty()) out.push_back(std::move(v));
        }
    };

    if (embedder_) {
        encode_all(kChat, chat_exemplar_embs_);
        encode_all(config_.extra_chat_exemplars, chat_exemplar_embs_);
        encode_all(kOos, oos_exemplar_embs_);
        encode_all(config_.extra_oos_exemplars, oos_exemplar_embs_);
        RAG_INFO("IntentGate 示例句编码完成: chat=" +
                 std::to_string(chat_exemplar_embs_.size()) +
                 " oos=" + std::to_string(oos_exemplar_embs_.size()));
    }
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
        return it != replies.end() ? it->second
                                   : std::string("你好！我是知识库问答助手。");
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

RouteDecision IntentGate::decide_by_embedding(const std::string& query) {
    RouteDecision d;
    d.matched_by = "embedding";

    auto q = embedder_->encode(query);
    if (q.empty()) return d;  // encode 失败，放行

    // encode() 返回 L2 归一化向量，点积即余弦相似度
    auto best_sim = [&](const std::vector<std::vector<float>>& embs) {
        float best = 0.0f;
        for (const auto& e : embs) {
            float dot = 0.0f;
            for (size_t i = 0; i < q.size() && i < e.size(); ++i) dot += q[i] * e[i];
            if (dot > best) best = dot;
        }
        return best;
    };

    float chat_sim = best_sim(chat_exemplar_embs_);
    float oos_sim  = best_sim(oos_exemplar_embs_);
    float thr = config_.embedding_threshold;

    if (chat_sim >= thr && chat_sim >= oos_sim) {
        d.action = RouteAction::CHAT_REPLY;
        d.confidence = chat_sim;
        d.reply = pick_chat_reply(query);
    } else if (oos_sim >= thr) {
        d.action = RouteAction::OUT_OF_SCOPE;
        d.confidence = oos_sim;
        d.reply = config_.out_of_scope_reply;
    }
    return d;
}

IntentGate::Stats IntentGate::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

}  // namespace mmrag
