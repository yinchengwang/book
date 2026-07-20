# 文档数据库横向对比

## 分类概述

文档数据库以 JSON/BSON 文档形式存储数据，schema-less 特性使其灵活适应业务变化。每条记录是可独立查询的文档，支持嵌套结构，特别适合内容管理、产品目录、用户配置等半结构化数据场景。

## 库一览

- **Appwrite** - 开源后端即服务平台，文档存储 + 认证 + 函数
- **FerretDB** - MongoDB 开源替代，PostgreSQL 协议，MongoDB 兼容
- **NocoDB** - 开源 Airtable 替代，电子表格即数据库，MySQL/PG/SQLite

## 功能对比表

| 维度 | Appwrite | FerretDB | NocoDB |
|------|----------|----------|--------|
| 编程语言 | Dart/TypeScript | Go | TypeScript |
| 开源协议 | GPLv3 | Apache 2.0 | GPLv3 |
| 首次发布 | 2019 | 2022 | 2020 |
| GitHub Stars | 45k+ | 8k+ | 38k+ |
| 协议兼容 | REST/SDK | MongoDB Wire | REST/GraphQL |
| 查询语言 | Appwrite SDK | MongoDB Query | SQL/No-code |
| Schema | 固定/灵活 | Schema-less | 动态 |
| 存储后端 | 可扩展 | PostgreSQL | MySQL/PG/SQLite |
| API | REST | MongoDB Protocol | REST/GraphQL |
| 生态 | 认证/存储/函数 | MongoDB 驱动 | 表格 UI/协作 |
| 一句话定位 | 开源后端即服务平台 | MongoDB PostgreSQL 替代 | 表格化无代码数据库 |

## 选型指南

- **后端即服务需求**：推荐 Appwrite（完整 BaaS 平台）
- **MongoDB 迁移**：推荐 FerretDB（PG 后端、协议兼容）
- **无代码/协作表格**：推荐 NocoDB（电子表格界面）
- **轻量级文档存储**：推荐 FerretDB（MongoDB 生态）

## 学习路径

1. **先学 NocoDB** — 最直观，表格界面易于理解
2. **再学 FerretDB** — 理解 MongoDB 协议和 PG 存储结合
3. **然后学 Appwrite** — 理解完整 BaaS 架构

## 关联项目

- `doc_engine` — 项目多模态存储引擎中的文档模块
- `docs/storage-architecture.md` — 文档存储在多模态架构中的定位
