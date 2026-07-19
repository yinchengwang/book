# TiDB 使用场景

## 学习目标

- 掌握 TiDB 的典型使用场景
- 理解 TiDB 在不同场景下的架构决策
- 对比 TiDB 与 CockroachDB 的场景选择

## 使用场景

### 1. 分库分表替代

```mermaid
graph LR
    A[MySQL 分库分表] --> B[应用层分片<br/>ShardingSphere]
    B --> C[多实例运维<br/>复杂]
    B --> D[跨库事务<br/>困难]

    E[TiDB 替代] --> F[透明分片<br/>自动路由]
    F --> G[统一运维<br/>简单]
    F --> H[分布式事务<br/>天然支持]

    I[迁移路径] --> J[直接迁移<br/>MySQL 协议兼容]
```

**场景特点**：

- 单表数据量超过 10TB
- 需要跨分片 JOIN 查询
- 需要分布式事务支持
- 分库分表中间件运维复杂

### 2. 高并发 OLTP 业务

```mermaid
graph TB
    A[高并发 OLTP] --> B[电商交易系统]
    A --> C[金融支付系统]
    A --> D[社交平台用户]

    B --> E[订单写入<br/>高并发写入]
    B --> F[库存查询<br/>频繁读取]

    C --> G[账户余额<br/>一致性要求高]
    C --> H[交易记录<br/>持久化要求]

    D --> I[用户信息<br/>水平扩展]
    D --> J[关系数据<br/>图查询]
```

**场景特点**：

- 高并发写入（10K+ TPS）
- 数据一致性要求高
- 需要水平扩展能力
- 读取延迟要求低（< 10ms）

### 3. HTAP 混合负载

```mermaid
graph TB
    A[HTAP 场景] --> B[实时报表系统]
    A --> C[实时风控系统]
    A --> D[实时推荐系统]

    B --> E[TiKV 行存<br/>实时写入]
    B --> F[TiFlash 列存<br/>实时分析]

    C --> G[交易写入<br/>TiKV 事务]
    C --> H[规则分析<br/>TiFlash 扫描]

    D --> I[用户行为<br/>TiKV 写入]
    D --> J[推荐计算<br/>TiFlash 聚合]
```

**场景特点**：

- 需要实时写入和实时分析
- 不想维护两套系统（OLTP + OLAP）
- 数据新鲜度要求高（秒级延迟）

### 4. 云原生部署

```mermaid
graph TB
    A[云原生部署] --> B[K8s 集群<br/>TiDB Operator]
    B --> C[TiDB Server<br/>Deployment]
    B --> D[TiKV<br/>StatefulSet]
    B --> E[PD<br/>StatefulSet]
    B --> F[TiFlash<br/>StatefulSet]

    G[特性] --> H[自动扩缩容]
    G --> I[故障自愈]
    G --> J[滚动升级]
    G --> K[监控集成]
```

**场景特点**：

- 基础设施云原生（K8s）
- 需要弹性扩缩容
- 需要自动化运维

## 场景决策树

```mermaid
graph TB
    A[是否 MySQL 兼容？] -->|是| B[TiDB 候选]
    A -->|否| C[考虑其他数据库]

    B --> D{数据量 > 1TB？}
    D -->|是| E{需要分布式事务？}
    D -->|否| F[MySQL 即可]

    E -->|是| G{需要实时分析？}
    E -->|否| H[TiDB 或 CockroachDB]

    G -->|是| I[TiDB + TiFlash<br/>HTAP]
    G -->|否| J[TiDB<br/>仅 OLTP]

    C --> K{需要 PG 兼容？}
    K -->|是| L[CockroachDB]
    K -->|否| M[按需求选择]
```

## 与 CockroachDB 场景对比

| 场景 | TiDB | CockroachDB |
|------|------|------------|
| MySQL 分库分表替代 | 首选（MySQL 兼容） | 不适用（PG 兼容） |
| 全球多活部署 | 支持（但不如 CRDB 成熟） | 首选（HLC 分布时钟） |
| HTAP 场景 | 首选（TiFlash 列存） | 不适用（无 HTAP） |
| 纯 OLTP 高并发 | 支持 | 支持 |
| 实时分析 | 支持（TiFlash） | 不适用 |
| 云原生部署 | 支持（K8s Operator） | 支持（K8s Operator） |

### 场景选择建议

**选择 TiDB 的场景**：

- 现有 MySQL 应用需要分布式扩展
- 分库分表中间件运维困难
- 需要 HTAP 能力（实时分析）
- 需要计算/存储独立扩展

**选择 CockroachDB 的场景**：

- 现有 PostgreSQL 应用需要分布式扩展
- 需要全球多活部署
- 纯 OLTP 场景，无 HTAP 需求
- 需要完全去中心化架构

## 实际案例

### 案例 1：电商平台分库分表迁移

**背景**：某电商平台 MySQL 分库分表（128 个分片），跨库 JOIN 困难。

**迁移方案**：

```sql
-- 1. 使用 TiDB Dumpling 导出数据
dumpling -h mysql_host -P 3306 -u root -p \
  -B mydb -F 256MB -o /data/dump

-- 2. 使用 TiDB Lightning 导入
tidb-lightning -config tidb-lightning.toml
```

**效果**：

- 性能提升：跨分片查询从 30s 降到 1s
- 运维简化：从 128 个实例降到 3 个节点
- 扩展灵活：按需扩容计算节点或存储节点

### 案例 2：实时报表系统

**背景**：某金融科技公司需要实时交易报表，之前用 MySQL + Elasticsearch 双写。

**迁移方案**：

```sql
-- 创建 TiFlash 副本
ALTER TABLE transactions SET TIFLASH REPLICA 1;
```

**效果**：

- 数据新鲜度：从 5 分钟降到 1 秒
- 架构简化：从双写降到单系统
- 查询性能：复杂聚合查询提升 10x

## 要点总结

- TiDB 适合 MySQL 分库分表替代、高并发 OLTP、HTAP 混合负载、云原生部署
- 场景选择：现有 MySQL 应用首选 TiDB，HTAP 需求首选 TiDB
- 与 CockroachDB 对比：MySQL vs PG 兼容，HTAP vs 不能
- 实际案例：电商分库分表迁移，金融实时报表系统

## 思考题

1. 如果现有系统使用 MySQL 且数据量在 1TB 以内，是否有必要迁移到 TiDB？迁移成本是否值得？
2. TiDB 的 HTAP 能力（TiFlash）相比传统 Lambda 架构（MySQL + ClickHouse），在数据一致性和架构复杂度上有何优势？
3. 在云原生场景下，TiDB Operator 相比 CockroachDB Operator 在部署和管理上有何差异？
4. 如果应用场景同时需要 MySQL 兼容和全球多活部署，TiDB 和 CockroachDB 哪个更适合？