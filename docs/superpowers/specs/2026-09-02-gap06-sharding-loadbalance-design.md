# Gap#6 自动化分片与负载均衡设计

> **日期:** 2026-09-02
> **状态:** 已批准

## 1. 目标

设计并实现自动化分片与负载均衡系统，实现：
- 阈值触发的分片再平衡
- 负载感知的跨分片调度
- 可配置的迁移策略（增量迁移 + 虚拟节点迁移）
- 与 Gap#3 执行引擎无缝集成

## 2. 架构概览

```
┌─────────────────────────────────────────────────────────────┐
│                      分片协调器                              │
│                 (shard_coordinator.c)                       │
│                                                              │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐ │
│  │ 阈值检测器   │  │ 负载调度器   │  │ 分片再平衡器         │ │
│  │ (skew > 1.5)│  │ (least-load)│  │ (incremental/vnode) │ │
│  └──────┬──────┘  └──────┬──────┘  └──────────┬──────────┘ │
└─────────┼────────────────┼────────────────────┼─────────────┘
          │                │                    │
          ▼                ▼                    ▼
┌─────────────────────────────────────────────────────────────┐
│                      分片路由器                             │
│                 (shard_router + shard_routing)              │
│                                                              │
│  Hash/Range/List 路由 + 虚拟节点管理                         │
└─────────────────────────────────────────────────────────────┘
          │
          ▼
┌─────────────────────────────────────────────────────────────┐
│                      执行引擎集成                            │
│              (Gap#3 的 ExecNode 分片路由)                    │
└─────────────────────────────────────────────────────────────┘
```

## 3. 核心组件

### 3.1 分片协调器 (shard_coordinator.c)

```c
typedef struct shard_coordinator {
    shard_balance_config_t config;
    shard_router_t *router;
    load_collector_t *collector;
    migrate_manager_t *migrate_mgr;
    bool running;
    pthread_t monitor_thread;
} shard_coordinator_t;
```

**职责：**
- 定期检测分片倾斜度
- 触发负载均衡决策
- 管理迁移生命周期

### 3.2 负载收集器 (load_collector.c)

```c
typedef struct shard_load {
    int shard_id;
    uint64_t row_count;        // 行数
    double qps;                // 每秒查询数
    double latency_ms;         // 平均延迟
    double cpu_usage;          // CPU 使用率
    int64_t size_bytes;        // 数据大小
    time_t last_updated;
} shard_load_t;

typedef struct load_collector {
    shard_load_t *shards;
    int shard_count;
    pthread_mutex_t mutex;
} load_collector_t;
```

**职责：**
- 收集各分片的负载指标
- 计算倾斜度（max_load / avg_load）
- 上报给协调器

### 3.3 迁移管理器 (migrate_manager.c)

```c
typedef enum {
    MIGRATE_INCREMENTAL,     // 增量迁移（Range/List 分片）
    MIGRATE_VIRTUAL_NODE     // 虚拟节点迁移（Hash 分片）
} migrate_strategy_t;

typedef struct migrate_task {
    int task_id;
    int source_shard;
    int target_shard;
    migrate_strategy_t strategy;
    void *key_range;          // 迁移的 key 范围
    double progress;          // 进度 0.0-1.0
    migrate_status_t status;  // PENDING/RUNNING/COMPLETED/FAILED
} migrate_task_t;
```

**职责：**
- 计算迁移计划
- 执行数据迁移
- 追踪迁移进度

### 3.4 路由集成 (shard_exec.c)

```c
typedef struct shard_exec_node {
    ExecNode base;                    // Gap#3 ExecNode
    shard_coordinator_t *coord;       // 分片协调器
    shard_router_t *router;           // 路由器
} shard_exec_node_t;
```

**职责：**
- 将 ExecNode 请求路由到对应分片
- 聚合多分片结果
- 负载感知调度

## 4. 用户配置

### 4.1 平衡配置结构

```c
typedef struct shard_balance_config {
    double skew_threshold;        // 倾斜阈值（默认 1.5）
                                // 当 max_load / avg_load > skew_threshold 时触发
    int64_t max_shard_size;      // 最大分片大小（默认 10GB = 10*1024*1024*1024）
    int check_interval_ms;       // 检查间隔（默认 60000ms = 1分钟）
    migrate_strategy_t strategy; // 默认迁移策略
    bool auto_rebalance;         // 自动再平衡开关（默认 true）
} shard_balance_config_t;
```

### 4.2 配置来源优先级

1. **CLI 运行时参数**（最高优先级）
2. **配置文件**（shard.conf）
3. **代码默认值**（最低优先级）

## 5. 再平衡触发机制

### 5.1 阈值触发流程

```
1. 负载收集器定期收集 shard_load_t 指标
2. 计算倾斜度：skew = max(row_count) / avg(row_count)
3. 如果 skew > config.skew_threshold：
   - 标记需要再平衡
   - 通知协调器
4. 协调器选择迁移策略（用户配置 / 自动选择）
5. 迁移管理器计算迁移计划
6. 执行迁移（双写 + 增量同步）
7. 验证完成，更新路由表
```

### 5.2 迁移策略选择

| 分片策略 | 默认迁移策略 | 说明 |
|----------|-------------|------|
| Hash | VIRTUAL_NODE | 虚拟节点迁移，改动 hash 环 |
| Range | INCREMENTAL | 增量迁移，按范围边界 |
| List | INCREMENTAL | 增量迁移，按列表值 |

用户可通过 `shard_balance_config.strategy` 覆盖默认策略。

## 6. 负载感知调度

### 6.1 最小负载调度

```c
int shard_select_least_load(const shard_coordinator_t *coord,
                            const int *candidate_shards,
                            int count) {
    int best_shard = -1;
    double min_load = DBL_MAX;

    for (int i = 0; i < count; i++) {
        int shard_id = candidate_shards[i];
        double load = calculate_load(coord->collector, shard_id);
        if (load < min_load) {
            min_load = load;
            best_shard = shard_id;
        }
    }
    return best_shard;
}
```

### 6.2 负载计算公式

```
load = α * row_count + β * qps + γ * latency_ms

默认权重：α = 0.4, β = 0.3, γ = 0.3
```

## 7. CLI 接口

### 7.1 配置命令

```bash
# 设置倾斜阈值
mmdb shard config --set-skew-threshold=1.5

# 设置最大分片大小
mmdb shard config --set-max-shard-size=10GB

# 设置检查间隔
mmdb shard config --set-check-interval=60000

# 设置默认迁移策略
mmdb shard config --set-migrate-strategy=virtual-node

# 启用/禁用自动再平衡
mmdb shard config --set-auto-rebalance=true
```

### 7.2 监控命令

```bash
# 查看分片状态
mmdb shard status

# 查看负载指标
mmdb shard status --metrics

# 查看迁移任务
mmdb shard migrate --list
```

### 7.3 紧急干预

```bash
# 强制触发再平衡
mmdb shard rebalance --force

# 取消迁移任务
mmdb shard migrate --cancel --task-id=123
```

## 8. 文件结构

```
engineering/
├── include/db/sharding/
│   ├── shard_routing.h           # 已有
│   ├── sharding.h                # 已有
│   ├── shard_balance.h           # 新增：平衡配置
│   └── shard_coordinator.h       # 新增：协调器接口
├── src/db/sharding/
│   ├── sharding.c                # 已有
│   ├── shard_routing.c           # 已有
│   ├── shard_balance.c           # 新增：平衡配置实现
│   ├── shard_coordinator.c       # 新增：协调器实现
│   ├── load_collector.c          # 新增：负载收集
│   ├── migrate_manager.c         # 新增：迁移管理
│   └── shard_exec.c              # 新增：Executor 集成
├── src/db/executor/
│   └── operators/
│       └── shard_scan_exec.c     # 新增：分片扫描算子
└── test/db/sharding/
    ├── balance_test.cpp          # 新增：平衡测试
    ├── migrate_test.cpp          # 新增：迁移测试
    └── coordinator_test.cpp      # 新增：协调器测试
```

## 9. 与现有代码的关系

### 9.1 复用现有代码

- `shard_router_t` — 分片路由器（已有）
- `shard_config_t` — 分片配置（已有）
- `shard_info_t` — 分片信息（已有）
- `shard_routing_t` — 路由表（已有）

### 9.2 新增代码

- 负载收集器（独立模块）
- 迁移管理器（独立模块）
- 分片协调器（核心调度）
- Executor 集成（分片扫描）

## 10. 关键设计决策

| 决策点 | 选择 | 理由 |
|--------|------|------|
| 再平衡触发 | 阈值触发 | 业界主流，MongoDB/TiDB/DynamoDB |
| 跨分片调度 | 最小负载 | 业界主流，动态响应 |
| 迁移策略 | 用户配置 + 自动选择 | Hash→vnode，Range/List→incremental |
| 与 Executor 集成 | ExecNode 路由层 | Gap#3 已完成 |
| 配置优先级 | CLI > 配置 > 默认值 | 灵活性优先 |

## 11. 扩展点

未来可扩展：
1. **地理分布** — 多数据中心感知调度
2. **资源配额** — per-tenant 配额限制
3. **预测性调度** — 基于历史数据的预测再平衡

## 12. 成功标准

- [ ] 分片协调器可工作
- [ ] 阈值检测正确（skew > threshold 触发）
- [ ] 负载收集器收集指标正确
- [ ] 最小负载调度工作
- [ ] 增量迁移实现
- [ ] 虚拟节点迁移实现
- [ ] Executor 集成（分片扫描）
- [ ] CLI 配置和监控命令
- [ ] 单元测试覆盖
- [ ] 集成测试通过
