# 时序数据库学习资源

## 官方资源

| 数据库 | GitHub | 文档 |
|--------|--------|------|
| TimescaleDB | [timescale/timescaledb](https://github.com/timescale/timescaledb) | [docs.timescale.com](https://docs.timescale.com/) |
| InfluxDB | [influxdata/influxdb](https://github.com/influxdata/influxdb) | [docs.influxdata.com](https://docs.influxdata.com/influxdb/) |
| QuestDB | [questdb/questdb](https://github.com/questdb/questdb) | [questdb.io/docs](https://questdb.io/docs/) |
| GreptimeDB | [GreptimeTeam/greptimedb](https://github.com/GreptimeTeam/greptimedb) | [docs.greptime.com](https://docs.greptime.com/) |
| VictoriaMetrics | [VictoriaMetrics/VictoriaMetrics](https://github.com/VictoriaMetrics/VictoriaMetrics) | [docs.victoriametrics.com](https://docs.victoriametrics.com/) |

## 源码阅读路径

```
TimescaleDB:
src/                    # C 扩展
├── chunk.c/h           # Chunk 管理
├── hypertable.c/h      # Hypertable
├── compression.c/h     # 压缩
├── continuous_agg.c/h  # 连续聚合
└── process.c/h         # 触发器

InfluxDB:
tsdb/                   # 存储引擎
├── tsm/                # TSM 存储
├── models/             # 数据模型
├── query/              # 查询引擎
└── coordinator/        # 集群协调

QuestDB:
core/src/main/java/io/questdb/
├── griffin/            # SQL 引擎
├── cas/                # 并发
├── store/              # 存储
└── csv/                # CSV 导入

VictoriaMetrics:
lib/storage/            # 存储
lib/storage/index/      # 倒排索引
lib/storage/series/     # 时序索引
```

## 推荐阅读

| 资源 | 说明 |
|------|------|
| TSM 论文 | InfluxDB 存储引擎设计 |
| TimescaleDB 论文 | Hypertable 分区设计 |
| VictoriaMetrics 架构 | 高性能时序存储 |
| PromQL 教程 | 监控系统查询语言 |

## 要点总结

- 官方文档是最佳学习资源
- 源码阅读从存储引擎开始
- 时序压缩算法是核心
- PromQL 是监控系统通用语言