# 意图路由门控设计（Intent Routing Gate）

- **日期**: 2026-09-23
- **状态**: 已批准
- **范围**: `apps/multimodal_rag`

## 1. 背景与目标

当前服务器的 modular 管线（naive/advanced/hybrid 等 9 种）流程为"检索 → 拼 prompt → 无条件调用 LLM"，分类器未接入。闲聊（"你好"）和知识库外问题也会走完整个 RAG + 大模型，浪费 token 与延迟，且容易幻觉。

**目标**（对标业界分层护栏实践）：

1. **意图门（检索前）**：非知识库意图的查询短路返回，不调 LLM、不做检索。
2. **证据门（检索后）**：检索结果分数过低时不调 LLM，返回诚实的"未覆盖"话术（fail-closed）。
3. 闲聊走小回复表，越界走固定引导话术，全程零 LLM 成本。

**非目标**：不做 LLM-as-judge 前置分类；不做多轮对话上下文意图继承；不改动各管线的检索/生成算法本身。

## 2. 关键约束

- 知识库由用户任意上传，**无预定主题**。意图门只能识别"非提问型意图"（闲聊、命令、元问题）；"话题越界"只能由证据门在检索后判定。两道门缺一不可。
- 现有基础设施可复用：`RuleBasedQueryClassifier`（规则层）、`minilm_embedder`（embedding 层）、`observability/metrics`（指标）、`eval/`（评测）。
- 现存 bug 顺带修复：`query_classifier.cpp` 中 CHAT 模式 `R"(.*吗$|.*吗？)"` 会把所有疑问句误判为闲聊，需收窄为闲聊句式（"在吗/好吗/行吗"等）。

## 3. 架构

```
用户查询
   │
   ▼
┌─────────────────────────────────────────────┐
│ ModularPipeline::query() 入口（基类模板方法） │
│                                              │
│  ① 意图门 IntentGate                         │
│     ├─ 规则分类（复用 query_classifier）      │
│     │    命中 CHAT → 闲聊回复表 → 直接返回     │
│     │    命中 OUT_OF_SCOPE → 固定话术 → 返回  │
│     └─ 未命中 → MiniLM embedding 相似度       │
│          ≥ 阈值 → 同上短路                    │
│          < 阈值 → 放行 ↓                      │
│                                              │
│  ② 各管线原有流程（do_query，检索 → 生成）     │
│                                              │
│  ③ 证据门（do_query 内、LLM 调用前）          │
│     检索为空或最高分 < 阈值 → "未覆盖"话术     │
│     否则 → 正常调 LLM                         │
└─────────────────────────────────────────────┘
```

### 3.1 挂载点：基类模板方法

`ModularPipeline::query()` 改为模板方法（template method）：基类 `query()` 先跑意图门，PROCEED 才调用子类新增的 `do_query()` 虚函数。9 个子类把原 `query()` 改名为 `do_query()` 并改为 `protected` 覆盖。

### 3.2 QueryType 扩展

`mmrag/pipeline.h` 的 `QueryType` 枚举新增 `OUT_OF_SCOPE`；`query_type_to_string` / `string_to_query_type` 同步加分支（值 `"out_of_scope"`）。

### 3.3 结果标记

`ModularQueryResult`（`modular/types.h`）新增字段：

```cpp
std::string routed_by;  // "intent_gate_chat" | "intent_gate_oos" | "evidence_gate" | "normal"
```

`api_server.cpp` 查询 handler 将该字段透传进 JSON 响应（新增字段，向后兼容），前端可给短路回答加 UI 标记。

## 4. 组件

### 4.1 IntentGate（新文件）

- 头文件 `include/mmrag/intent_gate.h`，实现 `src/retrieval/intent_gate.cpp`
- 无状态、可独立单测；不依赖 pipeline/server

```cpp
enum class RouteAction {
    PROCEED,        // 放行，走正常 RAG
    CHAT_REPLY,     // 闲聊，查表返回
    OUT_OF_SCOPE    // 越界/非提问，固定话术
};

struct RouteDecision {
    RouteAction action;
    float confidence;        // 规则 0.8+ / embedding 相似度
    std::string matched_by;  // "rule" | "embedding" | "disabled" | "error_fallback"
    std::string reply;       // CHAT_REPLY / OUT_OF_SCOPE 时的响应文本
};

class IntentGate {
public:
    IntentGate(const IntentGateConfig& config,
               std::shared_ptr<MiniLMEmbedder> embedder);  // 允许为空 → 降级 rule_only
    RouteDecision route(const std::string& query);
private:
    RuleBasedQueryClassifier rule_classifier_;   // 复用现有（扩充模式库）
    std::vector<Exemplar> chat_exemplars_;       // 闲聊示例句 + 预计算 embedding
    std::vector<Exemplar> oos_exemplars_;        // 越界示例句 + 预计算 embedding
};
```

### 4.2 示例句库（Exemplar）

- 内置默认集（中文为主）：闲聊 ~15 句（"你好""在吗""谢谢""你能做什么"…），越界 ~15 句（"帮我写个周报""讲个笑话""今天天气"…）
- 构造时一次性用 MiniLM 编码缓存；查询时 1 次 encode + 30 次点积，~10ms
- `default.yaml` 可追加自定义示例句，无需改代码

### 4.3 回复文案配置

`IntentGateConfig` 内含：

- `chat_replies`：问候/感谢/告别/能力询问 → 各 1-2 条固定回复
- `out_of_scope_reply`：默认 "我是知识库问答助手，这个问题不在知识库范围内。你可以问我关于已上传文档的问题。"
- `no_evidence_reply`：默认 "知识库中未找到相关信息，请换个问法或先上传相关文档。"

### 4.4 证据门（接法 A：子类各加 2 行）

基类提供辅助函数：

```cpp
bool has_sufficient_evidence(const std::vector<RetrievalResult>& results) const;
ModularQueryResult make_no_evidence_result(const ModularQuery& query) const;
```

每个子类 `do_query()` 在拿到检索结果后、调 LLM 前插入：

```cpp
if (!has_sufficient_evidence(retrieval_results)) {
    return make_no_evidence_result(query);
}
```

注意 corrective 管线已有检索质量评估逻辑，接入时与其共用判断而非重复实现。

### 4.5 配置（`ModularConfig` 新增段）

```yaml
intent_gate:
  enabled: true
  strategy: "rule_then_embedding"   # rule_only | rule_then_embedding
  embedding_threshold: 0.70
  evidence_threshold: 0.35          # 起点值，按线上拦截率调
  chat_replies: { ... }
  out_of_scope_reply: "..."
  no_evidence_reply: "..."
```

### 4.6 指标

`routed_by` 四个取值各一个 counter（`observability/metrics`），另加 `intent_gate_error` counter。用于线上调阈值：误拦多降阈值、漏放多升阈值。

### 4.7 缓存交互

短路路径（CHAT_REPLY / OUT_OF_SCOPE / 证据门）**不写**语义缓存（回复是静态的）；放行路径维持现有缓存逻辑不变，缓存键与命中率统计不受污染。

## 5. 数据流

**放行路径**：

```
query() [基类]
  ├─ IntentGate::route() → PROCEED                (~0-10ms)
  ├─ do_query() [子类]
  │    ├─ 检索 → results
  │    ├─ has_sufficient_evidence(results)
  │    │     false → make_no_evidence_result, routed_by="evidence_gate", 跳过 LLM
  │    └─ true  → 拼 prompt → LLM → answer, routed_by="normal"
  └─ 返回 ModularQueryResult
```

**闲聊路径**：`CHAT_REPLY → answer=回复表内容, success=true, context 为空, routed_by="intent_gate_chat", total_time_ms < 15`，不碰检索器与 LLM。

**越界路径**：同上，`routed_by="intent_gate_oos"`，answer 为固定话术。

**API 层**：响应 JSON 新增 `routed_by` 字段。

## 6. 错误处理

| 故障场景 | 行为 | 理由 |
|---|---|---|
| MiniLM embedder 加载失败 | 降级 rule_only，启动 `RAG_WARN`，服务正常启动 | 门是增强，不拖垮主服务 |
| 查询时 encode 抛异常 | catch → PROCEED 放行，`RAG_WARN` + `intent_gate_error` | 意图门 fail-open |
| 配置缺失/非法 | 用默认值 + 启动 `RAG_WARN` | 与现有 config 风格一致 |
| 示例句配置为空 | 用内置默认集 | 开箱可用 |
| `enabled=false` | `route()` 直接 PROCEED，零开销 | 一键回退 |
| 检索器本身抛异常 | 维持各管线现有错误路径，证据门不介入 | 不扩大改动面 |

核心原则：**意图门永远 fail-open（坏了就当不存在），回答永远 fail-closed（没证据就不硬答）**。

## 7. 测试

**单元测试（新增 `tests/test_intent_gate.cpp`，gtest）：**

1. 规则命中：你好/谢谢/在吗 → `CHAT_REPLY`；"帮我写代码" → `OUT_OF_SCOPE`
2. 规则未命中但 embedding 相似："哈喽呀" → `CHAT_REPLY`（embedder 不在则 skip）
3. 正常知识库问题 → `PROCEED`；覆盖"这个配置生效了吗"不误判 CHAT（驱动修复 `.*吗$` 模式）
4. embedder 为 nullptr → 自动 rule_only，不崩
5. 阈值边界：相似度 ≥ 阈值判定为拦截（含等号）

**集成测试：**

6. NaivePipeline 全链路 mock：闲聊查询 → `EXPECT_CALL(generate).Times(0)`，断言 `routed_by` 正确（核心验收："不走大模型"）
7. 证据门：mock 检索器返回低分 → 返回 `no_evidence_reply` 且 LLM 未被调用
8. API 层：POST `/api/v1/query` 闲聊 → 响应 JSON 含 `routed_by`

**评测钩子：** golden dataset 增加 ~20 条闲聊/越界样本，跑现有 evaluator 看路由准确率。

## 8. 改动文件清单

| 文件 | 改动 |
|---|---|
| `include/mmrag/intent_gate.h` / `src/retrieval/intent_gate.cpp` | 新增 IntentGate |
| `include/mmrag/pipeline.h` / `src/pipeline/pipeline.cpp` | QueryType 加 OUT_OF_SCOPE |
| `src/retrieval/query_classifier.cpp` | 扩充模式库；修复 `.*吗$` 误伤 |
| `include/mmrag/modular/pipeline/pipeline_base.h` / `src/pipeline/pipeline_base.cpp` | 模板方法 query/do_query；证据门辅助函数 |
| `src/pipeline/{naive,advanced,hybrid,hyde,graph,corrective,react,iterative,recursive}_pipeline.cpp` | query→do_query 改名 + 证据门 2 行 |
| `include/mmrag/modular/types.h` | ModularQueryResult 加 routed_by |
| `include/mmrag/modular/config.h` / `src/core/config.cpp` | IntentGateConfig 加载 |
| `config/default.yaml` | intent_gate 配置段 |
| `src/server/api_server.cpp` | 响应透传 routed_by；注入 embedder |
| `src/observability/metrics.cpp` | routed_by / intent_gate_error counters |
| `tests/test_intent_gate.cpp` | 新增 |
