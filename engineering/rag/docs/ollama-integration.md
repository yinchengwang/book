# Ollama 集成指南

> 本文档介绍如何在 RAG 项目中集成和使用 Ollama 本地模型服务。

## 概述

Ollama 是一个本地大模型运行框架，通过简单的 HTTP API 提供 Embedding 和 LLM 推理能力。RAG 项目通过 Ollama API 实现真实的语义编码和文本生成，无需依赖外部云服务。

## 快速开始

### 1. 安装 Ollama

**Windows:**
```bash
# 下载并安装 Ollama
# https://ollama.com/download

# 验证安装
ollama --version
```

**Linux/macOS:**
```bash
curl -fsSL https://ollama.com/install.sh | sh
```

### 2. 下载模型

```bash
# 下载 Embedding 模型
ollama pull nomic-embed-text

# 下载 LLM 模型
ollama pull llama3.2

# 或者使用中文优化的模型
ollama pull qwen2.5:7b
```

### 3. 启动服务

```bash
# 启动 Ollama 服务（默认端口 11434）
ollama serve

# 后台运行（Linux/macOS）
ollama serve &
```

### 4. 验证服务

```bash
# 检查服务状态
curl http://localhost:11434/api/tags

# 测试 Embedding
curl -X POST http://localhost:11434/api/embeddings \
  -H "Content-Type: application/json" \
  -d '{"model": "nomic-embed-text", "prompt": "Hello, world!"}'

# 测试生成
curl -X POST http://localhost:11434/api/generate \
  -H "Content-Type: application/json" \
  -d '{"model": "llama3.2", "prompt": "What is RAG?", "stream": false}'
```

## 代码集成

### Embedding 服务

```cpp
#include "rag/ollama_embedding.h"

using namespace rag;

// 创建 Ollama Embedding 服务
OllamaEmbeddingConfig config;
config.base_url = "http://localhost:11434";
config.model = "nomic-embed-text";  // 或 "bge-m3"
config.max_cache_size = 10000;

auto embed_service = create_ollama_embedding_service(config);

// 编码单个文本
std::vector<float> embedding = embed_service->encode("要编码的文本");

// 批量编码
std::vector<std::string> texts = {"文本1", "文本2", "文本3"};
auto embeddings = embed_service->encode_batch(texts);

// 检查服务状态
if (embed_service->is_ready()) {
    std::cout << "Ollama Embedding 服务就绪" << std::endl;
} else if (embed_service->using_fallback()) {
    std::cout << "使用降级服务（SimpleEmbeddingService）" << std::endl;
}
```

### LLM 服务

```cpp
#include "rag/ollama_llm.h"

using namespace rag;

// 创建 Ollama LLM 服务
OllamaLLMConfig config;
config.base_url = "http://localhost:11434";
config.model = "llama3.2";  // 或 "qwen2.5:7b"

auto llm_service = create_ollama_llm_service(config);

// 加载模型
LLMConfig llm_cfg;
llm_cfg.model_type = "llama";
llm_cfg.n_ctx = 4096;
llm_service->load("", llm_cfg);

// 同步生成
GenerateOptions options;
options.max_tokens = 512;
options.temperature = 0.7f;
options.top_p = 0.9f;

auto result = llm_service->generate("解释 RAG 的工作原理", options);
std::cout << result.text << std::endl;

// 流式生成
std::string full_response;
llm_service->generate_stream("讲一个关于 AI 的故事", options,
    [&full_response](const std::string& token, bool complete) {
        std::cout << token << std::flush;
        full_response += token;
        return !complete;
    });
```

### 降级机制

当 Ollama 服务不可用时，服务会自动降级到 Simple 实现：

```cpp
auto embed_service = create_ollama_embedding_service(config);

if (embed_service->using_fallback()) {
    std::cout << "Ollama 不可用，使用随机向量" << std::endl;
    // 仍然可以使用，但结果是随机向量
}

// 获取降级服务
if (embed_service->using_fallback()) {
    auto fallback = embed_service->fallback();
    auto vec = fallback->encode("测试");
}
```

## 配置参数

### OllamaEmbeddingConfig

| 参数 | 默认值 | 说明 |
|------|--------|------|
| base_url | http://localhost:11434 | Ollama 服务地址 |
| model | nomic-embed-text | Embedding 模型名称 |
| max_cache_size | 10000 | 缓存大小 |
| timeout_seconds | 30 | 请求超时时间 |
| max_retries | 3 | 最大重试次数 |

### OllamaLLMConfig

| 参数 | 默认值 | 说明 |
|------|--------|------|
| base_url | http://localhost:11434 | Ollama 服务地址 |
| model | llama3.2 | LLM 模型名称 |
| timeout_seconds | 120 | 请求超时时间 |
| max_retries | 3 | 最大重试次数 |

## 支持的模型

### Embedding 模型

| 模型 | 维度 | 说明 |
|------|------|------|
| nomic-embed-text | 768 | 默认推荐，支持多语言 |
| bge-m3 | 1024 | BGE-M3，高精度 |
| e5-base-v2 | 768 | E5 系列 |
|gte | 768 | GTE 系列 |

### LLM 模型

| 模型 | 上下文 | 说明 |
|------|--------|------|
| llama3.2 | 8192 | 默认推荐，英文为主 |
| qwen2.5:7b | 8192 | 中文优化 |
| deepseek-r1:7b | 4096 | 推理能力强 |
| mistral | 8192 | 平衡性能 |

## 性能优化

### 1. 启用缓存

```cpp
// Embedding 缓存减少 API 调用
OllamaEmbeddingConfig config;
config.max_cache_size = 100000;  // 增大缓存
```

### 2. 批量处理

```cpp
// 批量编码比逐个调用更高效
std::vector<std::string> texts = {/* ... */};
auto embeddings = embed_service->encode_batch(texts);
```

### 3. 并行查询

```cpp
// 使用线程池并行查询
std::vector<std::future<std::vector<float>>> futures;
for (const auto& query : queries) {
    futures.push_back(std::async(std::launch::async, [&] {
        return embed_service->encode(query);
    }));
}
```

## 故障排除

### 服务无法连接

```bash
# 检查 Ollama 是否运行
ps aux | grep ollama

# 重启服务
pkill ollama
ollama serve

# 检查端口
netstat -an | grep 11434
```

### 模型未找到

```bash
# 列出已安装模型
ollama list

# 拉取模型
ollama pull nomic-embed-text
ollama pull llama3.2
```

### 内存不足

```bash
# 量化模型减少内存使用
ollama pull llama3.2:3b  # 3B 参数版本

# 或者使用更小的模型
ollama pull phi3:3.8b
```

## 下一步

- 查看 [RAG 评估指南](rag-evaluation.md) 了解如何评估系统效果
- 查看 [RAG 系统设计](../README.md) 了解整体架构
