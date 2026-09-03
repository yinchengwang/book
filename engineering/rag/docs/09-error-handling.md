# RAG 系统错误处理设计

## 概述

本文档定义 RAG 系统的错误处理设计，包括错误码定义、异常类、重试策略和降级方案。

---

## 1. 错误码定义

### 1.1 错误码结构

```
错误码格式：RAG-XXX-YYY
├── RAG: 系统前缀
├── XXX: 模块代码
│   ├── 100: 系统错误
│   ├── 200: 配置错误
│   ├── 300: 模型错误
│   ├── 400: 索引错误
│   ├── 500: 检索错误
│   ├── 600: LLM 错误
│   └── 900: 用户错误
└── YYY: 具体错误编号
```

### 1.2 错误码表

| 错误码 | 名称 | 说明 | HTTP 状态 |
|--------|------|------|-----------|
| `RAG-000-000` | SUCCESS | 成功 | 200 |
| `RAG-100-001` | INTERNAL_ERROR | 内部错误 | 500 |
| `RAG-100-002` | NOT_IMPLEMENTED | 功能未实现 | 501 |
| `RAG-100-003` | OPERATION_TIMEOUT | 操作超时 | 504 |
| `RAG-100-004` | RESOURCE_EXHAUSTED | 资源耗尽 | 503 |
| `RAG-200-001` | CONFIG_NOT_FOUND | 配置文件不存在 | 500 |
| `RAG-200-002` | CONFIG_INVALID | 配置无效 | 400 |
| `RAG-200-003` | CONFIG_MISSING_FIELD | 缺少配置字段 | 400 |
| `RAG-300-001` | MODEL_NOT_FOUND | 模型文件不存在 | 500 |
| `RAG-300-002` | MODEL_LOAD_FAILED | 模型加载失败 | 500 |
| `RAG-300-003` | MODEL_UNSUPPORTED | 不支持的模型 | 400 |
| `RAG-300-004` | MODEL_INFERENCE_FAILED | 模型推理失败 | 500 |
| `RAG-300-005` | MODEL_OUT_OF_MEMORY | 内存不足 | 503 |
| `RAG-400-001` | INDEX_NOT_FOUND | 索引不存在 | 404 |
| `RAG-400-002` | INDEX_NOT_READY | 索引未就绪 | 503 |
| `RAG-400-003` | INDEX_BUILD_FAILED | 索引构建失败 | 500 |
| `RAG-400-004` | INDEX_CORRUPTED | 索引损坏 | 500 |
| `RAG-400-005` | DOCUMENT_NOT_FOUND | 文档不存在 | 404 |
| `RAG-400-006` | DOCUMENT_TOO_LARGE | 文档过大 | 400 |
| `RAG-400-007` | UNSUPPORTED_FILE_TYPE | 不支持的文件类型 | 400 |
| `RAG-500-001` | RETRIEVAL_FAILED | 检索失败 | 500 |
| `RAG-500-002` | EMPTY_RESULT | 检索结果为空 | 404 |
| `RAG-500-003` | QUERY_TOO_SHORT | 查询过短 | 400 |
| `RAG-500-004` | QUERY_TOO_LONG | 查询过长 | 400 |
| `RAG-600-001` | LLM_NOT_AVAILABLE | LLM 服务不可用 | 503 |
| `RAG-600-002` | LLM_GENERATION_FAILED | 生成失败 | 500 |
| `RAG-600-003` | LLM_TIMEOUT | 生成超时 | 504 |
| `RAG-600-004` | LLM_CONTENT_FILTERED | 内容被过滤 | 400 |
| `RAG-900-001` | INVALID_REQUEST | 无效请求 | 400 |
| `RAG-900-002` | UNAUTHORIZED | 未授权 | 401 |
| `RAG-900-003` | RATE_LIMITED | 请求过于频繁 | 429 |

---

## 2. 异常类设计

### 2.1 异常基类

```cpp
// error.h
#pragma once

#include <string>
#include <exception>
#include <map>

namespace rag {

// 错误码
struct ErrorCode {
    std::string code;                    // 错误码字符串
    int http_status;                    // HTTP 状态码
    std::string category;                // 错误类别

    bool is_success() const { return code == "RAG-000-000"; }
};

// 异常类
class RAGException : public std::exception {
public:
    RAGException(const ErrorCode& code,
                 const std::string& message,
                 const std::string& details = "");

    const char* what() const noexcept override;

    const ErrorCode& code() const { return code_; }
    const std::string& message() const { return message_; }
    const std::string& details() const { return details_; }

    // 添加上下文
    void set_context(const std::string& key, const std::string& value);
    std::map<std::string, std::string> context() const { return context_; }

    // 转换为 JSON
    std::string to_json() const;

private:
    ErrorCode code_;
    std::string message_;
    std::string details_;
    mutable std::string what_buffer_;
    std::map<std::string, std::string> context_;
};

}  // namespace rag
```

### 2.2 具体异常类

```cpp
// exceptions.h
#pragma once

#include "error.h"

namespace rag {

// 配置异常
class ConfigException : public RAGException {
public:
    using RAGException::RAGException;
    static ErrorCode NOT_FOUND;
    static ErrorCode INVALID;
    static ErrorCode MISSING_FIELD;
};

// 模型异常
class ModelException : public RAGException {
public:
    using RAGException::RAGException;
    static ErrorCode NOT_FOUND;
    static ErrorCode LOAD_FAILED;
    static ErrorCode UNSUPPORTED;
    static ErrorCode INFERENCE_FAILED;
    static ErrorCode OUT_OF_MEMORY;
};

// 索引异常
class IndexException : public RAGException {
public:
    using RAGException::RAGException;
    static ErrorCode NOT_FOUND;
    static ErrorCode NOT_READY;
    static ErrorCode BUILD_FAILED;
    static ErrorCode CORRUPTED;
};

// 文档异常
class DocumentException : public RAGException {
public:
    using RAGException::RAGException;
    static ErrorCode NOT_FOUND;
    static ErrorCode TOO_LARGE;
    static ErrorCode UNSUPPORTED_TYPE;
    static ErrorCode PARSE_FAILED;
};

// 检索异常
class RetrievalException : public RAGException {
public:
    using RAGException::RAGException;
    static ErrorCode FAILED;
    static ErrorCode EMPTY_RESULT;
    static ErrorCode QUERY_INVALID;
};

// LLM 异常
class LLMException : public RAGException {
public:
    using RAGException::RAGException;
    static ErrorCode NOT_AVAILABLE;
    static ErrorCode GENERATION_FAILED;
    static ErrorCode TIMEOUT;
    static ErrorCode CONTENT_FILTERED;
};

// 用户异常
class UserException : public RAGException {
public:
    using RAGException::RAGException;
    static ErrorCode INVALID_REQUEST;
    static ErrorCode UNAUTHORIZED;
    static ErrorCode RATE_LIMITED;
};

}  // namespace rag
```

### 2.3 异常使用示例

```cpp
// 抛出异常示例

// 1. 基本抛出
throw RAGException(ErrorCodes::INTERNAL_ERROR, "Unexpected error");

// 2. 带详情
throw ConfigException(
    ConfigException::NOT_FOUND,
    "Configuration file not found",
    "Expected at: ./rag-config.yaml");

// 3. 带上下文
auto e = ModelException(
    ModelException::LOAD_FAILED,
    "Failed to load model");
e.set_context("model_path", path);
e.set_context("error_code", "1234");
throw e;
```

---

## 3. 错误处理策略

### 3.1 重试策略

```cpp
// retry.h
#pragma once

#include <functional>
#include <chrono>

namespace rag {

enum class BackoffStrategy {
    LINEAR,       // 线性退避: delay = initial_delay * attempt
    EXPONENTIAL,  // 指数退避: delay = initial_delay * (2 ^ attempt)
    FIBONACCI     // 斐波那契退避: delay = fib(attempt) * initial_delay
};

struct RetryConfig {
    int max_attempts = 3;              // 最大重试次数
    int initial_delay_ms = 1000;      // 初始延迟
    int max_delay_ms = 10000;         // 最大延迟
    BackoffStrategy backoff = BackoffStrategy::EXPONENTIAL;
    std::vector<int> retryable_codes;  // 可重试的错误码

    // 可重试的错误类型
    bool should_retry(const RAGException& e) const;
};

class RetryHandler {
public:
    explicit RetryHandler(const RetryConfig& config);

    // 执行带重试的操作
    template<typename T>
    T execute(std::function<T()> operation) {
        int attempt = 0;
        std::chrono::milliseconds delay(config_.initial_delay_ms);

        while (true) {
            try {
                return operation();
            } catch (const RAGException& e) {
                attempt++;
                if (attempt >= config_.max_attempts ||
                    !config_.should_retry(e)) {
                    throw;
                }
                std::this_thread::sleep_for(delay);
                delay = calculate_next_delay(attempt);
                RAG_WARN("Retry attempt " + std::to_string(attempt));
            }
        }
    }

private:
    std::chrono::milliseconds calculate_next_delay(int attempt);
    RetryConfig config_;
};

}  // namespace rag
```

### 3.2 降级策略

```cpp
// fallback.h
#pragma once

#include <string>
#include <vector>

namespace rag {

enum class FallbackLevel {
    NONE,           // 不降级
    SKIP,           // 跳过失败的步骤
    USE_BM25_ONLY,  // 只使用 BM25
    USE_DUMMY,      // 使用假数据
    RETURN_ERROR    // 返回错误
};

struct FallbackConfig {
    FallbackLevel on_embedding_error = FallbackLevel::USE_BM25_ONLY;
    FallbackLevel on_llm_error = FallbackLevel::RETURN_ERROR;
    FallbackLevel on_retrieval_error = FallbackLevel::RETURN_ERROR;

    // 备用模型
    std::string fallback_embedding_model;
    std::string fallback_llm_model;
};

class FallbackHandler {
public:
    explicit FallbackHandler(const FallbackConfig& config);

    // 获取备用检索结果
    std::vector<RetrievalResult> get_fallback_chunks(
        const std::string& query);

    // 获取备用答案
    std::string get_fallback_answer(
        const std::string& question,
        const std::vector<Chunk>& chunks);

    // 检查是否可以使用主方案
    bool should_use_primary(
        FallbackLevel level,
        const std::string& operation);

private:
    FallbackConfig config_;
};

}  // namespace rag
```

### 3.3 错误处理钩子

```cpp
// error_handler.h
#pragma once

#include <functional>
#include "error.h"

namespace rag {

// 错误处理钩子
using ErrorHook = std::function<void(const RAGException&)>;

class ErrorHandler {
public:
    // 注册错误钩子
    void add_hook(const std::string& name, ErrorHook hook);

    // 处理错误
    void handle(const RAGException& e);

    // 清理
    void clear_hooks();

private:
    std::map<std::string, ErrorHook> hooks_;
};

// 全局错误处理器
ErrorHandler& get_global_error_handler();
```

---

## 4. 错误响应格式

### 4.1 JSON 错误响应

```json
{
    "error": {
        "code": "RAG-300-002",
        "message": "Failed to load embedding model",
        "details": "Model file not found: ./models/bge-base-zh-v1.5",
        "category": "model",
        "http_status": 500
    },
    "context": {
        "model_path": "./models/bge-base-zh-v1.5",
        "attempt": 1,
        "timestamp": "2026-07-06T10:30:00Z"
    },
    "request_id": "req_abc123"
}
```

### 4.2 错误响应生成

```cpp
// error_response.h
#pragma once

#include <string>
#include "error.h"

namespace rag {

class ErrorResponse {
public:
    // 从异常生成响应
    static std::string from_exception(
        const RAGException& e,
        const std::string& request_id = "");

    // 从错误码生成响应
    static std::string from_code(
        const ErrorCode& code,
        const std::string& message,
        const std::string& details = "",
        const std::string& request_id = "");

    // 获取 HTTP 状态码
    static int http_status(const RAGException& e);
};

}  // namespace rag
```

---

## 5. 错误日志记录

### 5.1 错误日志格式

```json
{
    "timestamp": "2026-07-06T10:30:00.123Z",
    "level": "ERROR",
    "logger": "RAG",
    "message": "Model loading failed",
    "error": {
        "code": "RAG-300-002",
        "type": "ModelException",
        "message": "Failed to load model",
        "details": "File not found"
    },
    "context": {
        "model_path": "./models/bge-base",
        "attempt": 3,
        "operation": "load_model"
    },
    "stack_trace": "..."
}
```

### 5.2 错误日志处理器

```cpp
// error_logger.h
#pragma once

#include "error.h"
#include "logger.h"

namespace rag {

class ErrorLogger {
public:
    // 记录错误
    void log_error(const RAGException& e,
                   const std::string& operation);

    // 记录重试
    void log_retry(const RAGException& e,
                    int attempt,
                    int max_attempts);

    // 记录降级
    void log_fallback(const std::string& operation,
                       FallbackLevel level);

    // 记录成功恢复
    void log_recovery(const std::string& operation,
                      int attempts);
};

}  // namespace rag
```

---

## 6. 常见错误处理

### 6.1 模型相关

```cpp
// 模型错误处理

// 1. 模型加载失败
try {
    model.load(path);
} catch (const ModelException& e) {
    if (e.code() == ModelException::NOT_FOUND) {
        // 下载模型
        download_model(path);
        model.load(path);
    } else if (e.code() == ModelException::OUT_OF_MEMORY) {
        // 尝试更小的模型
        model.load(fallback_model_path);
    } else {
        throw;
    }
}

// 2. 推理失败
try {
    auto result = model.inference(input);
} catch (const ModelException& e) {
    if (e.code() == ModelException::INFERENCE_FAILED) {
        // 重试一次
        retry_with_timeout(model, input);
    }
}
```

### 6.2 索引相关

```cpp
// 索引错误处理

// 1. 索引不存在
try {
    store.load();
} catch (const IndexException& e) {
    if (e.code() == IndexException::NOT_FOUND) {
        // 构建新索引
        build_index();
    } else if (e.code() == IndexException::CORRUPTED) {
        // 清理并重建
        clear_index();
        build_index();
    }
}

// 2. 索引构建失败
try {
    builder.build();
} catch (const IndexException& e) {
    // 记录错误并回退
    rollback_partial_build();
    throw;
}
```

### 6.3 检索相关

```cpp
// 检索错误处理

// 1. 结果为空
auto results = retriever.search(query);
if (results.empty()) {
    // 尝试放宽条件
    results = retriever.search(query, top_k * 2);

    if (results.empty()) {
        // 返回友好提示
        return FallbackHandler::no_results_response();
    }
}

// 2. 检索超时
try {
    results = retriever.search_with_timeout(query, timeout);
} catch (const RetrievalException& e) {
    // 使用缓存结果
    return cache_.get_cached(query);
}
```

### 6.4 LLM 相关

```cpp
// LLM 错误处理

// 1. 生成失败
try {
    answer = llm.generate(prompt);
} catch (const LLMException& e) {
    if (e.code() == LLMException::TIMEOUT) {
        // 减少上下文长度重试
        auto shorter_prompt = truncate_context(prompt, 2000);
        answer = llm.generate(shorter_prompt);
    } else if (e.code() == LLMException::CONTENT_FILTERED) {
        // 返回检索结果
        return FallbackHandler::return_chunks_only(results);
    }
}

// 2. LLM 不可用
if (!llm.is_available()) {
    // 使用备用回复
    return generate_fallback_answer(query, results);
}
```

---

## 7. 配置

```yaml
# 错误处理配置
error_handling:
  # 重试配置
  retry:
    max_attempts: 3
    initial_delay_ms: 1000
    max_delay_ms: 10000
    backoff: exponential
    retryable_errors:
      - "RAG-100-001"  # 内部错误
      - "RAG-300-004"  # 推理失败
      - "RAG-500-001"  # 检索失败
      - "RAG-600-002"  # 生成失败

  # 降级配置
  fallback:
    on_embedding_error: use_bm25_only  # skip, use_bm25_only, use_dummy
    on_llm_error: return_chunks         # return_chunks, return_error
    on_retrieval_error: return_error
    fallback_embedding_model: "./models/m3e-base"
    fallback_llm_model: ""

  # 日志配置
  logging:
    log_errors: true
    log_level: warn
    log_to_file: true
    log_file: "./rag_data/logs/errors.log"

  # 用户提示配置
  user_messages:
    model_load_failed: "模型加载失败，请检查配置"
    index_not_ready: "索引正在构建中，请稍后重试"
    no_results: "未找到相关结果，请尝试其他关键词"
    server_error: "服务器繁忙，请稍后重试"
```

---

## 8. 错误恢复

### 8.1 自动恢复策略

```cpp
// recovery.h
#pragma once

#include <string>
#include <functional>

namespace rag {

class RecoveryManager {
public:
    // 注册恢复策略
    void register_recovery(
        const std::string& error_code,
        std::function<bool()> recovery_fn);

    // 尝试恢复
    bool try_recover(const std::string& error_code);

    // 检查是否需要恢复
    bool needs_recovery(const std::string& error_code);

private:
    std::map<std::string, std::function<bool()>> recoveries_;
};

// 预定义恢复策略
namespace RecoveryStrategies {
    // 重新加载模型
    bool reload_embedding_model();
    bool reload_llm_model();

    // 重建索引
    bool rebuild_index();

    // 清理缓存
    bool clear_cache();

    // 释放内存
    bool free_memory();
}

}  // namespace rag
```

### 8.2 健康检查与恢复

```cpp
// 自动恢复流程
void check_and_recover() {
    auto status = health_checker.check_all();

    for (const auto& check : status) {
        if (check.status != HealthStatus::HEALTHY) {
            auto recovery = recovery_manager.needs_recovery(check.name);
            if (recovery) {
                RAG_INFO("Attempting recovery for: " + check.name);
                if (recovery_manager.try_recover(check.name)) {
                    RAG_INFO("Recovery successful");
                } else {
                    RAG_ERROR("Recovery failed for: " + check.name);
                }
            }
        }
    }
}
```

---

*文档版本：1.0.0*
*最后更新：2026-07-06*
