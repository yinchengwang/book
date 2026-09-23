/**
 * @file intent_gate_config.h
 * @brief 意图门配置（独立小头文件，供 config.h / modular/config.h / intent_gate.h 共用）
 */
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace mmrag {

struct IntentGateConfig {
    bool enabled = true;
    std::string strategy = "rule_then_embedding";  // rule_only | rule_then_embedding
    float embedding_threshold = 0.70f;             // ≥ 判定为拦截（含等号）
    float evidence_threshold = 0.35f;              // 检索 score ≥ 才有证据（含等号）
    std::string embedder_model_dir;                // MiniLM FP32 目录；空 = 禁用 embedding 兜底
    int embedder_dim = 384;

    // 闲聊回复表：category(greeting/thanks/goodbye/capability) → 回复
    std::unordered_map<std::string, std::string> chat_replies;

    std::string out_of_scope_reply =
        "我是知识库问答助手，这个问题不在知识库范围内。你可以问我关于已上传文档的问题。";
    std::string no_evidence_reply =
        "知识库中未找到相关信息，请换个问法或先上传相关文档。";

    // 追加的自定义示例句（内置默认集之外的增量）
    std::vector<std::string> extra_chat_exemplars;
    std::vector<std::string> extra_oos_exemplars;
};

}  // namespace mmrag
