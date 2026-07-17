# DailyDigest

每日数据库 & AI 技术速览 —— 自动聚合、摘要生成、定时推送的技术资讯平台。

## 项目结构

```
daily-digest/
├── docker-compose.yml       # PostgreSQL 服务
├── .env.example             # 环境变量模板
├── backend/
│   ├── requirements.txt     # Python 依赖
│   ├── alembic.ini          # 数据库迁移配置
│   ├── alembic/             # 迁移脚本
│   │   └── env.py
│   └── app/
│       ├── __init__.py
│       ├── config.py        # 配置管理
│       ├── database.py      # 数据库连接
│       ├── main.py          # FastAPI 入口
│       ├── models/          # SQLAlchemy 模型
│       ├── schemas/         # Pydantic 模式
│       ├── routers/         # API 路由
│       └── services/        # 业务服务
│           ├── aggregator/  # 聚合服务
│           ├── processor/   # 处理服务
│           ├── recommender/ # 推荐服务
│           └── pusher/      # 推送服务
└── web/                     # 前端（待建）
```

## 快速启动

```bash
# 1. 复制环境变量
cp .env.example .env

# 2. 启动 PostgreSQL
docker compose up -d

# 3. 安装后端依赖
cd backend
pip install -r requirements.txt

# 4. 运行数据库迁移
alembic upgrade head

# 5. 启动服务
uvicorn app.main:app --reload --port 8000
```

服务启动后访问 `http://localhost:8000/health` 确认运行状态。