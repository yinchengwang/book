# SurrealDB 与项目关联

## 学习目标

- 理解 SurrealDB 与本项目的关系
- 掌握多模态设计思路在项目中的应用

## 架构对比

| 维度 | SurrealDB | 本项目 mm_storage |
|------|-----------|-------------------|
| 数据模型 | 文档+图+KV | KV+Vector+Timeseries+Document+Spatial+Graph+Yang |
| 存储引擎 | RocksDB/TiKV/PG | 自研 Buffer Pool + WAL |
| 查询语言 | SurrealQL | SQL + 扩展 |
| 部署模式 | 嵌入式+服务端 | 服务端 |

## 可借鉴的设计点

| 借鉴点 | SurrealDB 实现 | 项目应用 |
|--------|---------------|----------|
| 多模态统一接口 | SurrealQL | SQL 扩展支持多模型 |
| 图遍历语法 | -> 箭头操作符 | 添加图遍历语法 |
| 实时订阅 | WebSocket 推送 | 实现事件通知机制 |

## 要点总结

- 多模态数据模型设计思路一致
- 图遍历语法可借鉴到 SQL 扩展
- 实时订阅机制值得实现

## 思考题

1. 项目如何实现 SQL 语法扩展支持图遍历？
2. mm_storage 的 Graph 引擎与 SurrealDB 的图模型有何异同？
