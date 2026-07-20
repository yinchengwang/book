# InfluxDB 架构设计

## 学习目标

- 理解 InfluxDB 的 TSM 存储引擎
- 掌握 InfluxDB 的写入和查询流程

## TSM 存储引擎

```mermaid
graph TB
    subgraph "写入路径"
        W[写入请求] --> CM[Cache<br/>内存缓存]
        CM --> WBL[WAL<br/>预写日志]
        WBL --> TSM[TSM Files<br/>SSTable]
        TSM --> CM2[定期快照]
    end

    subgraph "TSM File 结构"
        T1[Header]
        T2[Data Blocks<br/>压缩数据]
        T3[Index Block<br/>索引]
        T4[Footer]
    end

    CM --> TSM
    TSM --> T1
```

## TSM vs LSM-Tree

| 特性 | LSM-Tree | TSM |
|------|----------|-----|
| 索引 | 行键索引 | Series Key 索引 |
| 数据块 | 按行组织 | 按时间块组织 |
| 压缩 | 通用压缩 | 时间序列专用压缩 |
| 删除 | 墓碑标记 | 批处理删除 |

## 查询执行

```mermaid
graph LR
    Q[查询] --> P[解析计划]
    P --> TS[Tag Search<br/>倒排索引]
    P --> FS[Field Search<br/>数据扫描]
    TS -->|Series Keys| FS
    FS --> A[聚合/计算]
    A --> R[结果]
```

## 连续查询

```sql
-- 创建连续查询
CREATE CONTINUOUS QUERY "cpu_avg" ON "mydb"
BEGIN
    SELECT mean(cpu) INTO "cpu_avg"
    FROM "cpu"
    GROUP BY time(1m), host
END;

-- 效果：每分钟自动计算平均值
```

## 要点总结

- TSM 是 LSM-Tree 的时序优化变体
- Cache + WAL 保证写入持久性
- 倒排索引加速 Tag 查询
- 连续查询自动数据聚合