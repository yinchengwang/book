# OceanBase 应用场景

## 学习目标

- 掌握 OceanBase 的典型应用场景
- 理解 OceanBase 的场景选择依据
- 对比 OceanBase 与 TiDB、CockroachDB 的场景差异

## 典型应用场景

### 1. 金融核心交易

OceanBase 最初为蚂蚁集团"双 11"设计，适合金融级 OLTP 场景。

```mermaid
graph TB
    A[金融核心交易] --> B[高并发<br/>双 11 峰值]
    A --> C[强一致<br/>Paxos 强同步]
    A --> D[高可用<br/>故障自动切换]
    A --> E[数据安全<br/>多副本 + 备份]
```

**典型客户**：蚂蚁集团、网商银行、中国人寿

### 2. 分布式数据库替代

替代 MySQL 分库分表方案。

```mermaid
graph LR
    A[MySQL 分库分表] --> B[OceanBase 分布式]
    C[问题] --> D[手动分片复杂]
    C --> E[跨分片查询困难]
    C --> F[扩容影响业务]

    B --> G[自动分片]
    B --> H[分布式查询]
    B --> I[在线扩容]
```

### 3. HTAP 混合负载

同时支持 OLTP 和 OLAP 场景。

```sql
-- OLTP 查询（行存）
SELECT * FROM orders WHERE id = 123;

-- OLAP 查询（列存）
SELECT user_id, SUM(amount) FROM orders
WHERE order_date >= '2024-01-01'
GROUP BY user_id;
```

### 4. 跨地域部署

```mermaid
graph TB
    A[Region 1<br/>北京] --> B[主集群]
    C[Region 2<br/>上海] --> D[从集群]
    E[Region 3<br/>深圳] --> F[从集群]

    B --> C
    B --> E
```

## 场景选择决策树

```mermaid
flowchart TD
    A[需要分布式数据库] --> B{数据一致性要求}
    B -->|强一致| C{是否金融场景}
    B -->|最终一致| D[考虑其他方案]
    C -->|是| E[OceanBase 首选]
    C -->|否| F{是否需要列存}
    F -->|是| G[OceanBase / TiDB]
    F -->|否| H[OceanBase / CockroachDB]
```

## 场景对比

| 场景 | OceanBase | TiDB | CockroachDB |
|------|-----------|------|-------------|
| 金融核心交易 | 首选 | 次选 | 次选 |
| 分库分表替代 | 首选 | 首选 | 次选 |
| HTAP 混合负载 | 首选 | 首选 | 不支持 |
| 跨地域部署 | 支持 | 支持 | 支持 |
| MySQL 迁移 | 原生兼容 | 原生兼容 | 不兼容 |
| PostgreSQL 迁移 | 不兼容 | 不兼容 | 原生兼容 |
| 多租户 SaaS | 支持 | 支持 | 支持 |

## 与 PostgreSQL 场景对比

| 场景 | OceanBase | PostgreSQL |
|------|-----------|------------|
| 金融交易 | 原生支持 | 需扩展 |
| 数据分析 | 内置列存 | 需扩展 |
| 地理空间 | 不支持 | 原生支持 |
| 时序数据 | 可选 | 需扩展 |

## 要点总结

- OceanBase 最适合金融核心交易场景
- 替代 MySQL 分库分表方案
- HTAP 混合负载场景优势明显
- 跨地域部署支持良好
- 与 TiDB 场景重叠度高（MySQL 兼容生态）
- 与 CockroachDB 相比：金融场景首选

## 思考题

1. OceanBase 在金融场景中的核心优势是什么？双 11 场景对数据库提出了哪些挑战？
2. 如果业务是 MySQL 分库分表架构，迁移到 OceanBase 的收益和风险是什么？
3. OceanBase 的 HTAP 能力与 TiDB 的 TiFlash 相比，在性能和易用性上有何差异？