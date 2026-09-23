#include <gtest/gtest.h>
#include <cstdlib>
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

// 20 样本路由准确率表：规则层必须 100% 通过（不依赖 embedding 模型）
TEST(IntentGate, RoutingAccuracyTable) {
    auto gate = make_gate();
    struct Case { const char* query; RouteAction want; };
    const Case cases[] = {
        // 闲聊 8 条
        {"你好", RouteAction::CHAT_REPLY},
        {"您好", RouteAction::CHAT_REPLY},
        {"hi", RouteAction::CHAT_REPLY},
        {"hello", RouteAction::CHAT_REPLY},
        {"谢谢", RouteAction::CHAT_REPLY},
        {"thanks", RouteAction::CHAT_REPLY},
        {"再见", RouteAction::CHAT_REPLY},
        {"你能做什么", RouteAction::CHAT_REPLY},
        // 越界 8 条
        {"帮我写个工作总结", RouteAction::OUT_OF_SCOPE},
        {"帮我写一封请假邮件", RouteAction::OUT_OF_SCOPE},
        {"讲个笑话吧", RouteAction::OUT_OF_SCOPE},
        {"今天天气怎么样", RouteAction::OUT_OF_SCOPE},
        {"陪我聊聊天", RouteAction::OUT_OF_SCOPE},
        {"陪我聊天", RouteAction::OUT_OF_SCOPE},
        {"推荐一下明天的股票", RouteAction::OUT_OF_SCOPE},
        {"昨晚比分多少", RouteAction::OUT_OF_SCOPE},
        // 知识库问题 5 条（不得误拦）
        {"什么是HNSW索引", RouteAction::PROCEED},
        {"向量检索和BM25的区别", RouteAction::PROCEED},
        {"如何配置data_dir", RouteAction::PROCEED},
        {"这个配置生效了吗", RouteAction::PROCEED},
        {"总结这篇文档的要点", RouteAction::PROCEED},
    };
    int wrong = 0;
    for (const auto& c : cases) {
        auto d = gate.route(c.query);
        if (d.action != c.want) {
            wrong++;
            ADD_FAILURE() << "misrouted: " << c.query
                          << " want=" << static_cast<int>(c.want)
                          << " got=" << static_cast<int>(d.action)
                          << " matched_by=" << d.matched_by;
        }
    }
    EXPECT_EQ(wrong, 0);
}

// embedding 兜底：仅在提供 FP32 模型目录时运行（否则 GTEST_SKIP）
TEST(IntentGate, EmbeddingFallbackCatchesVariant) {
    const char* dir = std::getenv("MMRAG_TEST_FP32_DIR");
    if (!dir || !*dir) GTEST_SKIP() << "set MMRAG_TEST_FP32_DIR to run";
    auto embedder = std::make_shared<MiniLMEmbedder>(dir, 384);
    if (!embedder->is_ready()) GTEST_SKIP() << "embedder not ready";

    IntentGateConfig cfg;
    IntentGate gate(cfg, embedder);
    // 规则未覆盖的闲聊变体（不含任何规则关键词）
    auto d = gate.route("good morning");
    EXPECT_EQ(d.action, RouteAction::CHAT_REPLY);
    EXPECT_EQ(d.matched_by, "embedding");
}
