#include <gtest/gtest.h>
#include "mmrag/intent_gate.h"

using namespace mmrag;

static IntentGate make_gate() {
    IntentGateConfig cfg;          // 全默认
    return IntentGate(cfg, nullptr);  // nullptr embedder = rule_only
}

TEST(IntentGate, ChatShortCircuit) {
    auto gate = make_gate();
    auto d = gate.route("你好");
    EXPECT_EQ(d.action, RouteAction::CHAT_REPLY);
    EXPECT_EQ(d.matched_by, "rule");
    EXPECT_FALSE(d.reply.empty());
}

TEST(IntentGate, ChatReplyVariesByCategory) {
    auto gate = make_gate();
    EXPECT_NE(gate.route("谢谢你").reply.find("不客气"), std::string::npos);
    EXPECT_NE(gate.route("再见").reply.find("再见"), std::string::npos);
}

TEST(IntentGate, OutOfScopeShortCircuit) {
    auto gate = make_gate();
    auto d = gate.route("帮我写个周报");
    EXPECT_EQ(d.action, RouteAction::OUT_OF_SCOPE);
    EXPECT_NE(d.reply.find("知识库"), std::string::npos);
}

TEST(IntentGate, KnowledgeQuestionProceeds) {
    auto gate = make_gate();
    auto d = gate.route("什么是HNSW索引");
    EXPECT_EQ(d.action, RouteAction::PROCEED);
}

TEST(IntentGate, DisabledMeansProceed) {
    IntentGateConfig cfg;
    cfg.enabled = false;
    IntentGate gate(cfg, nullptr);
    auto d = gate.route("你好");
    EXPECT_EQ(d.action, RouteAction::PROCEED);
    EXPECT_EQ(d.matched_by, "disabled");
}

TEST(IntentGate, StatsAccumulate) {
    auto gate = make_gate();
    gate.route("你好");
    gate.route("帮我写个周报");
    gate.route("什么是索引");
    auto s = gate.get_stats();
    EXPECT_EQ(s.total, 3u);
    EXPECT_EQ(s.chat_replies, 1u);
    EXPECT_EQ(s.out_of_scope, 1u);
    EXPECT_EQ(s.proceed, 1u);
}

TEST(IntentGate, PartialChatRepliesDoesNotThrow) {
    IntentGateConfig cfg;
    cfg.chat_replies = {{"thanks", "自定义感谢"}};  // 缺 greeting/goodbye/capability
    IntentGate gate(cfg, nullptr);
    auto d = gate.route("你好");
    EXPECT_EQ(d.action, RouteAction::CHAT_REPLY);
    EXPECT_FALSE(d.reply.empty());               // 默认 greeting 被补齐
    auto d2 = gate.route("谢谢你");
    EXPECT_EQ(d2.reply, "自定义感谢");            // 用户覆盖生效
}
