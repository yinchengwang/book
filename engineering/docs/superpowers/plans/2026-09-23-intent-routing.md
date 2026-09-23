# 意图路由门控（Intent Gate + Evidence Gate）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 multimodal_rag 的 modular 管线前加意图门（非知识库问题短路、不调 LLM），在 LLM 调用前加证据门（检索分数过低不硬答）。

**Architecture:** `ModularPipeline::query()` 改为模板方法：先跑 `IntentGate`（规则优先 + MiniLM embedding 兜底），PROCEED 才进入子类 `do_query()`；子类在 LLM 调用前用基类辅助函数做证据门检查。设计文档：`docs/superpowers/specs/2026-09-23-intent-routing-design.md`。

**Tech Stack:** C++17、CMake（gtest 来自 `third_part/googletest`，`BUILD_TESTING` 开启时可用）、已有组件 `RuleBasedQueryClassifier` / `MiniLMEmbedder`。

## Global Constraints

- 不新增任何第三方依赖。
- `apps/multimodal_rag/CMakeLists.txt` 中**已有的 EXCLUDE 正则规则一律不得删除**（每条都有历史原因）；本计划只在文件末尾追加 `add_subdirectory(tests)`。
- 注释/日志风格跟随现有代码：中文 doc comment、`RAG_INFO`/`RAG_WARN`/`RAG_ERROR`。
- 意图门永远 fail-open（自身故障 = 放行）；回答永远 fail-closed（没证据不硬答）。
- 阈值比较含等号：相似度/分数 **≥** 阈值判定为拦截。
- 命名空间：门相关代码在 `mmrag`；管线改动在 `mmrag::modular`。
- 所有路径相对仓库根 `D:\code\book\engineering`，下文简写为 `<repo>`；应用根为 `<repo>\apps\multimodal_rag`，简写 `<app>`。
- 构建命令中的 `<build>` 指开发者现有的 CMake 构建目录（内含 `bin/`）；Windows 下测试可执行文件为 `mmrag_intent_tests.exe`。

## 与 spec 的两处偏离（已确认合理）

1. **指标**：spec 写 Prometheus counters，但代码库里 `MetricsCollector` 没有全局单例（`create_metrics_collector()` 每次新建）。改为跟随 `BaseQueryClassifier::ClassifierStats` 先例：`IntentGate::get_stats()` 内存计数 + 每次短路 `RAG_INFO` 日志。
2. **评测钩子**：spec 写"golden dataset + evaluator"，改为在 `test_intent_gate.cpp` 里加 20 条闲聊/越界/知识库样本的路由准确率表测试——结果等价且不依赖评测服务器运行。

---

### Task 1: 测试基建 + QueryType::OUT_OF_SCOPE + 规则修复与扩充

**Files:**
- Modify: `<app>/include/mmrag/pipeline.h:49-55`（枚举）与 `:251-253`（needs_retrieval）
- Modify: `<app>/src/pipeline/pipeline.cpp:275-295`（字符串转换）
- Modify: `<app>/src/retrieval/query_classifier.cpp:54-115`（init_rules）
- Modify: `<app>/src/retrieval/adaptive_rrf.cpp:19-60`（switch 补 case）
- Modify: `<app>/apps/multimodal_rag/CMakeLists.txt`（末尾追加一行）
- Create: `<app>/tests/CMakeLists.txt`（覆盖现有占位文件）
- Create: `<app>/tests/test_query_classifier.cpp`
- Test: `<app>/tests/test_query_classifier.cpp`

**Interfaces:**
- Produces: `QueryType::OUT_OF_SCOPE`（`query_type_to_string` → `"out_of_scope"`，`needs_retrieval` → false）；规则分类器对"帮我写个…"返回 OUT_OF_SCOPE，对"这个配置生效了吗"返回 FACTUAL。Task 2/3 的 IntentGate 依赖这些行为。

- [ ] **Step 1: 写失败测试**

创建 `<app>/tests/test_query_classifier.cpp`：

```cpp
#include <gtest/gtest.h>
#include "mmrag/query_classifier.h"
#include "mmrag/pipeline.h"

using namespace mmrag;

TEST(QueryClassifier, ChatGreeting) {
    RuleBasedQueryClassifier c;
    EXPECT_EQ(c.classify("你好"), QueryType::CHAT);
    EXPECT_EQ(c.classify("谢谢你"), QueryType::CHAT);
}

// 现存 bug：".*吗$" 把所有疑问句误判为 CHAT，本测试驱动修复
TEST(QueryClassifier, QuestionMarkSentenceIsNotChat) {
    RuleBasedQueryClassifier c;
    EXPECT_NE(c.classify("这个配置生效了吗"), QueryType::CHAT);
}

TEST(QueryClassifier, OutOfScopeCommand) {
    RuleBasedQueryClassifier c;
    EXPECT_EQ(c.classify("帮我写个周报"), QueryType::OUT_OF_SCOPE);
    EXPECT_EQ(c.classify("讲个笑话"), QueryType::OUT_OF_SCOPE);
    EXPECT_EQ(c.classify("今天天气怎么样"), QueryType::OUT_OF_SCOPE);
}

TEST(QueryClassifier, KnowledgeQuestionProceeds) {
    RuleBasedQueryClassifier c;
    QueryType t = c.classify("什么是HNSW索引");
    EXPECT_NE(t, QueryType::CHAT);
    EXPECT_NE(t, QueryType::OUT_OF_SCOPE);
}

TEST(QueryType, OutOfScopeStringRoundTrip) {
    EXPECT_EQ(query_type_to_string(QueryType::OUT_OF_SCOPE), "out_of_scope");
    EXPECT_EQ(string_to_query_type("out_of_scope"), QueryType::OUT_OF_SCOPE);
}

TEST(QueryType, OutOfScopeNeedsNoRetrieval) {
    RuleBasedQueryClassifier c;
    EXPECT_FALSE(c.needs_retrieval(QueryType::OUT_OF_SCOPE));
    EXPECT_FALSE(c.needs_retrieval(QueryType::CHAT));
    EXPECT_TRUE(c.needs_retrieval(QueryType::FACTUAL));
}
```

- [ ] **Step 2: 接通测试基建并确认编译失败**

覆盖 `<app>/tests/CMakeLists.txt`（当前是占位注释）：

```cmake
# multimodal_rag 单元测试（BUILD_TESTING 且 gtest 可用时构建）
if(NOT BUILD_TESTING)
  return()
endif()
if(NOT TARGET gtest)
  return()
endif()

add_executable(mmrag_intent_tests
    test_query_classifier.cpp
    test_intent_gate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/retrieval/query_classifier.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/retrieval/intent_gate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/pipeline/pipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/pipeline/pipeline_base.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/types.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/observability/logger.cpp
)

target_include_directories(mmrag_intent_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${ENGINEERING_SOURCE_DIR}/third_part/nlohmann/single_include
    ${ENGINEERING_SOURCE_DIR}/include
)

target_link_libraries(mmrag_intent_tests PRIVATE
    gtest gtest_main project_includes Threads::Threads
)

set_target_properties(mmrag_intent_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    FOLDER "Apps/multimodal_rag"
)

add_test(NAME mmrag_intent_tests COMMAND mmrag_intent_tests)
```

注意：`test_intent_gate.cpp` 和 `intent_gate.cpp` 在 Task 2 才创建，本步骤先在 CMake 里留空文件引用会编译失败——**本步骤先把这两行从 `add_executable` 列表里删掉，Task 2 Step 2 再加回来**。

在 `<app>/CMakeLists.txt` 文件**末尾**追加（不得改动/删除任何已有行，尤其是 EXCLUDE 规则）：

```cmake
add_subdirectory(tests)
```

运行构建：

```
cmake --build <build> --target mmrag_intent_tests
```

预期：编译失败（`QueryType::OUT_OF_SCOPE` 未定义）。若出现 `undefined reference` 链接错误，把缺失符号所在的 `<app>/src/**/*.cpp` 追加到 `add_executable` 列表。

- [ ] **Step 3: 实现枚举与字符串转换**

`<app>/include/mmrag/pipeline.h` 枚举改为：

```cpp
enum class QueryType {
    FACTUAL,        // 事实型问题 - 需要精确检索
    ANALYTICAL,     // 分析型问题 - 需要综合分析
    COMPARATIVE,    // 比较型问题 - 需要对比分析
    SUMMARY,        // 总结型问题 - 需要摘要生成
    CHAT,           // 闲聊型 - 可能不需要检索
    MULTI_HOP,      // 多跳问题 - 需要 Graph 检索
    OUT_OF_SCOPE    // 知识库外/非提问型 - 短路拒答
};
```

（以实际现有枚举成员为准，只**追加** `OUT_OF_SCOPE` 一行，不重排已有成员。）

同文件 `needs_retrieval`（约 :251）改为：

```cpp
    virtual bool needs_retrieval(QueryType type) const {
        return type != QueryType::CHAT && type != QueryType::OUT_OF_SCOPE;
    }
```

`<app>/src/pipeline/pipeline.cpp` 两个函数各加分支：

```cpp
// query_type_to_string 中：
        case QueryType::OUT_OF_SCOPE: return "out_of_scope";
// string_to_query_type 中：
    if (str == "out_of_scope") return QueryType::OUT_OF_SCOPE;
```

- [ ] **Step 4: 修复 CHAT 误伤 + 新增 OUT_OF_SCOPE 规则**

`<app>/src/retrieval/query_classifier.cpp` `init_rules()` 中，CHAT 模式整体替换为：

```cpp
    // CHAT: 闲聊（收窄：只匹配明确闲聊句式，不误伤疑问句）
    patterns_[QueryType::CHAT] = {
        R"(你好|您好|hello|hi|hey)",
        R"(谢谢|感谢|thank you|thanks)",
        R"(再见|拜拜|goodbye|bye)",
        R"(^在吗|^在不在|最近如何|how are you)",
        R"(你能做什么|你会什么|介绍一下你自己|who are you)"
    };
```

新增 OUT_OF_SCOPE 模式：

```cpp
    // OUT_OF_SCOPE: 知识库外/非提问型指令
    patterns_[QueryType::OUT_OF_SCOPE] = {
        R"(帮我写|写一封|写个|写一篇)",
        R"(讲个笑话|讲个故事|唱首歌)",
        R"(今天天气|明天天气|天气预报|会下雨吗)",
        R"(陪我聊天|我好无聊|睡不着)",
        R"(股票|股价|彩票|比分)"
    };
```

`analyze_keywords()` 的 if-else 链中追加：

```cpp
        // OUT_OF_SCOPE 关键词
        else if (keyword == "帮我写" || keyword == "笑话" || keyword == "天气" ||
                 keyword == "无聊" || keyword == "write" || keyword == "joke" ||
                 keyword == "weather") {
            type_scores[QueryType::OUT_OF_SCOPE] += 1.0f;
        }
```

- [ ] **Step 5: adaptive_rrf.cpp 补 case**

`<app>/src/retrieval/adaptive_rrf.cpp` `get_weights()` 的 switch 中追加（与 FACTUAL 相同权重）：

```cpp
        case QueryType::OUT_OF_SCOPE:
            // 与 FACTUAL 相同：实际不会走到（意图门已短路），防御性定义
            weights.vector_weight = 0.7f;
            weights.bm25_weight = 0.3f;
            weights.graph_weight = 0.0f;
            break;
```

（字段名以该文件现有 `RRFWeights` 实际成员为准抄写 FACTUAL 分支的值。）

- [ ] **Step 6: 跑测试确认通过**

```
cmake --build <build> --target mmrag_intent_tests
<build>/bin/mmrag_intent_tests.exe
```

预期：7 个测试全 PASS（Windows 下在 `<build>/bin/` 找 exe；其他平台路径无 `.exe`）。

- [ ] **Step 7: Commit**

```bash
cd <repo>
git add apps/multimodal_rag/include/mmrag/pipeline.h \
        apps/multimodal_rag/src/pipeline/pipeline.cpp \
        apps/multimodal_rag/src/retrieval/query_classifier.cpp \
        apps/multimodal_rag/src/retrieval/adaptive_rrf.cpp \
        apps/multimodal_rag/CMakeLists.txt \
        apps/multimodal_rag/tests/
git commit -m "feat(intent-gate): QueryType::OUT_OF_SCOPE + 修复 CHAT 吗字误伤 + 测试基建"
```

---

### Task 2: IntentGateConfig + IntentGate 规则路径

**Files:**
- Create: `<app>/include/mmrag/intent_gate_config.h`
- Create: `<app>/include/mmrag/intent_gate.h`
- Create: `<app>/src/retrieval/intent_gate.cpp`
- Create: `<app>/tests/test_intent_gate.cpp`
- Modify: `<app>/tests/CMakeLists.txt`（把 Task 1 删掉的两个文件名加回来）

**Interfaces:**
- Consumes: Task 1 的 `QueryType::OUT_OF_SCOPE`、`RuleBasedQueryClassifier::classify_with_confidence()`。
- Produces: `mmrag::IntentGateConfig`、`mmrag::RouteAction`、`mmrag::RouteDecision`、`mmrag::IntentGate(config, embedder)->route(query)`、`IntentGate::Stats` / `get_stats()`。Task 3/5 依赖。

- [ ] **Step 1: 写失败测试**

创建 `<app>/tests/test_intent_gate.cpp`：

```cpp
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
```

- [ ] **Step 2: 把测试文件加回 CMake**

`<app>/tests/CMakeLists.txt` 的 `add_executable` 列表恢复 `test_intent_gate.cpp` 和 `../src/retrieval/intent_gate.cpp` 两行（Task 1 Step 2 删掉的）。跑构建确认失败（`mmrag/intent_gate.h` 不存在）。

- [ ] **Step 3: 创建 IntentGateConfig（零依赖头文件）**

创建 `<app>/include/mmrag/intent_gate_config.h`：

```cpp
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
```

- [ ] **Step 4: 创建 IntentGate 头文件**

创建 `<app>/include/mmrag/intent_gate.h`：

```cpp
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
```

- [ ] **Step 5: 实现规则路径（embedding 部分先留 decide_by_embedding 返回 PROCEED）**

创建 `<app>/src/retrieval/intent_gate.cpp`：

```cpp
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
```

- [ ] **Step 6: 跑测试确认通过**

```
cmake --build <build> --target mmrag_intent_tests
<build>/bin/mmrag_intent_tests.exe
```

预期：Task 1 + Task 2 共 13 个测试全 PASS。

- [ ] **Step 7: Commit**

```bash
git add apps/multimodal_rag/include/mmrag/intent_gate_config.h \
        apps/multimodal_rag/include/mmrag/intent_gate.h \
        apps/multimodal_rag/src/retrieval/intent_gate.cpp \
        apps/multimodal_rag/tests/
git commit -m "feat(intent-gate): IntentGate 规则路径（闲聊/越界短路 + stats）"
```

---

### Task 3: IntentGate embedding 兜底 + 路由准确率表测试

**Files:**
- Modify: `<app>/src/retrieval/intent_gate.cpp`（init_exemplars + decide_by_embedding）
- Test: `<app>/tests/test_intent_gate.cpp`（追加）

**Interfaces:**
- Consumes: Task 2 的 `IntentGate` 私有成员；`MiniLMEmbedder::encode()`（返回 L2 归一化 384 维向量）。
- Produces: `IntentGate` 完整行为；无新公开接口。

**⚠️ 中文 vocab 风险**：`all-MiniLM-L6-v2` 的 WordPiece 词表是英文的。本项目实际模型词表是否覆盖中文，必须先验证（Step 3），否则 embedding 兜底只对英文有效、中文完全靠规则层。

- [ ] **Step 1: 写失败测试（20 样本路由准确率表 + embedding 变体）**

在 `<app>/tests/test_intent_gate.cpp` 末尾追加：

```cpp
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
        // 越界 7 条
        {"帮我写个工作总结", RouteAction::OUT_OF_SCOPE},
        {"帮我写一封请假邮件", RouteAction::OUT_OF_SCOPE},
        {"讲个笑话吧", RouteAction::OUT_OF_SCOPE},
        {"今天天气怎么样", RouteAction::OUT_OF_SCOPE},
        {"陪我聊聊天", RouteAction::OUT_OF_SCOPE},
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
```

- [ ] **Step 2: 确认新测试失败（embedding 用例 skip，准确率表应已全过或暴露规则缺口）**

```
cmake --build <build> --target mmrag_intent_tests
<build>/bin/mmrag_intent_tests.exe
```

预期：`RoutingAccuracyTable` PASS（若 FAIL，先修规则库而不是放行阈值）；`EmbeddingFallbackCatchesVariant` SKIP。

- [ ] **Step 3: 验证模型词表中文能力（一次性手动检查）**

设置 `MMRAG_TEST_FP32_DIR=<fp32 目录>` 后跑一个临时断言（可写在测试里跑完即删，或用现有任何能调 `encode()` 的工具）：比较 `dot(encode("你好"), encode("您好"))` 与 `dot(encode("你好"), encode("什么是索引"))`。**若两者几乎无差异（词表无中文），则在 `default.yaml` 中把 `intent_gate.strategy` 配为 `rule_only`**，并在设计文档里补一行说明；英文 embedding 兜底仍保留。此步只产出结论，不改代码。

- [ ] **Step 4: 实现 init_exemplars + decide_by_embedding**

`<app>/src/retrieval/intent_gate.cpp` 中替换两个函数：

```cpp
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
```

并在 `route()` 的 else-if embedding 分支保持现状（已在 Task 2 写好调用与 fail-open）。

- [ ] **Step 5: 跑测试 + 带模型跑 embedding 用例**

```
cmake --build <build> --target mmrag_intent_tests
<build>/bin/mmrag_intent_tests.exe
set MMRAG_TEST_FP32_DIR=<fp32 目录> && <build>/bin/mmrag_intent_tests.exe --gtest_filter=IntentGate.Embedding*
```

预期：默认全 PASS；带模型时 embedding 用例 PASS（若 Step 3 结论为词表无中文，英文用例仍应 PASS）。

- [ ] **Step 6: Commit**

```bash
git add apps/multimodal_rag/src/retrieval/intent_gate.cpp apps/multimodal_rag/tests/
git commit -m "feat(intent-gate): MiniLM embedding 兜底 + 20 样本路由准确率表测试"
```

---

### Task 4: 配置链路（Config + YAML + default.yaml）

**Files:**
- Modify: `<app>/include/mmrag/config.h:168-216`（Config 类加成员）
- Modify: `<app>/include/mmrag/modular/config.h:31-42`（ModularConfig 加成员）
- Modify: `<app>/src/core/config.cpp`（load_from_string 键解析，约 :236 附近）
- Modify: `<app>/config/default.yaml`
- Test: `<app>/tests/test_intent_gate.cpp`（追加 YAML 解析测试）

**Interfaces:**
- Produces: `mmrag::Config::intent_gate`、`mmrag::modular::ModularConfig::intent_gate`（类型均为 Task 2 的 `IntentGateConfig`）。Task 5/8 依赖。

- [ ] **Step 1: 写失败测试**

`<app>/tests/test_intent_gate.cpp` 末尾追加：

```cpp
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
```

同时把 `../src/core/config.cpp` 加入 `<app>/tests/CMakeLists.txt` 的 `add_executable` 源列表（config.cpp 若引入新的链接依赖，按 Task 1 Step 2 的兜底规则追加对应 src 文件）。

- [ ] **Step 2: 跑构建确认失败**（`Config` 无 `intent_gate` 成员）

- [ ] **Step 3: Config / ModularConfig 加成员**

`<app>/include/mmrag/config.h` 顶部 include 区加 `#include "mmrag/intent_gate_config.h"`；`Config` 类中 `RetrievalConfig retrieval;` 之后加：

```cpp
    // 意图门控
    IntentGateConfig intent_gate;
```

`<app>/include/mmrag/modular/config.h` include 区加 `#include "mmrag/intent_gate_config.h"`；`ModularConfig` 中 `AgentConfig agent;` 之后加：

```cpp
    IntentGateConfig intent_gate;    // 意图门控配置
```

- [ ] **Step 4: config.cpp 解析键**

`<app>/src/core/config.cpp` `load_from_string` 的 if-else 链（`key == "retrieval.top_k"` 附近）追加：

```cpp
        } else if (key == "intent_gate.enabled") {
            config.intent_gate.enabled = (value == "true" || value == "1");
        } else if (key == "intent_gate.strategy") {
            config.intent_gate.strategy = value;
        } else if (key == "intent_gate.embedding_threshold") {
            try {
                config.intent_gate.embedding_threshold = std::stof(value);
            } catch (const std::exception&) {
                RAG_WARN("intent_gate.embedding_threshold 非法，使用默认值: " + value);
            }
        } else if (key == "intent_gate.evidence_threshold") {
            try {
                config.intent_gate.evidence_threshold = std::stof(value);
            } catch (const std::exception&) {
                RAG_WARN("intent_gate.evidence_threshold 非法，使用默认值: " + value);
            }
        } else if (key == "intent_gate.embedder_model_dir") {
            config.intent_gate.embedder_model_dir = value;
        } else if (key == "intent_gate.out_of_scope_reply") {
            config.intent_gate.out_of_scope_reply = value;
        } else if (key == "intent_gate.no_evidence_reply") {
            config.intent_gate.no_evidence_reply = value;
        }
```

（若该文件未 include `mmrag/logger.h` 则补上。注意现有解析器对带引号值的处理方式——若它会保留引号，字符串键需按该文件既有惯例去引号，参考 `llm.model_path` 的处理代码抄写。）

- [ ] **Step 5: default.yaml 加配置段**

`<app>/config/default.yaml` 末尾追加（键风格与文件内现有扁平键一致）：

```yaml
# 意图门控（非知识库问题短路，不走大模型）
intent_gate.enabled: true
intent_gate.strategy: "rule_then_embedding"
intent_gate.embedding_threshold: 0.70
intent_gate.evidence_threshold: 0.35
intent_gate.embedder_model_dir: ""
intent_gate.out_of_scope_reply: "我是知识库问答助手，这个问题不在知识库范围内。你可以问我关于已上传文档的问题。"
intent_gate.no_evidence_reply: "知识库中未找到相关信息，请换个问法或先上传相关文档。"
```

- [ ] **Step 6: 跑测试确认通过**

```
cmake --build <build> --target mmrag_intent_tests && <build>/bin/mmrag_intent_tests.exe
```

预期：含 2 个新用例全 PASS。

- [ ] **Step 7: Commit**

```bash
git add apps/multimodal_rag/include/mmrag/config.h \
        apps/multimodal_rag/include/mmrag/modular/config.h \
        apps/multimodal_rag/src/core/config.cpp \
        apps/multimodal_rag/config/default.yaml \
        apps/multimodal_rag/tests/
git commit -m "feat(intent-gate): 配置链路（YAML → Config/ModularConfig.intent_gate）"
```

---

### Task 5: ModularPipeline 基类模板方法 + 证据门辅助 + routed_by

**Files:**
- Modify: `<app>/include/mmrag/modular/types.h:48-57`（routed_by 字段）
- Modify: `<app>/include/mmrag/modular/pipeline/pipeline_base.h`
- Modify: `<app>/src/pipeline/pipeline_base.cpp`
- Test: `<app>/tests/test_intent_gate.cpp`（追加模板方法测试）

**Interfaces:**
- Consumes: Task 2 `IntentGate`、Task 4 `ModularConfig::intent_gate`。
- Produces:
  - `ModularPipeline::query(const ModularQuery&)` → `ModularQueryResult`（非虚，模板方法）
  - `virtual ModularQueryResult do_query(const ModularQuery&) = 0`（protected 纯虚，Task 6/7 实现）
  - `bool ModularPipeline::has_sufficient_evidence(const std::vector<RetrievalResult>&) const`
  - `ModularQueryResult ModularPipeline::make_no_evidence_result(const ModularQuery&) const`
  - `ModularQueryResult::routed_by`（`"intent_gate_chat"|"intent_gate_oos"|"evidence_gate"|"normal"`）

**⚠️ 破坏性签名变更**：本任务后所有子类 `query() override` 会编译失败——预期之中，Task 6/7 修复。中间状态不可单独构建 server，只构建 `mmrag_intent_tests`（它不编译子类管线）。

- [ ] **Step 1: 写失败测试**

`<app>/tests/test_intent_gate.cpp` 末尾追加：

```cpp
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
```

- [ ] **Step 2: 确认编译失败**（基类还没有这些成员）

- [ ] **Step 3: types.h 加 routed_by**

`<app>/include/mmrag/modular/types.h` `ModularQueryResult` 中 `std::string error_message;` 之后加：

```cpp
    std::string routed_by;                           // 路由来源: intent_gate_chat | intent_gate_oos | evidence_gate | normal
```

- [ ] **Step 4: pipeline_base.h 改模板方法**

`<app>/include/mmrag/modular/pipeline/pipeline_base.h` 整体替换为：

```cpp
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
```

- [ ] **Step 5: pipeline_base.cpp 实现**

在 `<app>/src/pipeline/pipeline_base.cpp` 中（保留现有 build_context / generate_with_llm，新增以下函数；include 加 `<algorithm>`）：

```cpp
ModularQueryResult ModularPipeline::query(const ModularQuery& query) {
    ensure_intent_gate();

    auto decision = intent_gate_->route(query.text);
    if (decision.action != mmrag::RouteAction::PROCEED) {
        ModularQueryResult result;
        result.success = true;
        result.answer = decision.reply;
        result.routed_by = (decision.action == mmrag::RouteAction::CHAT_REPLY)
                               ? "intent_gate_chat" : "intent_gate_oos";
        RAG_INFO("IntentGate 短路: routed_by=" + result.routed_by +
                 " matched_by=" + decision.matched_by +
                 " confidence=" + std::to_string(decision.confidence));
        return result;
    }

    auto result = do_query(query);
    if (result.routed_by.empty()) {
        result.routed_by = "normal";
    }
    return result;
}

void ModularPipeline::ensure_intent_gate() {
    std::call_once(gate_once_flag_, [this] {
        const auto& gc = config_.intent_gate;
        std::shared_ptr<mmrag::MiniLMEmbedder> embedder;
        if (gc.enabled && gc.strategy == "rule_then_embedding" &&
            !gc.embedder_model_dir.empty()) {
            auto e = std::make_shared<mmrag::MiniLMEmbedder>(
                gc.embedder_model_dir, gc.embedder_dim);
            if (e->is_ready()) {
                embedder = std::move(e);
            } else {
                RAG_WARN("IntentGate embedder 加载失败，降级为 rule_only: " +
                         gc.embedder_model_dir);
            }
        }
        intent_gate_ = std::make_unique<mmrag::IntentGate>(gc, embedder);
    });
}

bool ModularPipeline::has_sufficient_evidence(
    const std::vector<RetrievalResult>& results) const {
    if (results.empty()) return false;
    float best = 0.0f;
    for (const auto& r : results) {
        best = std::max(best, r.score);
    }
    return best >= config_.intent_gate.evidence_threshold;
}

ModularQueryResult ModularPipeline::make_no_evidence_result(
    const ModularQuery& /*query*/) const {
    ModularQueryResult result;
    result.success = true;
    result.answer = config_.intent_gate.no_evidence_reply;
    result.routed_by = "evidence_gate";
    return result;
}
```

- [ ] **Step 6: 只构建测试目标确认通过**

```
cmake --build <build> --target mmrag_intent_tests && <build>/bin/mmrag_intent_tests.exe
```

预期：全部 PASS（此时**不要**构建 `multimodal_rag_server`，子类尚未适配，编译失败属预期）。

- [ ] **Step 7: Commit**

```bash
git add apps/multimodal_rag/include/mmrag/modular/types.h \
        apps/multimodal_rag/include/mmrag/modular/pipeline/pipeline_base.h \
        apps/multimodal_rag/src/pipeline/pipeline_base.cpp \
        apps/multimodal_rag/tests/
git commit -m "feat(intent-gate): ModularPipeline 模板方法 + 证据门辅助 + routed_by"
```

---

### Task 6: NaivePipeline 接入（改名 + 证据门）

**Files:**
- Modify: `<app>/include/mmrag/modular/pipeline/naive_pipeline.h:54-58`
- Modify: `<app>/src/pipeline/naive_pipeline.cpp:44,62-69`

**Interfaces:**
- Consumes: Task 5 的 `do_query` / `has_sufficient_evidence` / `make_no_evidence_result`。
- Produces: 第一个可端到端短路的管线；`handle_query` 的 `pipeline->query()` 调用点（api_server.cpp:817）无需改动。

- [ ] **Step 1: 头文件改名**

`<app>/include/mmrag/modular/pipeline/naive_pipeline.h`：把 public 区的

```cpp
    ModularQueryResult query(const ModularQuery& query) override;
```

删除，在 `private:` 之前加：

```cpp
protected:
    /**
     * @brief 检索 + 生成（由基类模板方法 query() 调用）
     */
    ModularQueryResult do_query(const ModularQuery& query) override;
```

- [ ] **Step 2: .cpp 改名 + 证据门**

`<app>/src/pipeline/naive_pipeline.cpp`：

```cpp
// :44 函数签名
ModularQueryResult NaivePipeline::do_query(const ModularQuery& query) {
```

原"检索结果为空"分支（:62-69）替换为证据门（覆盖空结果 + 低分两种情况）：

```cpp
    // 证据门：空结果或最高分低于阈值 → 不调 LLM
    if (!has_sufficient_evidence(retrieval_results)) {
        RAG_WARN("证据不足，跳过 LLM: " + query.text);
        auto r = make_no_evidence_result(query);
        r.retrieval_time_ms = result.retrieval_time_ms;
        r.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();
        return r;
    }
```

（`result.retrieval_time_ms` 在其上方已赋值；其余逻辑不动。）

- [ ] **Step 3: 构建 server 确认只剩其余 8 个管线的报错**

```
cmake --build <build> --target multimodal_rag_server 2>&1 | grep -c "do_query\|query"
```

预期：报错仅来自其余 8 个管线（advanced/hybrid/hyde/graph/corrective/react/iterative/recursive）的 `query` override 失配；naive_pipeline.cpp 无错。

- [ ] **Step 4: Commit**

```bash
git add apps/multimodal_rag/include/mmrag/modular/pipeline/naive_pipeline.h \
        apps/multimodal_rag/src/pipeline/naive_pipeline.cpp
git commit -m "feat(intent-gate): NaivePipeline 接入模板方法 + 证据门"
```

---

### Task 7: 其余 8 条管线接入

**Files:**
- Modify: `<app>/include/mmrag/modular/pipeline/{advanced,hybrid,hyde,graph,corrective,react,iterative,recursive}_pipeline.h`
- Modify: `<app>/src/pipeline/{advanced,hybrid,hyde,graph,corrective,react,iterative,recursive}_pipeline.cpp`

**Interfaces:**
- Consumes: 同 Task 6。
- Produces: 全部管线适配模板方法；server 恢复可编译。

**说明**：8 条管线的改动模式与 Task 6 完全一致。每条管线重复以下两步；插入点用 grep 定位（各文件结构不同，不预设行号）：

```
grep -n "generate_with_llm\|::query\|query(const ModularQuery" <app>/src/pipeline/<name>_pipeline.cpp
```

- [ ] **Step 1: advanced_pipeline — 头文件**：public 区 `ModularQueryResult query(const ModularQuery& query) override;` 移到 protected 并改名 `do_query`（同 Task 6 Step 1 的代码）。
- [ ] **Step 2: advanced_pipeline — .cpp**：`AdvancedPipeline::query` 改名 `AdvancedPipeline::do_query`；在**首次** `generate_with_llm(` 调用之前（检索/重排序完成之后）插入 Task 6 Step 2 的证据门代码块（变量名按该文件实际检索结果变量调整）。
- [ ] **Step 3: hybrid_pipeline — 重复 Step 1-2**。该文件有多处 `generate_with_llm`，只在主回答生成前插一次；中间步骤（如查询改写）的 LLM 调用不拦。
- [ ] **Step 4: hyde_pipeline — 重复 Step 1-2**。注意 HyDE 流程是"先 LLM 生成假设文档再检索"：证据门插在最后回答生成前，**不要**拦 HyDE 假设生成。
- [ ] **Step 5: graph_pipeline — 重复 Step 1-2**。
- [ ] **Step 6: corrective_pipeline — 重复 Step 1-2，但**：该管线自带检索质量评估逻辑，证据门应**复用其现有评估结果**——若其内部已判定检索不合格并会走纠正/拒答路径，则在其拒答分支里把 `routed_by` 设为 `"evidence_gate"`（`result.routed_by = "evidence_gate";`），不再额外插 `has_sufficient_evidence` 调用。
- [ ] **Step 7: react_pipeline — 重复 Step 1-2**。ReAct 多轮工具调用，证据门插在**最终**回答生成前。
- [ ] **Step 8: iterative_pipeline — 重复 Step 1-2**（最终回答前）。
- [ ] **Step 9: recursive_pipeline — 重复 Step 1-2**（最终回答前）。
- [ ] **Step 10: 全量构建 + 调用点检查**

```
cmake --build <build> --target multimodal_rag_server
grep -rn "->query(" <app>/src --include=*.cpp | grep -v "engine_->query\|do_query"
```

预期：构建成功；grep 结果的调用点（如 api_server、agent）调用的都是基类公开 `query()`，无需改动（若有子类指针直接调用且依赖旧虚函数行为，确认其走的是基类模板方法即可）。

- [ ] **Step 11: 回归测试 + Commit**

```
<build>/bin/mmrag_intent_tests.exe
git add apps/multimodal_rag/include/mmrag/modular/pipeline/ \
        apps/multimodal_rag/src/pipeline/
git commit -m "feat(intent-gate): 8 条管线接入模板方法 + 证据门"
```

---

### Task 8: Server/API 接线 + 手动验证

**Files:**
- Modify: `<app>/include/mmrag/server.h`（Server 类加方法声明）
- Modify: `<app>/src/server/api_server.cpp`（Impl 存配置、m_config 接线 :369-374、JSON 透传 :844）
- Modify: `<app>/src/main.cpp:155` 附近（set_intent_gate_config 调用）
- Modify: `<app>/src/server/server.cpp`（若 Server 接口在此实现）

**Interfaces:**
- Consumes: Task 4 `Config::intent_gate`。
- Produces: `Server::set_intent_gate_config(const mmrag::IntentGateConfig&)`；`/api/v1/query` 响应 JSON 新增 `routed_by` 字段。

- [ ] **Step 1: Server 加 setter**

`server.h` `Server` 类 public 区（`set_engine` 声明旁）加：

```cpp
    /**
     * @brief 设置意图门配置（注入 modular pipeline 系统）
     */
    void set_intent_gate_config(const mmrag::IntentGateConfig& config);
```

（若 server.h 未 include `mmrag/intent_gate_config.h` 则补上。）

`api_server.cpp` `Impl` 结构体（:222 附近，`pipelines` 成员旁）加：

```cpp
    mmrag::IntentGateConfig intent_gate_config;
```

`api_server.cpp`（或 `server.cpp`，以 `set_engine` 实现所在文件为准）加：

```cpp
void Server::set_intent_gate_config(const mmrag::IntentGateConfig& config) {
    impl_->intent_gate_config = config;
}
```

- [ ] **Step 2: m_config 接线**

`api_server.cpp` 构建 `m_config` 处（:369-374，`m_config.llm.temperature = 0.7f;` 之后）加：

```cpp
            m_config.intent_gate = impl_->intent_gate_config;
```

- [ ] **Step 3: JSON 透传 routed_by**

`api_server.cpp:844` 处：

```cpp
// 原：
                oss << "\"total_tokens\": " << m_result.total_tokens;
// 改为：
                oss << "\"total_tokens\": " << m_result.total_tokens << ",";
                oss << "\"routed_by\": \"" << json_escape(m_result.routed_by) << "\"";
```

（只改 modular 路径这一段；下方 engine fallback 路径的 JSON 不动，保持向后兼容。）

- [ ] **Step 4: main.cpp 注入**

`src/main.cpp` `server->set_engine(engine);`（:155）之后加：

```cpp
    server->set_intent_gate_config(config.intent_gate);
```

- [ ] **Step 5: 构建 + 单元回归**

```
cmake --build <build> --target multimodal_rag_server mmrag_intent_tests
<build>/bin/mmrag_intent_tests.exe
```

预期：构建成功、测试全 PASS。

- [ ] **Step 6: 手动端到端验证**

启动服务器（`MMRAG_DATA_DIR` 按现有方式设置）后依次 curl `/api/v1/query`（`pipeline_type` 用 `naive` 之外的 modular 类型，如 `advanced`）：

```
# 闲聊 → 秒回，routed_by=intent_gate_chat，answer 为问候回复
curl -s -X POST localhost:8080/api/v1/query -d '{"query":"你好","pipeline_type":"advanced"}'

# 越界 → routed_by=intent_gate_oos
curl -s -X POST localhost:8080/api/v1/query -d '{"query":"帮我写个周报","pipeline_type":"advanced"}'

# 知识库内问题 → routed_by=normal，有 LLM 答案
curl -s -X POST localhost:8080/api/v1/query -d '{"query":"<知识库内问题>","pipeline_type":"advanced"}'

# 知识库外但像问题的 → routed_by=evidence_gate，answer 为 no_evidence_reply
curl -s -X POST localhost:8080/api/v1/query -d '{"query":"<与库中文档无关的严肃问题>","pipeline_type":"advanced"}'
```

检查项：四条响应的 `routed_by` 分别正确；前两条 `total_time_ms` 应 < 50ms；服务器日志出现 `IntentGate 短路` 行；LLM 侧无对应请求记录。

- [ ] **Step 7: Commit**

```bash
git add apps/multimodal_rag/include/mmrag/server.h \
        apps/multimodal_rag/src/server/ \
        apps/multimodal_rag/src/main.cpp
git commit -m "feat(intent-gate): server 接线 + /api/v1/query 响应透传 routed_by"
```

---

## Self-Review 结论（已内联修复）

- **Spec 覆盖**：意图门(规则+embedding)→T2/T3；证据门→T5/T6/T7；回复话术配置→T2/T4；routed_by 透传→T5/T8；配置链路→T4；测试 1-5→T1-T3，测试 6（LLM 未被调用）→T5 `do_query_calls==0` 等价断言，测试 7→T5 EvidenceGate 用例，测试 8（API JSON）→T8 手动验证；eval 钩子→T3 准确率表（偏离说明见文首）；metrics→IntentGate::Stats（偏离说明见文首）。
- **已知遗留（非本计划范围）**：`/api/v1/query/stream` 流式路径与 `engine_->query` naive fallback 路径未接门控；`LLMQueryClassifier` 的 prompt 未加 OUT_OF_SCOPE 类型说明。如需可在后续迭代补。
- **类型一致性**：`RouteAction`/`RouteDecision`/`IntentGateConfig`/`routed_by` 四个取值在所有任务间一致。
