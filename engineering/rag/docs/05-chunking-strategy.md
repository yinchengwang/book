# RAG 系统分块策略设计

## 概述

本文档定义 RAG 系统的文档分块策略，包括各种分块算法、参数配置和代码感知分块方案。

---

## 1. 分块策略概览

### 1.1 支持的分块策略

| 策略 | 适用场景 | 优点 | 缺点 |
|------|----------|------|------|
| **固定大小** | 通用文本 | 简单、快速 | 语义割裂 |
| **递归字符** | 长文档 | 保持句子完整 | 依赖分隔符 |
| **语义分块** | 结构化文档 | 语义连贯 | 需要模型 |
| **代码感知** | 源代码 | 保持函数完整 | 实现复杂 |

### 1.2 分块流程

```
┌─────────────────────────────────────────────────────────────┐
│                      分块流程                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────┐                                           │
│  │  原始文档   │                                           │
│  └─────────────┘                                           │
│        │                                                    │
│        ▼                                                    │
│  ┌─────────────┐                                           │
│  │  文本清洗   │ ──→ 移除多余空白、规范换行                   │
│  └─────────────┘                                           │
│        │                                                    │
│        ▼                                                    │
│  ┌─────────────┐                                           │
│  │  结构识别   │ ──→ Markdown 标题、代码块、表格              │
│  └─────────────┘                                           │
│        │                                                    │
│        ▼                                                    │
│  ┌─────────────┐                                           │
│  │  分块策略   │ ──→ 根据类型选择分块算法                      │
│  └─────────────┘                                           │
│        │                                                    │
│        ▼                                                    │
│  ┌─────────────┐                                           │
│  │  边界优化   │ ──→ 合并小片段、拆分大片段                    │
│  └─────────────┘                                           │
│        │                                                    │
│        ▼                                                    │
│  ┌─────────────┐                                           │
│  │  元数据添加 │ ──→ 标题、位置、父子关系                      │
│  └─────────────┘                                           │
│        │                                                    │
│        ▼                                                    │
│  ┌─────────────┐                                           │
│  │   Chunk[]   │                                           │
│  └─────────────┘                                           │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. Chunk 数据结构

### 2.1 Chunk 定义

```cpp
// chunk.h
#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace rag {

struct Chunk {
    std::string id;                     // 唯一ID
    std::string content;               // 文本内容
    size_t content_length;              // 内容长度

    // 文档关联
    std::string document_id;           // 所属文档ID
    int chunk_index;                    // 在文档中的序号

    // 位置信息
    int start_line;                    // 起始行号
    int end_line;                      // 结束行号
    int start_char;                    // 起始字符偏移
    int end_char;                      // 结束字符偏移

    // 父子关系 (用于语义分块)
    std::string parent_id;             // 父Chunk ID
    std::vector<std::string> child_ids; // 子Chunk ID

    // 元数据
    std::unordered_map<std::string, std::string> metadata;

    // 分数 (检索后填充)
    float score = 0.0f;

    // 方法
    std::string to_string() const;
    bool is_valid() const;
};

struct ChunkConfig {
    size_t chunk_size = 512;           // 目标块大小 (字符)
    size_t chunk_overlap = 50;         // 重叠大小
    size_t min_chunk_size = 100;       // 最小块大小
    size_t max_chunk_size = 1024;      // 最大块大小
    bool keep_separator = true;        // 保留分隔符
    std::string separator = "\n";       // 默认分隔符
};

}  // namespace rag
```

### 2.2 分块元数据结构

```json
{
    "chunk": {
        "id": "chunk_x9y8z7w6",
        "type": "paragraph",
        "parent_heading": "快速排序",
        "level": 2
    },
    "document": {
        "path": "docs/algorithm/sort.md",
        "title": "排序算法",
        "section": "3.1 快速排序"
    },
    "code_blocks": 1,
    "images": 0,
    "links": 2,
    "language": "zh-CN"
}
```

---

## 3. 固定大小分块

### 3.1 算法

```cpp
// fixed_chunker.h
#pragma once

#include "chunker.h"

namespace rag {

class FixedChunker : public Chunker {
public:
    explicit FixedChunker(const ChunkConfig& config);

    std::vector<Chunk> chunk(const std::string& text,
                             const std::string& document_id) override;

private:
    ChunkConfig config_;
};

// 实现
std::vector<Chunk> FixedChunker::chunk(const std::string& text,
                                        const std::string& document_id) {
    std::vector<Chunk> chunks;
    int text_len = static_cast<int>(text.size());

    int offset = 0;
    int chunk_index = 0;

    while (offset < text_len) {
        int end = std::min(offset + config_.chunk_size, text_len);

        // 提取块内容
        std::string content = text.substr(offset, end - offset);
        Chunk chunk;
        chunk.id = generate_chunk_id(document_id, chunk_index);
        chunk.content = content;
        chunk.document_id = document_id;
        chunk.chunk_index = chunk_index;
        chunk.start_char = offset;
        chunk.end_char = end;

        chunks.push_back(chunk);

        // 滑动窗口
        offset += config_.chunk_size - config_.chunk_overlap;
        chunk_index++;
    }

    return chunks;
}

}  // namespace rag
```

### 3.2 特点

- **优点**：简单、快速、可预测
- **缺点**：可能在句子中间断开，语义不连贯
- **适用**：对语义要求不高的场景、测试阶段

---

## 4. 递归字符分块

### 4.1 算法

```
分块过程：
1. 使用最高优先级分隔符 "\n\n" 分割
2. 如果块太大，使用 "\n" 继续分割
3. 如果还是太大，使用 "。" 分割（中文）
4. 如果依然太大，使用 " " 分割
5. 最后使用空字符串，直接按大小截断
```

```cpp
// recursive_chunker.h
#pragma once

#include "chunker.h"

namespace rag {

class RecursiveChunker : public Chunker {
public:
    explicit RecursiveChunker(const ChunkConfig& config);

    std::vector<Chunk> chunk(const std::string& text,
                             const std::string& document_id) override;

private:
    std::vector<Chunk> split_text(
        const std::string& text,
        const std::string& document_id,
        int chunk_index);

    // 递归尝试不同分隔符
    std::vector<std::string> split_by_separator(
        const std::string& text,
        const std::string& separator);

    ChunkConfig config_;
    std::vector<std::string> separators_ = {
        "\n\n",    // 段落
        "\n",      // 换行
        "。",      // 中文句号
        "！",      // 中文感叹号
        "？",      // 中文问号
        ". ",      // 英文句号+空格
        "! ",      // 英文感叹号+空格
        "? ",      // 英文问号+空格
        "; ",      // 分号+空格
        ", ",      // 逗号+空格
        " ",       // 空格
        ""         // 字符级别
    };
};

}  // namespace rag
```

### 4.2 实现

```cpp
std::vector<Chunk> RecursiveChunker::chunk(const std::string& text,
                                           const std::string& document_id) {
    std::vector<Chunk> result;

    auto segments = split_recursive(text, separators_, config_.chunk_size);

    int chunk_index = 0;
    int char_offset = 0;

    for (const auto& segment : segments) {
        if (segment.size() < config_.min_chunk_size && !result.empty()) {
            // 合并到上一个块
            result.back().content += segment;
            result.back().end_char += segment.size();
        } else if (segment.size() > config_.max_chunk_size) {
            // 递归拆分
            auto sub_chunks = chunk(segment, document_id);
            for (auto& sc : sub_chunks) {
                sc.chunk_index = chunk_index++;
                sc.start_char = char_offset;
                sc.end_char = char_offset + sc.content.size();
                result.push_back(std::move(sc));
            }
        } else {
            Chunk chunk;
            chunk.id = generate_chunk_id(document_id, chunk_index);
            chunk.content = segment;
            chunk.document_id = document_id;
            chunk.chunk_index = chunk_index;
            chunk.start_char = char_offset;
            chunk.end_char = char_offset + segment.size();
            result.push_back(chunk);
            chunk_index++;
        }
        char_offset += segment.size();
    }

    return result;
}
```

### 4.3 Markdown 分块增强

```cpp
// markdown_chunker.h
#pragma once

namespace rag {

class MarkdownChunker : public RecursiveChunker {
public:
    explicit MarkdownChunker(const ChunkConfig& config);

    std::vector<Chunk> chunk(const std::string& text,
                              const std::string& document_id) override;

private:
    // 提取 Markdown 结构
    struct MarkdownStructure {
        int level;                      // 标题级别 1-6
        std::string heading;            // 标题内容
        int start_line;
        int end_line;
    };

    std::vector<MarkdownStructure> parse_headings(const std::string& text);
    std::string extract_title(const std::vector<MarkdownStructure>& headings,
                             int line);
};

}  // namespace rag
```

---

## 5. 代码感知分块

### 5.1 设计目标

```
C++ 代码分块原则：
1. 保持函数/类完整性
2. 保留注释和文档字符串
3. 识别 include 依赖
4. 处理命名空间
```

### 5.2 分块策略

```
┌─────────────────────────────────────────────────────────────┐
│                    代码分块策略                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────────┐                                       │
│  │  解析语法树      │ ← Clang 库或 Tree-sitter              │
│  └─────────────────┘                                       │
│         │                                                  │
│         ▼                                                  │
│  ┌─────────────────┐                                       │
│  │  提取结构单元    │                                       │
│  │  - 函数定义     │                                       │
│  │  - 类定义       │                                       │
│  │  - 命名空间     │                                       │
│  │  - 全局变量     │                                       │
│  └─────────────────┘                                       │
│         │                                                  │
│         ▼                                                  │
│  ┌─────────────────┐                                       │
│  │  评估块大小      │                                       │
│  │  - 合并小函数    │                                       │
│  │  - 拆分超大函数  │                                       │
│  └─────────────────┘                                       │
│         │                                                  │
│         ▼                                                  │
│  ┌─────────────────┐                                       │
│  │  添加元数据      │                                       │
│  │  - 函数签名     │                                       │
│  │  - 参数列表     │                                       │
│  │  - 依赖关系     │                                       │
│  └─────────────────┘                                       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 5.3 C++ 分块器实现

```cpp
// code_chunker.h
#pragma once

#include "chunker.h"

namespace rag {

struct CodeChunkConfig {
    size_t max_function_lines = 200;
    size_t max_chunk_size = 1024;
    bool preserve_comments = true;
    bool preserve_docstrings = true;
    bool include_header_context = true;
    size_t header_context_lines = 5;    // include 上下文行数
};

struct CodeChunk : public Chunk {
    // 代码特定元数据
    std::string language;               // cpp, py, java
    std::string function_name;          // 函数名
    std::string function_signature;      // 函数签名
    std::string class_name;             // 所属类
    std::string namespace_name;         // 命名空间
    std::vector<std::string> includes;   // include 列表
    std::vector<std::string> dependencies; // 依赖的函数/类
    int cyclomatic_complexity;           // 圈复杂度
};

class CodeChunker : public Chunker {
public:
    explicit CodeChunker(const CodeChunkConfig& config);

    std::vector<Chunk> chunk(const std::string& text,
                             const std::string& document_id) override;

    std::string get_language() const { return language_; }
    void set_language(const std::string& lang) { language_ = lang; }

private:
    // 解析代码结构
    std::vector<CodeUnit> parse_code(const std::string& text);

    // 按结构类型分块
    std::vector<Chunk> chunk_by_structure(
        const std::vector<CodeUnit>& units,
        const std::string& document_id);

    // 合并或拆分
    std::vector<Chunk> merge_or_split(
        const std::vector<Chunk>& chunks);

    CodeChunkConfig config_;
    std::string language_ = "cpp";
};

}  // namespace rag
```

### 5.4 C++ 代码解析

```cpp
// C++ 代码解析示例
std::vector<CodeUnit> CppCodeParser::parse(const std::string& code) {
    std::vector<CodeUnit> units;

    // 匹配函数定义
    std::regex func_regex(
        R"(^[\w:*& ]+?\s+(\w+)\s*\([^)]*\)\s*(const)?\s*\{)",
        std::regex::ECMAScript | std::regex::multiline
    );

    // 匹配类定义
    std::regex class_regex(
        R"(^class\s+(\w+)(\s*:\s*public\s+\w+)?\s*\{)",
        std::regex::ECMAScript | std::regex::multiline
    );

    // 匹配命名空间
    std::regex ns_regex(
        R"(^namespace\s+(\w+|\s*\{))",
        std::regex::ECMAScript | std::regex::multiline
    );

    // 匹配 include
    std::regex include_regex(
        R"(#include\s*<([^>]+)>)",
        std::regex::ECMAScript
    );

    // ... 解析逻辑

    return units;
}

// 获取函数上下文 (include + 函数定义)
std::string get_function_context(const std::string& code,
                                 int function_line,
                                 const CodeChunkConfig& config) {
    std::string context;
    auto lines = split_lines(code);

    // 收集前面的 include
    int start = std::max(0, function_line - config.header_context_lines);
    for (int i = start; i < function_line; ++i) {
        if (starts_with(lines[i], "#include")) {
            context += lines[i] + "\n";
        }
    }

    // 添加函数
    context += extract_function(lines, function_line);

    return context;
}
```

### 5.5 分块元数据

```json
{
    "chunk": {
        "type": "function",
        "function_name": "quick_sort",
        "function_signature": "void quick_sort(int arr[], int left, int right)",
        "class_name": "",
        "namespace": "sort",
        "includes": ["<vector>", "<algorithm>"],
        "dependencies": ["partition", "swap"],
        "lines": "45-78",
        "cyclomatic_complexity": 3
    }
}
```

---

## 6. 语义分块

### 6.1 设计

使用 Embedding 模型判断句子间的语义相似度，自动决定分块边界。

```
┌─────────────────────────────────────────────────────────────┐
│                    语义分块流程                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  输入文本                                                    │
│     │                                                      │
│     ▼                                                      │
│  ┌─────────────────┐                                       │
│  │  句子分割       │ ──→ 使用 NLP 分句                      │
│  └─────────────────┘                                       │
│     │                                                      │
│     ▼                                                      │
│  ┌─────────────────┐                                       │
│  │  生成句子向量   │ ──→ Embedding.encode_batch()           │
│  └─────────────────┘                                       │
│     │                                                      │
│     ▼                                                      │
│  ┌─────────────────┐                                       │
│  │  计算相邻相似度 │ ──→ cosine_sim(s[i], s[i+1])         │
│  └─────────────────┘                                       │
│     │                                                      │
│     ▼                                                      │
│  ┌─────────────────┐                                       │
│  │  找断点         │ ──→ 相似度 < 阈值 → 断开               │
│  └─────────────────┘                                       │
│     │                                                      │
│     ▼                                                      │
│  ┌─────────────────┐                                       │
│  │  构建 Chunk    │                                       │
│  └─────────────────┘                                       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 6.2 实现

```cpp
// semantic_chunker.h
#pragma once

#include "chunker.h"
#include "embed_service.h"

namespace rag {

struct SemanticChunkConfig {
    float similarity_threshold = 0.8f;
    int min_sentences = 2;              // 每块最小句子数
    int max_sentences = 10;            // 每块最大句子数
    size_t min_chunk_size = 200;
    size_t max_chunk_size = 1024;
    bool use_mean_embeddings = true;    // 合并时使用平均向量
};

class SemanticChunker : public Chunker {
public:
    SemanticChunker(const ChunkConfig& chunk_config,
                    const SemanticChunkConfig& sem_config,
                    std::shared_ptr<EmbeddingService> embed_service);

    std::vector<Chunk> chunk(const std::string& text,
                             const std::string& document_id) override;

private:
    // 句子分割
    std::vector<std::string> split_sentences(const std::string& text);

    // 计算相似度
    float cosine_similarity(const Vector& a, const Vector& b);

    // 找断点
    std::vector<int> find_breakpoints(
        const std::vector<Vector>& embeddings,
        float threshold);

    ChunkConfig chunk_config_;
    SemanticChunkConfig sem_config_;
    std::shared_ptr<EmbeddingService> embed_service_;
};

}  // namespace rag
```

---

## 7. 分块配置指南

### 7.1 按文件类型推荐配置

```yaml
# 分块配置示例
chunking:
  # Markdown 文档
  md:
    strategy: recursive
    chunk_size: 768
    chunk_overlap: 100
    min_chunk_size: 150
    separators: ["\n\n", "\n", "。", "！", "？", ". ", " "]

  # 源代码 (C/C++)
  cpp:
    strategy: code_aware
    chunk_size: 1024
    max_function_lines: 200
    preserve_comments: true
    include_header_context: true

  # Python
  py:
    strategy: code_aware
    chunk_size: 768
    max_function_lines: 100
    preserve_docstrings: true

  # 纯文本
  txt:
    strategy: recursive
    chunk_size: 512
    chunk_overlap: 50
```

### 7.2 分块大小选择

| 使用场景 | chunk_size | chunk_overlap | 说明 |
|----------|-------------|---------------|------|
| 问答系统 | 300-500 | 50-100 | 保留上下文但不过长 |
| 摘要生成 | 500-1000 | 100-200 | 需要更多上下文 |
| 代码搜索 | 512-1024 | 50 | 保持函数完整 |
| 文档检索 | 512-768 | 100 | 平衡精度和召回 |

### 7.3 块大小估算

```
字符数 → Token 数估算：
- 中文：1 字符 ≈ 1-2 tokens
- 英文：1 单词 ≈ 1.3 tokens
- 代码：1 字符 ≈ 0.25 tokens (含符号)

示例：
- chunk_size = 512 字符
- 中文文档 → 约 256-512 tokens
- 英文文档 → 约 400 tokens
- 代码 → 约 2000 tokens (实际会截断)
```

---

## 8. 分块质量评估

### 8.1 评估指标

```cpp
struct ChunkQualityMetrics {
    // 语义连贯性
    float avg_chunk_coherence;          // 块内句子间平均相似度

    // 完整性
    float function_completeness;        // 函数完整保留率
    float sentence_completeness;        // 句子完整保留率

    // 大小分布
    size_t num_chunks;
    size_t avg_chunk_size;
    size_t min_chunk_size;
    size_t max_chunk_size;
    float size_variance;

    // 召回率
    float retrieval_recall;             // 关键信息召回率
};
```

### 8.2 质量检查

```cpp
// 质量检查规则
std::vector<std::string> validate_chunks(const std::vector<Chunk>& chunks) {
    std::vector<std::string> issues;

    for (const auto& chunk : chunks) {
        // 1. 大小检查
        if (chunk.content.size() < 10) {
            issues.push_back("块内容过短: " + chunk.id);
        }
        if (chunk.content.size() > 10000) {
            issues.push_back("块内容过长: " + chunk.id);
        }

        // 2. 内容检查
        if (trim(chunk.content).empty()) {
            issues.push_back("块内容为空: " + chunk.id);
        }

        // 3. 重复检查
        if (has_excessive_repetition(chunk.content)) {
            issues.push_back("块内容重复过多: " + chunk.id);
        }
    }

    return issues;
}
```

---

*文档版本：1.0.0*
*最后更新：2026-07-06*
