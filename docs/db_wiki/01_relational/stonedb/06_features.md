# 关键特性

## 学习目标

- 理解 StoneDB 相比 MySQL 的核心增强特性
- 掌握 Tianmu 引擎的关键技术优势

## HTAP 混合负载

StoneDB 的核心定位是 HTAP——同时处理 OLTP 和 OLAP 负载：

```mermaid
graph LR
    subgraph "StoneDB HTAP"
        OLTP[OLTP 事务型<br/>InnoDB 引擎] --> APP[应用]
        OLAP[OLAP 分析型<br/>Tianmu 引擎] --> APP
    end

    OLTP --> TP[高并发小事务<br/>行级锁/MVCC]
    OLAP --> AP[大查询分析<br/>列存/知识网格]

    APP --> R[同一数据库<br/>无 ETL 延迟]
```

### HTAP 的优势

| 维度 | 传统方案（MySQL + ETL + 分析库） | StoneDB 方案 |
|------|----------------------------------|-------------|
| 数据延迟 | ETL 过程有时间差（分钟级） | 实时，写后即可查 |
| 运维复杂度 | 维护两套系统 | 一套系统 |
| 存储冗余 | 两份数据 | 一份数据 |
| 查询一致性 | 无法保证 | 同一数据的实时视图 |

## 知识网格

知识网格是 Tianmu 引擎最核心的特性：

```mermaid
graph TD
    KG[知识网格] --> DPN[Data Pack Node<br/>MIN/MAX/SUM/COUNT]
    KG --> KN[Knowledge Node<br/>直方图/CMAP/P2P]
    KG --> GRID[两级级联过滤]

    DPN --> L1[第一层过滤<br/>DP 级别]
    KN --> L2[第二层过滤<br/>细粒度]
    GRID --> RES[保留候选 DP < 1%]
```

## 列式存储与高压缩比

```mermaid
graph LR
    RAW[原始数据] -->|列存| COL[按列组织]
    COL -->|同类型| COMP[密集压缩]
    COMP -->|20+ 算法| RESULT[10:1 ~ 40:1]

    RAW -->|行存| ROW[按行组织]
    ROW -->|混合类型| R_COMP[通用压缩]
    R_COMP --> R_RESULT[2:1 ~ 5:1]
```

## MySQL 全兼容

```mermaid
graph TD
    COMP[MySQL 兼容性] --> PROTO[协议兼容<br/>MySQL Wire Protocol]
    COMP --> SQL[SQL 语法兼容<br/>DML/DDL/存储过程]
    COMP --> ECO[生态工具兼容<br/>Navicat/Workbench/mysqldump]
    COMP --> API[API 兼容<br/>JDBC/ODBC/C/PHP/Python]
    COMP --> MIG[迁移零修改<br/>不改代码切换]

    PROTO --> APP[现有应用<br/>无缝连接]
```

## LOAD DATA 高速导入

Tianmu 引擎针对批量导入做了深度优化：

```mermaid
flowchart TD
    LOAD[LOAD DATA INFILE] --> PARSE[解析数据]
    PARSE --> BATCH[分批处理<br/>65536 行一批]
    BATCH --> BUILD[构建 DP + DPN]
    BUILD --> COMPRESS[压缩 DP]
    COMPRESS --> WRITE[写入列文件]
    WRITE --> KG_KEEP[更新知识网格]
```

性能对比：

| 数据量 | MySQL InnoDB 导入 | StoneDB Tianmu 导入 |
|--------|------------------|--------------------|
| 1GB | ~30s | ~3s |
| 10GB | ~5min | ~30s |
| 100GB | ~50min | ~5min |

## 双引擎可切换

用户可随时指定表的存储引擎：

```sql
-- 创建行存表（OLTP）
CREATE TABLE users (
    id INT PRIMARY KEY,
    name VARCHAR(100)
) ENGINE=InnoDB;

-- 创建列存表（OLAP）
CREATE TABLE orders (
    id INT,
    amount DECIMAL(10,2),
    city VARCHAR(50)
) ENGINE=Tianmu;

-- 转换引擎
ALTER TABLE orders ENGINE=InnoDB;
```

## 特性全景

```mermaid
graph TB
    F[StoneDB 特性] --> F1[HTAP 混合负载]
    F --> F2[知识网格过滤]
    F --> F3[列存高压缩]
    F --> F4[MySQL 全兼容]
    F --> F5[高速批量导入]
    F --> F6[双引擎切换]
    F --> F7[20+ 压缩算法]
    F --> F8[粗糙集过滤]

    F1 --> F1A[OLTP + OLAP 同一库]
    F2 --> F2A[DPN + 知识节点]
    F3 --> F3A[40:1 压缩比]
    F4 --> F4A[协议/SQL/工具全兼容]
    F5 --> F5A[LOAD DATA 10x 加速]
```

## 要点总结

- StoneDB 是 HTAP 数据库，同一实例处理 OLTP 和 OLAP 负载
- 知识网格是核心创新：两级元数据过滤，减少 I/O
- 列存 + 20+ 压缩算法实现 10:1 ~ 40:1 压缩比
- MySQL 全兼容，应用迁移零修改
- LOAD DATA 批量导入比 InnoDB 快 10 倍

## 思考题

1. StoneDB 的 HTAP 方案和"MySQL + ETL + ClickHouse"方案相比，各自优缺点是什么？
2. 知识网格的过滤效率取决于统计信息的精度。如果数据分布极度不均匀，过滤效果会如何？
3. 列存引擎在 INSERT/UPDATE 频繁的场景下，相比行存有哪些劣势？StoneDB 如何通过双引擎缓解？