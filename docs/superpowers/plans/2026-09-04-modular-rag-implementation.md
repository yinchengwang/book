# Modular RAG 框架实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现支持 9 种 RAG 类型的模块化 RAG 框架，包含 llama.cpp 集成、自研 Agent 框架和自研数据库存储。

**Architecture:** 基于现有 RAG 项目（`engineering/rag/`）扩展，采用 Pipeline 模式编排各模块。核心层提供基础组件（分块器、检索器、LLM），Pipeline 层组合成 9 种 RAG 类型，Agent 层支持动态编排。

**Tech Stack:** C++17, CMake 3.20+, llama.cpp, 自研数据库引擎

## Global Constraints

- 所有代码必须使用**中文注释**
- Commit Message 必须使用**中文**
- 代码风格遵循现有 `.clang-format`
- 编译产物输出到 `build/modular_rag/`
- 测试产物输出到 `test-results/modular_rag/`
- 文档写入 `docs/modular-rag/`

---

## 文件结构

```
engineering/rag/
├── include/rag/
│   ├── modular/                    # 新增 Modular RAG 头文件
│   │   ├── modular_rag.h         # 主头文件
│   │   ├── types.h               # Modular RAG 类型
│   │   ├── config.h              # 配置
│   │   │
│   │   ├── llm/                  # LLM 服务（新增）
│   │   │   ├── llama_service.h   # llama.cpp 封装
│   │   │   └── embedding_service.h
│   │   │
│   │   ├── agent/                # Agent 框架（新增）
│   │   │   ├── agent.h
│   │   │   ├── tool.h
│   │   │   ├── memory.h
│   │   │   └── planner.h
│   │   │
│   │   └── pipeline/             # 9 种 Pipeline（新增）
│   │       ├── pipeline_factory.h
│   │       └── pipeline_types.h
│   │
│   └── rag/                      # 现有头文件保留
│       ├── types.h
│       ├── pipeline.h
│       └── ...
│
├── src/rag/
│   ├── modular/                   # 新增实现
│   │   ├── llm/
│   │   ├── agent/
│   │   └── pipeline/
│   │
│   └── rag/                      # 现有实现保留
│       ├── chunker/
│       ├── retriever/
│       └── ...
│
├── third_party/                   # 第三方依赖
│   └── llama.cpp/                 # 作为 git submodule
│
├── test/rag/
│   └── modular/                   # 新增测试
│
└── CMakeLists.txt                 # 修改
```

---

## Phase 1: 基础设施

### Task 1: 集成 llama.cpp

**Files:**
- Create: `engineering/rag/include/rag/modular/llm/llama_service.h`
- Create: `engineering/rag/src/rag/modular/llm/llama_service.cpp`
- Create: `engineering/rag/include/rag/modular/llm/embedding_service.h`
- Create: `engineering/rag/src/rag/modular/llm/embedding_service.cpp`
- Modify: `engineering/rag/CMakeLists.txt` (添加 llama.cpp 集成)

**Interfaces:**
- Produces: `class LlamaService : public LLMService`, `class ModularEmbeddingService`

- [ ] **Step 1: 添加 llama.cpp 作为 git submodule**

```bash
cd D:/code/book/engineering/rag
git submodule add https://github.com/ggerganov/llama.cpp third_party/llama.cpp
git commit -m "feat(rag): 添加 llama.cpp 作为 submodule"
```

- [ ] **Step 2: 创建 llama_service.h 头文件**

```cpp
// engineering/rag/include/rag/modular/llm/llama_service.h
#pragma once

#include "rag/llm_service.h"
#include <string>
#include <memory>

namespace rag::modular {

// LlamaService 配置
struct LlamaConfig {
    std::string model_path;          // GGUF 模型文件路径
    int n_ctx = 4096;               // 上下文窗口
    int n_gpu_layers = -1;           // GPU 层数 (-1=全部)
    int n_threads = 4;              // CPU 线程
    int n_batch = 512;              // 批处理大小
    float temperature = 0.7f;       // 采样温度
    int max_tokens = 2048;          // 最大生成长度
    bool verbose = false;            // 详细输出
};

// llama.cpp LLM 服务实现
class LlamaService : public LLMService {
public:
    LlamaService();
    ~LlamaService() override;

    // 实现 LLMService 接口
    void load(const std::string& model_path, const LLMConfig& config) override;
    void unload() override;
    bool is_loaded() const override;

    GenerateResult generate(const std::string& prompt,
                           const GenerateOptions& options = {}) override;

    void generate_stream(const std::string& prompt,
                        const GenerateOptions& options,
                        StreamCallback callback) override;

    int context_window() const override;
    const std::string& model_type() const override;
    const std::string& model_path() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace rag::modular
```

- [ ] **Step 3: 创建 llama_service.cpp 实现**

```cpp
// engineering/rag/src/rag/modular/llm/llama_service.cpp
#include "rag/modular/llm/llama_service.h"
#include "rag/logger.h"
#include <common/ggml.h>
#include <llama.h>
// ... 实现细节
```

- [ ] **Step 4: 更新 CMakeLists.txt 添加 llama.cpp 编译**

```cmake
# 添加 llama.cpp 子目录
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/llama.cpp/examples/llama-bench
                 ${CMAKE_BINARY_DIR}/llama-bench EXCLUDE_FROM_ALL)

# 添加 modular RAG 库
add_library(modular_rag_lib STATIC
    src/rag/modular/llm/llama_service.cpp
    src/rag/modular/llm/embedding_service.cpp
)
target_link_libraries(modular_rag_lib PRIVATE llama)
```

- [ ] **Step 5: 编译验证**

```bash
cd D:/code/book/engineering/rag
cmake -B build_modular -S . -DBUILD_MODULAR_RAG=ON
cmake --build build_modular --target modular_rag_lib
```

- [ ] **Step 6: 提交**

```bash
git add -A
git commit -m "feat(rag): 集成 llama.cpp 实现 LlamaService"
```

---

### Task 2: 定义 Modular RAG 类型

**Files:**
- Create: `engineering/rag/include/rag/modular/types.h`
- Create: `engineering/rag/include/rag/modular/config.h`
- Create: `engineering/rag/src/rag/modular/types.cpp`

**Interfaces:**
- Produces: `enum class PipelineType`, `struct ModularQuery`, `struct ModularQueryResult`, `struct ModularConfig`

- [ ] **Step 1: 创建 types.h**

```cpp
// engineering/rag/include/rag/modular/types.h
#pragma once

#include "rag/types.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace rag::modular {

// ========== Pipeline 类型 ==========

enum class PipelineType {
    NAIVE,        // 基础 RAG
    ADVANCED,     // 高级 RAG (混合检索 + 重排序)
    HYBRID,       // 三路召回 (Vector + BM25 + Graph)
    HYDE,         // HyDE (假设答案引导)
    GRAPH,        // Graph RAG (知识图谱)
    CORRECTIVE,   // Corrective RAG (纠正)
    REACT,        // ReAct RAG (推理+行动)
    ITERATIVE,    // Iterative RAG (迭代)
    RECURSIVE     // Recursive RAG (递归分解)
};

// ========== 查询 ==========

struct ModularQuery {
    std::string text;
    PipelineType pipeline_type = PipelineType::ADVANCED;
    int top_k = 5;
    std::unordered_map<std::string, std::string> options;
};

// ========== 查询结果 ==========

struct ModularQueryResult {
    bool success = false;
    std::string answer;
    std::vector<RetrievalResult> context;
    int64_t retrieval_time_ms = 0;
    int64_t generation_time_ms = 0;
    int64_t total_time_ms = 0;
    int total_tokens = 0;
    std::string error_message;
};

// ========== 工具函数 ==========

std::string pipeline_type_to_string(PipelineType type);
PipelineType string_to_pipeline_type(const std::string& str);
std::vector<std::string> list_pipeline_types();

} // namespace rag::modular
```

- [ ] **Step 2: 创建 config.h**

```cpp
// engineering/rag/include/rag/modular/config.h
#pragma once

#include "rag/config.h"
#include "rag/modular/types.h"
#include <string>

namespace rag::modular {

// Modular RAG 配置
struct ModularConfig {
    // Pipeline 选择
    PipelineType default_pipeline = PipelineType::ADVANCED;

    // LLM 配置
    LLMConfig llm;

    // Embedding 配置
    EmbeddingConfig embedding;

    // 检索配置
    RetrievalConfig retrieval;

    // Agent 配置
    AgentConfig agent;

    // 路径配置
    std::string model_path;          // LLM 模型路径
    std::string embedding_model_path; // Embedding 模型路径
    std::string data_dir;           // 数据目录
    std::string index_dir;          // 索引目录
};

} // namespace rag::modular
```

- [ ] **Step 3: 提交**

```bash
git add -A
git commit -m "feat(rag): 定义 Modular RAG 类型和配置"
```

---

## Phase 2: Pipeline 框架

### Task 3: 创建 Pipeline 基类和工厂

**Files:**
- Create: `engineering/rag/include/rag/modular/pipeline/pipeline_base.h`
- Create: `engineering/rag/include/rag/modular/pipeline/pipeline_factory.h`
- Create: `engineering/rag/src/rag/modular/pipeline/pipeline_base.cpp`
- Create: `engineering/rag/src/rag/modular/pipeline/pipeline_factory.cpp`

**Interfaces:**
- Consumes: `class LlamaService`, `class Retriever`, `class Chunker`
- Produces: `class ModularPipeline`, `class PipelineFactory`

- [ ] **Step 1: 创建 pipeline_base.h**

```cpp
// engineering/rag/include/rag/modular/pipeline/pipeline_base.h
#pragma once

#include "rag/modular/types.h"
#include "rag/modular/config.h"
#include "rag/llm_service.h"
#include "rag/retriever.h"
#include <string>
#include <memory>

namespace rag::modular {

// Pipeline 基类
class ModularPipeline {
public:
    virtual ~ModularPipeline() = default;

    // 获取 Pipeline 类型
    virtual PipelineType type() const = 0;

    // 获取名称
    virtual std::string name() const = 0;

    // 初始化
    virtual bool init(const ModularConfig& config) = 0;

    // 查询
    virtual ModularQueryResult query(const ModularQuery& query) = 0;

    // 健康检查
    virtual bool is_ready() const = 0;

protected:
    // 构建上下文
    std::string build_context(
        const std::string& query,
        const std::vector<RetrievalResult>& results);

    // 调用 LLM 生成
    std::string generate_with_llm(
        const std::string& prompt,
        const GenerateOptions& options = {});

    std::shared_ptr<LLMService> llm_;
    std::shared_ptr<Retriever> retriever_;
    ModularConfig config_;
};

} // namespace rag::modular
```

- [ ] **Step 2: 创建 pipeline_factory.h**

```cpp
// engineering/rag/include/rag/modular/pipeline/pipeline_factory.h
#pragma once

#include "rag/modular/pipeline/pipeline_base.h"
#include "rag/modular/config.h"
#include <memory>
#include <unordered_map>

namespace rag::modular {

class PipelineFactory {
public:
    // 创建指定类型的 Pipeline
    static std::unique_ptr<ModularPipeline> create(
        PipelineType type,
        const ModularConfig& config);

    // 创建所有 Pipeline
    static std::unordered_map<PipelineType, std::unique_ptr<ModularPipeline>>
    create_all(const ModularConfig& config);

    // 获取 Pipeline 描述
    static std::string get_description(PipelineType type);

    // 获取 Pipeline 需要的组件
    static std::vector<std::string> get_required_components(PipelineType type);
};

} // namespace rag::modular
```

- [ ] **Step 3: 实现 pipeline_base.cpp**

```cpp
// engineering/rag/src/rag/modular/pipeline/pipeline_base.cpp
#include "rag/modular/pipeline/pipeline_base.h"

namespace rag::modular {

std::string ModularPipeline::build_context(
    const std::string& query,
    const std::vector<RetrievalResult>& results) {
    
    std::string context = "基于以下文档回答问题：\n\n";
    
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& chunk = results[i].chunk;
        context += "【文档 " + std::to_string(i + 1) + "】\n";
        context += chunk.content + "\n\n";
    }
    
    context += "问题：" + query + "\n";
    return context;
}

std::string ModularPipeline::generate_with_llm(
    const std::string& prompt,
    const GenerateOptions& options) {
    
    if (!llm_ || !llm_->is_loaded()) {
        return "LLM 服务未初始化";
    }
    
    auto result = llm_->generate(prompt, options);
    return result.text;
}

} // namespace rag::modular
```

- [ ] **Step 4: 提交**

```bash
git add -A
git commit -m "feat(rag): 创建 Pipeline 基类和工厂"
```

---

### Task 4: 实现 9 种 Pipeline（第 1 批）

**Files:**
- Create: `engineering/rag/include/rag/modular/pipeline/naive_pipeline.h`
- Create: `engineering/rag/src/rag/modular/pipeline/naive_pipeline.cpp`
- Create: `engineering/rag/include/rag/modular/pipeline/advanced_pipeline.h`
- Create: `engineering/rag/src/rag/modular/pipeline/advanced_pipeline.cpp`
- Create: `engineering/rag/include/rag/modular/pipeline/hybrid_pipeline.h`
- Create: `engineering/rag/src/rag/modular/pipeline/hybrid_pipeline.cpp`

**Interfaces:**
- Consumes: `class ModularPipeline`, `class LlamaService`, `class Retriever`
- Produces: `class NaivePipeline`, `class AdvancedPipeline`, `class HybridPipeline`

- [ ] **Step 1: 创建 naive_pipeline.h**

```cpp
// engineering/rag/include/rag/modular/pipeline/naive_pipeline.h
#pragma once

#include "rag/modular/pipeline/pipeline_base.h"

namespace rag::modular {

// Naive RAG Pipeline
// 流程: Query → Vector检索 → Context → LLM
class NaivePipeline : public ModularPipeline {
public:
    PipelineType type() const override { return PipelineType::NAIVE; }
    std::string name() const override { return "Naive RAG"; }
    
    bool init(const ModularConfig& config) override;
    ModularQueryResult query(const ModularQuery& query) override;
};

} // namespace rag::modular
```

- [ ] **Step 2: 实现 naive_pipeline.cpp**

```cpp
// engineering/rag/src/rag/modular/pipeline/naive_pipeline.cpp
#include "rag/modular/pipeline/naive_pipeline.h"
#include "rag/logger.h"

namespace rag::modular {

bool NaivePipeline::init(const ModularConfig& config) {
    config_ = config;
    // 初始化组件...
    return true;
}

ModularQueryResult NaivePipeline::query(const ModularQuery& query) {
    auto start = std::chrono::steady_clock::now();
    
    ModularQueryResult result;
    
    // 1. 检索
    auto retrieval_start = std::chrono::steady_clock::now();
    auto retrieved = retriever_->retrieve(query.text, query.top_k);
    auto retrieval_end = std::chrono::steady_clock::now();
    result.retrieval_time_ms = 
        std::chrono::duration_cast<std::chrono::milliseconds>(
            retrieval_end - retrieval_start).count();
    
    // 2. 构建上下文
    auto context = build_context(query.text, retrieved);
    
    // 3. LLM 生成
    auto gen_start = std::chrono::steady_clock::now();
    std::string prompt = context + "\n请基于以上文档回答问题。";
    result.answer = generate_with_llm(prompt);
    auto gen_end = std::chrono::steady_clock::now();
    result.generation_time_ms = 
        std::chrono::duration_cast<std::chrono::milliseconds>(
            gen_end - gen_start).count();
    
    result.context = std::move(retrieved);
    result.success = true;
    result.total_time_ms = result.retrieval_time_ms + result.generation_time_ms;
    
    return result;
}

} // namespace rag::modular
```

- [ ] **Step 3: 创建 advanced_pipeline.h/cpp**

```cpp
// engineering/rag/include/rag/modular/pipeline/advanced_pipeline.h
#pragma once

#include "rag/modular/pipeline/pipeline_base.h"
#include "rag/reranker.h"

namespace rag::modular {

// Advanced RAG Pipeline
// 流程: Query → QueryExp → Vector+BM25 → RRF → Rerank → Context → LLM
class AdvancedPipeline : public ModularPipeline {
public:
    PipelineType type() const override { return PipelineType::ADVANCED; }
    std::string name() const override { return "Advanced RAG"; }
    
    bool init(const ModularConfig& config) override;
    ModularQueryResult query(const ModularQuery& query) override;

private:
    // 查询扩展
    std::string expand_query(const std::string& query);
    
    // RRF 融合
    std::vector<RetrievalResult> rrf_fusion(
        const std::vector<std::vector<RetrievalResult>>& results,
        int k = 60);
    
    // 重排序
    std::vector<RetrievalResult> rerank(
        const std::string& query,
        const std::vector<RetrievalResult>& candidates);
    
    std::shared_ptr<Reranker> reranker_;
};

} // namespace rag::modular
```

- [ ] **Step 4: 创建 hybrid_pipeline.h/cpp**

```cpp
// engineering/rag/include/rag/modular/pipeline/hybrid_pipeline.h
#pragma once

#include "rag/modular/pipeline/pipeline_base.h"
#include "rag/graph_retriever.h"

namespace rag::modular {

// Hybrid RAG Pipeline
// 流程: Query → Vector + BM25 + Graph → RRF融合 → LLM
class HybridPipeline : public ModularPipeline {
public:
    PipelineType type() const override { return PipelineType::HYBRID; }
    std::string name() const override { return "Hybrid RAG"; }
    
    bool init(const ModularConfig& config) override;
    ModularQueryResult query(const ModularQuery& query) override;

private:
    std::shared_ptr<GraphRetriever> graph_retriever_;
    std::vector<RetrievalResult> multi_way_fusion(
        const std::vector<std::vector<RetrievalResult>>& results);
};

} // namespace rag::modular
```

- [ ] **Step 5: 提交**

```bash
git add -A
git commit -m "feat(rag): 实现 Naive/Advanced/Hybrid Pipeline"
```

---

### Task 5: 实现 9 种 Pipeline（第 2 批）

**Files:**
- Create: `engineering/rag/include/rag/modular/pipeline/hyde_pipeline.h`
- Create: `engineering/rag/src/rag/modular/pipeline/hyde_pipeline.cpp`
- Create: `engineering/rag/include/rag/modular/pipeline/graph_pipeline.h`
- Create: `engineering/rag/src/rag/modular/pipeline/graph_pipeline.cpp`
- Create: `engineering/rag/include/rag/modular/pipeline/corrective_pipeline.h`
- Create: `engineering/rag/src/rag/modular/pipeline/corrective_pipeline.cpp`

**Interfaces:**
- Consumes: `class ModularPipeline`, `class LlamaService`, `class GraphRetriever`
- Produces: `class HyDEPipeline`, `class GraphPipeline`, `class CorrectivePipeline`

- [ ] **Step 1: 实现 HyDE Pipeline**

```cpp
// HyDE: 生成假设答案 → 用假设答案检索
// engineering/rag/src/rag/modular/pipeline/hyde_pipeline.cpp

ModularQueryResult HyDEPipeline::query(const ModularQuery& query) {
    // 1. 用 LLM 生成假设答案
    std::string hypothesis_prompt = 
        "请为以下问题生成一个简短的可能答案：\n" + query.text;
    std::string hypothetical = generate_with_llm(hypothesis_prompt);
    
    // 2. 用假设答案检索
    auto results = retriever_->retrieve(hypothetical, query.top_k);
    
    // 3. 用原始问题 + 检索结果生成最终答案
    std::string context = build_context(query.text, results);
    std::string final_prompt = context + "\n请基于以上文档回答：\n" + query.text;
    std::string answer = generate_with_llm(final_prompt);
    
    // ... 返回结果
}
```

- [ ] **Step 2: 实现 Graph Pipeline**

```cpp
// Graph RAG: 实体提取 → 图检索 → 子图 → LLM
// engineering/rag/src/rag/modular/pipeline/graph_pipeline.cpp

ModularQueryResult GraphPipeline::query(const ModularQuery& query) {
    // 1. 实体提取
    auto entities = extract_entities(query.text);
    
    // 2. 图检索（多跳）
    auto graph_results = graph_retriever_->retrieve(
        entities, query.top_k);
    
    // 3. 向量检索补充
    auto vector_results = retriever_->retrieve(query.text, query.top_k);
    
    // 4. 合并结果
    auto merged = merge_results(graph_results, vector_results);
    
    // 5. 生成答案
    // ...
}
```

- [ ] **Step 3: 实现 Corrective Pipeline**

```cpp
// Corrective RAG: 检索 → 评估质量 → 纠正（如果需要）
// engineering/rag/src/rag/modular/pipeline/corrective_pipeline.cpp

ModularQueryResult CorrectivePipeline::query(const ModularQuery& query) {
    // 1. 检索
    auto results = retriever_->retrieve(query.text, query.top_k);
    
    // 2. 评估质量
    double quality = evaluate_quality(query.text, results);
    
    // 3. 如果质量不够，尝试纠正
    if (quality < quality_threshold_) {
        // 改写查询重试
        std::string rewritten = rewrite_query(query.text);
        results = retriever_->retrieve(rewritten, query.top_k);
        
        // 或者用更多上下文补充
        auto additional = retriever_->retrieve(query.text, query.top_k * 2);
        results = merge_and_filter(results, additional);
    }
    
    // 4. 生成答案
    // ...
}
```

- [ ] **Step 4: 提交**

```bash
git add -A
git commit -m "feat(rag): 实现 HyDE/Graph/Corrective Pipeline"
```

---

### Task 6: 实现 9 种 Pipeline（第 3 批）

**Files:**
- Create: `engineering/rag/include/rag/modular/pipeline/react_pipeline.h`
- Create: `engineering/rag/src/rag/modular/pipeline/react_pipeline.cpp`
- Create: `engineering/rag/include/rag/modular/pipeline/iterative_pipeline.h`
- Create: `engineering/rag/src/rag/modular/pipeline/iterative_pipeline.cpp`
- Create: `engineering/rag/include/rag/modular/pipeline/recursive_pipeline.h`
- Create: `engineering/rag/src/rag/modular/pipeline/recursive_pipeline.cpp`

**Interfaces:**
- Consumes: `class ModularPipeline`, `class LlamaService`, `class Agent`
- Produces: `class ReActPipeline`, `class IterativePipeline`, `class RecursivePipeline`

- [ ] **Step 1: 实现 ReAct Pipeline（需要 Agent 框架，先占位）**

```cpp
// ReAct RAG: Agent 驱动的动态检索
// engineering/rag/src/rag/modular/pipeline/react_pipeline.cpp

ModularQueryResult ReActPipeline::query(const ModularQuery& query) {
    // ReAct 循环：Thought → Action → Observation → ...
    // 需要先实现 Agent 框架，这里先占位
    
    // TODO: 集成 Agent
    // auto agent = create_agent(config_.agent);
    // return agent->execute(query);
    
    // 暂时回退到 Advanced Pipeline
    return fallback_to_advanced(query);
}
```

- [ ] **Step 2: 实现 Iterative Pipeline**

```cpp
// Iterative RAG: 迭代检索 refinement
// engineering/rag/src/rag/modular/pipeline/iterative_pipeline.cpp

ModularQueryResult IterativePipeline::query(const ModularQuery& query) {
    std::string current_query = query.text;
    std::vector<RetrievalResult> all_results;
    
    for (int iter = 0; iter < max_iterations_; ++iter) {
        // 检索
        auto results = retriever_->retrieve(current_query, query.top_k);
        all_results.insert(all_results.end(), results.begin(), results.end());
        
        // 评估是否足够
        std::string answer = generate_with_llm(
            build_context(current_query, results) + "\n这个问题回答完整吗？");
        
        if (evaluate_completeness(answer)) {
            break;
        }
        
        // 改写查询继续
        current_query = rewrite_for_next_iteration(current_query, answer);
    }
    
    // 最终生成
    return finalize_answer(query.text, all_results);
}
```

- [ ] **Step 3: 实现 Recursive Pipeline**

```cpp
// Recursive RAG: 递归问题分解
// engineering/rag/src/rag/modular/pipeline/recursive_pipeline.cpp

ModularQueryResult RecursivePipeline::query(const ModularQuery& query) {
    // 1. 分解问题
    std::vector<std::string> sub_queries = decompose(query.text);
    
    // 2. 递归回答每个子问题
    std::vector<std::string> sub_answers;
    for (const auto& sq : sub_queries) {
        auto result = retriever_->retrieve(sq, query.top_k);
        std::string answer = generate_with_llm(
            build_context(sq, result) + "\n回答：" + sq);
        sub_answers.push_back(answer);
    }
    
    // 3. 合并答案
    std::string synthesis_prompt = 
        "请综合以下答案回答原问题：\n" +
        "原问题：" + query.text + "\n\n" +
        "子问题答案：\n" + join(sub_answers, "\n---\n");
    
    ModularQueryResult result;
    result.answer = generate_with_llm(synthesis_prompt);
    result.success = true;
    return result;
}
```

- [ ] **Step 4: 提交**

```bash
git add -A
git commit -m "feat(rag): 实现 ReAct/Iterative/Recursive Pipeline"
```

---

## Phase 3: Agent 框架

### Task 7: 实现 Tool 系统

**Files:**
- Create: `engineering/rag/include/rag/modular/agent/tool.h`
- Create: `engineering/rag/src/rag/modular/agent/tool.cpp`

**Interfaces:**
- Produces: `class Tool`, `class VectorSearchTool`, `class ToolRegistry`

- [ ] **Step 1: 创建 tool.h**

```cpp
// engineering/rag/include/rag/modular/agent/tool.h
#pragma once

#include "rag/types.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>

namespace rag::modular::agent {

// Tool 执行结果
struct ToolResult {
    bool success = false;
    std::string output;
    std::string error;
    int64_t duration_ms = 0;
};

// Tool 基类
class Tool {
public:
    virtual ~Tool() = default;
    
    virtual std::string name() const = 0;
    virtual std::string description() const = 0;
    virtual std::string parameters_json_schema() const = 0;
    virtual ToolResult execute(const std::string& parameters) = 0;
    virtual bool validate_parameters(const std::string& params) const { return true; }
};

// 向量搜索 Tool
class VectorSearchTool : public Tool {
public:
    VectorSearchTool(std::shared_ptr<Retriever> retriever);
    
    std::string name() const override { return "vector_search"; }
    std::string description() const override { 
        return "搜索向量数据库，返回相关文档片段"; 
    }
    std::string parameters_json_schema() const override;
    ToolResult execute(const std::string& parameters) override;

private:
    std::shared_ptr<Retriever> retriever_;
};

// BM25 搜索 Tool
class BM25SearchTool : public Tool { /* ... */ };

// 图搜索 Tool
class GraphSearchTool : public Tool { /* ... */ };

// LLM 生成 Tool
class LLMGenerateTool : public Tool { /* ... */ };

// Tool 注册表
class ToolRegistry {
public:
    void register_tool(std::shared_ptr<Tool> tool);
    std::shared_ptr<Tool> get_tool(const std::string& name);
    std::vector<std::string> list_tools() const;
    
private:
    std::unordered_map<std::string, std::shared_ptr<Tool>> tools_;
};

} // namespace rag::modular::agent
```

- [ ] **Step 2: 实现 tool.cpp**

```cpp
// engineering/rag/src/rag/modular/agent/tool.cpp
#include "rag/modular/agent/tool.h"
#include "rag/logger.h"

namespace rag::modular::agent {

ToolResult VectorSearchTool::execute(const std::string& parameters) {
    auto start = std::chrono::steady_clock::now();
    
    ToolResult result;
    try {
        // 解析参数
        auto params = json::parse(parameters);
        std::string query = params["query"];
        int top_k = params.value("top_k", 5);
        
        // 执行检索
        auto results = retriever_->retrieve(query, top_k);
        
        // 序列化结果
        json output;
        for (const auto& r : results) {
            output.push_back({
                {"chunk_id", r.chunk.id},
                {"content", r.chunk.content},
                {"score", r.score}
            });
        }
        result.output = output.dump();
        result.success = true;
    } catch (const std::exception& e) {
        result.error = e.what();
        result.success = false;
    }
    
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    
    return result;
}

void ToolRegistry::register_tool(std::shared_ptr<Tool> tool) {
    tools_[tool->name()] = tool;
    RAG_LOG_INFO("Registered tool: {}", tool->name());
}

} // namespace rag::modular::agent
```

- [ ] **Step 3: 提交**

```bash
git add -A
git commit -m "feat(rag): 实现 Agent Tool 系统"
```

---

### Task 8: 实现 Agent 核心

**Files:**
- Create: `engineering/rag/include/rag/modular/agent/agent.h`
- Create: `engineering/rag/src/rag/modular/agent/agent.cpp`
- Create: `engineering/rag/include/rag/modular/agent/memory.h`
- Create: `engineering/rag/src/rag/modular/agent/memory.cpp`

**Interfaces:**
- Consumes: `class Tool`, `class LlamaService`
- Produces: `class Agent`, `class Memory`

- [ ] **Step 1: 创建 agent.h**

```cpp
// engineering/rag/include/rag/modular/agent/agent.h
#pragma once

#include "rag/modular/agent/tool.h"
#include "rag/modular/types.h"
#include <string>
#include <vector>
#include <memory>

namespace rag::modular::agent {

// Agent 配置
struct AgentConfig {
    int max_iterations = 10;
    int max_retries = 3;
    float temperature = 0.0f;
    bool verbose = false;
};

// Agent 状态
enum class AgentState {
    IDLE,
    THINKING,
    ACTING,
    OBSERVING,
    FINISHED,
    FAILED
};

// ReAct 步骤
struct ReActStep {
    int step_id;
    std::string thought;
    std::string action;
    std::string action_input;
    std::string observation;
    bool is_final = false;
};

// Agent 响应
struct AgentResponse {
    bool success = false;
    std::string output;
    std::vector<ReActStep> steps;
    int iterations_used = 0;
    std::string error;
};

// 核心 Agent
class Agent {
public:
    Agent(const AgentConfig& config);
    
    void initialize(std::shared_ptr<LLMService> llm);
    void register_tool(std::shared_ptr<Tool> tool);
    void register_tools(const std::vector<std::shared_ptr<Tool>>& tools);
    
    AgentResponse execute(const std::string& query);
    AgentState state() const { return state_; }
    
private:
    // ReAct 循环
    ReActStep think(const std::string& query, 
                    const std::vector<ReActStep>& history);
    ToolResult act(const std::string& action, const std::string& params);
    std::string observe(const ToolResult& result);
    bool is_finished(const std::string& final_answer);
    
    AgentConfig config_;
    AgentState state_ = AgentState::IDLE;
    std::shared_ptr<LLMService> llm_;
    std::shared_ptr<ToolRegistry> tools_;
    std::shared_ptr<Memory> memory_;
};

} // namespace rag::modular::agent
```

- [ ] **Step 2: 实现 agent.cpp (ReAct 循环)**

```cpp
// engineering/rag/src/rag/modular/agent/agent.cpp
#include "rag/modular/agent/agent.h"

namespace rag::modular::agent {

AgentResponse Agent::execute(const std::string& query) {
    AgentResponse response;
    std::vector<ReActStep> history;
    
    state_ = AgentState::THINKING;
    
    for (int iter = 0; iter < config_.max_iterations; ++iter) {
        // 1. Think: LLM 生成 thought
        auto step = think(query, history);
        step.step_id = iter;
        
        // 2. Check if finished
        if (step.is_final) {
            response.output = step.observation;
            response.success = true;
            response.steps = history;
            response.iterations_used = iter + 1;
            return response;
        }
        
        // 3. Act: 执行 Tool
        auto tool_result = act(step.action, step.action_input);
        step.observation = tool_result.success ? 
            tool_result.output : tool_result.error;
        
        history.push_back(step);
        
        // 4. Observe: 获取结果
        // (observation 已填充)
    }
    
    response.success = false;
    response.error = "达到最大迭代次数";
    return response;
}

ReActStep Agent::think(const std::string& query,
                       const std::vector<ReActStep>& history) {
    ReActStep step;
    
    // 构建 prompt
    std::string prompt = build_react_prompt(query, history);
    
    // 调用 LLM
    auto result = llm_->generate(prompt, { .temperature = config_.temperature });
    
    // 解析结果 (简单解析，实际需要更复杂的解析逻辑)
    auto parsed = parse_llm_response(result.text);
    step.thought = parsed.thought;
    step.action = parsed.action;
    step.action_input = parsed.action_input;
    step.is_final = parsed.is_final;
    
    return step;
}

ToolResult Agent::act(const std::string& action, const std::string& params) {
    auto tool = tools_->get_tool(action);
    if (!tool) {
        return { false, "", "Tool not found: " + action };
    }
    return tool->execute(params);
}

} // namespace rag::modular::agent
```

- [ ] **Step 3: 创建 memory.h/cpp**

```cpp
// engineering/rag/include/rag/modular/agent/memory.h
#pragma once

#include <string>
#include <vector>

namespace rag::modular::agent {

// 记忆条目
struct MemoryEntry {
    std::string role;        // user/assistant/system/tool
    std::string content;
    std::string timestamp;
};

// 记忆类型
enum class MemoryType {
    SHORT_TERM,    // 短期记忆
    LONG_TERM,     // 长期记忆
    WORKING        // 工作记忆
};

class Memory {
public:
    Memory(MemoryType type);
    
    void add(const MemoryEntry& entry);
    std::vector<MemoryEntry> get_recent(int count);
    std::vector<MemoryEntry> search(const std::string& query);
    void clear();
    std::string to_context();

private:
    MemoryType type_;
    std::vector<MemoryEntry> entries_;
};

} // namespace rag::modular::agent
```

- [ ] **Step 4: 提交**

```bash
git add -A
git commit -m "feat(rag): 实现 Agent 核心和 Memory"
```

---

### Task 9: 集成 ReAct Pipeline 到 Agent

**Files:**
- Modify: `engineering/rag/src/rag/modular/pipeline/react_pipeline.cpp`

**Interfaces:**
- Consumes: `class Agent`, `class ToolRegistry`
- Produces: 完整的 ReAct Pipeline

- [ ] **Step 1: 更新 react_pipeline.cpp**

```cpp
// engineering/rag/src/rag/modular/pipeline/react_pipeline.cpp
#include "rag/modular/pipeline/react_pipeline.h"
#include "rag/modular/agent/agent.h"

namespace rag::modular {

ModularQueryResult ReActPipeline::query(const ModularQuery& query) {
    auto start = std::chrono::steady_clock::now();
    
    // 创建 Agent
    auto agent = std::make_shared<Agent>(config_.agent);
    agent->initialize(llm_);
    
    // 注册 Tool
    agent->register_tools(create_tools());
    
    // 执行
    auto response = agent->execute(query.text);
    
    ModularQueryResult result;
    result.success = response.success;
    result.answer = response.output;
    result.total_time_ms = 
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
    
    if (!response.success) {
        result.error_message = response.error;
    }
    
    return result;
}

std::vector<std::shared_ptr<agent::Tool>> 
ReActPipeline::create_tools() {
    return {
        std::make_shared<agent::VectorSearchTool>(retriever_),
        std::make_shared<agent::BM25SearchTool>(/* ... */),
        std::make_shared<agent::GraphSearchTool>(/* ... */)
    };
}

} // namespace rag::modular
```

- [ ] **Step 2: 提交**

```bash
git add -A
git commit -m "feat(rag): 集成 Agent 到 ReAct Pipeline"
```

---

## Phase 4: 接口层

### Task 10: 实现 REST API

**Files:**
- Create: `engineering/rag/include/rag/modular/api/server.h`
- Create: `engineering/rag/src/rag/modular/api/server.cpp`

**Interfaces:**
- Consumes: `class PipelineFactory`, `class ModularPipeline`
- Produces: HTTP Server with endpoints

- [ ] **Step 1: 创建 server.h**

```cpp
// engineering/rag/include/rag/modular/api/server.h
#pragma once

#include "rag/modular/pipeline/pipeline_factory.h"
#include "rag/modular/config.h"
#include <string>
#include <memory>

namespace rag::modular::api {

class HttpServer {
public:
    HttpServer(const ModularConfig& config);
    ~HttpServer();
    
    void start(int port = 8080);
    void stop();

private:
    void register_routes();
    void handle_query(const HttpRequest& req, HttpResponse& res);
    void handle_status(const HttpRequest& req, HttpResponse& res);
    void handle_pipelines(const HttpRequest& req, HttpResponse& res);
    
    ModularConfig config_;
    std::unordered_map<PipelineType, std::unique_ptr<ModularPipeline>> pipelines_;
};

} // namespace rag::modular::api
```

- [ ] **Step 2: 提交**

```bash
git add -A
git commit -m "feat(rag): 实现 REST API 服务器"
```

---

### Task 11: 实现 CLI

**Files:**
- Create: `engineering/rag/src/rag/modular/cli/main.cpp`

- [ ] **Step 1: 创建 CLI 主程序**

```cpp
// engineering/rag/src/rag/modular/cli/main.cpp
#include <iostream>
#include <string>
#include "rag/modular/pipeline/pipeline_factory.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "用法: modular_rag <命令> [参数]\n";
        std::cout << "命令:\n";
        std::cout << "  query <问题> [--pipeline=xxx]\n";
        std::cout << "  list-pipelines\n";
        std::cout << "  server [--port=8080]\n";
        return 1;
    }
    
    std::string cmd = argv[1];
    
    if (cmd == "list-pipelines") {
        for (const auto& type : modular::list_pipeline_types()) {
            std::cout << "- " << type << "\n";
        }
    } else if (cmd == "query") {
        // ... 实现查询
    }
    
    return 0;
}
```

- [ ] **Step 2: 提交**

```bash
git add -A
git commit -m "feat(rag): 实现 CLI 工具"
```

---

## Phase 5: 测试

### Task 12: 单元测试

**Files:**
- Create: `engineering/rag/test/rag/modular/test_pipeline.cpp`
- Create: `engineering/rag/test/rag/modular/test_agent.cpp`

- [ ] **Step 1: 创建测试**

```cpp
// engineering/rag/test/rag/modular/test_pipeline.cpp
#include <gtest/gtest.h>
#include "rag/modular/pipeline/naive_pipeline.h"
#include "rag/modular/pipeline/advanced_pipeline.h"

TEST(ModularPipeline, NaivePipelineInit) {
    ModularConfig config;
    auto pipeline = PipelineFactory::create(PipelineType::NAIVE, config);
    EXPECT_NE(pipeline, nullptr);
    EXPECT_EQ(pipeline->type(), PipelineType::NAIVE);
}

TEST(ModularPipeline, AdvancedPipelineQuery) {
    ModularConfig config;
    auto pipeline = PipelineFactory::create(PipelineType::ADVANCED, config);
    
    ModularQuery query;
    query.text = "测试问题";
    query.top_k = 3;
    
    auto result = pipeline->query(query);
    EXPECT_EQ(result.success, true);  // 取决于 LLM 是否加载
}

TEST(ModularPipeline, AllPipelineTypes) {
    ModularConfig config;
    auto pipelines = PipelineFactory::create_all(config);
    
    EXPECT_EQ(pipelines.size(), 9);  // 9 种 Pipeline
    EXPECT_TRUE(pipelines.contains(PipelineType::NAIVE));
    EXPECT_TRUE(pipelines.contains(PipelineType::REACT));
    EXPECT_TRUE(pipelines.contains(PipelineType::GRAPH));
}
```

- [ ] **Step 2: 运行测试**

```bash
cd D:/code/book/engineering/rag
cmake --build build_modular --target modular_rag_test
./build_modular/test/rag/modular/test_pipeline
```

- [ ] **Step 3: 提交**

```bash
git add -A
git commit -m "test(rag): 添加 Modular RAG 单元测试"
```

---

## Phase 6: 文档

### Task 13: 编写文档

**Files:**
- Create: `docs/modular-rag/01-overview.md`
- Create: `docs/modular-rag/02-pipeline-guide.md`
- Create: `docs/modular-rag/03-agent-guide.md`
- Create: `docs/modular-rag/04-api-reference.md`

- [ ] **Step 1: 编写文档**

```markdown
# Modular RAG 框架概述

## 架构

[详细架构图和说明]

## 9 种 Pipeline

| 类型 | 说明 | 适用场景 |
|------|------|----------|
| Naive | 基础检索-生成 | 简单问答 |
| Advanced | 混合检索+RRF+重排序 | 通用场景 |
| ... | ... | ... |

## 快速开始

```bash
# 构建
cmake -B build -S . -DBUILD_MODULAR_RAG=ON
cmake --build build

# 查询
./bin/modular_rag query "HNSW 索引如何构建？" --pipeline=advanced
```
```

- [ ] **Step 2: 提交**

```bash
git add -A
git commit -m "docs(rag): 编写 Modular RAG 文档"
```

---

## 总结

实现顺序：
1. Task 1: 集成 llama.cpp
2. Task 2: 定义类型和配置
3. Task 3: Pipeline 基类和工厂
4. Task 4: Pipeline 第 1 批 (Naive/Advanced/Hybrid)
5. Task 5: Pipeline 第 2 批 (HyDE/Graph/Corrective)
6. Task 6: Pipeline 第 3 批 (ReAct/Iterative/Recursive)
7. Task 7: Tool 系统
8. Task 8: Agent 核心
9. Task 9: ReAct Pipeline 集成
10. Task 10: REST API
11. Task 11: CLI
12. Task 12: 单元测试
13. Task 13: 文档
