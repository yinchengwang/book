# Agent 框架指南

> 版本：1.0
> 日期：2026-09-04

---

## 目录

1. [Agent 框架概述](#1-agent-框架概述)
2. [Tool 系统](#2-tool-系统)
3. [Memory 系统](#3-memory-系统)
4. [ReAct 循环说明](#4-react-循环说明)
5. [集成示例](#5-集成示例)

---

## 1. Agent 框架概述

### 1.1 整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                       Agent Orchestrator                              │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                      Agent Core                             │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐        │   │
│  │  │   Planner   │  │   Memory   │  │    Tool    │        │   │
│  │  │   (规划器)   │  │   (记忆)   │  │   (工具)   │        │   │
│  │  └─────────────┘  └─────────────┘  └─────────────┘        │   │
│  │         │                │                │                │   │
│  │         └────────────────┼────────────────┘                │   │
│  │                          ▼                                 │   │
│  │                   ┌─────────────┐                          │   │
│  │                   │  ReAct Loop │                          │   │
│  │                   │  (推理循环)  │                          │   │
│  │                   └─────────────┘                          │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                              │                                      │
│                              ▼                                      │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                    LLM Service                               │   │
│  │              (llama.cpp 内置推理引擎)                        │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 1.2 核心组件

| 组件 | 职责 | 说明 |
|------|------|------|
| **Planner** | 规划器 | 决策下一步行动、分解任务 |
| **Memory** | 记忆系统 | 短记忆、长记忆、工作记忆 |
| **Tool** | 工具系统 | 注册、执行外部工具 |
| **ReAct Loop** | 推理循环 | Think-Act-Observe 循环 |
| **LLM Service** | LLM 服务 | llama.cpp 推理引擎 |

### 1.3 Agent 类型

| 类型 | 说明 | 适用场景 |
|------|------|----------|
| **ReAct Agent** | 基于 ReAct 循环的 Agent | 通用问答、复杂推理 |
| **Plan Agent** | 基于规划的 Agent | 任务分解、执行 |
| **Chat Agent** | 对话型 Agent | 闲聊、助手 |
| **RAG Agent** | RAG 驱动的 Agent | 知识库问答 |

---

## 2. Tool 系统

### 2.1 Tool 基类定义

```cpp
// 工具基类 - 位于 include/modular_rag/agent/tool.h
namespace modular_rag::agent {

/**
 * Tool 参数定义
 */
struct ToolParameter {
    std::string name;           // 参数名称
    std::string description;    // 参数描述
    std::string type;           // 参数类型 (string, int, float, bool, array, object)
    bool required;              // 是否必需
    json default_value;         // 默认值
};

/**
 * Tool 执行结果
 */
struct ToolResult {
    bool success;               // 是否成功
    std::string output;         // 执行输出
    std::string error;          // 错误信息（如果失败）
    json metadata;              // 额外元数据
};

/**
 * Tool 基类
 */
class Tool {
public:
    virtual ~Tool() = default;

    // 获取工具名称
    virtual std::string name() const = 0;

    // 获取工具描述
    virtual std::string description() const = 0;

    // 获取工具参数列表
    virtual std::vector<ToolParameter> parameters() const = 0;

    // 执行工具
    virtual ToolResult execute(const json& args) = 0;

    // 验证参数
    virtual bool validate_args(const json& args) const;
};

}  // namespace modular_rag::agent
```

### 2.2 内置 Tool 实现

#### 2.2.1 Retrieve Tool（检索工具）

```cpp
// 检索工具 - 用于从文档库检索相关内容
class RetrieveTool : public Tool {
public:
    RetrieveTool(std::shared_ptr<RetrievalEngine> engine);

    std::string name() const override {
        return "retrieve";
    }

    std::string description() const override {
        return "从文档库中检索与查询相关的文档内容";
    }

    std::vector<ToolParameter> parameters() const override {
        return {
            {"query", "检索查询文本", "string", true, nullptr},
            {"top_k", "返回结果数量", "int", false, 5},
            {"filter", "元数据过滤条件", "object", false, nullptr}
        };
    }

    ToolResult execute(const json& args) override;

private:
    std::shared_ptr<RetrievalEngine> engine_;
};
```

#### 2.2.2 KnowledgeGraph Tool（知识图谱工具）

```cpp
// 知识图谱工具 - 用于查询实体和关系
class KnowledgeGraphTool : public Tool {
public:
    KnowledgeGraphTool(std::shared_ptr<GraphStore> graph_store);

    std::string name() const override {
        return "knowledge_graph";
    }

    std::string description() const override {
        return "查询知识图谱中的实体和关系";
    }

    std::vector<ToolParameter> parameters() const override {
        return {
            {"entity", "要查询的实体名称", "string", true, nullptr},
            {"relation_type", "关系类型过滤", "string", false, nullptr},
            {"max_hops", "最大跳数", "int", false, 2}
        };
    }

    ToolResult execute(const json& args) override;

private:
    std::shared_ptr<GraphStore> graph_store_;
};
```

#### 2.2.3 Calculator Tool（计算工具）

```cpp
// 计算工具 - 用于执行数学计算
class CalculatorTool : public Tool {
public:
    CalculatorTool() = default;

    std::string name() const override {
        return "calculator";
    }

    std::string description() const override {
        return "执行数学计算";
    }

    std::vector<ToolParameter> parameters() const override {
        return {
            {"expression", "要计算的数学表达式", "string", true, nullptr}
        };
    }

    ToolResult execute(const json& args) override;
};
```

### 2.3 Tool 注册与调用

```cpp
// Tool 注册器 - 位于 include/modular_rag/agent/tool_registry.h
namespace modular_rag::agent {

class ToolRegistry {
public:
    // 单例获取
    static ToolRegistry& instance();

    // 注册工具
    void register_tool(std::shared_ptr<Tool> tool);

    // 注销工具
    void unregister_tool(const std::string& name);

    // 获取工具
    std::shared_ptr<Tool> get_tool(const std::string& name);

    // 列出所有工具
    std::vector<std::string> list_tools() const;

    // 工具是否存在
    bool has_tool(const std::string& name) const;

private:
    ToolRegistry() = default;
    std::unordered_map<std::string, std::shared_ptr<Tool>> tools_;
};

}  // namespace modular_rag::agent
```

### 2.4 自定义 Tool 示例

```cpp
// 自定义天气查询工具示例
class WeatherTool : public Tool {
public:
    WeatherTool() = default;

    std::string name() const override {
        return "weather";
    }

    std::string description() const override {
        return "查询指定城市的天气信息";
    }

    std::vector<ToolParameter> parameters() const override {
        return {
            {"city", "城市名称", "string", true, nullptr},
            {"date", "日期（可选，默认今天）", "string", false, "today"}
        };
    }

    ToolResult execute(const json& args) override {
        // 参数验证
        if (!args.contains("city")) {
            return {false, "", "缺少必需参数: city", nullptr};
        }

        std::string city = args["city"];
        std::string date = args.value("date", "today");

        // 调用天气 API（伪代码）
        std::string weather = call_weather_api(city, date);

        return {true, weather, "", {{"city", city}, {"date", date}}};
    }
};

// 注册工具
ToolRegistry::instance().register_tool(std::make_shared<WeatherTool>());
```

---

## 3. Memory 系统

### 3.1 记忆类型

```
┌─────────────────────────────────────────────────────────────────────┐
│                         Memory Architecture                           │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                      Working Memory                          │   │
│  │                   (工作记忆 - 当前任务)                       │   │
│  │                                                             │   │
│  │  • 当前上下文                                                │   │
│  │  • 推理过程中的中间结果                                       │   │
│  │  • Tool 执行结果                                             │   │
│  │                                                             │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                              │                                      │
│  ┌───────────────────────────┼───────────────────────────────┐   │
│  │                           │                               │   │
│  ▼                           ▼                               ▼   │
│  ┌───────────────┐    ┌───────────────┐    ┌───────────────┐   │
│  │ Short-Term    │    │ Long-Term     │    │ Session       │   │
│  │ Memory        │    │ Memory        │    │ Memory        │   │
│  │ (短期记忆)     │    │ (长期记忆)     │    │ (会话记忆)    │   │
│  │               │    │               │    │               │   │
│  │ • 最近 N 条    │    │ • 向量存储     │    │ • 对话历史    │   │
│  │   消息        │    │ • 经验积累     │    │ • 用户信息    │   │
│  │ • 临时信息     │    │ • 知识库      │    │ • 偏好设置    │   │
│  └───────────────┘    └───────────────┘    └───────────────┘   │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 3.2 Memory 接口定义

```cpp
// 记忆基类 - 位于 include/modular_rag/agent/memory.h
namespace modular_rag::agent {

/**
 * 记忆条目
 */
struct MemoryItem {
    std::string id;             // 唯一标识
    std::string content;         // 记忆内容
    std::string type;           // 记忆类型
    int64_t timestamp;          // 时间戳
    json metadata;             // 额外元数据
    float importance;           // 重要性评分 (0-1)
};

/**
 * Memory 基类
 */
class Memory {
public:
    virtual ~Memory() = default;

    // 添加记忆
    virtual void add(const MemoryItem& item) = 0;

    // 获取记忆
    virtual std::vector<MemoryItem> get(
        const std::string& query,
        int limit = 10
    ) = 0;

    // 搜索记忆
    virtual std::vector<MemoryItem> search(
        const std::string& query,
        int limit = 10
    ) = 0;

    // 删除记忆
    virtual bool remove(const std::string& id) = 0;

    // 清空记忆
    virtual void clear() = 0;

    // 获取记忆数量
    virtual size_t size() const = 0;
};

}  // namespace modular_rag::agent
```

### 3.3 短期记忆实现

```cpp
// 短期记忆 - 基于固定窗口的队列
class ShortTermMemory : public Memory {
public:
    explicit ShortTermMemory(size_t max_size = 50);

    void add(const MemoryItem& item) override;
    std::vector<MemoryItem> get(const std::string& query, int limit = 10) override;
    std::vector<MemoryItem> search(const std::string& query, int limit = 10) override;
    bool remove(const std::string& id) override;
    void clear() override;
    size_t size() const override;

private:
    size_t max_size_;
    std::deque<MemoryItem> items_;  // 双向队列，FIFO
    std::mutex mutex_;
};
```

### 3.4 长期记忆实现

```cpp
// 长期记忆 - 基于向量存储
class LongTermMemory : public Memory {
public:
    LongTermMemory(std::shared_ptr<VectorStore> vector_store);

    void add(const MemoryItem& item) override;
    std::vector<MemoryItem> get(const std::string& query, int limit = 10) override;
    std::vector<MemoryItem> search(const std::string& query, int limit = 10) override;
    bool remove(const std::string& id) override;
    void clear() override;
    size_t size() const override;

    // 记忆压缩
    void compress(float threshold = 0.5);

    // 记忆强化
    void reinforce(const std::string& id, float delta = 0.1);

private:
    std::shared_ptr<VectorStore> vector_store_;
    std::mutex mutex_;
};
```

### 3.5 会话记忆实现

```cpp
// 会话记忆 - 管理对话历史
class SessionMemory : public Memory {
public:
    SessionMemory();

    // 会话管理
    void create_session(const std::string& session_id);
    void switch_session(const std::string& session_id);
    void delete_session(const std::string& session_id);

    // 消息添加
    void add_message(const std::string& role, const std::string& content);
    void add_user_message(const std::string& content);
    void add_assistant_message(const std::string& content);
    void add_system_message(const std::string& content);

    // 获取对话历史
    std::vector<MemoryItem> get_conversation_history(
        const std::string& session_id = "",
        int limit = 0  // 0 表示全部
    );

    // 构建 prompt 上下文
    std::string build_context_prompt(
        int max_turns = 10,
        int max_tokens = 4096
    );

    // 其他接口实现...
    void add(const MemoryItem& item) override;
    std::vector<MemoryItem> get(const std::string& query, int limit = 10) override;
    std::vector<MemoryItem> search(const std::string& query, int limit = 10) override;
    bool remove(const std::string& id) override;
    void clear() override;
    size_t size() const override;

private:
    std::string current_session_id_;
    std::unordered_map<std::string, std::deque<MemoryItem>> sessions_;
    std::mutex mutex_;
};
```

### 3.6 记忆管理器

```cpp
// 记忆管理器 - 整合所有记忆组件
class MemoryManager {
public:
    MemoryManager(
        std::shared_ptr<ShortTermMemory> short_term,
        std::shared_ptr<LongTermMemory> long_term,
        std::shared_ptr<SessionMemory> session
    );

    // 获取工作记忆（整合所有记忆层）
    std::vector<MemoryItem> get_working_memory(
        const std::string& query,
        int short_term_limit = 10,
        int long_term_limit = 5
    );

    // 添加记忆（自动选择合适的存储层）
    void add(const MemoryItem& item);

    // 记忆优先级处理
    void prioritize(MemoryItem& item);

    // 记忆遗忘（模拟人类遗忘）
    void forget(float importance_threshold = 0.3);

    // 获取所有短期记忆
    std::vector<MemoryItem> get_short_term() const;

    // 获取所有长期记忆
    std::vector<MemoryItem> get_long_term() const;

private:
    std::shared_ptr<ShortTermMemory> short_term_;
    std::shared_ptr<LongTermMemory> long_term_;
    std::shared_ptr<SessionMemory> session_;
};
```

---

## 4. ReAct 循环说明

### 4.1 ReAct 概述

ReAct（Reasoning + Acting）是一种结合推理和行动的 Agent 框架，通过交替执行推理和行动来解决复杂问题。

### 4.2 ReAct 循环流程

```
┌─────────────────────────────────────────────────────────────────────┐
│                        ReAct 循环                                    │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│     ┌─────────────────────────────────────────────────────────┐    │
│     │                     ReAct Loop                           │    │
│     │                                                         │    │
│     │   ┌─────────┐                                          │    │
│     │   │  Start  │                                          │    │
│     │   └────┬────┘                                          │    │
│     │        ▼                                                │    │
│     │   ┌─────────┐     ┌─────────┐     ┌─────────┐          │    │
│  ┌──│──▶│  Think  │────▶│  Action │────▶│ Observe │──────────┤    │
│  │  │   └─────────┘     └────┬────┘     └─────────┘          │    │
│  │  │                         │                                 │    │
│  │  │                         ▼                                 │    │
│  │  │   ┌─────────────────────────────────┐                    │    │
│  │  │   │        Decision Point           │                    │    │
│  │  │   │  finish / continue / max_iter  │                    │    │
│  │  │   └─────────────────────────────────┘                    │    │
│  │  │         │                  │              │              │    │
│  │  │         ▼                  ▼              ▼              │    │
│  │  │   ┌──────────┐      ┌──────────┐   ┌──────────┐        │    │
│  │  └──▶│  Finish  │      │  Next    │   │  Max     │        │    │
│  │      │  (结束)   │      │  Think   │   │  Iter    │        │    │
│  │      └──────────┘      └──────────┘   └──────────┘        │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 4.3 ReAct 状态定义

```cpp
// ReAct 状态 - 位于 include/modular_rag/agent/react.h
namespace modular_rag::agent {

/**
 * ReAct 动作类型
 */
enum class ActionType {
    RETRIEVE,           // 检索文档
    KNOWLEDGE_GRAPH,    // 查询知识图谱
    CALCULATE,          // 执行计算
    LOOKUP,             // 查找信息
    FINISH,             // 完成执行
    UNKNOWN             // 未知动作
};

/**
 * ReAct 单步状态
 */
struct ReActStep {
    int step_number;           // 步骤编号
    ActionType action_type;    // 动作类型
    std::string thought;       // 推理过程
    std::string action;         // 执行的动作
    json action_args;          // 动作参数
    std::string observation;   // 观察结果
    float confidence;          // 置信度
    bool is_final;            // 是否为最终步骤
};

/**
 * ReAct 执行结果
 */
struct ReActResult {
    std::string answer;                     // 最终答案
    std::vector<ReActStep> steps;          // 执行步骤历史
    int total_steps;                       // 总步数
    int total_tokens;                      // 消耗的 token 数
    int execution_time_ms;                  // 执行时间
    bool success;                          // 是否成功
    std::string error;                      // 错误信息（如果失败）
};

}  // namespace modular_rag::agent
```

### 4.4 ReAct 提示模板

```cpp
// ReAct 提示模板
const std::string REACT_PROMPT_TEMPLATE = R"(
你是一个智能助手，可以执行各种动作来回答用户的问题。

你可用的动作包括：
- retrieve: 从文档库检索相关信息（参数：query, top_k）
- knowledge_graph: 查询知识图谱（参数：entity, relation_type, max_hops）
- calculator: 执行数学计算（参数：expression）
- finish: 完成回答（参数：answer）

对于每个问题，你需要进行推理（Thought），然后决定动作（Action）。

开始：

问题：{question}

上下文：{context}

{steps}

请按照以下格式回答：

Thought: [你的推理过程]
Action: [选择的动作名称]
Action Args: [动作参数，使用 JSON 格式]
)";

// ReAct 解析响应
const std::string REACT_PARSE_PROMPT = R"(
根据以下 LLM 输出，解析出 Thought、Action 和 Action Args：

{llm_output}

请按以下格式返回：
THOUGHT: [推理内容]
ACTION: [动作名称]
ACTION_ARGS: [参数 JSON]
)";
```

### 4.5 ReAct 核心循环实现

```cpp
// ReAct Agent 实现
class ReActAgent {
public:
    ReActAgent(
        std::shared_ptr<LLMService> llm,
        std::shared_ptr<ToolRegistry> tools,
        std::shared_ptr<MemoryManager> memory
    );

    // 执行查询
    ReActResult query(const std::string& question);

    // 设置最大迭代次数
    void set_max_iterations(int max_iterations) {
        max_iterations_ = max_iterations;
    }

    // 设置超时时间
    void set_timeout_ms(int timeout_ms) {
        timeout_ms_ = timeout_ms;
    }

private:
    // 执行单步推理
    ReActStep execute_step(
        const std::string& question,
        const std::vector<ReActStep>& history,
        const std::string& context
    );

    // 解析 LLM 输出
    bool parse_llm_output(
        const std::string& output,
        std::string& thought,
        std::string& action,
        json& action_args
    );

    // 执行工具
    std::string execute_tool(
        const std::string& tool_name,
        const json& args
    );

    // 决策：继续、结束还是终止
    ActionType decide_continuation(
        const ReActStep& step,
        int current_iteration
    );

    // 构建上下文
    std::string build_context(
        const std::string& question,
        const std::vector<ReActStep>& history
    );

private:
    std::shared_ptr<LLMService> llm_;
    std::shared_ptr<ToolRegistry> tools_;
    std::shared_ptr<MemoryManager> memory_;
    int max_iterations_ = 10;
    int timeout_ms_ = 60000;
};

// 执行查询实现
ReActResult ReActAgent::query(const std::string& question) {
    ReActResult result;
    std::vector<ReActStep> steps;
    auto start_time = std::chrono::steady_clock::now();

    std::string context = build_context(question, {});

    for (int i = 0; i < max_iterations_; ++i) {
        // 执行单步
        auto step = execute_step(question, steps, context);
        steps.push_back(step);

        // 添加观察结果到上下文
        context += "\n\nStep " + std::to_string(i + 1) + ":\n";
        context += "Thought: " + step.thought + "\n";
        context += "Action: " + action_type_to_string(step.action_type) + "\n";
        context += "Observation: " + step.observation + "\n";

        // 决策
        auto decision = decide_continuation(step, i);
        if (decision == ActionType::FINISH) {
            result.answer = step.observation;
            result.is_final = true;
            break;
        } else if (decision == ActionType::UNKNOWN) {
            result.error = "无法解析动作";
            break;
        }
    }

    // 计算统计数据
    result.steps = steps;
    result.total_steps = steps.size();
    result.success = result.error.empty();

    auto end_time = std::chrono::steady_clock::now();
    result.execution_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    return result;
}
```

### 4.6 ReAct 执行示例

```
用户问题：谁是特斯拉 CEO 的父亲？

Step 1:
Thought: 我需要先查询埃隆·马斯克的信息，然后找出他父亲的信息。
Action: knowledge_graph
Action Args: {"entity": "埃隆·马斯克", "max_hops": 2}
Observation: 埃隆·马斯克是特斯拉和SpaceX的CEO。他的父亲是埃罗尔·马斯克。

Step 2:
Thought: 我已经找到了答案。埃隆·马斯克的父亲是埃罗尔·马斯克。
Action: finish
Action Args: {"answer": "特斯拉 CEO 埃隆·马斯克的父亲是埃罗尔·马斯克。"}
```

---

## 5. 集成示例

### 5.1 完整 Agent 使用示例

```cpp
#include <modular_rag/agent/agent.h>
#include <modular_rag/agent/tool.h>
#include <modular_rag/agent/memory.h>
#include <modular_rag/agent/react.h>

int main() {
    // 1. 初始化 LLM 服务
    auto llm = std::make_shared<LlamaService>();
    llm->init("./models/llama-2-7b-chat.gguf");

    // 2. 初始化存储
    auto vector_store = std::make_shared<VectorStore>();
    vector_store->init("./data/vectors.db");

    auto graph_store = std::make_shared<GraphStore>();
    graph_store->init("./data/graph.db");

    // 3. 初始化 Tool 系统
    auto tool_registry = std::make_shared<ToolRegistry>();
    tool_registry->register_tool(std::make_shared<RetrieveTool>(vector_store));
    tool_registry->register_tool(std::make_shared<KnowledgeGraphTool>(graph_store));
    tool_registry->register_tool(std::make_shared<CalculatorTool>());

    // 4. 初始化 Memory 系统
    auto short_term = std::make_shared<ShortTermMemory>(50);
    auto long_term = std::make_shared<LongTermMemory>(vector_store);
    auto session = std::make_shared<SessionMemory>();
    auto memory_manager = std::make_shared<MemoryManager>(
        short_term, long_term, session
    );

    // 5. 创建 ReAct Agent
    auto agent = std::make_shared<ReActAgent>(llm, tool_registry, memory_manager);
    agent->set_max_iterations(10);

    // 6. 执行查询
    std::string question = "特斯拉 CEO 的父亲是谁？";
    auto result = agent->query(question);

    // 7. 输出结果
    std::cout << "Answer: " << result.answer << std::endl;
    std::cout << "Steps: " << result.total_steps << std::endl;
    std::cout << "Time: " << result.execution_time_ms << "ms" << std::endl;

    return 0;
}
```

### 5.2 Agent 配置示例

```yaml
# agent 配置
agent:
  type: "react"  # agent 类型: react, plan, chat, rag
  max_iterations: 10
  timeout_ms: 60000
  confidence_threshold: 0.7

llm:
  model_path: "./models/llama-2-7b-chat.gguf"
  temperature: 0.7
  max_tokens: 2048

memory:
  short_term:
    max_size: 50
  long_term:
    enabled: true
    compression_threshold: 0.5
  session:
    max_history: 100

tools:
  - name: "retrieve"
    enabled: true
  - name: "knowledge_graph"
    enabled: true
  - name: "calculator"
    enabled: true
```

---

## 6. 相关文档

- [概述](./01-overview.md) - 项目概述、架构、9 种 Pipeline 总览
- [Pipeline 详细指南](./02-pipeline-guide.md) - 9 种 Pipeline 详细介绍
- [API 参考文档](./04-api-reference.md) - REST API 端点、CLI 命令
