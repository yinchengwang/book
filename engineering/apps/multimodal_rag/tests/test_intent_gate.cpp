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

#include "mmrag/config.h"

TEST(IntentGateConfig, YamlParsing) {
    const char* yaml = R"(
intent_gate.enabled: true
intent_gate.strategy: "rule_only"
intent_gate.embedding_threshold: 0.8
intent_gate.evidence_threshold: 0.4
intent_gate.embedder_model_dir: "/tmp/fp32"
intent_gate.out_of_scope_reply: "超出范围"
intent_gate.no_evidence_reply: "没找到"
intent_gate.bad_float_key_should_not_exist: 1
)";
    Config cfg = ConfigLoader().load_from_string(yaml);
    EXPECT_TRUE(cfg.intent_gate.enabled);
    EXPECT_EQ(cfg.intent_gate.strategy, "rule_only");
    EXPECT_FLOAT_EQ(cfg.intent_gate.embedding_threshold, 0.8f);
    EXPECT_FLOAT_EQ(cfg.intent_gate.evidence_threshold, 0.4f);
    EXPECT_EQ(cfg.intent_gate.embedder_model_dir, "/tmp/fp32");
    EXPECT_EQ(cfg.intent_gate.out_of_scope_reply, "超出范围");
    EXPECT_EQ(cfg.intent_gate.no_evidence_reply, "没找到");
}

TEST(IntentGateConfig, InvalidFloatKeepsDefault) {
    const char* yaml = "intent_gate.embedding_threshold: not_a_number\n";
    Config cfg = ConfigLoader().load_from_string(yaml);
    EXPECT_FLOAT_EQ(cfg.intent_gate.embedding_threshold, 0.70f);  // 默认值
}

#include "mmrag/modular/pipeline/pipeline_base.h"
#include "mmrag/modular/config.h"

using namespace mmrag::modular;

namespace {

// 最小测试管线：记录 do_query 是否被调用
class TestPipeline : public ModularPipeline {
public:
    PipelineType type() const override { return PipelineType::NAIVE; }
    std::string name() const override { return "TestPipeline"; }
    bool init(const ModularConfig& config) override { config_ = config; return true; }
    bool is_ready() const override { return true; }
    int do_query_calls = 0;

    // 暴露基类 protected 成员用于测试
    using ModularPipeline::has_sufficient_evidence;
    using ModularPipeline::make_no_evidence_result;

protected:
    ModularQueryResult do_query(const ModularQuery& q) override {
        do_query_calls++;
        ModularQueryResult r;
        r.success = true;
        r.answer = "llm answer";
        return r;
    }
};

static ModularConfig test_config() {
    ModularConfig c;  // intent_gate 全默认（enabled, rule 有效）
    return c;
}

}  // namespace

TEST(TemplateMethod, ChatShortCircuitsBeforeDoQuery) {
    TestPipeline p;
    p.init(test_config());
    ModularQuery q; q.text = "你好";
    auto r = p.query(q);
    EXPECT_EQ(p.do_query_calls, 0);            // 核心断言：子类逻辑未执行
    EXPECT_TRUE(r.success);
    EXPECT_EQ(r.routed_by, "intent_gate_chat");
    EXPECT_FALSE(r.answer.empty());
}

TEST(TemplateMethod, OutOfScopeShortCircuits) {
    TestPipeline p;
    p.init(test_config());
    ModularQuery q; q.text = "帮我写个周报";
    auto r = p.query(q);
    EXPECT_EQ(p.do_query_calls, 0);
    EXPECT_EQ(r.routed_by, "intent_gate_oos");
}

TEST(TemplateMethod, KnowledgeQuestionReachesDoQuery) {
    TestPipeline p;
    p.init(test_config());
    ModularQuery q; q.text = "什么是HNSW索引";
    auto r = p.query(q);
    EXPECT_EQ(p.do_query_calls, 1);
    EXPECT_EQ(r.routed_by, "normal");
    EXPECT_EQ(r.answer, "llm answer");
}

TEST(TemplateMethod, GateDisabledPassesThrough) {
    TestPipeline p;
    auto cfg = test_config();
    cfg.intent_gate.enabled = false;
    p.init(cfg);
    ModularQuery q; q.text = "你好";
    auto r = p.query(q);
    EXPECT_EQ(p.do_query_calls, 1);
    EXPECT_EQ(r.routed_by, "normal");
}

TEST(EvidenceGate, ThresholdSemantics) {
    TestPipeline p;
    p.init(test_config());  // evidence_threshold = 0.35

    std::vector<RetrievalResult> empty;
    EXPECT_FALSE(p.has_sufficient_evidence(empty));

    std::vector<RetrievalResult> low(1);
    low[0].score = 0.34f;
    EXPECT_FALSE(p.has_sufficient_evidence(low));

    std::vector<RetrievalResult> at(1);
    at[0].score = 0.35f;                      // 含等号 → 有证据
    EXPECT_TRUE(p.has_sufficient_evidence(at));

    auto r = p.make_no_evidence_result(ModularQuery{});
    EXPECT_TRUE(r.success);
    EXPECT_EQ(r.routed_by, "evidence_gate");
    EXPECT_NE(r.answer.find("知识库"), std::string::npos);
}
