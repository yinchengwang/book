# Embedding 服务规格

> 状态: 草稿 | 创建日期: 2026-07-07

## 1. 概述

### 1.1 目的

定义 Embedding 服务的接口和行为规范，确保不同实现（Simple/BGE-M3）的一致性和可替换性。

### 1.2 范围

- Embedding 向量编码
- 批量编码
- 稀疏向量编码（可选）
- 向量缓存

## 2. 接口规范

### 2.1 EmbeddingService 接口

```cpp
namespace rag {

class EmbeddingService {
public:
    virtual ~EmbeddingService() = default;
    
    /**
     * @brief 编码单个文本为向量
     * @param text 输入文本
     * @return 归一化向量，维度由 dimension() 返回
     * @throws 无异常（失败时返回零向量）
     */
    virtual std::vector<float> encode(const std::string& text) = 0;
    
    /**
     * @brief 批量编码
     * @param texts 输入文本列表
     * @return 向量列表，顺序与输入一致
     */
    virtual std::vector<std::vector<float>> encode_batch(
        const std::vector<std::string>& texts) = 0;
    
    /**
     * @brief 获取向量维度
     */
    virtual int dimension() const = 0;
    
    /**
     * @brief 是否就绪
     * @return true 表示模型已加载，可正常编码
     */
    virtual bool is_ready() const = 0;
    
    /**
     * @brief 获取模型名称
     */
    virtual std::string model_name() const = 0;
};

}  // namespace rag
```

### 2.2 BGEM3EmbeddingService 扩展

```cpp
class BGEM3EmbeddingService : public EmbeddingService {
public:
    /**
     * @brief 构造函数
     * @param model_path 模型路径或 HuggingFace 模型名
     * @param device 设备类型: "cpu", "cuda"
     * @param batch_size 批处理大小，默认 32
     */
    BGEM3EmbeddingService(
        const std::string& model_path = "",
        const std::string& device = "cpu",
        int batch_size = 32);
    
    // 基础接口
    std::vector<float> encode(const std::string& text) override;
    std::vector<std::vector<float>> encode_batch(
        const std::vector<std::string>& texts) override;
    int dimension() const override { return 1024; }
    bool is_ready() const override { return ready_; }
    std::string model_name() const override { return "bge-m3"; }
    
    // ========== 扩展接口 ==========
    
    /**
     * @brief 稀疏向量编码
     * @note 用于混合检索
     */
    struct SparseVector {
        std::vector<uint32_t> indices;  // token ID
        std::vector<float> values;       // 权重
    };
    
    SparseVector encode_sparse(const std::string& text);
    
    /**
     * @brief 带详细信息的编码
     */
    struct EmbeddingResult {
        std::vector<float> dense;    // 稠密向量
        SparseVector sparse;          // 稀疏向量
    };
    
    EmbeddingResult encode_with_info(const std::string& text);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool ready_ = false;
};

}  // namespace rag
```

## 3. 行为规范

### 3.1 编码行为

| 场景 | 返回值 | 说明 |
|------|--------|------|
| 正常编码 | 归一化向量 | L2 归一化 |
| 空文本 | 全零向量 | 长度为 dimension() |
| 模型未就绪 | 全零向量 | 记录警告日志 |
| 异常 | 全零向量 | 记录错误日志，不抛异常 |

### 3.2 批量编码

| 场景 | 返回值 | 说明 |
|------|--------|------|
| 正常批量 | 向量列表 | 顺序与输入一致 |
| 部分失败 | 部分结果 | 失败位置返回零向量 |
| 空列表 | 空向量列表 | 不抛异常 |

### 3.3 缓存行为

- **缓存容量**: 默认 10000 条
- **缓存键**: 文本内容
- **缓存命中**: 直接返回缓存向量
- **缓存淘汰**: LRU 策略

### 3.4 向量规范

| 属性 | 值 |
|------|-----|
| 数据类型 | float32 |
| 维度 | 768 (Simple) / 1024 (BGE-M3) |
| 归一化 | L2 归一化 |
| 范围 | [-1, 1] |

## 4. 配置规范

### 4.1 配置结构

```cpp
struct EmbeddingConfig {
    // 类型: "simple" | "bge-m3"
    std::string type = "simple";
    
    // 维度 (用于 Simple 模式)
    int dimension = 768;
    
    // BGE-M3 配置
    struct BGEConfig {
        std::string model_path;      // 模型路径
        std::string device = "cpu";  // cpu | cuda
        int batch_size = 32;         // 批处理大小
        bool use_cache = true;       // 是否启用缓存
        size_t max_cache_size = 10000;  // 缓存大小
    } bge;
};
```

### 4.2 配置示例

```yaml
# Simple 模式
embedding:
  type: "simple"
  dimension: 768

# BGE-M3 模式
embedding:
  type: "bge-m3"
  bge:
    model_path: "./models/bge-m3"
    device: "cpu"
    batch_size: 32
    use_cache: true
    max_cache_size: 10000
```

## 5. 性能规范

### 5.1 延迟要求

| 操作 | Simple | BGE-M3 |
|------|--------|--------|
| 单条编码 | < 1ms | < 100ms |
| 批量编码 (32条) | < 1ms | < 200ms |

### 5.2 内存要求

| 项目 | 内存 |
|------|------|
| Simple | < 1MB |
| BGE-M3 | < 2GB |

## 6. 错误处理

### 6.1 错误日志

| 场景 | 日志级别 | 内容 |
|------|----------|------|
| 模型加载失败 | WARN | "Failed to load model: {error}" |
| 模型未就绪 | DEBUG | "Embedding service not ready, using fallback" |
| 编码异常 | ERROR | "Encode failed: {error}" |

### 6.2 回退机制

当 BGE-M3 模型不可用时：
1. 记录警告日志
2. 返回全零向量
3. 不影响系统运行

## 7. 测试规范

### 7.1 单元测试

```cpp
TEST(EmbeddingService, SimpleDimension) {
    SimpleEmbeddingService service(768);
    service.load();
    
    EXPECT_EQ(service.dimension(), 768);
    EXPECT_TRUE(service.is_ready());
}

TEST(EmbeddingService, EncodeReturnsNormalizedVector) {
    SimpleEmbeddingService service(128);
    service.load();
    
    auto vec = service.encode("test text");
    EXPECT_EQ(vec.size(), 128);
    
    // 验证 L2 归一化
    float norm = 0;
    for (float v : vec) norm += v * v;
    EXPECT_NEAR(norm, 1.0f, 0.001f);
}

TEST(EmbeddingService, BatchEncodeOrderPreserved) {
    SimpleEmbeddingService service(128);
    service.load();
    
    std::vector<std::string> texts = {"a", "b", "c"};
    auto results = service.encode_batch(texts);
    
    EXPECT_EQ(results.size(), 3);
    // 验证顺序一致
    EXPECT_EQ(results[0].size(), 128);
    EXPECT_EQ(results[1].size(), 128);
    EXPECT_EQ(results[2].size(), 128);
}
```

### 7.2 集成测试

```cpp
TEST(EmbeddingService, BGELoadsSuccessfully) {
    BGEM3EmbeddingService service("./models/bge-m3", "cpu");
    
    if (service.is_ready()) {
        auto vec = service.encode("test");
        EXPECT_EQ(vec.size(), 1024);
    } else {
        // 跳过测试但记录原因
        GTEST_SKIP() << "BGE model not available";
    }
}
```

## 8. 扩展接口

### 8.1 统计信息

```cpp
struct EmbeddingStats {
    uint64_t encode_count;      // 编码次数
    uint64_t cache_hits;        // 缓存命中
    uint64_t cache_misses;      // 缓存未命中
    double cache_hit_rate;      // 命中率
    double avg_encode_time_ms;  // 平均编码时间
};

virtual EmbeddingStats get_stats() const;
```

### 8.2 缓存管理

```cpp
virtual void clear_cache();
virtual size_t cache_size() const;
```
