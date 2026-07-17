# RAG 系统 Prompt 工程设计

## 概述

本文档定义 RAG 系统的 Prompt 工程设计，包括系统提示词、用户提示词模板、few-shot 示例等。

---

## 1. Prompt 设计原则

### 1.1 设计目标

```
┌─────────────────────────────────────────────────────────────┐
│                   Prompt 设计目标                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 准确性：确保基于上下文回答，不编造信息                     │
│  2. 连贯性：回答逻辑清晰，结构分明                           │
│  3. 简洁性：直接回答问题，避免冗余                          │
│  4. 可追溯性：引用来源明确                                 │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 设计原则

| 原则 | 说明 | 示例 |
|------|------|------|
| **明确指令** | 使用动词开头 | "基于以下内容回答" 而非 "你可以..." |
| **结构化** | 分步骤执行 | "首先...然后...最后..." |
| **约束边界** | 明确限制条件 | "只使用提供的信息" |
| **格式指定** | 说明输出格式 | "请用列表形式回答" |

---

## 2. Prompt 模板

### 2.1 系统提示词

```yaml
# 系统提示词模板
system_prompt: |
  你是一个专业的技术助手，专门基于提供的上下文信息回答用户的问题。

  你的职责：
  1. 准确理解用户问题
  2. 从提供的上下文中提取相关信息
  3. 生成准确、简洁、有条理的回答

  回答规则：
  1. 只使用提供的上下文信息进行回答，不要编造或添加未提供的信息
  2. 如果上下文中没有相关信息，明确告知用户"我无法从提供的文档中找到答案"
  3. 回答要准确、简洁、有条理，使用清晰的结构
  4. 对于技术概念，提供简洁的定义和解释
  5. 对于代码相关问题，标注代码语言并适当格式化
  6. 引用相关来源，帮助用户追溯信息

  回答格式：
  - 简短问题：直接给出答案
  - 复杂问题：使用结构化格式（列表、步骤等）
  - 代码问题：使用代码块并标注语言
```

### 2.2 用户提示词模板

```yaml
# 用户提示词模板
user_prompt_template: |
  ## 上下文信息
  {{context}}

  ## 用户问题
  {{question}}

  请基于上述上下文信息回答问题。
```

### 2.3 完整提示词组合

```cpp
// prompt_builder.h
#pragma once

#include <string>
#include <vector>
#include "chunk.h"

namespace rag {

struct PromptConfig {
    std::string system_prompt;
    std::string user_template;
    std::string no_context_template;
    int max_context_length = 4000;      // 最大上下文长度 (token)
    int max_chunks = 10;                 // 最大使用的 Chunk 数
    bool include_sources = true;          // 是否包含来源
    bool include_scores = false;          // 是否包含相似度分数
};

class PromptBuilder {
public:
    explicit PromptBuilder(const PromptConfig& config);

    // 构建完整提示词
    struct Prompt {
        std::string system;
        std::string user;
    };

    Prompt build(const std::string& question,
                const std::vector<Chunk>& chunks);

    // 构建无上下文时的提示词
    Prompt build_no_context(const std::string& question);

private:
    // 格式化上下文
    std::string format_context(const std::vector<Chunk>& chunks);

    // 截断上下文
    std::string truncate_context(const std::string& context,
                                 int max_tokens);

    PromptConfig config_;
};

}  // namespace rag
```

### 2.4 Prompt 构建示例

```cpp
// 提示词构建示例
Prompt PromptBuilder::build(const std::string& question,
                           const std::vector<Chunk>& chunks) {
    // 1. 格式化上下文
    std::string context = format_context(chunks);

    // 2. 填充用户模板
    std::string user = config_.user_template;
    replace(user, "{{context}}", context);
    replace(user, "{{question}}", question);

    // 3. 返回结果
    return {config_.system_prompt, user};
}

std::string PromptBuilder::format_context(const std::vector<Chunk>& chunks) {
    std::string context;

    for (int i = 0; i < chunks.size(); ++i) {
        const auto& chunk = chunks[i];

        // 添加来源信息
        context += "【来源 " + std::to_string(i + 1) + "】\n";
        context += "文件：" + chunk.metadata["file_path"] + "\n";

        if (chunk.start_line > 0) {
            context += "位置：第 " + std::to_string(chunk.start_line) + " 行\n";
        }

        context += "\n";
        context += chunk.content;
        context += "\n\n";
        context += "---\n\n";
    }

    return context;
}
```

---

## 3. 场景化 Prompt

### 3.1 通用问答

```yaml
system: |
  你是一个技术文档助手。请基于提供的上下文准确回答问题。

user: |
  上下文：
  {{context}}

  问题：{{question}}

  要求：
  1. 直接回答问题
  2. 如需分点，请使用编号列表
  3. 如有代码，标注语言
```

### 3.2 代码解释

```yaml
system: |
  你是一个代码分析助手。请解释代码的功能、实现原理和使用方法。

user: |
  代码上下文：
  {{context}}

  请解释：
  1. 这段代码的功能
  2. 主要的实现逻辑
  3. 使用方法和注意事项
```

### 3.3 概念解释

```yaml
system: |
  你是一个技术概念解释助手。请用通俗易懂的语言解释技术概念。

user: |
  概念定义：
  {{context}}

  请解释"{{question}}"这个概念：
  1. 简要定义
  2. 核心要点
  3. 应用场景
  4. 相关概念（可选）
```

### 3.4 对比分析

```yaml
system: |
  你是一个技术对比分析助手。请基于提供的信息进行对比分析。

user: |
  对比信息：
  {{context}}

  问题：{{question}}

  请按以下格式回答：
  1. 简要对比结论
  2. 详细对比表格
  3. 各场景推荐
```

### 3.5 教程指南

```yaml
system: |
  你是一个技术教程助手。请提供清晰、实用的操作指南。

user: |
  教程内容：
  {{context}}

  请基于以上内容：
  1. 概述目标
  2. 列出关键步骤
  3. 提供代码示例
  4. 列出常见问题和解决方案
```

---

## 4. Few-Shot 示例

### 4.1 带示例的 Prompt

```yaml
system: |
  你是一个技术助手。请参考以下示例格式回答问题。

user: |
  ## 示例 1

  上下文：
  快速排序是一种高效的排序算法，平均时间复杂度为 O(n log n)。

  问题：什么是快速排序？

  回答：
  快速排序是一种基于分治思想的排序算法。
  - **基本思想**：选择一个基准元素，将数组分为两部分
  - **时间复杂度**：平均 O(n log n)，最坏 O(n²)
  - **空间复杂度**：O(log n)

  ## 示例 2

  上下文：
  {{context}}

  问题：{{question}}

  回答：
```

### 4.2 示例管理器

```cpp
// example_manager.h
#pragma once

#include <string>
#include <vector>

namespace rag {

struct Example {
    std::string question;
    std::string answer;
    std::string category;                // 代码/概念/对比/教程
    std::vector<std::string> tags;      // 标签
};

class ExampleManager {
public:
    // 添加示例
    void add_example(const Example& example);

    // 按类别获取示例
    std::vector<Example> get_examples(const std::string& category,
                                     int max_count = 3);

    // 选择最相关的示例
    std::vector<Example> select_relevant(const std::string& query,
                                         int max_count = 3);

    // 加载示例集
    void load_from_file(const std::string& path);

private:
    std::vector<Example> examples_;
};

}  // namespace rag
```

---

## 5. 上下文管理

### 5.1 上下文长度控制

```cpp
// context_manager.h
#pragma once

#include <string>

namespace rag {

// Token 计数器 (简化版)
class TokenCounter {
public:
    // 估算 token 数 (中文约 2 chars/token, 英文约 4 chars/token)
    int estimate_tokens(const std::string& text) {
        int chinese_chars = count_chinese(text);
        int other_chars = text.size() - chinese_chars;
        return (chinese_chars + 1) / 2 + (other_chars + 3) / 4;
    }

    // 截断到指定 token 数
    std::string truncate(const std::string& text, int max_tokens) {
        int current = estimate_tokens(text);
        if (current <= max_tokens) return text;

        // 二分查找截断点
        int left = 0, right = text.size();
        while (left < right) {
            int mid = (left + right + 1) / 2;
            if (estimate_tokens(text.substr(0, mid)) <= max_tokens) {
                left = mid;
            } else {
                right = mid - 1;
            }
        }

        return text.substr(0, left);
    }

private:
    int count_chinese(const std::string& text);
};

// 上下文压缩
class ContextCompressor {
public:
    // 压缩上下文
    std::string compress(const std::vector<Chunk>& chunks,
                         int max_tokens);

    // 句子级别压缩
    std::string compress_sentences(const std::string& text,
                                   int max_tokens);
};

}  // namespace rag
```

### 5.2 上下文选择策略

```cpp
// 上下文选择策略

// 1. 按分数选择
std::vector<Chunk> select_by_score(const std::vector<Chunk>& chunks,
                                    int top_k) {
    auto sorted = chunks;
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) {
                  return a.score > b.score;
              });
    return {sorted.begin(), sorted.begin() + std::min(top_k, (int)sorted.size())};
}

// 2. 多样性选择 (MMR)
std::vector<Chunk> select_with_diversity(
    const std::vector<Chunk>& chunks,
    const std::string& query,
    int top_k,
    float lambda = 0.7) {
    // 参见 diversity_enhancer.h
}

// 3. 分层选择
std::vector<Chunk> select_hierarchical(
    const std::vector<Chunk>& chunks,
    int top_k) {
    // 优先选择不同文档的 Chunk
    std::set<std::string> seen_docs;
    std::vector<Chunk> selected;

    for (const auto& chunk : chunks) {
        if (selected.size() >= top_k) break;

        if (seen_docs.find(chunk.document_id) == seen_docs.end()) {
            selected.push_back(chunk);
            seen_docs.insert(chunk.document_id);
        }
    }

    // 如果不够，补充高分结果
    if (selected.size() < top_k) {
        for (const auto& chunk : chunks) {
            if (selected.size() >= top_k) break;
            if (std::find(selected.begin(), selected.end(), chunk) == selected.end()) {
                selected.push_back(chunk);
            }
        }
    }

    return selected;
}
```

---

## 6. Prompt 优化

### 6.1 A/B 测试

```cpp
// prompt_ab_test.h
#pragma once

#include <string>
#include <vector>
#include <random>

namespace rag {

struct ABTestConfig {
    std::vector<std::string> variants;   // Prompt 变体
    std::vector<float> weights;         // 分配权重
};

class PromptABTester {
public:
    void init(const ABTestConfig& config);

    // 选择变体
    std::string select_variant(const std::string& user_id);

    // 记录结果
    void record_result(const std::string& variant,
                       bool helpful,
                       int rating);

    // 获取统计
    struct TestResult {
        std::string variant;
        int sample_count;
        float avg_rating;
        float helpful_rate;
    };
    std::vector<TestResult> get_results();
};

}  // namespace rag
```

### 6.2 Prompt 版本管理

```yaml
# prompt 版本配置
prompt:
  version: "v1.0"
  variants:
    - name: "baseline"
      system: "..."
      user: "..."
    - name: "with_examples"
      system: "..."
      user: "..."
  active: "baseline"
```

---

## 7. 错误回复模板

### 7.1 无上下文回复

```yaml
no_context_response: |
  对不起，我无法从当前索引的文档中找到与您问题相关的信息。

  可能的原因：
  1. 文档库中尚未包含相关内容
  2. 问题的表述方式与文档不同

  建议：
  1. 尝试使用不同的关键词
  2. 扩大搜索范围
  3. 重新构建索引
```

### 7.2 上下文不足回复

```yaml
insufficient_context_response: |
  我找到了一些相关信息，但不完整：

  {{partial_context}}

  问题"{{question}}"的完整答案需要在更多文档中查找。
```

### 7.3 检索失败回复

```yaml
retrieval_failure_response: |
  检索过程中遇到问题，暂时无法回答您的问题。

  请稍后重试，或联系管理员检查系统状态。
```

---

## 8. Prompt 配置示例

```yaml
# prompt 配置
prompt:
  # 版本
  version: "1.0"

  # 系统提示词
  system: |
    你是一个专业的技术助手。请基于提供的上下文信息回答问题。

  # 用户模板
  user_template: |
    ## 上下文
    {{context}}

    ## 问题
    {{question}}

    请准确回答。

  # 无上下文模板
  no_context: |
    对不起，无法找到相关信息。

  # 配置
  config:
    max_context_tokens: 4000
    max_chunks: 5
    include_sources: true
    include_scores: false
```

---

*文档版本：1.0.0*
*最后更新：2026-07-06*
