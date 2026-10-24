# Reference 目录模型化重构

## 变更目标

将 `reference/` 从平铺的 `open-source/` 目录迁移到按数据模型分类的目录，并补充主流与技术特色开源数据库参考项目。

## 变更范围

- 新增 14 个模型目录（relational, distributed-sql, key-value, document, columnar, time-series, search, vector, graph, stream, embedded, extension, benchmark）
- 迁移现有子模块路径（7 个已克隆：faiss, redis, pgvector, postgres, sqlite3, chroma, neo4j）
- 补充新项目元数据清单（catalog.yml 共 74 个项目）
- 更新仓库文档与引用路径
- 建立分阶段克隆指引（fetch-manifest.txt）

## 执行记录

- 2026-08-27：完成目录结构、元数据规范、审核表、子模块迁移、文档更新
- 5 个子模块因网络限制暂未克隆（elasticsearch, milvus, ann-benchmarks, openGauss, mysql）
