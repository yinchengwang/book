# DailyDigest — 每日数据库 & AI 技术速览

## 概述

DailyDigest 是一个**每日自动聚合**数据库与 AI 领域最新进展的信息服务。每天自动从学术界和业界多个源获取内容，经 LLM 摘要和分类后，通过 Web 页面、微信小程序和主动推送三种渠道触达用户。

## 目标定位

- **受众**：面向技术从业者（DB/Infra/AI 工程师、研究者）
- **覆盖范围**：数据库（存储引擎、分布式系统、查询优化） + AI/ML（LLM、向量检索、训练推理）
- **内容类型**：学术论文、技术博客、开源项目 Release、会议动态、产品发布
- **频率**：每日一次聚合推送（08:00），支持手动刷新
- **推送渠道**：Web 页面 + 微信小程序 + 主动推送（订阅消息/邮件）

## 技术架构

### 方案选择

| 维度 | 选型 |
|------|------|
| 后端框架 | Python FastAPI |
| 数据聚合 | Python（httpx + feedparser + arxiv） |
| 摘要生成 | LLM API（Claude/OpenAI，用户提供 Key） |
| 数据库 | PostgreSQL（后续迁移至自研数据库） |
| 定时任务 | APScheduler（内嵌在 FastAPI 进程中） |
| 前端 Web | Vue 3 + Vite（参考已有 todo-app 风格） |
| 微信小程序 | 原生或 Taro |
| 推送 | 微信订阅消息 + 可选邮件 |
| 部署 | VPS / 云服务器 |
| 目录 | `engineering/apps/daily-digest/` |

### 系统架构

```
┌─────────────────────────────────────────────────────────────┐
│                   VPS / 云服务器                              │
│                                                             │
│  ┌──────────────────┐    ┌───────────────────────────────┐  │
│  │   数据聚合层       │    │   API 服务层 (FastAPI)        │  │
│  │                   │    │                               │  │
│  │  ┌─────────────┐  │    │  ┌────────────────────────┐  │  │
│  │  │ arXiv API    │  │    │  │ GET  /api/v1/daily     │  │  │
│  │  │ Semantic Sch │  │    │  │ GET  /api/v1/search    │  │  │
│  │  │ DBLP         │──┼────│─│ POST /api/v1/subscribe │  │  │
│  │  │ RSS Feeds    │  │    │  │ POST /api/v1/collect   │  │  │
│  │  │ GitHub Rel.  │  │    │  │ GET  /api/v1/history   │  │  │
│  │  └──────┬──────┘  │    │  └────────────────────────┘  │  │
│  │         │          │    └───────────┬───────────────────┘  │
│  │  ┌──────▼──────┐  │                │                     │
│  │  │  处理管道     │  │                │                     │
│  │  │  去重/摘要   │  │                │                     │
│  │  │  分类/排序   │  │                │                     │
│  │  └──────┬──────┘  │                │                     │
│  │         │          │  ┌─────────────▼──────────────────┐  │
│  │  ┌──────▼──────┐  │  │  定时任务 (APScheduler)        │  │
│  │  │  PostgreSQL  │  │  │  每天 08:00 自动聚合           │  │
│  │  │  内容/用户   │  │  │  可通过 API 手动触发            │  │
│  │  └─────────────┘  │  └────────────────────────────────┘  │
│  └──────────────────┘                                       │
└──────────────────────────────────┬──────────────────────────┘
                                   │ HTTPS
                    ┌──────────────┼──────────────┐
                    ▼              ▼              ▼
               Vue 3 Web      微信小程序     推送服务
              (静态 / Nginx)  (原生/Taro)  (微信订阅消息)
```

### 核心工作流

```
每天 08:00
  │
  ├─→ 聚合器（Aggregator）并行抓取所有数据源
  │     ├─ arXiv (cs.DB, cs.AI, cs.LG, cs.IR)
  │     ├─ Semantic Scholar (热门论文)
  │     ├─ DBLP (数据库/信息检索)
  │     ├─ 公司技术博客 RSS（Oracle、PG、MongoDB、Databricks、OpenAI、Meta AI 等）
  │     ├─ GitHub Releases（选定的开源项目）
  │     └─ Hugging Face Daily Papers
  │
  ├─→ 处理管道（Processor）
  │     ├─ 去重（由 source + source_id 索引确保）
  │     ├─ LLM 摘要生成（调用 Claude/OpenAI API）
  │     ├─ 自动分类（db / ai / ml / infra / sys）
  │     ├─ 标签提取（关键词 + 实体识别）
  │     └─ 重要度评分（引用数 + 来源权重 + 时效性）
  │
  ├─→ 存入 PostgreSQL
  │
  └─→ 匹配用户订阅 → 生成个性化推送队列
```

## 数据模型

### items — 聚合内容

| 字段 | 类型 | 说明 |
|------|------|------|
| id | SERIAL | 主键 |
| source | VARCHAR(32) | 来源分类：arxiv / blog / github / news / conference |
| source_id | VARCHAR(128) | 原始 ID（如 arXiv ID），用于去重 |
| title | TEXT | 标题 |
| url | TEXT | 原文链接 |
| summary | TEXT | LLM 生成的 200-300 字摘要 |
| raw_content | TEXT | 原文内容（可选，存储论文摘要或博客正文） |
| category | VARCHAR(32) | 分类：db / ai / ml / infra / sys / other |
| tags | TEXT[] | 标签数组：["llm", "vector-db", "storage"] |
| score | FLOAT | 重要度评分 [0, 1] |
| published | TIMESTAMP | 原始发布时间 |
| source_weight | INTEGER | 数据源权重，用于排序 |
| created_at | TIMESTAMP | 入库时间 |
| is_push | BOOLEAN | 是否已推送给用户 |

### users — 用户

| 字段 | 类型 | 说明 |
|------|------|------|
| id | SERIAL | 主键 |
| wechat_openid | VARCHAR(64) UNIQUE | 微信 OpenID |
| email | VARCHAR(255) | 邮箱（可选） |
| preferences | JSONB | 偏好设置：推送时间、推送频率等 |
| created_at | TIMESTAMP | 注册时间 |
| last_active | TIMESTAMP | 最后活跃时间 |

### subscriptions — 用户订阅方向

| 字段 | 类型 | 说明 |
|------|------|------|
| id | SERIAL | 主键 |
| user_id | INT FK→users | 用户 |
| category | VARCHAR(32) | 订阅方向：db / ai / ml / infra |
| keywords | TEXT[] | 细化关键词，如 ["Raft", "LSM-Tree"] |
| weight | INT | 权重，控制推送优先级 |
| created_at | TIMESTAMP | |

约束：`UNIQUE(user_id, category)`

### user_actions — 用户行为（用于个性化推荐）

| 字段 | 类型 | 说明 |
|------|------|------|
| id | SERIAL | 主键 |
| user_id | INT FK→users | 用户 |
| item_id | INT FK→items | 内容 |
| action | VARCHAR(16) | view / bookmark / click_through / share |
| created_at | TIMESTAMP | |

### collections — 收藏夹分组

| 字段 | 类型 | 说明 |
|------|------|------|
| id | SERIAL | 主键 |
| user_id | INT FK→users | 用户 |
| name | VARCHAR(64) | 收藏夹名称，如"论文精读"、"架构笔记" |
| description | TEXT | 描述 |
| created_at | TIMESTAMP | |

约束：`UNIQUE(user_id, name)`

### collection_items — 收藏夹内容

| 字段 | 类型 | 说明 |
|------|------|------|
| id | SERIAL | 主键 |
| collection_id | INT FK→collections | 收藏夹（级联删除） |
| item_id | INT FK→items | 内容（级联删除） |
| note | TEXT | 用户备注："后面要细看" |
| created_at | TIMESTAMP | |

约束：`UNIQUE(collection_id, item_id)`

### 实体关系图

```
users ──1:N──→ subscriptions
users ──1:N──→ user_actions ──N:1── items
users ──1:N──→ collections ──1:N──→ collection_items ──N:1── items
```

## API 设计

### 内容接口

```
GET  /api/v1/daily
  → 获取当日速览（支持 ?category=ai&page=1&size=20）
  → 返回当日聚合内容列表，已按 score 排序

GET  /api/v1/daily/{item_id}
  → 获取单条内容详情（摘要 + 原文链接 + 标签）

GET  /api/v1/search?q=LSM-Tree&category=db&page=1
  → 全文搜索历史内容

GET  /api/v1/history?user_id=xxx&page=1
  → 用户的阅读历史
```

### 用户接口

```
POST /api/v1/auth/wechat
  → 微信小程序登录

POST /api/v1/auth/email
  → Web 端邮箱注册/登录

GET  /api/v1/user/preferences
POST /api/v1/user/preferences
  → 获取/更新用户偏好（推送时间、频率）
```

### 订阅接口

```
GET  /api/v1/subscriptions?user_id=xxx
  → 获取用户订阅方向列表

POST /api/v1/subscriptions
  → 新增订阅方向
  → Body: { user_id, category, keywords }

PUT  /api/v1/subscriptions/{id}
  → 更新订阅（如修改关键词）

DELETE /api/v1/subscriptions/{id}
  → 取消订阅
```

### 收藏夹接口

```
GET  /api/v1/collections?user_id=xxx
  → 获取用户的收藏夹列表（含每个收藏夹的内容数量）

POST /api/v1/collections
  → 新建收藏夹
  → Body: { user_id, name, description }

PUT  /api/v1/collections/{id}
  → 修改收藏夹（重命名、改描述）

DELETE /api/v1/collections/{id}
  → 删除收藏夹（级联删除其中的收藏项）

GET  /api/v1/collections/{id}/items?page=1
  → 获取收藏夹中的内容列表

POST /api/v1/collections/{id}/items
  → 将内容加入收藏夹
  → Body: { item_id, note }

DELETE /api/v1/collections/{id}/items/{item_id}
  → 从收藏夹移除内容
```

### 触发接口

```
POST /api/v1/trigger/refresh
  → 手动触发一次数据聚合（立即执行）

POST /api/v1/trigger/push
  → 手动触发推送

GET  /api/v1/stats
  → 统计信息（今日新增、总数、订阅数等）
```

## 数据源详表

| 数据源 | 获取方式 | 内容类型 | 更新频率 | 权重 |
|--------|---------|---------|---------|------|
| arXiv (cs.DB, cs.AI, cs.LG, cs.IR) | API | 学术论文 | 每日（工作日） | 高 |
| Semantic Scholar | API | 论文 + 引用数据 | 每日 | 高 |
| DBLP | API | 数据库/信息检索论文 | 每日 | 高 |
| Hugging Face Daily Papers | API | 热门 AI 论文 | 每日 | 高 |
| 公司技术博客 RSS | RSS Feed | 技术博客 | 即时 | 中 |
| GitHub Releases | API | 开源版本发布 | 即时 | 中 |
| 技术会议日程 | API/爬虫 | 会议论文收录 | 按会期 | 中 |
| Hacker News / Reddit | API | 社区热度信号 | 每日 | 低 |

### 公司技术博客初始列表

| 公司 | Feed URL（示例） | 领域 |
|------|-----------------|------|
| PostgreSQL | planet.postgresql.org | DB |
| MongoDB | mongodb.com/blog | DB |
| Databricks | databricks.com/blog | DB/AI |
| Snowflake | snowflake.com/blog | DB |
| OpenAI | openai.com/blog | AI |
| Anthropic | anthropic.com/blog | AI |
| Meta AI | ai.meta.com/blog | AI |
| Google AI | ai.googleblog.com | AI |
| Google Research | research.google/blog | ML |
| Microsoft Research | microsoft.com/research | DB/AI |

（该列表在配置文件中维护，可扩展）

## 摘要生成与分类

### 调用方式

- **LLM Provider**：通过环境变量配置（支持 Claude API 或 OpenAI API）
- **调用频率**：每天 20-50 次请求（每次处理一条内容）
- **成本估算**：使用 GPT-4o-mini / Claude Haiku，每天 < $0.01

### 摘要 Prompt 设计

```
你是一个数据库和 AI 领域的技术编辑。
请为以下技术内容生成 150-300 字的中文摘要：
1. 核心贡献/创新点是什么？
2. 为什么它重要？
3. 适合谁关注？

同时给出分类（db/ai/ml/infra/other）和 3-5 个标签。

原文标题：{title}
原文摘要：{abstract}
```

### 分类体系

| 分类 | 说明 | 示例 |
|------|------|------|
| db | 数据库 | 存储引擎、事务、SQL 优化、分布式 DB |
| ai | 人工智能 | LLM、多模态、Agent、训练方法 |
| ml | 机器学习 | 模型架构、优化算法、特征工程 |
| infra | 基础设施 | 分布式系统、网络、存储、调度 |
| sys | 系统 | 操作系统、编译器、硬件 |
| other | 其他 | 无法归类的 |

## 个性化推荐策略

### 分层推荐

1. **热门层**（所有人必推）：
   - 高影响力论文（引用多/来自顶会）
   - 重大产品发布
   - 评分 > 0.8 的内容

2. **订阅层**（按用户订阅方向筛选）：
   - 匹配 `subscriptions.category` 的内容
   - 匹配 `subscriptions.keywords` 的内容（关键词命中加权）

3. **探索层**（协同过滤/行为相似）：
   - 相同收藏夹的其他用户在看什么
   - 同类内容的高评分未读项

### 当天推送流程

```
1. 聚合完成 → 计算每条内容的 score
2. 遍历用户：
   a. 取热门层（所有用户共享）
   b. 匹配订阅层（过滤 + 排序）
   c. 补充探索层（补足到 20 条）
   d. 去重（用户已读的剔除，但保留在历史中可查）
   e. 生成个性化推送队列
```

## 前端页面结构

### Web 端（Vue 3）

| 页面 | 功能 |
|------|------|
| 首页/每日速览 | 今日内容列表，按分类 tab 切换，支持无穷滚动 |
| 详情页 | 内容详情，原文跳转，收藏/分享/深度搜索 |
| 搜索页 | 全文搜索 + 分类/标签/时间筛选 |
| 收藏夹页 | 收藏夹列表 → 每个收藏夹的内容列表 |
| 订阅管理页 | 编辑订阅方向 + 关键词 |
| 历史页 | 阅读历史（按天分组） |
| 设置页 | 推送时间、推送开关、账号管理 |

### 微信小程序页面

与 Web 端功能对齐，但 UI 适配移动端：
- 首页：每日速览（顶部轮播推荐 + 下方分类列表流）
- 发现：搜索入口 + 分类浏览
- 收藏：收藏夹分组展示
- 我的：订阅设置 + 历史 + 偏好

## 目录结构

```
engineering/apps/daily-digest/
├── README.md               # 项目说明 + 快速启动
├── docker-compose.yml      # PostgreSQL + 后端一键启动
├── .env.example            # 环境变量模板
│
├── backend/
│   ├── requirements.txt    # Python 依赖
│   ├── alembic/            # 数据库迁移
│   ├── app/
│   │   ├── main.py         # FastAPI 入口
│   │   ├── config.py       # 配置管理
│   │   ├── database.py     # 数据库连接
│   │   ├── models/         # SQLAlchemy 模型
│   │   ├── schemas/        # Pydantic schemas
│   │   ├── routers/        # API 路由
│   │   ├── services/       # 业务逻辑
│   │   │   ├── aggregator/ # 数据聚合
│   │   │   │   ├── arxiv.py
│   │   │   │   ├── semantic_scholar.py
│   │   │   │   ├── dblp.py
│   │   │   │   ├── rss_feeds.py
│   │   │   │   └── github_releases.py
│   │   │   ├── processor/  # 处理管道
│   │   │   │   ├── summarizer.py    # LLM 摘要
│   │   │   │   ├── classifier.py    # 分类 + 标签
│   │   │   │   └── scorer.py        # 评分
│   │   │   ├── recommender/  # 推荐引擎
│   │   │   └── pusher/      # 推送服务
│   │   └── scheduler.py    # APScheduler 定时任务
│   └── tests/
│
├── web/                    # Vue 3 前端
│   ├── package.json
│   ├── src/
│   │   ├── pages/          # 页面组件
│   │   ├── components/     # 通用组件
│   │   ├── api/            # API 调用
│   │   └── router/         # 路由配置
│   └── vite.config.ts
│
└── miniapp/                # 微信小程序
    ├── app.json
    ├── pages/
    └── components/
```

## 部署方案

### 环境要求

- Python 3.11+
- Node.js 18+（前端构建）
- PostgreSQL 15+
- 1 vCPU + 2GB RAM 及以上（VPS）

### 部署步骤

```bash
# 1. 克隆项目
git clone ... daily-digest
cd daily-digest

# 2. 启动 PostgreSQL（docker-compose）
docker-compose up -d db

# 3. 安装 Python 依赖
cd backend
pip install -r requirements.txt

# 4. 初始化数据库
alembic upgrade head

# 5. 启动后端
uvicorn app.main:app --host 0.0.0.0 --port 8000

# 6. 构建前端
cd ../web
npm install
npm run build
# 静态文件部署到 Nginx

# 7. 配置定时任务
# APScheduler 内嵌在后端进程中，启动即注册
```

## 错误处理与降级

- **聚合器降级**：某个数据源不可用 → 跳过该源，记录日志，不影响其他源
- **LLM 降级**：LLM API 超时/错误 → 使用原文摘要（如 arXiv abstract）兜底
- **空日降级**：当天无新内容 → 推送"今日暂无更新"，推荐历史未读内容
- **推送失败**：单个用户推送失败 → 重试 3 次，仍失败则标记不打扰

## 测试策略

| 层级 | 工具 | 覆盖内容 |
|------|------|---------|
| 单元测试 | pytest | 数据源解析、分类逻辑、评分计算 |
| API 测试 | httpx + pytest | 所有 API 端点的正确性 |
| 集成测试 | docker-compose | 完整聚合 → 摘要 → 推送流程 |
| 前端测试 | Vitest + Playwright | 组件渲染、用户交互 |
