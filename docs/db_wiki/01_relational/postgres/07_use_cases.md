# 使用场景与选型

## 学习目标

- 掌握 PostgreSQL 的核心适用场景
- 理解 PG 与 MySQL、MongoDB、Redis、Elasticsearch 的选型差异
- 熟悉不同场景下的 PG 配置与优化建议

## 核心概念

- **OLTP（Online Transaction Processing）**：在线事务处理，强事务、高并发
- **OLAP（Online Analytical Processing）**：在线分析处理，复杂查询、聚合
- **GIS（Geographic Information System）**：地理信息系统
- **全文检索**：文本搜索、模糊匹配
- **时序数据**：时间序列，按时间分区、聚合
- **数据中台**：数据集成、数据仓库

## 适用场景总览

```mermaid
graph TD
    A[PostgreSQL 适用场景] --> B[复杂 OLTP]
    A --> C[GIS 地理信息]
    A --> D[全文检索]
    A --> E[时序数据]
    A --> F[数据仓库]
    A --> G[JSON 文档]
    A --> H[多租户 SaaS]

    B --> I[强事务、复杂 JOIN]
    C --> J[PostGIS 扩展]
    D --> K[GIN 索引 + tsvector]
    E --> L[TimescaleDB 扩展]
    F --> M[窗口函数、CTE]
    G --> N[JSONB + GIN]
    H --> O[Row Level Security]
```

## 场景 1：复杂 OLTP

PG 在复杂 OLTP 场景表现出色：

```mermaid
graph TD
    A[OLTP 场景特点] --> B[强事务 ACID]
    A --> C[复杂 JOIN]
    A --> D[外键约束]
    A --> E[多表关联]

    F[PG 优势] --> G[完整 MVCC]
    F --> H[成熟的 JOIN 优化]
    F --> I[外键级联删除]
    F --> J[CTE/窗口函数]
```

**典型业务**：

- ERP 系统（财务、库存、订单）
- CRM 系统（客户管理、销售）
- 订单系统（订单、支付、退款）

**配置建议**：

```ini
shared_buffers = 4GB
work_mem = 64MB
maintenance_work_mem = 1GB
effective_cache_size = 12GB
max_connections = 200
```

## 场景 2：GIS 地理信息

通过 PostGIS 扩展，PG 成为最强大的开源 GIS 数据库：

```mermaid
graph TD
    A[PostGIS 能力] --> B[几何类型<br/>Point/Linestring/Polygon]
    A --> C[空间索引<br/>GiST]
    A --> D[空间函数<br/>距离/包含/相交]
    A --> E[栅格支持<br/>Raster]

    F[应用场景] --> G[位置服务 LBS]
    F --> H[地图应用]
    F --> I[物流路径规划]
    F --> J[地理围栏]
```

**示例**：

```sql
-- 创建 PostGIS 扩展
CREATE EXTENSION postgis;

-- 创建地理列
ALTER TABLE locations ADD COLUMN geom GEOMETRY(Point, 4326);
CREATE INDEX idx_geom ON locations USING gist(geom);

-- 查询附近 5km 的点
SELECT * FROM locations
WHERE ST_DWithin(
    geom,
    ST_MakePoint(116.4, 39.9)::GEOMETRY(Point, 4326),
    5000
);
```

## 场景 3：全文检索

PG 内置全文检索能力：

```mermaid
graph TD
    A[全文检索能力] --> B[tsvector 分词]
    A --> C[tsquery 查询]
    A --> D[GIN 索引]
    A --> E[多语言支持]

    F[应用场景] --> G[文档搜索]
    F --> H[商品搜索]
    F --> I[日志检索]
    F --> J[评论搜索]
```

**示例**：

```sql
-- 创建全文索引
CREATE INDEX idx_content_fts ON documents USING gin(to_tsvector('english', content));

-- 搜索包含 'postgresql' 和 'database' 的文档
SELECT * FROM documents
WHERE to_tsvector('english', content) @@ to_tsquery('english', 'postgresql & database');

-- 模糊搜索
SELECT * FROM documents
WHERE to_tsvector('english', content) @@ to_tsquery('english', 'post:*');
```

## 场景 4：时序数据

通过 TimescaleDB 扩展，PG 成为时序数据库：

```mermaid
graph TD
    A[TimescaleDB 特性] --> B[超级表 Hypertable]
    A --> C[自动分区]
    A --> D[压缩策略]
    A --> E[保留策略]
    A --> F[连续聚合]

    G[应用场景] --> H[IoT 传感器]
    G --> I[监控指标]
    G --> J[金融 tick 数据]
    G --> K[事件流]
```

**示例**：

```sql
-- 创建 TimescaleDB 扩展
CREATE EXTENSION timescaledb;

-- 创建超级表
CREATE TABLE sensor_data (
    time        TIMESTAMP NOT NULL,
    sensor_id   INT,
    temperature DOUBLE PRECISION
);

SELECT create_hypertable('sensor_data', 'time');

-- 连续聚合（实时物化视图）
CREATE MATERIALIZED VIEW sensor_hourly
    WITH (timescaledb.continuous) AS
SELECT sensor_id,
       time_bucket('1 hour', time) AS bucket,
       AVG(temperature) AS avg_temp
FROM sensor_data
GROUP BY sensor_id, bucket;

-- 自动压缩策略
SELECT add_compression_policy('sensor_data', INTERVAL '7 days');
```

## 场景 5：数据仓库

PG 的分析能力适合中小规模数据仓库：

```mermaid
graph TD
    A[数据仓库能力] --> B[窗口函数]
    A --> C[CTE 递归查询]
    A --> D[聚合函数]
    A --> E[物化视图]
    A --> F[分区表]

    G[适用规模] --> H[千万级 ~ 亿级]
    G --> I[超亿级考虑 ClickHouse]

    J[优化建议] --> K[大 work_mem]
    J --> L[并行查询]
    J --> M[BRIN 索引]
```

**示例**：

```sql
-- 窗口函数：计算移动平均
SELECT
    date,
    revenue,
    AVG(revenue) OVER (ORDER BY date ROWS BETWEEN 6 PRECEDING AND CURRENT ROW) AS moving_avg_7d
FROM daily_revenue;

-- CTE 递归：层级查询
WITH RECURSIVE org_tree AS (
    SELECT id, name, parent_id, 1 AS level
    FROM employees WHERE parent_id IS NULL
    UNION ALL
    SELECT e.id, e.name, e.parent_id, t.level + 1
    FROM employees e
    JOIN org_tree t ON e.parent_id = t.id
)
SELECT * FROM org_tree;
```

## 与竞品对比

### PostgreSQL vs MySQL

```mermaid
graph TD
    A[对比维度] --> B[架构]
    A --> C[事务]
    A --> D[索引]
    A --> E[复制]

    B --> B1[PG: 单引擎深度优化]
    B --> B2[MySQL: 多引擎可插拔]

    C --> C1[PG: MVCC + VACUUM]
    C --> C2[MySQL: InnoDB undo log]

    D --> D1[PG: 多种索引类型]
    D --> D2[MySQL: B+Tree + Hash]

    E --> E1[PG: 流复制 + 逻辑复制]
    E --> E2[MySQL: 主从复制 + Group Replication]
```

| 维度 | PostgreSQL | MySQL |
|------|-----------|-------|
| 默认隔离级别 | Read Committed | Repeatable Read |
| MVCC | 多版本 + VACUUM | Undo log + purge |
| JSON | JSONB（二进制） | JSON（文本） |
| GIS | PostGIS（强） | 空间函数（弱） |
| 全文检索 | 内置 GIN | 全文索引（弱） |
| 窗口函数 | 完善 | 8.0+ 支持 |
| 复制 | 流复制 + 逻辑复制 | 主从 + Group Replication |
| 集群 | 无原生（需 Patroni） | InnoDB Cluster |

### PostgreSQL vs MongoDB

```mermaid
graph TD
    A[PG vs MongoDB] --> B[数据模型]
    B --> B1[PG: 关系 + JSONB]
    B --> B2[MongoDB: 纯文档]

    C[事务] --> C1[PG: 完整 ACID]
    C --> C2[MongoDB: 4.0+ 支持（弱）]

    D[查询] --> D1[PG: SQL + JOIN]
    D --> D2[MongoDB: 聚合管道]

    E[适用场景] --> E1[PG: 混合模型]
    E --> E2[MongoDB: 纯文档、无 schema]
```

### PostgreSQL vs Redis

```mermaid
graph TD
    A[PG vs Redis] --> B[定位]
    B --> B1[PG: 持久化数据库]
    B --> B2[Redis: 内存缓存/KV]

    C[性能] --> C1[PG: 磁盘 IO 级]
    C --> C2[Redis: 内存级微秒]

    D[数据结构] --> D1[PG: 表 + 索引]
    D --> D2[Redis: String/List/Hash/Set/ZSet]

    E[适用场景] --> E1[PG: 持久化存储]
    E --> E2[Redis: 缓存/会话/排行榜]
```

### PostgreSQL vs Elasticsearch

```mermaid
graph TD
    A[PG vs ES] --> B[全文检索]
    B --> B1[PG: GIN + tsvector]
    B --> B2[ES: 倒排索引 + 分布式]

    C[事务] --> C1[PG: 完整 ACID]
    C --> C2[ES: 无事务]

    D[实时性] --> D1[PG: 实时]
    D --> D2[ES: 近实时 1s]

    E[适用场景] --> E1[PG: 中小规模全文]
    E --> E2[ES: 大规模分布式检索]
```

## 选型决策树

```mermaid
flowchart TD
    A[选型起点] --> B{需要强事务?}
    B -->|是| C{复杂 JOIN?}
    B -->|否| D{纯文档模型?}

    C -->|是| E[PostgreSQL]
    C -->|否| F{读多写少?}

    F -->|是| G[MySQL]
    F -->|否| H{需要分布式?}

    D -->|是| I[MongoDB]
    D -->|否| J{需要全文检索?}

    H -->|是| K[TiDB / CockroachDB]
    H -->|否| G

    J -->|大规模| L[Elasticsearch]
    J -->|中小规模| E

    M{GIS 场景?} -->|是| E
    M -->|否| N{时序数据?}

    N -->|是| O[TimescaleDB / QuestDB]
    N -->|否| P{缓存场景?}

    P -->|是| Q[Redis]
    P -->|否| E
```

## 典型案例

### 案例 1：电商订单系统

```mermaid
graph LR
    A[用户下单] --> B[订单表]
    B --> C[库存扣减]
    C --> D[支付记录]
    D --> E[物流跟踪]

    F[PG 特性应用] --> G[事务: ACID]
    F --> H[外键: 级联删除]
    F --> I[索引: BTree + GIN]
```

### 案例 2：物联网平台

```mermaid
graph TD
    A[传感器上报] --> B[时序表 TimescaleDB]
    B --> C[实时聚合]
    C --> D[告警触发]

    E[PG 特性应用] --> F[超级表: 自动分区]
    E --> G[连续聚合: 物化视图]
    E --> H[压缩策略: 节省空间]
```

### 案例 3：内容管理系统

```mermaid
graph TD
    A[文章发布] --> B[文章表]
    B --> C[全文索引]
    C --> D[用户搜索]

    E[PG 特性应用] --> F[JSONB: 灵活属性]
    E --> G[GIN: 全文检索]
    E --> H[Row Level Security: 多租户]
```

## 要点总结

- PG 适合**复杂 OLTP、GIS、全文检索、时序数据、数据仓库**
- 与 MySQL 相比，PG 在 SQL 兼容性、GIS、全文检索上更强
- 与 MongoDB 相比，PG 支持 ACID + JSONB 混合模型
- 与 Redis 相比，PG 是持久化存储，Redis 是缓存
- 与 ES 相比，PG 适合中小规模全文检索，ES 适合大规模分布式

## 思考题

1. 为什么 PG 的 JSONB 比 MySQL 的 JSON 更适合存储 JSON 文档？
2. TimescaleDB 在什么数据规模下比 InfluxDB 更有优势？
3. 如果业务需要"强事务 + 分布式"，应该选择 PG + Patroni 还是 TiDB？各有什么权衡？