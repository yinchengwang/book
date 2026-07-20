# 数据库 DevOps 工具横向对比

## 分类概述

数据库 DevOps 工具专注于数据库变更管理、Schema 版本控制、访问控制和合规审计。它们将数据库纳入 DevOps 流程，解决「数据库变更难回滚、权限难管控、变更难审计」等痛点，实现 Schema 即代码（Schema as Code）的理念。

## 库一览

- **Bytebase** - 开源数据库 CI/CD 平台，Schema 变更管理，GitOps 支持

## 核心功能

| 维度 | Bytebase | Liquibase | Flyway | Pt-osc |
|------|----------|-----------|--------|--------|
| 编程语言 | TypeScript | Java | Java | Perl |
| 开源协议 | Apache 2.0 | Apache 2.0 | Apache 2.0 | GPL |
| 首次发布 | 2022 | 2007 | 2010 | 2009 |
| GitHub Stars | 18k+ | 7k+ | 6k+ | 5k+ |
| SQL 审核 | 原生 | 有限 | 无 | 无 |
| GitOps | 原生 | 有限 | 有限 | 无 |
| 回滚 | 原生 | 支持 | 有限 | 支持 |
| 多数据库 | 广泛 | 广泛 | 有限 | MySQL |
| UI 控制台 | 完整 | 无 | 无 | CLI |
| 变更审批 | 原生 | 无 | 无 | 无 |
| 一句话定位 | 数据库 DevOps 全平台 | Schema 版本管理 | 轻量迁移工具 | MySQL 在线变更 |

## 数据库 DevOps 最佳实践

**Schema 版本控制**：
- SQL 迁移文件纳入 Git 版本控制
- 每次变更创建新迁移文件
- 迁移文件命名规范：`V{version}__{description}.sql`
- 支持回滚到任意版本

**CI/CD 集成**：
```
开发环境 → Git Push → CI 审核 → 预发布 → 生产发布
                ↓
          SQL Lint + 变更预览 + 审批流程
```

**数据库变更流程**：
1. **开发**：本地数据库测试迁移脚本
2. **审核**：提交 Pull Request，触发 SQL 审核规则
3. **审批**：数据库管理员审批变更
4. **执行**：灰度发布，支持暂停和回滚
5. **审计**：完整变更历史，可追溯

## 选型指南

- **全栈数据库 DevOps**：推荐 Bytebase（完整平台）
- **Java 项目**：推荐 Liquibase（生态成熟）
- **轻量级迁移**：推荐 Flyway（简单易用）
- **MySQL 在线变更**：推荐 pt-osc（无锁变更）

## 学习路径

1. **先学 Flyway/Liquibase** — 理解 Schema 版本控制概念
2. **再学 Bytebase** — 理解完整数据库 DevOps 流程
3. **理解 GitOps** — 数据库即代码实践

## 关联项目

- `db/storage/` — 项目存储引擎，变更管理相关
- `docs/storage-architecture.md` — 数据库变更流程在架构中的定位
