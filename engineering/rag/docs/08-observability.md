# RAG 系统可观测性设计

## 概述

本文档定义 RAG 系统的可观测性设计，包括日志系统、指标收集和健康检查。

---

## 1. 日志系统

### 1.1 日志级别

| 级别 | 使用场景 | 颜色 |
|------|----------|------|
| `TRACE` | 详细调试信息 | 灰色 |
| `DEBUG` | 开发调试信息 | 蓝色 |
| `INFO` | 常规运行信息 | 绿色 |
| `WARN` | 警告信息 | 黄色 |
| `ERROR` | 错误信息 | 红色 |
| `FATAL` | 致命错误 | 红色+高亮 |

### 1.2 日志格式

```yaml
# 日志格式
format: "[{timestamp}] [{level}] [{logger}] {message} {context}"

# 示例
[2026-07-06 10:30:15.123] [INFO] [RAGEngine] 索引构建完成, 文档数: 150
[2026-07-06 10:30:15.456] [DEBUG] [Embedding] 向量编码完成, token数: 512
[2026-07-06 10:30:15.789] [ERROR] [BM25] 索引加载失败: 文件不存在
```

### 1.3 日志接口

```cpp
// logger.h
#pragma once

#include <string>
#include <memory>
#include <source_location>

namespace rag {

enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    FATAL = 5
};

class Logger {
public:
    virtual ~Logger() = default;

    // 初始化
    virtual void init(const Config& config) = 0;

    // 记录日志
    virtual void log(LogLevel level,
                    const std::string& logger_name,
                    const std::string& message,
                    const std::source_location& location = std::source_location::current()) = 0;

    // 便捷方法
    void trace(const std::string& logger, const std::string& msg);
    void debug(const std::string& logger, const std::string& msg);
    void info(const std::string& logger, const std::string& msg);
    void warn(const std::string& logger, const std::string& msg);
    void error(const std::string& logger, const std::string& msg);
    void fatal(const std::string& logger, const std::string& msg);

    // 结构化日志
    void log_structured(LogLevel level,
                        const std::string& logger,
                        const std::string& message,
                        const std::map<std::string, std::string>& fields);
};

// 全局日志器
Logger& get_logger();

}  // namespace rag

// 便捷宏
#define RAG_TRACE(msg) rag::get_logger().trace("RAG", msg)
#define RAG_DEBUG(msg) rag::get_logger().debug("RAG", msg)
#define RAG_INFO(msg) rag::get_logger().info("RAG", msg)
#define RAG_WARN(msg) rag::get_logger().warn("RAG", msg)
#define RAG_ERROR(msg) rag::get_logger().error("RAG", msg)
```

### 1.4 日志上下文

```cpp
// 日志上下文
struct LogContext {
    std::string request_id;              // 请求ID
    std::string user_id;                // 用户ID
    std::string operation;              // 操作类型
    std::chrono::steady_clock::time_point start_time;
};

class ScopedLogContext {
public:
    ScopedLogContext(const std::string& operation);
    ~ScopedLogContext();

    void set(const std::string& key, const std::string& value);
    LogContext& context() { return ctx_; }

private:
    LogContext ctx_;
};

// 使用示例
void index_document(const std::string& path) {
    SCOPED_LOG_CONTEXT("index_document");
    ctx.set("file_path", path);

    RAG_INFO("开始索引文档");
    // ...
    RAG_INFO("文档索引完成");
}
```

---

## 2. 指标收集

### 2.1 指标类型

| 类型 | 说明 | 示例 |
|------|------|------|
| **Counter** | 只增不减计数器 | 请求次数、错误次数 |
| **Gauge** | 可增可减数值 | 队列长度、内存使用 |
| **Histogram** | 数值分布 | 延迟分布 |
| **Summary** | 分位数统计 | P50/P95/P99 延迟 |

### 2.2 指标定义

```cpp
// metrics.h
#pragma once

#include <string>
#include <atomic>
#include <chrono>
#include <memory>

namespace rag {

// 指标接口
class Metrics {
public:
    virtual ~Metrics() = default;

    // 计数器
    virtual void inc_counter(const std::string& name,
                            const std::string& label = "") = 0;
    virtual void inc_counter_by(const std::string& name,
                               double value,
                               const std::string& label = "") = 0;

    // 仪表
    virtual void set_gauge(const std::string& name,
                          double value,
                          const std::string& label = "") = 0;

    // 直方图
    virtual void observe_histogram(const std::string& name,
                                  double value,
                                  const std::string& label = "") = 0;

    // 计时器
    class Timer {
    public:
        Timer(Metrics& metrics, const std::string& name);
        ~Timer();

        double elapsed_ms() const;

    private:
        Metrics& metrics_;
        std::string name_;
        std::chrono::steady_clock::time_point start_;
    };
};

// Prometheus 格式输出
class PrometheusExporter {
public:
    std::string export();
};

}  // namespace rag
```

### 2.3 RAG 系统指标

```cpp
// rag_metrics.h
#pragma once

#include "metrics.h"

namespace rag {

// RAG 系统指标收集器
class RAGMetrics {
public:
    RAGMetrics(Metrics& metrics);

    // ========== 索引指标 ==========

    // 索引文档数
    void inc_indexed_documents(int count = 1);
    void inc_index_chunks(int count = 1);

    // 索引构建时间
    void observe_index_duration(double ms);

    // 索引错误
    void inc_index_errors();

    // ========== 查询指标 ==========

    // 查询计数
    void inc_query_count();

    // 查询延迟
    void observe_query_latency(double ms);
    void observe_embedding_latency(double ms);
    void observe_search_latency(double ms);
    void observe_generation_latency(double ms);

    // 查询结果
    void observe_chunks_returned(int count);
    void inc_empty_results();

    // ========== 模型指标 ==========

    // 模型加载状态
    void set_embedding_loaded(bool loaded);
    void set_llm_loaded(bool loaded);

    // Token 使用
    void inc_tokens_processed(int count);
    void inc_tokens_generated(int count);

    // ========== 系统指标 ==========

    // 内存使用
    void set_memory_usage(size_t bytes);

    // 队列长度
    void set_embedding_queue_size(int size);
    void set_query_queue_size(int size);

private:
    Metrics& metrics_;
};

// 指标名称常量
struct MetricNames {
    // 索引
    static constexpr const char* INDEXED_DOCS = "rag_indexed_documents_total";
    static constexpr const char* INDEX_CHUNKS = "rag_index_chunks_total";
    static constexpr const char* INDEX_DURATION = "rag_index_duration_seconds";
    static constexpr const char* INDEX_ERRORS = "rag_index_errors_total";

    // 查询
    static constexpr const char* QUERY_COUNT = "rag_query_total";
    static constexpr const char* QUERY_LATENCY = "rag_query_latency_seconds";
    static constexpr const char* EMBEDDING_LATENCY = "rag_embedding_latency_seconds";
    static constexpr const char* SEARCH_LATENCY = "rag_search_latency_seconds";
    static constexpr const char* GENERATION_LATENCY = "rag_generation_latency_seconds";
    static constexpr const char* CHUNKS_RETURNED = "rag_chunks_returned_total";
    static constexpr const char* EMPTY_RESULTS = "rag_empty_results_total";

    // 模型
    static constexpr const char* EMBEDDING_LOADED = "rag_embedding_model_loaded";
    static constexpr const char* LLM_LOADED = "rag_llm_model_loaded";
    static constexpr const char* TOKENS_PROCESSED = "rag_tokens_processed_total";
    static constexpr const char* TOKENS_GENERATED = "rag_tokens_generated_total";

    // 系统
    static constexpr const char* MEMORY_USAGE = "rag_memory_usage_bytes";
    static constexpr const char* EMBEDDING_QUEUE = "rag_embedding_queue_size";
    static constexpr const char* QUERY_QUEUE = "rag_query_queue_size";
};

}  // namespace rag
```

### 2.4 Prometheus 导出格式

```
# HELP rag_indexed_documents_total Total number of indexed documents
# TYPE rag_indexed_documents_total counter
rag_indexed_documents_total 150

# HELP rag_query_latency_seconds Query latency in seconds
# TYPE rag_query_latency_seconds histogram
rag_query_latency_seconds_bucket{le="0.01"} 100
rag_query_latency_seconds_bucket{le="0.05"} 500
rag_query_latency_seconds_bucket{le="0.1"} 800
rag_query_latency_seconds_bucket{le="0.5"} 950
rag_query_latency_seconds_bucket{le="1.0"} 990
rag_query_latency_seconds_bucket{le="+Inf"} 1000
rag_query_latency_seconds_sum 50.5
rag_query_latency_seconds_count 1000

# HELP rag_embedding_model_loaded Whether embedding model is loaded
# TYPE rag_embedding_model_loaded gauge
rag_embedding_model_loaded 1
```

---

## 3. 健康检查

### 3.1 健康检查接口

```cpp
// health_check.h
#pragma once

#include <string>
#include <vector>
#include <chrono>

namespace rag {

enum class HealthStatus {
    HEALTHY,
    DEGRADED,
    UNHEALTHY
};

struct HealthCheckResult {
    std::string name;                   // 检查项名称
    HealthStatus status;                // 状态
    std::string message;                // 详细信息
    double latency_ms;                  // 检查耗时
    std::chrono::steady_clock::time_point timestamp;
};

class HealthCheck {
public:
    virtual ~HealthCheck() = default;

    // 执行检查
    virtual HealthCheckResult check() = 0;

    // 检查名称
    virtual std::string name() const = 0;
};

// 健康检查管理器
class HealthChecker {
public:
    void register_check(std::shared_ptr<HealthCheck> check);

    // 执行所有检查
    std::vector<HealthCheckResult> check_all();

    // 获取总体状态
    HealthStatus overall_status();

    // JSON 格式输出
    std::string to_json();
};

}  // namespace rag
```

### 3.2 内置健康检查

```cpp
// 内置健康检查实现

// 1. 模型加载检查
class ModelHealthCheck : public HealthCheck {
public:
    ModelHealthCheck(std::shared_ptr<EmbeddingService> embed,
                    std::shared_ptr<LLMService> llm);

    HealthCheckResult check() override;
    std::string name() const override { return "models"; }

private:
    std::shared_ptr<EmbeddingService> embed_;
    std::shared_ptr<LLMService> llm_;
};

// 2. 索引状态检查
class IndexHealthCheck : public HealthCheck {
public:
    IndexHealthCheck(std::shared_ptr<VectorStore> store);

    HealthCheckResult check() override;
    std::string name() const override { return "index"; }

private:
    std::shared_ptr<VectorStore> store_;
};

// 3. 磁盘空间检查
class DiskSpaceCheck : public HealthCheck {
public:
    explicit DiskSpaceCheck(const std::string& path, size_t min_free_gb = 5);

    HealthCheckResult check() override;
    std::string name() const override { return "disk_space"; }

private:
    std::string path_;
    size_t min_free_gb_;
};

// 4. 内存检查
class MemoryCheck : public HealthCheck {
public:
    explicit MemoryCheck(size_t max_usage_gb = 12);

    HealthCheckResult check() override;
    std::string name() const override { return "memory"; }

private:
    size_t max_usage_gb_;
};

// 5. 依赖服务检查
class DependencyCheck : public HealthCheck {
public:
    struct ServiceEndpoint {
        std::string name;
        std::string url;
        int timeout_ms;
    };

    void add_service(const std::string& name,
                    const std::string& url,
                    int timeout_ms = 1000);

    HealthCheckResult check() override;
    std::string name() const override { return "dependencies"; }

private:
    std::vector<ServiceEndpoint> services_;
};
```

### 3.3 健康检查端点

```
GET /health

Response 200 OK:
{
  "status": "healthy",
  "timestamp": "2026-07-06T10:30:00Z",
  "checks": [
    {
      "name": "models",
      "status": "healthy",
      "message": "All models loaded",
      "latency_ms": 5.2
    },
    {
      "name": "index",
      "status": "healthy",
      "message": "Index ready with 150 documents",
      "latency_ms": 1.3
    },
    {
      "name": "disk_space",
      "status": "healthy",
      "message": "10.5 GB free",
      "latency_ms": 0.1
    }
  ]
}

Response 503 Service Unavailable:
{
  "status": "unhealthy",
  "timestamp": "2026-07-06T10:30:00Z",
  "checks": [
    {
      "name": "models",
      "status": "unhealthy",
      "message": "Embedding model not loaded",
      "latency_ms": 100.5
    }
  ]
}
```

---

## 4. 追踪系统

### 4.1 追踪接口

```cpp
// tracer.h
#pragma once

#include <string>
#include <memory>
#include <chrono>

namespace rag {

struct Span {
    std::string trace_id;
    std::string span_id;
    std::string name;
    std::string service;
    std::chrono::steady_clock::time_point start;
    std::chrono::steady_clock::time_point end;
    std::string status;
    std::map<std::string, std::string> attributes;
    std::vector<Span> children;
};

class Tracer {
public:
    virtual ~Tracer() = default;

    // 创建追踪跨度
    virtual std::unique_ptr<Scope> start_span(const std::string& name,
                                               const std::map<std::string, std::string>& attrs = {}) = 0;

    // 注入上下文
    virtual void inject(std::string& carrier,
                        const std::string& format = "http_header") = 0;

    // 提取上下文
    virtual void extract(const std::string& carrier,
                         std::map<std::string, std::string>& context,
                         const std::string& format = "http_header") = 0;
};

// 追踪作用域
class Scope {
public:
    virtual ~Scope() = default;

    // 添加属性
    virtual void set_attribute(const std::string& key,
                              const std::string& value) = 0;

    // 记录事件
    virtual void add_event(const std::string& name,
                          const std::map<std::string, std::string>& attrs = {}) = 0;

    // 记录错误
    virtual void record_error(const std::exception& e) = 0;

    // 结束
    virtual void end() = 0;
};

// 自动管理追踪作用域
class ScopedSpan {
public:
    ScopedSpan(Tracer& tracer, const std::string& name);
    ~ScopedSpan();

    void set_attribute(const std::string& key, const std::string& value);

private:
    std::unique_ptr<Scope> scope_;
};

}  // namespace rag
```

### 4.2 RAG 追踪点

```cpp
// 追踪点

// 1. 索引构建追踪
void build_index() {
    SCOPED_TRACE("index_build");

    {
        SCOPED_TRACE("scan_documents");
        // 扫描文档
    }

    {
        SCOPED_TRACE("parse_documents");
        // 解析文档
    }

    {
        SCOPED_TRACE("chunk_documents");
        // 分块
    }

    {
        SCOPED_TRACE("generate_embeddings");
        set_attribute("batch_size", "32");
        // 生成向量
    }

    {
        SCOPED_TRACE("store_index");
        // 存储索引
    }
}

// 2. 查询追踪
void query() {
    SCOPED_TRACE("query");
    set_attribute("query_length", std::to_string(question.size()));

    {
        SCOPED_TRACE("embedding");
        auto vec = embed(question);
    }

    {
        SCOPED_TRACE("search");
        auto results = retriever.search(vec);
    }

    {
        SCOPED_TRACE("generate");
        auto answer = llm.generate(prompt);
    }
}
```

---

## 5. 配置

```yaml
# 可观测性配置
observability:
  # 日志
  log:
    level: info
    file: "./rag_data/logs/rag.log"
    max_size: 100MB
    max_backups: 5
    format: json

  # 指标
  metrics:
    enabled: true
    port: 9090
    path: "/metrics"

  # 追踪
  tracing:
    enabled: false
    endpoint: "http://localhost:4318"
    sample_rate: 0.1

  # 健康检查
  health:
    check_interval: 30
    endpoints:
      - "127.0.0.1:8080"
      - "./models"
      - "./rag_data"
```

---

*文档版本：1.0.0*
*最后更新：2026-07-06*
