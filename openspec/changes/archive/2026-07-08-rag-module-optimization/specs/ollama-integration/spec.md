# Ollama Integration Spec

## Purpose

定义 Ollama API 集成的能力规格，支持本地 Embedding 模型和 LLM 推理。

## ADDED Requirements

### Requirement: OllamaEmbeddingService 基本接口

OllamaEmbeddingService SHALL 实现 EmbeddingService 接口，通过 Ollama API 生成真实语义向量。

#### Scenario: 单文本编码
- **WHEN** 调用 `encode("什么是 RAG")`
- **THEN** 返回 768 维归一化向量（nomic-embed-text）或 1024 维（bge-m3）

#### Scenario: 批量编码
- **WHEN** 调用 `encode_batch({"文本1", "文本2", "文本3"})`
- **THEN** 返回 3 个向量的向量列表，顺序与输入一致

#### Scenario: Ollama 服务不可用时的降级
- **WHEN** Ollama 服务不可用（连接超时/返回错误）
- **THEN** 自动回退到 SimpleEmbeddingService，并记录警告日志

#### Scenario: 服务就绪检查
- **WHEN** 调用 `is_ready()`
- **THEN** 如果 Ollama 服务正常响应，返回 true

### Requirement: OllamaEmbeddingService 配置

OllamaEmbeddingService SHALL 支持以下配置参数：

#### Scenario: 自定义服务端点
- **WHEN** 构造函数传入 `base_url="http://localhost:11434"`
- **THEN** HTTP 请求发送到该地址

#### Scenario: 自定义模型
- **WHEN** 构造函数传入 `model="nomic-embed-text"`
- **THEN** 使用该模型进行编码

#### Scenario: 维度自动检测
- **WHEN** 使用 nomic-embed-text 模型
- **THEN** `dimension()` 返回 768

#### Scenario: 维度自动检测
- **WHEN** 使用 bge-m3 模型
- **THEN** `dimension()` 返回 1024

### Requirement: Embedding 缓存

OllamaEmbeddingService SHALL 实现 LRU 缓存以减少 API 调用。

#### Scenario: 缓存命中
- **WHEN** 相同文本被编码两次
- **THEN** 第二次直接从缓存返回，不调用 Ollama API

#### Scenario: 缓存未命中
- **WHEN** 新文本被编码
- **THEN** 调用 Ollama API，结果存入缓存

#### Scenario: 缓存大小限制
- **WHEN** 缓存达到最大容量（默认 10000 条）
- **THEN** 使用 LRU 策略淘汰最旧条目

### Requirement: OllamaLLMService 基本接口

OllamaLLMService SHALL 实现 LLMService 接口，通过 Ollama API 进行文本生成。

#### Scenario: 同步生成
- **WHEN** 调用 `generate("RAG 是什么？", options)`
- **THEN** 返回包含生成文本的 GenerateResult

#### Scenario: 流式生成
- **WHEN** 调用 `generate_stream(prompt, options, callback)`
- **THEN** 回调函数被多次调用，逐 token 返回生成内容

#### Scenario: Ollama 服务不可用时的降级
- **WHEN** Ollama 服务不可用
- **THEN** 抛出 LLMException，并记录错误日志

### Requirement: OllamaLLMService 生成选项

OllamaLLMService SHALL 支持以下生成选项：

#### Scenario: 温度参数
- **WHEN** 设置 `options.temperature = 0.7`
- **THEN** Ollama API 请求包含该参数

#### Scenario: 最大 token 数
- **WHEN** 设置 `options.max_tokens = 1024`
- **THEN** 生成文本不超过 1024 tokens

#### Scenario: 停止词
- **WHEN** 设置 `options.stop = "。"` （中文句号）
- **THEN** 生成文本遇到该词时停止

### Requirement: 上下文窗口

OllamaLLMService SHALL 报告模型的上下文窗口大小。

#### Scenario: 获取上下文窗口
- **WHEN** 调用 `context_window()`
- **THEN** 返回模型支持的上下文长度（llama3.2: 8192, qwen2.5: 8192）

### Requirement: 错误处理

Ollama 服务调用 SHALL 实现完善的错误处理。

#### Scenario: 连接失败
- **WHEN** 无法连接到 Ollama 服务
- **THEN** 抛出 LLMException 或 EmbeddingException，错误码 NETWORK_ERROR

#### Scenario: 模型不存在
- **WHEN** 请求的模型未安装
- **THEN** Ollama 返回 404，抛出 MODEL_NOT_FOUND 异常

#### Scenario: 请求超时
- **WHEN** Ollama 处理超时（默认 30s）
- **THEN** 抛出 TIMEOUT 异常，可配置重试

## 接口定义

```cpp
// Ollama Embedding
class OllamaEmbeddingService : public EmbeddingService {
public:
    OllamaEmbeddingService(
        const std::string& base_url = "http://localhost:11434",
        const std::string& model = "nomic-embed-text",
        size_t max_cache_size = 10000);

    std::vector<float> encode(const std::string& text) override;
    std::vector<std::vector<float>> encode_batch(
        const std::vector<std::string>& texts) override;
    int dimension() const override;
    bool is_ready() const override;
    std::string model_name() const override;
};

// Ollama LLM
class OllamaLLMService : public LLMService {
public:
    OllamaLLMService(
        const std::string& base_url = "http://localhost:11434",
        const std::string& model = "llama3.2");

    void load(const std::string& model_path,
              const LLMConfig& config) override;
    void unload() override;
    bool is_loaded() const override;
    GenerateResult generate(const std::string& prompt,
                           const GenerateOptions& options) override;
    void generate_stream(const std::string& prompt,
                       const GenerateOptions& options,
                       StreamCallback callback) override;
    int context_window() const override;
    const std::string& model_type() const override;
    const std::string& model_path() const override;
};
```

## 验收标准

- [ ] encode() 返回语义一致的向量（相似文本的向量余弦相似度 > 0.7）
- [ ] encode_batch() 批量编码比逐个调用快 50% 以上
- [ ] 缓存命中率 > 80%（重复查询场景）
- [ ] generate() 支持 temperature/top_p/max_tokens 等选项
- [ ] generate_stream() 正确回调每个生成的 token
- [ ] 服务不可用时正确降级到 Simple 实现
