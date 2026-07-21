# Redpanda 流式查询引擎

## 学习目标

- 理解 Redpanda 的持续查询（Continuous Query）机制
- 掌握窗口函数的类型与计算原理
- 了解物化视图的增量更新策略
- 对比 Redpanda 查询与项目流处理模块的关联

## 正文

### 1. 持续查询（Continuous Query）

Redpanda 作为消息队列，主要提供消息的生产和消费能力。持续查询通常由下游流处理引擎（如 Flink、Kafka Streams、RisingWave）实现，Redpanda 作为数据源提供数据流。

```mermaid
graph LR
    subgraph "数据源层"
        P["Producer"]
        R["Redpanda<br/>Topic"]
        C["Consumer Group"]
    end
    
    subgraph "流处理层"
        S["Stream Processor<br/>Flink/RisingWave"]
        Q["Continuous Query"]
    end
    
    subgraph "输出层"
        M["Materialized View"]
        T["Target Topic"]
        DB["Database"]
    end
    
    P --> R
    R --> C
    C --> S
    S --> Q
    Q --> M
    Q --> T
    Q --> DB
```

**持续查询特点**：

| 特点 | 说明 |
|------|------|
| 无界数据 | 处理永不结束的数据流 |
| 增量计算 | 新数据到达时更新结果 |
| 状态管理 | 维护中间状态支持复杂计算 |
| 容错恢复 | 通过 Checkpoint 机制恢复状态 |

**Redpanda 在流处理中的角色**：

```mermaid
sequenceDiagram
    participant P as Producer
    participant R as Redpanda
    participant C as Consumer
    participant S as Stream Processor
    
    loop 持续生产
        P->>R: Produce Message
    end
    
    loop 持续消费
        R->>C: Fetch Records
        C->>S: Forward to Processor
        S->>S: 更新查询状态
        S->>S: 输出结果
    end
```

### 2. 窗口函数

流处理中的窗口将无界数据流划分为有限的数据块进行计算。常见窗口类型包括：

```mermaid
graph TB
    subgraph "窗口类型"
        T["Tumbling Window<br/>滚动窗口"]
        S["Sliding Window<br/>滑动窗口"]
        H["Hopping Window<br/>跳跃窗口"]
        SS["Session Window<br/>会话窗口"]
    end
    
    T --> T1["固定大小<br/>不重叠"]
    S --> S1["固定大小<br/>可重叠"]
    H --> H1["固定大小<br/>固定步长"]
    SS --> SS1["动态大小<br/>基于活动"]
```

#### 2.1 Tumbling Window（滚动窗口）

滚动窗口是最简单的窗口类型，窗口大小固定，窗口之间不重叠：

```mermaid
gantt
    title 滚动窗口示例（窗口大小 10s）
    dateFormat X
    axisFormat %s
    
    section 数据流
    Event 1 :0, 3
    Event 2 :4, 5
    Event 3 :7, 8
    Event 4 :12, 13
    Event 5 :15, 16
    Event 6 :18, 19
    
    section 窗口
    Window 1 [0-10] :0, 10
    Window 2 [10-20] :10, 20
    Window 3 [20-30] :20, 30
```

**滚动窗口计算逻辑**：

```sql
-- 每分钟计算订单数量
SELECT 
    TUMBLE_START(event_time, INTERVAL '1' MINUTE) AS window_start,
    TUMBLE_END(event_time, INTERVAL '1' MINUTE) AS window_end,
    COUNT(*) AS order_count,
    SUM(amount) AS total_amount
FROM orders
GROUP BY TUMBLE(event_time, INTERVAL '1' MINUTE);
```

#### 2.2 Sliding Window（滑动窗口）

滑动窗口窗口大小固定，但窗口可以重叠，适合计算移动平均等指标：

```mermaid
gantt
    title 滑动窗口示例（窗口大小 10s，滑动步长 5s）
    dateFormat X
    axisFormat %s
    
    section 数据流
    Event 1 :0, 3
    Event 2 :4, 5
    Event 3 :7, 8
    Event 4 :12, 13
    
    section 窗口
    Window 1 [0-10] :0, 10
    Window 2 [5-15] :5, 15
    Window 3 [10-20] :10, 20
```

**滑动窗口特点**：

- 窗口大小：`window_size`
- 滑动步长：`slide`（通常 `slide < window_size`）
- 数据可能属于多个窗口

```sql
-- 每 5 秒计算过去 10 秒的平均值
SELECT 
    HOP_START(event_time, INTERVAL '5' SECOND, INTERVAL '10' SECOND) AS window_start,
    HOP_END(event_time, INTERVAL '5' SECOND, INTERVAL '10' SECOND) AS window_end,
    AVG(temperature) AS avg_temp
FROM sensor_data
GROUP BY HOP(event_time, INTERVAL '5' SECOND, INTERVAL '10' SECOND);
```

#### 2.3 Hopping Window（跳跃窗口）

跳跃窗口是滑动窗口的特例，滑动步长等于窗口大小时即变为滚动窗口：

```mermaid
graph LR
    subgraph "Hopping vs Tumbling"
        H["Hopping<br/>步长 < 窗口大小<br/>有重叠"]
        T["Tumbling<br/>步长 = 窗口大小<br/>无重叠"]
    end
```

#### 2.4 Session Window（会话窗口）

会话窗口根据数据活动动态划分，无活动时窗口关闭：

```mermaid
gantt
    title 会话窗口示例（间隙 5s）
    dateFormat X
    axisFormat %s
    
    section 数据流
    Event 1 :0, 1
    Event 2 :2, 3
    Event 3 :4, 5
    Event 4 :15, 16
    Event 5 :17, 18
    
    section 窗口
    Session 1 [0-5] :0, 6
    Session 2 [15-18] :15, 19
```

**会话窗口计算逻辑**：

```sql
-- 按用户会话计算事件数
SELECT 
    SESSION_START(event_time, INTERVAL '5' SECOND) AS session_start,
    SESSION_END(event_time, INTERVAL '5' SECOND) AS session_end,
    user_id,
    COUNT(*) AS event_count
FROM user_events
GROUP BY user_id, SESSION(event_time, INTERVAL '5' SECOND);
```

### 3. 物化视图增量更新

流处理中的物化视图（Materialized View）维护查询结果的持久化状态，当新数据到达时增量更新：

```mermaid
sequenceDiagram
    participant S as Source Topic
    participant E as Stream Engine
    participant V as Materialized View
    participant Q as Query
    
    Note over S: 新消息到达
    S->>E: Insert Event
    
    alt 简单聚合
        E->>V: +COUNT, +SUM
    else 去重聚合
        E->>V: 检查是否已存在
        alt 新键
            E->>V: INSERT
        else 已存在
            E->>V: UPDATE
        end
    end
    
    Q->>V: 实时查询
    V-->>Q: 返回当前状态
```

**增量更新策略**：

| 策略 | 适用场景 | 复杂度 |
|------|----------|--------|
| 直接聚合 | COUNT, SUM, MIN, MAX | O(1) |
| 去重聚合 | DISTINCT COUNT | O(N) 或 O(1) with HyperLogLog |
| Join 维护 | Stream-Table Join | O(N) |
| 窗口维护 | 时间窗口聚合 | O(W) 窗口大小 |

**增量更新示例**：

```mermaid
graph TB
    subgraph "增量聚合流程"
        N["新记录<br/>user_id=1, amount=100"]
        V["当前视图<br/>user_id=1: count=5, total=500"]
        U["更新后视图<br/>user_id=1: count=6, total=600"]
    end
    
    N -->|"增量计算"| V
    V --> U
```

```sql
-- 创建物化视图
CREATE MATERIALIZED VIEW order_summary AS
SELECT 
    user_id,
    COUNT(*) AS order_count,
    SUM(amount) AS total_amount,
    AVG(amount) AS avg_amount
FROM orders
GROUP BY user_id;

-- 增量更新（内部实现）
-- 新订单 (user_id=1, amount=100) 到达时：
-- order_count += 1
-- total_amount += 100
-- avg_amount = total_amount / order_count
```

### 4. 与项目流处理模块关联

项目中已有流处理执行器框架，实现了基础的窗口计算：

```mermaid
graph TB
    subgraph "项目流处理架构"
        E["stream_engine.c<br/>流引擎核心"]
        W["stream_window.c<br/>窗口计算"]
        A["stream_agg.c<br/>流聚合"]
        S["stream_scan.c<br/>流扫描"]
        J["stream_join.c<br/>流连接"]
    end
    
    subgraph "窗口类型支持"
        W --> T["Tumbling"]
        W --> SL["Sliding"]
        W --> SS["Session"]
    end
    
    subgraph "数据存储"
        B["环形缓冲区"]
    end
    
    E --> B
    S --> E
    W --> S
    A --> W
```

**项目流处理接口**：

```c
// stream_window.h - 窗口算子接口
StreamWindowState *exec_stream_window_init(PlanState *parent,
    int64_t window_size, int64_t slide, int window_type);

TupleTableSlot *exec_stream_window_next(StreamWindowState *state);

void exec_stream_window_close(StreamWindowState *state);

// stream_agg.h - 聚合算子接口
StreamAggState *exec_stream_agg_init(PlanState *parent,
    int agg_type, int64_t window_size);

TupleTableSlot *exec_stream_agg_next(StreamAggState *state);

void exec_stream_agg_close(StreamAggState *state);
```

**窗口计算实现**：

项目 `stream_window.c` 已实现三种窗口类型：

| 窗口类型 | window_type | 实现函数 |
|----------|-------------|----------|
| Tumbling | 0 | `compute_tumbling_window()` |
| Sliding | 1 | `compute_sliding_window()` |
| Session | 2 | `compute_session_window()` |

```c
// 窗口计算核心逻辑（简化版）
static TupleTableSlot *compute_tumbling_window(
    StreamWindowState *state, stream_window_internal_t *internal)
{
    // 1. 初始化窗口边界
    if (internal->current_window_end == 0) {
        stream_record_t *first = &internal->buffer[0];
        internal->current_window_start = 
            (first->timestamp / internal->window_size) * internal->window_size;
        internal->current_window_end = 
            internal->current_window_start + internal->window_size;
    }
    
    // 2. 遍历记录，计算窗口内数量
    while (internal->current_index < internal->buffer_count) {
        stream_record_t *record = &internal->buffer[internal->current_index];
        
        if (record->timestamp < internal->current_window_end) {
            internal->current_window_count++;
            internal->current_index++;
        } else {
            // 3. 输出当前窗口，移动到下一个窗口
            // 返回 (window_start, window_end, count)
            // ...
        }
    }
}
```

**与 Redpanda 对比**：

```mermaid
graph TB
    subgraph "Redpanda 流处理"
        R1["消息存储<br/>追加写日志"]
        R2["消费者组<br/>Offset 管理"]
        R3["外部处理器<br/>Flink/RisingWave"]
    end
    
    subgraph "项目流处理"
        P1["消息存储<br/>环形缓冲区"]
        P2["水位线管理<br/>Watermark"]
        P3["内置处理器<br/>窗口/聚合算子"]
    end
    
    R1 -.->|"持久化 vs 内存"| P1
    R2 -.->|"Offset vs Watermark"| P2
    R3 -.->|"外部 vs 内置"| P3
```

| 维度 | Redpanda + 外部引擎 | 项目内置引擎 |
|------|---------------------|--------------|
| 数据存储 | 持久化日志文件 | 内存环形缓冲区 |
| 状态管理 | 外部引擎管理 | 内置状态管理 |
| 窗口计算 | 外部引擎实现 | 内置算子实现 |
| 容错机制 | Checkpoint + 重放 | 暂无 |
| 扩展性 | 可插拔外部引擎 | 内置有限扩展 |

### 5. 查询执行流程

```mermaid
sequenceDiagram
    participant Q as Query
    participant P as Parser
    participant O as Optimizer
    participant E as Executor
    participant S as Stream Scan
    participant W as Window
    participant A as Agg
    participant R as Result
    
    Q->>P: SQL 查询
    P->>O: 逻辑计划
    O->>E: 物理计划
    
    E->>S: 初始化扫描
    S->>S: 获取流数据
    
    loop 持续执行
        S->>W: 流数据记录
        W->>W: 计算窗口
        W->>A: 窗口结果
        A->>A: 增量聚合
        A->>R: 输出结果
    end
    
    R-->>Q: 返回结果流
```

**执行器状态流转**：

```mermaid
stateDiagram-v2
    [*] --> Init: 创建执行器
    Init --> Running: 开始执行
    Running --> Running: 持续处理
    Running --> Complete: 数据结束/取消
    Complete --> [*]: 关闭执行器
    
    state Running {
        [*] --> Scan
        Scan --> Window
        Window --> Agg
        Agg --> Output
        Output --> Scan
    }
```

### 6. 性能优化技术

```mermaid
graph TB
    subgraph "计算优化"
        C1["增量计算"]
        C2["预聚合"]
        C3["状态压缩"]
    end
    
    subgraph "I/O 优化"
        I1["批量读取"]
        I2["异步 I/O"]
        I3["零拷贝"]
    end
    
    subgraph "内存优化"
        M1["内存池"]
        M2["对象复用"]
        M3["溢出到磁盘"]
    end
```

**优化技术对比**：

| 技术 | Redpanda | 项目实现 |
|------|----------|----------|
| 批量处理 | Record Batch | 单记录迭代 |
| 异步 I/O | Seastar Future | 同步阻塞 |
| 零拷贝 | sendfile DMA | 内存拷贝 |
| 状态管理 | RocksDB | 内存结构 |

## 要点总结

1. **持续查询**：处理无界数据流，增量更新结果
2. **窗口函数**：滚动/滑动/跳跃/会话四种类型，将无界流划分为有限块
3. **物化视图**：维护查询状态，增量更新避免全量重算
4. **项目关联**：已实现窗口算子和流聚合，可扩展状态管理和容错机制

## 思考题

1. 滚动窗口和滑动窗口各适用于什么场景？性能差异如何？
2. 物化视图的增量更新如何处理 Retract（撤回）操作？
3. 项目的环形缓冲区存储有哪些限制？如何扩展支持持久化？
4. 如何在项目中实现流处理的容错机制（Checkpoint + 恢复）？
