# Chroma FLAT 暴力搜索算法

## 学习目标

- 理解 FLAT 算法的基本原理和实现方式
- 掌握 FLAT 在 Chroma 中的适用场景
- 分析 FLAT 的复杂度与精度权衡

## 基本原理

FLAT（Flat / Brute Force）是向量搜索中最简单的算法：**全量遍历所有向量，逐一计算距离，返回 Top-K 最近邻**。

```mermaid
graph TB
    Q[查询向量 q] --> COMP[与集合中每个向量<br/>计算距离]
    COMP --> R1[dist(q, v1) = 0.85]
    COMP --> R2[dist(q, v2) = 0.23]
    COMP --> R3[dist(q, v3) = 0.67]
    COMP --> RN[dist(q, vN) = 0.91]
    R1 --> SORT[按距离排序]
    R2 --> SORT
    R3 --> SORT
    RN --> SORT
    SORT --> TOP[返回 Top-K<br/>最近邻结果]
```

- **全量遍历**：对集合中的 N 个向量无一遗漏
- **距离计算**：每对向量计算一次距离（如 L2、余弦、内积）
- **排序取 Top-K**：获取距离最小的 K 个结果

## 实现方式

### Chroma 中的 FLAT 实现

Chroma 底层使用 HNSW 作为默认索引，但 FLAT 逻辑体现在以下场景：

1. **小集合退化**：当集合大小低于阈值（默认约 1000 条）时，HNSW 退化为 FLAT 行为
2. **精确搜索模式**：用户通过 `exact_search=True` 参数强制使用 FLAT
3. **向量文件扫描**：Chroma 将向量存储为 `.bin` 文件，读取时按页加载

```python
# Chroma 源码中的精确搜索路径（伪代码示意）
def query_vectors(vectors, query, k, exact=False):
    if exact or len(vectors) < THRESHOLD:
        # FLAT 暴力搜索
        distances = [cosine_sim(v, query) for v in vectors]
        top_k = np.argsort(distances)[-k:]
        return top_k
    else:
        # HNSW 近似搜索
        return hnsw_search(vectors, query, k)
```

### 距离计算方式

Chroma 支持多种距离度量，由 `metadata["hnsw:space"]` 控制：

| 距离度量 | 公式 | 适用场景 |
|---------|------|---------|
| L2（欧氏距离） | `sqrt(sum((v1 - v2)^2))` | 默认，通用性强 |
| Cosine（余弦距离） | `1 - dot(v1, v2) / (|v1|*|v2|)` | 文本语义搜索 |
| Inner Product（内积） | `sum(v1 * v2)` | 归一化向量的快速近似 |

## 复杂度分析

### 时间复杂度

| 操作 | 复杂度 | 说明 |
|------|--------|------|
| 单次搜索 | O(N * d) | N 为向量数，d 为维度 |
| 批量插入 | O(N * d) | 仅存储，无需建索引 |
| 单次插入 | O(d) | 追加写入 |
| 删除 | O(N * d) | 需重建或标记删除 |

### 空间复杂度

- **向量存储**：O(N * d)，每个浮点数 4 字节
- **索引额外开销**：O(1)，FLAT 无需额外索引结构

### 对比分析

| 指标 | FLAT | HNSW | IVF |
|------|------|------|-----|
| 搜索时间复杂度 | O(N*d) | O(log N * d) | O(sqrt(N) * d) |
| 构建时间 | 无 | 高 | 中 |
| 精度 | 100% | 近似（90-99%） | 近似（85-95%） |
| 内存占用 | 低 | 高 | 中 |
| 适合数据量 | < 1万 | 百万级 | 十万级 |

## 适用场景

### 适合 FLAT 的场景

1. **小数据集**（< 1万条）：FLAT 的线性扫描开销可接受
2. **高精度需求**：需要 100% 召回率，例如：
   - 去重检测
   - 精确匹配验证
   - 测试/基准对比
3. **低延迟不敏感**：延迟在几十毫秒内可接受
4. **冷启动阶段**：数据量小，尚未达到建索引的阈值

### 不适合 FLAT 的场景

1. **百万级数据**：线性扫描耗时过长（秒级）
2. **高 QPS 服务**：每次查询都全量扫描，CPU 无法承受
3. **实时搜索**：需要毫秒级响应

## 要点总结

- FLAT 是最简单的向量搜索算法：全量遍历 + 距离计算 + Top-K 排序
- 时间复杂度 O(N*d)，精度 100%，无构建开销
- Chroma 在小集合或精确搜索模式下使用 FLAT
- 适合小数据集、高精度需求、冷启动阶段
- 不适合大容量、高并发场景

## 思考题

1. FLAT 搜索中，距离计算占总时间的比例大约是多少？哪个环节最耗时？
2. 如果向量维度 d=1536（如 OpenAI 的 embedding），N=10000，一次 FLAT 搜索的浮点运算量大约是多少？
3. 在什么阈值下应该从 FLAT 切换到 HNSW？如何确定这个阈值？
4. Chroma 为什么选择 HNSW 作为默认索引，而不是 FLAT 或 IVF？