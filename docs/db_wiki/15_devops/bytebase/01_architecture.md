# Bytebase 架构设计

## 学习目标
- 理解 Bytebase 的整体架构分层
- 掌握 SQL 审核流程与变更管理流水线

## 整体架构

```mermaid
graph TB
    subgraph "前端层"
        CONSOLE["Web Console<br/>Vue.js"]
        IDE["IDE 插件<br/>VSCode / JetBrains"]
        CLI["bb CLI<br/>命令行工具"]
    end

    subgraph "后端服务"
        SERVER["Backend Server<br/>Go"]
        API["REST API"]
        GRPC["gRPC"]
    end

    subgraph "任务执行"
        RUNNER["Runner<br/>任务执行器"]
        MIGRATOR["Migrator<br/>迁移执行"]
        BACKUP["Backup<br/>备份管理"]
    end

    subgraph "数据存储"
        DB_PG["PostgreSQL<br/>元数据"]
        GIT["Git<br/>VCS 仓库"]
    end

    subgraph "目标数据库"
        MYSQL["MySQL"]
        PG["PostgreSQL"]
        TIDB["TiDB"]
        SNOWFLAKE["Snowflake"]
        REDSHIFT["Redshift"]
        OCEANBASE["OceanBase"]
        MONGODB["MongoDB"]
    end

    CONSOLE --> SERVER
    IDE --> SERVER
    CLI --> SERVER
    SERVER --> API
    SERVER --> GRPC
    API --> RUNNER
    GRPC --> RUNNER
    RUNNER --> MIGRATOR
    RUNNER --> BACKUP
    MIGRATOR --> MYSQL
    MIGRATOR --> PG
    MIGRATOR --> TIDB
    MIGRATOR --> SNOWFLAKE
    MIGRATOR --> REDSHIFT
    MIGRATOR --> OCEANBASE
    MIGRATOR --> MONGODB
    SERVER --> DB_PG
    SERVER --> GIT
```

## SQL 审核流程

```mermaid
sequenceDiagram
    participant D as 开发者
    participant V as VCS(Git)
    participant B as Bytebase
    participant R as Reviewer
    participant DB as 数据库

    D->>V: 推送 SQL 变更
    V->>B: Webhook 触发
    B->>B: 语法检查
    B->>B: SQL Review 规则检查
    B->>B: 生成变更计划
    B->>R: 发送审批请求
    R->>B: 批准变更
    B->>DB: 自动备份
    B->>DB: 执行 DDL/DML
    DB-->>B: 执行结果
    B->>V: 回写状态
    B->>D: 通知完成
```

## 变更管理 Pipeline

```mermaid
graph LR
    A[Issue] --> B[Task]
    B --> C[Stage]
    C --> D[Run]
    D --> E{成功?}
    E -->|是| F[Done]
    E -->|否| G[Rollback]
    G --> H[Failed]

    style A fill:#e1f5fe
    style F fill:#e8f5e9
    style H fill:#ffebee
```

### 核心概念

| 概念 | 说明 |
|------|------|
| Issue | 变更需求，由开发者创建 |
| Task | 具体的数据库变更任务 |
| Stage | 任务在特定环境中的执行阶段 |
| Run | 一次实际的执行动作 |
| Rollback | 失败后自动回滚 |

## VCS 集成

```mermaid
flowchart TD
    A[Git Push] --> B[Webhook]
    B --> C[Bytebase 接收]
    C --> D{识别变更类型}
    D -->|迁移文件| E[创建 Issue]
    D -->|配置文件| F[更新配置]
    E --> G[触发审核流程]
    F --> G
    G --> H[执行变更]
    H --> I[回写 Commit Status]
    I --> J[PR 合并]
```

## 目录结构

```
bytebase/
├── backend/                 # Go 后端服务
│   ├── server/              # HTTP/gRPC 服务
│   ├── plugin/              # 数据库驱动插件
│   │   ├── db/              # 数据库连接
│   │   └── parser/          # SQL 解析器
│   ├── api/                 # API 定义
│   └── component/           # 业务组件
├── frontend/                # Vue.js 前端
│   ├── src/
│   └── public/
├── runner/                  # 任务执行器
├── migrator/                # 迁移工具
├── scripts/                 # 部署脚本
└── proto/                   # Protobuf 定义
```

## 要点总结

- 架构分为前端层（Console/IDE/CLI）、后端服务、任务执行、数据存储四层
- SQL 审核流程：语法检查 → SQL Review → 备份 → 执行 → 回滚（失败时）
- 变更管理 Pipeline：Issue → Task → Stage → Run → Done/Failed
- VCS 集成支持 GitOps，PR 触发审核，执行结果回写 Git

## 思考题

1. Bytebase 为什么采用 Issue-Task-Stage-Run 的四级结构？
2. VCS 集成的 GitOps 模式相比手动提交 SQL 有哪些优势？
3. 自动备份和回滚机制在生产环境中有哪些边界情况需要考虑？