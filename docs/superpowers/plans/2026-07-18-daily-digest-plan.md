# DailyDigest 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 构建每日数据库 & AI 技术速览服务，包含 Python FastAPI 后端 + Vue 3 Web 前端 + 微信小程序。

**Architecture:** 三层架构。后端 Python FastAPI 负责数据聚合、LLM 摘要、推荐引擎和 API 服务；Vue 3 前端提供 Web 端交互；微信小程序提供移动端入口。PostgreSQL 存储所有数据，APScheduler 驱动每日定时聚合。

**Tech Stack:** Python 3.11+, FastAPI, SQLAlchemy, PostgreSQL 15+, APScheduler, httpx, Vue 3 + Vite + TypeScript + Vue Router, 微信小程序原生

## 全局约束

- 项目根目录：`engineering/apps/daily-digest/`
- 后端 Python 3.11+，依赖通过 `requirements.txt` 管理
- 使用 PostgreSQL 15+，通过 docker-compose 启动
- 数据库迁移使用 Alembic
- 前端 Vue 3 + Vite + TypeScript，通过 `package.json` 管理
- 所有 API 接口统一前缀 `/api/v1`
- 所有代码提交用中文 commit message
- 索引必须使用 `UNIQUE` 约束确保去重（`items(source, source_id)`、`subscriptions(user_id, category)`、`collections(user_id, name)`、`collection_items(collection_id, item_id)`）
- 聚合器降级：单个数据源失败不影响其他源

---

## Phase 1：后端 API 与数据管道

### Task 1: 项目骨架与基础设施

**Files:**
- Create: `engineering/apps/daily-digest/README.md`
- Create: `engineering/apps/daily-digest/.gitignore`
- Create: `engineering/apps/daily-digest/docker-compose.yml`
- Create: `engineering/apps/daily-digest/.env.example`
- Create: `engineering/apps/daily-digest/backend/requirements.txt`
- Create: `engineering/apps/daily-digest/backend/app/__init__.py`
- Create: `engineering/apps/daily-digest/backend/app/config.py`
- Create: `engineering/apps/daily-digest/backend/app/database.py`
- Create: `engineering/apps/daily-digest/backend/app/main.py`

**Interfaces:**
- Consumes: 无
- Produces: `config.py` 暴露 `Settings` dataclass（`DATABASE_URL`, `LLM_API_KEY`, `LLM_PROVIDER`, `ARXIV_CATEGORIES` 等配置）
- `database.py` 暴露 `get_db()` 依赖注入函数，返回 `Session`
- `main.py` 暴露 FastAPI `app` 实例，包含根路径健康检查

- [ ] **Step 1: 创建项目目录结构**

```bash
mkdir -p "engineering/apps/daily-digest/backend/app/models"
mkdir -p "engineering/apps/daily-digest/backend/app/schemas"
mkdir -p "engineering/apps/daily-digest/backend/app/routers"
mkdir -p "engineering/apps/daily-digest/backend/app/services/aggregator"
mkdir -p "engineering/apps/daily-digest/backend/app/services/processor"
mkdir -p "engineering/apps/daily-digest/backend/app/services/recommender"
mkdir -p "engineering/apps/daily-digest/backend/app/services/pusher"
mkdir -p "engineering/apps/daily-digest/backend/tests"
```

- [ ] **Step 2: 创建 .gitignore**

```
# Python
__pycache__/
*.py[cod]
*.egg-info/
dist/
*.egg
.venv/
venv/

# Node
node_modules/
dist/

# Environment
.env
.env.local

# IDE
.vscode/
.idea/

# OS
.DS_Store
Thumbs.db
```

- [ ] **Step 3: 创建 docker-compose.yml**

```yaml
version: "3.9"
services:
  db:
    image: postgres:15
    environment:
      POSTGRES_DB: dailydigest
      POSTGRES_USER: dailydigest
      POSTGRES_PASSWORD: dailydigest
    ports:
      - "5432:5432"
    volumes:
      - pgdata:/var/lib/postgresql/data

volumes:
  pgdata:
```

- [ ] **Step 4: 创建 .env.example**

```env
# 数据库
DATABASE_URL=postgresql://dailydigest:dailydigest@localhost:5432/dailydigest

# LLM API
LLM_PROVIDER=openai
LLM_API_KEY=sk-your-key-here
LLM_MODEL=gpt-4o-mini

# 服务配置
SERVER_HOST=0.0.0.0
SERVER_PORT=8000
CORS_ORIGINS=http://localhost:5173,http://localhost:3000

# 聚合配置
ARXIV_CATEGORIES=cs.DB,cs.AI,cs.LG,cs.IR
MAX_ITEMS_PER_DAY=50
```

- [ ] **Step 5: 创建 requirements.txt**

```
fastapi==0.115.0
uvicorn[standard]==0.30.0
sqlalchemy==2.0.35
psycopg2-binary==2.9.9
alembic==1.13.2
pydantic==2.9.0
pydantic-settings==2.5.0
httpx==0.27.0
feedparser==6.0.11
apscheduler==3.10.4
python-multipart==0.0.9
pytest==8.3.3
pytest-asyncio==0.24.0
httpx-ws==0.6.0
```

- [ ] **Step 6: 创建 config.py**

```python
from pydantic_settings import BaseSettings

class Settings(BaseSettings):
    database_url: str = "postgresql://dailydigest:dailydigest@localhost:5432/dailydigest"
    llm_provider: str = "openai"
    llm_api_key: str = ""
    llm_model: str = "gpt-4o-mini"
    server_host: str = "0.0.0.0"
    server_port: int = 8000
    cors_origins: str = "http://localhost:5173,http://localhost:3000"
    arxiv_categories: str = "cs.DB,cs.AI,cs.LG,cs.IR"
    max_items_per_day: int = 50

    class Config:
        env_file = ".env"

settings = Settings()
```

- [ ] **Step 7: 创建 database.py**

```python
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker, declarative_base
from app.config import settings

engine = create_engine(settings.database_url)
SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)
Base = declarative_base()

def get_db():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()
```

- [ ] **Step 8: 创建 main.py**

```python
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from app.config import settings

app = FastAPI(title="DailyDigest API", version="0.1.0")

app.add_middleware(
    CORSMiddleware,
    allow_origins=settings.cors_origins.split(","),
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

@app.get("/health")
def health():
    return {"status": "ok", "version": "0.1.0"}
```

- [ ] **Step 9: 创建 README.md**

```markdown
# DailyDigest — 每日数据库 & AI 技术速览

## 快速启动

```bash
# 1. 启动 PostgreSQL
docker-compose up -d db

# 2. 安装 Python 依赖
cd backend
pip install -r requirements.txt

# 3. 复制环境变量
cp ../.env.example .env
# 编辑 .env 填入 LLM_API_KEY

# 4. 初始化数据库
alembic upgrade head

# 5. 启动服务
uvicorn app.main:app --reload
```

- [ ] **Step 10: 验证服务启动**

```bash
cd backend
pip install -r requirements.txt
uvicorn app.main:app --reload
```

Run: `curl http://localhost:8000/health`
Expected: `{"status": "ok", "version": "0.1.0"}`

- [ ] **Step 11: Commit**

```bash
git add engineering/apps/daily-digest/
git commit -m "feat(daily-digest): 初始化项目骨架与基础设施"
```

---

### Task 2: 数据库模型与迁移

**Files:**
- Create: `engineering/apps/daily-digest/backend/app/models/__init__.py`
- Create: `engineering/apps/daily-digest/backend/app/models/item.py`
- Create: `engineering/apps/daily-digest/backend/app/models/user.py`
- Create: `engineering/apps/daily-digest/backend/app/models/subscription.py`
- Create: `engineering/apps/daily-digest/backend/app/models/user_action.py`
- Create: `engineering/apps/daily-digest/backend/app/models/collection.py`
- Create: `engineering/apps/daily-digest/backend/app/models/collection_item.py`
- Create: `engineering/apps/daily-digest/backend/alembic.ini`
- Create: `engineering/apps/daily-digest/backend/alembic/env.py`
- Create: `engineering/apps/daily-digest/backend/alembic/versions/001_init.py`

**Interfaces:**
- Consumes: `app/database.py` 中的 `Base` 和 `engine`
- Produces: 所有 SQLAlchemy 模型类，`models/__init__.py` 导出所有模型供 Alembic 检测

- [ ] **Step 1: 创建 models/__init__.py**

```python
from app.models.item import Item
from app.models.user import User
from app.models.subscription import Subscription
from app.models.user_action import UserAction
from app.models.collection import Collection
from app.models.collection_item import CollectionItem

__all__ = ["Item", "User", "Subscription", "UserAction", "Collection", "CollectionItem"]
```

- [ ] **Step 2: 创建 item.py**

```python
from sqlalchemy import Column, Integer, String, Text, Float, DateTime, Boolean, ARRAY
from sqlalchemy.sql import func
from app.database import Base

class Item(Base):
    __tablename__ = "items"

    id = Column(Integer, primary_key=True, autoincrement=True)
    source = Column(String(32), nullable=False, index=True)
    source_id = Column(String(128), nullable=False)
    title = Column(Text, nullable=False)
    url = Column(Text, nullable=False)
    summary = Column(Text, nullable=True)
    raw_content = Column(Text, nullable=True)
    category = Column(String(32), nullable=True, index=True)
    tags = Column(ARRAY(String), nullable=True)
    score = Column(Float, default=0.0)
    published = Column(DateTime(timezone=True), nullable=True)
    source_weight = Column(Integer, default=1)
    is_push = Column(Boolean, default=False)
    created_at = Column(DateTime(timezone=True), server_default=func.now())

    __table_args__ = (
        __table_args__ = (UniqueConstraint("source", "source_id", name="uq_item_source"),)
    )
```

- [ ] **Step 3: 创建 user.py**

```python
from sqlalchemy import Column, Integer, String, DateTime, JSON
from sqlalchemy.sql import func
from app.database import Base

class User(Base):
    __tablename__ = "users"

    id = Column(Integer, primary_key=True, autoincrement=True)
    wechat_openid = Column(String(64), unique=True, nullable=True)
    email = Column(String(255), unique=True, nullable=True)
    preferences = Column(JSON, nullable=True, default=dict)
    created_at = Column(DateTime(timezone=True), server_default=func.now())
    last_active = Column(DateTime(timezone=True), nullable=True)
```

- [ ] **Step 4: 创建 subscription.py**

```python
from sqlalchemy import Column, Integer, String, DateTime, ARRAY, ForeignKey, UniqueConstraint
from sqlalchemy.sql import func
from app.database import Base

class Subscription(Base):
    __tablename__ = "subscriptions"

    id = Column(Integer, primary_key=True, autoincrement=True)
    user_id = Column(Integer, ForeignKey("users.id", ondelete="CASCADE"), nullable=False)
    category = Column(String(32), nullable=False)
    keywords = Column(ARRAY(String), nullable=True, default=list)
    weight = Column(Integer, default=1)
    created_at = Column(DateTime(timezone=True), server_default=func.now())

    __table_args__ = (
        UniqueConstraint("user_id", "category", name="uq_user_category"),
    )
```

- [ ] **Step 5: 创建 user_action.py**

```python
from sqlalchemy import Column, Integer, String, DateTime, ForeignKey
from sqlalchemy.sql import func
from app.database import Base

class UserAction(Base):
    __tablename__ = "user_actions"

    id = Column(Integer, primary_key=True, autoincrement=True)
    user_id = Column(Integer, ForeignKey("users.id", ondelete="CASCADE"), nullable=False)
    item_id = Column(Integer, ForeignKey("items.id", ondelete="CASCADE"), nullable=False)
    action = Column(String(16), nullable=False)
    created_at = Column(DateTime(timezone=True), server_default=func.now())
```

- [ ] **Step 6: 创建 collection.py**

```python
from sqlalchemy import Column, Integer, String, Text, DateTime, ForeignKey, UniqueConstraint
from sqlalchemy.sql import func
from app.database import Base

class Collection(Base):
    __tablename__ = "collections"

    id = Column(Integer, primary_key=True, autoincrement=True)
    user_id = Column(Integer, ForeignKey("users.id", ondelete="CASCADE"), nullable=False)
    name = Column(String(64), nullable=False)
    description = Column(Text, nullable=True)
    created_at = Column(DateTime(timezone=True), server_default=func.now())

    __table_args__ = (
        UniqueConstraint("user_id", "name", name="uq_user_collection_name"),
    )
```

- [ ] **Step 7: 创建 collection_item.py**

```python
from sqlalchemy import Column, Integer, Text, DateTime, ForeignKey, UniqueConstraint
from sqlalchemy.sql import func
from app.database import Base

class CollectionItem(Base):
    __tablename__ = "collection_items"

    id = Column(Integer, primary_key=True, autoincrement=True)
    collection_id = Column(Integer, ForeignKey("collections.id", ondelete="CASCADE"), nullable=False)
    item_id = Column(Integer, ForeignKey("items.id", ondelete="CASCADE"), nullable=False)
    note = Column(Text, nullable=True)
    created_at = Column(DateTime(timezone=True), server_default=func.now())

    __table_args__ = (
        UniqueConstraint("collection_id", "item_id", name="uq_collection_item"),
    )
```

- [ ] **Step 8: 初始化 Alembic 并创建迁移**

```bash
cd backend
alembic init alembic
```

编辑 `alembic/env.py` 中的 `target_metadata`：

```python
from app.models import Base
target_metadata = Base.metadata
```

- [ ] **Step 9: 生成迁移文件**

```bash
alembic revision --autogenerate -m "init all tables"
```

- [ ] **Step 10: 运行迁移**

```bash
alembic upgrade head
```

Expected: 6 张表（items, users, subscriptions, user_actions, collections, collection_items）创建成功

- [ ] **Step 11: 验证**

```bash
docker-compose exec db psql -U dailydigest -d dailydigest -c "\dt"
```

Expected: 显示 6 张表

- [ ] **Step 12: Commit**

```bash
git add engineering/apps/daily-digest/backend/app/models/
git add engineering/apps/daily-digest/backend/alembic/
git add engineering/apps/daily-digest/backend/alembic.ini
git commit -m "feat(daily-digest): 添加数据库模型与 Alembic 迁移"
```

---

### Task 3: Pydantic Schemas（请求/响应模型）

**Files:**
- Create: `engineering/apps/daily-digest/backend/app/schemas/__init__.py`
- Create: `engineering/apps/daily-digest/backend/app/schemas/common.py`
- Create: `engineering/apps/daily-digest/backend/app/schemas/item.py`
- Create: `engineering/apps/daily-digest/backend/app/schemas/user.py`
- Create: `engineering/apps/daily-digest/backend/app/schemas/subscription.py`
- Create: `engineering/apps/daily-digest/backend/app/schemas/collection.py`

**Interfaces:**
- Consumes: 无（纯数据类，无数据库依赖）
- Produces: Pydantic 模型，如 `ItemResponse`, `ItemListResponse`, `SubscriptionCreate`, `CollectionCreate`, `CollectionItemCreate`, `PaginatedResponse[T]`

- [ ] **Step 1: 创建 common.py**

```python
from pydantic import BaseModel
from typing import Generic, TypeVar, List, Optional

T = TypeVar("T")

class PaginatedResponse(BaseModel, Generic[T]):
    items: List[T]
    total: int
    page: int
    size: int
    total_pages: int

class MessageResponse(BaseModel):
    message: str
```

- [ ] **Step 2: 创建 item.py**

```python
from pydantic import BaseModel
from datetime import datetime
from typing import List, Optional

class ItemResponse(BaseModel):
    id: int
    source: str
    source_id: str
    title: str
    url: str
    summary: Optional[str] = None
    category: Optional[str] = None
    tags: Optional[List[str]] = None
    score: Optional[float] = 0.0
    published: Optional[datetime] = None
    created_at: datetime

    class Config:
        from_attributes = True

class ItemDetailResponse(ItemResponse):
    raw_content: Optional[str] = None

class ItemListResponse(BaseModel):
    items: list[ItemResponse]
    total: int
    page: int
    size: int
    total_pages: int
```

- [ ] **Step 3: 创建 user.py**

```python
from pydantic import BaseModel
from datetime import datetime
from typing import Optional

class WechatLoginRequest(BaseModel):
    code: str

class EmailLoginRequest(BaseModel):
    email: str

class UserPreferencesUpdate(BaseModel):
    preferences: dict

class UserResponse(BaseModel):
    id: int
    email: Optional[str] = None
    preferences: Optional[dict] = None
    created_at: datetime

    class Config:
        from_attributes = True
```

- [ ] **Step 4: 创建 subscription.py**

```python
from pydantic import BaseModel
from datetime import datetime
from typing import List, Optional

class SubscriptionCreate(BaseModel):
    category: str
    keywords: Optional[List[str]] = None
    weight: Optional[int] = 1

class SubscriptionUpdate(BaseModel):
    category: Optional[str] = None
    keywords: Optional[List[str]] = None
    weight: Optional[int] = None

class SubscriptionResponse(BaseModel):
    id: int
    user_id: int
    category: str
    keywords: Optional[List[str]] = None
    weight: int
    created_at: datetime

    class Config:
        from_attributes = True
```

- [ ] **Step 5: 创建 collection.py**

```python
from pydantic import BaseModel
from datetime import datetime
from typing import Optional

class CollectionCreate(BaseModel):
    name: str
    description: Optional[str] = None

class CollectionUpdate(BaseModel):
    name: Optional[str] = None
    description: Optional[str] = None

class CollectionItemCreate(BaseModel):
    item_id: int
    note: Optional[str] = None

class CollectionResponse(BaseModel):
    id: int
    user_id: int
    name: str
    description: Optional[str] = None
    item_count: int = 0
    created_at: datetime

    class Config:
        from_attributes = True

class CollectionDetailResponse(CollectionResponse):
    items: list = []

class CollectionItemResponse(BaseModel):
    id: int
    collection_id: int
    item_id: int
    note: Optional[str] = None
    created_at: datetime

    class Config:
        from_attributes = True
```

- [ ] **Step 6: 验证导入**

```python
python -c "from app.schemas import ItemResponse, SubscriptionCreate, CollectionCreate; print('OK')"
```

Expected: `OK`

- [ ] **Step 7: Commit**

```bash
git add engineering/apps/daily-digest/backend/app/schemas/
git commit -m "feat(daily-digest): 添加 Pydantic 请求/响应模型"
```

---

### Task 4: 数据聚合器基类 + arXiv 聚合器

**Files:**
- Create: `engineering/apps/daily-digest/backend/app/services/__init__.py`
- Create: `engineering/apps/daily-digest/backend/app/services/aggregator/__init__.py`
- Create: `engineering/apps/daily-digest/backend/app/services/aggregator/base.py`
- Create: `engineering/apps/daily-digest/backend/app/services/aggregator/arxiv.py`
- Create: `engineering/apps/daily-digest/backend/tests/conftest.py`
- Create: `engineering/apps/daily-digest/backend/tests/test_aggregator_arxiv.py`

**Interfaces:**
- Consumes: `app/config.py` 中的 `settings`
- Produces: `AggregatorBase` 基类（`fetch() -> list[RawItem]`），`ArxivAggregator` 实现类
- `RawItem` 类型：`dict` 含 keys `source`, `source_id`, `title`, `url`, `summary`, `raw_content`, `published`, `category`

- [ ] **Step 1: 创建 base.py**

```python
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from datetime import datetime
from typing import Optional, List

@dataclass
class RawItem:
    source: str
    source_id: str
    title: str
    url: str
    summary: str = ""
    raw_content: str = ""
    published: Optional[datetime] = None
    category: Optional[str] = None
    tags: List[str] = field(default_factory=list)

class AggregatorBase(ABC):
    @abstractmethod
    async def fetch(self) -> List[RawItem]:
        ...

    @property
    @abstractmethod
    def source_name(self) -> str:
        ...
```

- [ ] **Step 2: 创建 arxiv.py**

```python
import httpx
import xml.etree.ElementTree as ET
from datetime import datetime, timezone
from app.config import settings
from app.services.aggregator.base import AggregatorBase, RawItem

ARXIV_API_URL = "http://export.arxiv.org/api/query"

class ArxivAggregator(AggregatorBase):
    @property
    def source_name(self) -> str:
        return "arxiv"

    async def fetch(self) -> list[RawItem]:
        categories = settings.arxiv_categories.split(",")
        all_items = []

        async with httpx.AsyncClient(timeout=30) as client:
            for cat in categories:
                query = f"cat:{cat}"
                params = {
                    "search_query": query,
                    "sortBy": "submittedDate",
                    "sortOrder": "descending",
                    "max_results": 20,
                }
                try:
                    resp = await client.get(ARXIV_API_URL, params=params)
                    resp.raise_for_status()
                    items = self._parse_response(resp.text, cat)
                    all_items.extend(items)
                except Exception as e:
                    print(f"[ArxivAggregator] {cat} fetch failed: {e}")
                    continue

        return all_items

    def _parse_response(self, xml_text: str, category: str) -> list[RawItem]:
        ns = {"atom": "http://www.w3.org/2005/Atom"}
        root = ET.fromstring(xml_text)
        items = []

        for entry in root.findall("atom:entry", ns):
            title = entry.find("atom:title", ns)
            summary = entry.find("atom:summary", ns)
            published = entry.find("atom:published", ns)
            link = entry.find("atom:id", ns)

            item = RawItem(
                source="arxiv",
                source_id=link.text.strip().split("/")[-1] if link is not None else "",
                title=title.text.strip().replace("\n", " ") if title is not None else "",
                url=link.text.strip() if link is not None else "",
                summary=summary.text.strip() if summary is not None else "",
                published=datetime.fromisoformat(published.text.replace("Z", "+00:00")) if published is not None else None,
                category=category.removeprefix("cs.").lower(),
            )
            items.append(item)

        return items
```

- [ ] **Step 3: 创建 conftest.py**

```python
import pytest
from app.database import engine, SessionLocal, Base
from app.models import *

@pytest.fixture(autouse=True)
def setup_db():
    Base.metadata.create_all(bind=engine)
    yield
    Base.metadata.drop_all(bind=engine)

@pytest.fixture
def db():
    session = SessionLocal()
    yield session
    session.close()
```

- [ ] **Step 4: 创建 test_aggregator_arxiv.py**

```python
import pytest
from app.services.aggregator.arxiv import ArxivAggregator
from app.services.aggregator.base import RawItem

@pytest.mark.asyncio
async def test_arxiv_fetch_returns_items():
    agg = ArxivAggregator()
    items = await agg.fetch()
    assert len(items) > 0
    assert all(isinstance(item, RawItem) for item in items)
    assert all(item.source == "arxiv" for item in items)
    assert all(item.source_id for item in items)
    assert all(item.title for item in items)
    assert all(item.url for item in items)
```

- [ ] **Step 5: 运行测试确认失败**

```bash
cd backend
python -m pytest tests/test_aggregator_arxiv.py -v
```

Expected: 测试失败（因为尚未实现异步支持或依赖未安装）

- [ ] **Step 6: 实现并验证测试通过**

```bash
pip install pytest-asyncio
python -m pytest tests/test_aggregator_arxiv.py -v -x
```

Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add engineering/apps/daily-digest/backend/app/services/
git add engineering/apps/daily-digest/backend/tests/
git commit -m "feat(daily-digest): 添加聚合器基类与 arXiv 聚合器"
```

---

### Task 5: 其他聚合器（Semantic Scholar、DBLP、RSS、GitHub、HuggingFace）

**Files:**
- Create: `engineering/apps/daily-digest/backend/app/services/aggregator/semantic_scholar.py`
- Create: `engineering/apps/daily-digest/backend/app/services/aggregator/dblp.py`
- Create: `engineering/apps/daily-digest/backend/app/services/aggregator/rss_feeds.py`
- Create: `engineering/apps/daily-digest/backend/app/services/aggregator/github_releases.py`
- Create: `engineering/apps/daily-digest/backend/app/services/aggregator/huggingface.py`
- Create: `engineering/apps/daily-digest/backend/tests/test_aggregator_semantic_scholar.py`
- Test: `engineering/apps/daily-digest/backend/tests/test_aggregator_arxiv.py`

**Interfaces:**
- Consumes: `AggregatorBase`, `RawItem`
- Produces: `SemanticScholarAggregator`, `DBLPAggregator`, `RssFeedsAggregator`, `GitHubReleasesAggregator`, `HuggingFaceAggregator` 五个实现类

- [ ] **Step 1: 创建 semantic_scholar.py**

```python
import httpx
from datetime import datetime, timezone
from app.services.aggregator.base import AggregatorBase, RawItem

SEMANTIC_SCHOLAR_API = "https://api.semanticscholar.org/graph/v1"

class SemanticScholarAggregator(AggregatorBase):
    @property
    def source_name(self) -> str:
        return "semantic_scholar"

    async def fetch(self) -> list[RawItem]:
        fields = "title,url,externalIds,publicationDate,abstract"
        params = {
            "limit": 20,
            "fields": fields,
            "sort": "publicationDate:desc",
        }
        # 热门论文：使用 relevance search 按日期排序
        queries = ["database", "vector database", "large language model", "storage engine"]

        all_items = []
        async with httpx.AsyncClient(timeout=30) as client:
            for q in queries:
                try:
                    resp = await client.get(
                        f"{SEMANTIC_SCHOLAR_API}/paper/search",
                        params={"query": q, **params},
                    )
                    resp.raise_for_status()
                    data = resp.json()
                    for paper in data.get("data", []):
                        ext_ids = paper.get("externalIds", {}) or {}
                        item = RawItem(
                            source="semantic_scholar",
                            source_id=ext_ids.get("CorpusId", paper.get("paperId", "")),
                            title=paper.get("title", ""),
                            url=paper.get("url", ""),
                            summary=paper.get("abstract", ""),
                            published=datetime.fromisoformat(paper["publicationDate"]) if paper.get("publicationDate") else None,
                            category="ai" if "llm" in q.lower() or "language model" in q.lower() else "db",
                        )
                        all_items.append(item)
                except Exception as e:
                    print(f"[SemanticScholar] {q} failed: {e}")
                    continue

        return all_items
```

- [ ] **Step 2: 创建 dblp.py**

```python
import httpx
from datetime import datetime
from app.services.aggregator.base import AggregatorBase, RawItem

DBLP_API = "https://dblp.org/search/publ/api"

class DBLPAggregator(AggregatorBase):
    @property
    def source_name(self) -> str:
        return "dblp"

    async def fetch(self) -> list[RawItem]:
        queries = ["database", "information retrieval", "data management", "query processing"]
        all_items = []

        async with httpx.AsyncClient(timeout=30) as client:
            for q in queries:
                params = {
                    "q": q,
                    "format": "json",
                    "h": 10,
                    "sort": "resulted desc",
                }
                try:
                    resp = await client.get(DBLP_API, params=params)
                    resp.raise_for_status()
                    data = resp.json()
                    hits = data.get("result", {}).get("hits", {}).get("hit", [])
                    for hit in hits:
                        info = hit.get("info", {})
                        item = RawItem(
                            source="dblp",
                            source_id=info.get("key", ""),
                            title=info.get("title", ""),
                            url=info.get("url", ""),
                            published=datetime.strptime(info.get("year", "2026"), "%Y") if info.get("year") else None,
                            category="db",
                        )
                        all_items.append(item)
                except Exception as e:
                    print(f"[DBLP] {q} failed: {e}")
                    continue

        return all_items
```

- [ ] **Step 3: 创建 rss_feeds.py**

```python
import feedparser
from datetime import datetime, timezone
from app.services.aggregator.base import AggregatorBase, RawItem

BLOG_FEEDS = {
    "postgresql": "https://planet.postgresql.org/atom.xml",
    "mongodb": "https://www.mongodb.com/blog/rss",
    "databricks": "https://www.databricks.com/blog/feed",
    "openai": "https://openai.com/blog/feed.xml",
    "anthropic": "https://www.anthropic.com/feed.xml",
    "meta_ai": "https://ai.meta.com/blog/feed/",
    "google_ai": "https://blog.research.google/feed.xml",
}

class RssFeedsAggregator(AggregatorBase):
    @property
    def source_name(self) -> str:
        return "blog"

    async def fetch(self) -> list[RawItem]:
        all_items = []
        for source_name, feed_url in BLOG_FEEDS.items():
            try:
                feed = feedparser.parse(feed_url)
                for entry in feed.entries[:5]:
                    published = None
                    if hasattr(entry, "published_parsed") and entry.published_parsed:
                        published = datetime(*entry.published_parsed[:6], tzinfo=timezone.utc)

                    item = RawItem(
                        source="blog",
                        source_id=f"{source_name}:{entry.get('id', entry.get('link', ''))}",
                        title=entry.get("title", ""),
                        url=entry.get("link", ""),
                        summary=entry.get("summary", ""),
                        published=published,
                        tags=[source_name],
                    )
                    all_items.append(item)
            except Exception as e:
                print(f"[RSS] {source_name} failed: {e}")
                continue

        return all_items
```

- [ ] **Step 4: 创建 github_releases.py**

```python
import httpx
from datetime import datetime, timezone
from app.services.aggregator.base import AggregatorBase, RawItem

REPOS = [
    "postgres/postgres",
    "mongodb/mongo",
    "apache/kafka",
    "redis/redis",
    "cockroachdb/cockroach",
    "tikv/tikv",
    "dgraph-io/badger",
    "facebook/rocksdb",
]

class GitHubReleasesAggregator(AggregatorBase):
    @property
    def source_name(self) -> str:
        return "github"

    async def fetch(self) -> list[RawItem]:
        all_items = []
        async with httpx.AsyncClient(timeout=30) as client:
            for repo in REPOS:
                try:
                    resp = await client.get(
                        f"https://api.github.com/repos/{repo}/releases",
                        params={"per_page": 3},
                    )
                    resp.raise_for_status()
                    releases = resp.json()
                    for rel in releases:
                        item = RawItem(
                            source="github",
                            source_id=f"{repo}:{rel.get('id', '')}",
                            title=f"[{repo}] {rel.get('name', rel.get('tag_name', ''))}",
                            url=rel.get("html_url", ""),
                            summary=rel.get("body", "")[:500] if rel.get("body") else "",
                            published=datetime.fromisoformat(rel["published_at"].replace("Z", "+00:00")) if rel.get("published_at") else None,
                            tags=["release", repo.split("/")[-1]],
                        )
                        all_items.append(item)
                except Exception as e:
                    print(f"[GitHub] {repo} releases failed: {e}")
                    continue

        return all_items
```

- [ ] **Step 5: 创建 huggingface.py**

```python
import httpx
from datetime import datetime, timezone
from app.services.aggregator.base import AggregatorBase, RawItem

HF_DAILY_PAPERS = "https://huggingface.co/api/daily_papers"

class HuggingFaceAggregator(AggregatorBase):
    @property
    def source_name(self) -> str:
        return "huggingface"

    async def fetch(self) -> list[RawItem]:
        async with httpx.AsyncClient(timeout=30) as client:
            try:
                resp = await client.get(HF_DAILY_PAPERS)
                resp.raise_for_status()
                papers = resp.json()
                items = []
                for paper in papers[:20]:
                    item = RawItem(
                        source="huggingface",
                        source_id=paper.get("id", str(paper.get("paperId", ""))),
                        title=paper.get("title", ""),
                        url=paper.get("url", paper.get("paperUrl", "")),
                        summary=paper.get("abstract", ""),
                        published=datetime.fromisoformat(paper["publishedAt"].replace("Z", "+00:00")) if paper.get("publishedAt") else None,
                        category="ai",
                        tags=paper.get("keywords", []),
                    )
                    items.append(item)
                return items
            except Exception as e:
                print(f"[HuggingFace] fetch failed: {e}")
                return []
```

- [ ] **Step 6: 更新 aggregator/__init__.py**

```python
from app.services.aggregator.arxiv import ArxivAggregator
from app.services.aggregator.semantic_scholar import SemanticScholarAggregator
from app.services.aggregator.dblp import DBLPAggregator
from app.services.aggregator.rss_feeds import RssFeedsAggregator
from app.services.aggregator.github_releases import GitHubReleasesAggregator
from app.services.aggregator.huggingface import HuggingFaceAggregator

__all__ = [
    "ArxivAggregator",
    "SemanticScholarAggregator",
    "DBLPAggregator",
    "RssFeedsAggregator",
    "GitHubReleasesAggregator",
    "HuggingFaceAggregator",
]
```

- [ ] **Step 7: 创建 orchestrator 统一调度（新增文件）**

Create: `engineering/apps/daily-digest/backend/app/services/aggregator/orchestrator.py`

```python
from app.services.aggregator import (
    ArxivAggregator,
    SemanticScholarAggregator,
    DBLPAggregator,
    RssFeedsAggregator,
    GitHubReleasesAggregator,
    HuggingFaceAggregator,
)
from app.services.aggregator.base import RawItem

class AggregationOrchestrator:
    def __init__(self):
        self.aggregators = [
            ArxivAggregator(),
            HuggingFaceAggregator(),
            SemanticScholarAggregator(),
            DBLPAggregator(),
            RssFeedsAggregator(),
            GitHubReleasesAggregator(),
        ]

    async def run_all(self) -> list[RawItem]:
        all_items = []
        for agg in self.aggregators:
            try:
                items = await agg.fetch()
                all_items.extend(items)
                print(f"[Orchestrator] {agg.source_name}: {len(items)} items")
            except Exception as e:
                print(f"[Orchestrator] {agg.source_name} aggregator failed: {e}")
        return all_items
```

- [ ] **Step 8: 运行测试确认可用**

```bash
python -m pytest tests/test_aggregator_arxiv.py -v -x
```

Expected: PASS

- [ ] **Step 9: Commit**

```bash
git add engineering/apps/daily-digest/backend/app/services/aggregator/
git commit -m "feat(daily-digest): 添加所有数据聚合器与 orchestrator"
```

---

### Task 6: 处理管道（LLM 摘要、分类、评分）

**Files:**
- Create: `engineering/apps/daily-digest/backend/app/services/processor/__init__.py`
- Create: `engineering/apps/daily-digest/backend/app/services/processor/summarizer.py`
- Create: `engineering/apps/daily-digest/backend/app/services/processor/classifier.py`
- Create: `engineering/apps/daily-digest/backend/app/services/processor/scorer.py`
- Create: `engineering/apps/daily-digest/backend/tests/test_processor_summarizer.py`
- Create: `engineering/apps/daily-digest/backend/tests/test_processor_classifier.py`
- Create: `engineering/apps/daily-digest/backend/tests/test_processor_scorer.py`

**Interfaces:**
- Consumes: `RawItem`（来自聚合器）
- Produces: `Summarizer`（`summarize(items: list[RawItem], llm_client) -> list[RawItem]` 填充 `summary` 和 `category`），`Scorer`（`score(items: list[RawItem]) -> list[RawItem]` 填充 `score`）

- [ ] **Step 1: 创建 summarizer.py**

```python
import json
from typing import Optional
from app.services.aggregator.base import RawItem

SUMMARIZE_PROMPT = """你是一个数据库和 AI 领域的技术编辑。
请为以下技术内容生成 150-300 字的中文摘要：
1. 核心贡献/创新点是什么？
2. 为什么它重要？
3. 适合谁关注？

同时给出分类（db/ai/ml/infra/other）和 3-5 个标签。

请以 JSON 格式返回，格式为：
{{"summary": "摘要内容", "category": "分类", "tags": ["标签1", "标签2"]}}

原文标题：{title}
原文摘要：{abstract}
"""

class Summarizer:
    def __init__(self, llm_client: Optional[object] = None):
        self.llm_client = llm_client

    async def process(self, items: list[RawItem], llm_client=None) -> list[RawItem]:
        client = llm_client or self.llm_client
        if client is None:
            # 降级：无 LLM 时直接用原文摘要
            for item in items:
                item.summary = item.summary or item.raw_content[:200]
            return items

        for item in items:
            try:
                prompt = SUMMARIZE_PROMPT.format(
                    title=item.title,
                    abstract=item.summary or item.raw_content[:500],
                )
                result = await self._call_llm(client, prompt)
                if result:
                    item.summary = result.get("summary", item.summary)
                    item.category = result.get("category", item.category)
                    item.tags = result.get("tags", item.tags)
            except Exception as e:
                print(f"[Summarizer] failed for {item.source_id}: {e}")
                continue

        return items

    async def _call_llm(self, client, prompt: str) -> dict:
        # 支持 OpenAI 和 Anthropic 两种客户端
        if hasattr(client, "chat"):
            resp = client.chat.completions.create(
                model="gpt-4o-mini",
                messages=[{"role": "user", "content": prompt}],
                response_format={"type": "json_object"},
            )
            return json.loads(resp.choices[0].message.content)
        elif hasattr(client, "messages"):
            resp = client.messages.create(
                model="claude-3-haiku-20240307",
                max_tokens=500,
                messages=[{"role": "user", "content": prompt}],
            )
            return json.loads(resp.content[0].text)
        else:
            raise ValueError("Unsupported LLM client")
```

- [ ] **Step 2: 创建 classifier.py**

```python
from app.services.aggregator.base import RawItem

CATEGORY_KEYWORDS = {
    "db": ["database", "storage", "query", "sql", "transaction", "index",
           "lsm", "b-tree", "raft", "paxos", "shard", "distributed"],
    "ai": ["llm", "language model", "transformer", "attention", "gpt",
           "bert", "fine-tuning", "prompt", "agent", "rag", "vector"],
    "ml": ["training", "loss", "gradient", "optimization", "embedding",
           "classification", "regression", "reinforcement"],
    "infra": ["distributed system", "scheduling", "network", "container",
              "kubernetes", "cloud", "monitoring", "observability"],
    "sys": ["os", "kernel", "compiler", "hardware", "gpu", "memory",
            "filesystem", "io"],
}

class Classifier:
    def classify(self, items: list[RawItem]) -> list[RawItem]:
        for item in items:
            if item.category:
                continue
            text = (item.title + " " + (item.summary or "")).lower()
            scores = {}
            for cat, keywords in CATEGORY_KEYWORDS.items():
                scores[cat] = sum(1 for kw in keywords if kw in text)
            item.category = max(scores, key=scores.get) if max(scores.values()) > 0 else "other"
        return items
```

- [ ] **Step 3: 创建 scorer.py**

```python
from datetime import datetime, timezone, timedelta
from app.services.aggregator.base import RawItem

SOURCE_WEIGHTS = {
    "arxiv": 8,
    "huggingface": 9,
    "semantic_scholar": 7,
    "dblp": 6,
    "blog": 5,
    "github": 4,
}

class Scorer:
    def score(self, items: list[RawItem]) -> list[RawItem]:
        now = datetime.now(timezone.utc)
        for item in items:
            score = 0.0

            # 1. 来源权重
            score += SOURCE_WEIGHTS.get(item.source, 5) * 0.1

            # 2. 时效性：越新越高
            if item.published:
                days_ago = (now - item.published).days
                if days_ago <= 1:
                    score += 0.3
                elif days_ago <= 3:
                    score += 0.2
                elif days_ago <= 7:
                    score += 0.1

            # 3. 标题长度（有意义的标题更可能是有价值内容）
            if len(item.title) > 15:
                score += 0.1

            # 4. 有摘要加分
            if item.summary and len(item.summary) > 50:
                score += 0.1

            item.score = round(min(score, 1.0), 2)

        return items
```

- [ ] **Step 4: 创建 processor/__init__.py**

```python
from app.services.processor.summarizer import Summarizer
from app.services.processor.classifier import Classifier
from app.services.processor.scorer import Scorer

__all__ = ["Summarizer", "Classifier", "Scorer"]
```

- [ ] **Step 5: 创建测试文件**

```python
# tests/test_processor_classifier.py
import pytest
from app.services.processor.classifier import Classifier
from app.services.aggregator.base import RawItem

def test_classifier_db_keyword():
    c = Classifier()
    item = RawItem(
        source="arxiv",
        source_id="test1",
        title="A New LSM-Tree Storage Engine for Modern Hardware",
        url="http://example.com",
    )
    items = c.classify([item])
    assert items[0].category == "db"

def test_classifier_ai_keyword():
    c = Classifier()
    item = RawItem(
        source="arxiv",
        source_id="test2",
        title="Large Language Models Are Few-Shot Learners",
        url="http://example.com",
    )
    items = c.classify([item])
    assert items[0].category == "ai"

def test_classifier_fallback():
    c = Classifier()
    item = RawItem(
        source="arxiv",
        source_id="test3",
        title="Something Unrelated Here",
        url="http://example.com",
    )
    items = c.classify([item])
    assert items[0].category == "other"
```

```python
# tests/test_processor_scorer.py
import pytest
from datetime import datetime, timezone, timedelta
from app.services.processor.scorer import Scorer
from app.services.aggregator.base import RawItem

def test_scorer_today_item_higher():
    s = Scorer()
    today = RawItem(source="arxiv", source_id="t1", title="Today's Hot Paper", url="", published=datetime.now(timezone.utc))
    old = RawItem(source="arxiv", source_id="t2", title="Old Paper", url="", published=datetime.now(timezone.utc) - timedelta(days=30))
    items = s.score([today, old])
    assert items[0].score > items[1].score

def test_scorer_arxiv_high_weight():
    s = Scorer()
    item = RawItem(source="arxiv", source_id="t1", title="Important Database Paper", url="", published=datetime.now(timezone.utc))
    items = s.score([item])
    assert items[0].score > 0.0
```

- [ ] **Step 6: 运行测试**

```bash
python -m pytest tests/test_processor_classifier.py tests/test_processor_scorer.py -v -x
```

Expected: 所有测试 PASS

- [ ] **Step 7: Commit**

```bash
git add engineering/apps/daily-digest/backend/app/services/processor/
git add engineering/apps/daily-digest/backend/tests/test_processor_*.py
git commit -m "feat(daily-digest): 添加处理管道（摘要/分类/评分）"
```

---

### Task 7: 推荐引擎

**Files:**
- Create: `engineering/apps/daily-digest/backend/app/services/recommender/__init__.py`
- Create: `engineering/apps/daily-digest/backend/app/services/recommender/engine.py`
- Create: `engineering/apps/daily-digest/backend/tests/test_recommender.py`

**Interfaces:**
- Consumes: SQLAlchemy `Session`, `Item`, `User`, `Subscription`, `UserAction` 模型
- Produces: `RecommenderEngine` 类，提供 `get_daily_for_user(user_id, db) -> list[Item]`

- [ ] **Step 1: 创建 engine.py**

```python
from sqlalchemy.orm import Session
from sqlalchemy import desc, and_
from app.models.item import Item
from app.models.user import User
from app.models.subscription import Subscription
from app.models.user_action import UserAction

class RecommenderEngine:
    def __init__(self, hot_threshold: float = 0.8, max_items: int = 20):
        self.hot_threshold = hot_threshold
        self.max_items = max_items

    def get_daily_for_user(self, user_id: int, db: Session) -> list[Item]:
        # 1. 热门层：score > threshold
        hot_items = (
            db.query(Item)
            .filter(Item.score >= self.hot_threshold)
            .order_by(desc(Item.score), desc(Item.published))
            .limit(self.max_items)
            .all()
        )

        # 2. 已读 ID 集合
        read_ids = set(
            row[0] for row in db.query(UserAction.item_id)
            .filter(UserAction.user_id == user_id)
            .all()
        )

        # 3. 获取用户订阅
        subscriptions = db.query(Subscription).filter(Subscription.user_id == user_id).all()
        sub_categories = {s.category for s in subscriptions}
        sub_keywords = {kw for s in subscriptions for kw in (s.keywords or [])}

        # 4. 订阅层：匹配 category 或 keyword
        candidate_items = []
        for item in hot_items:
            if item.id in read_ids:
                continue
            if item.category in sub_categories:
                candidate_items.append(item)
                continue
            if sub_keywords and item.tags:
                if any(kw.lower() in (tag.lower() for tag in item.tags) for kw in sub_keywords):
                    candidate_items.append(item)
                    continue
            # 无订阅的用户看到所有热门内容
            if not sub_categories and not sub_keywords:
                candidate_items.append(item)

        # 5. 补足到 max_items
        if len(candidate_items) < self.max_items:
            remaining = self.max_items - len(candidate_items)
            more_items = (
                db.query(Item)
                .filter(
                    Item.id.notin_([item.id for item in candidate_items] + list(read_ids)),
                    Item.score >= 0.3,
                )
                .order_by(desc(Item.score))
                .limit(remaining)
                .all()
            )
            candidate_items.extend(more_items)

        return candidate_items[:self.max_items]
```

- [ ] **Step 2: 创建测试**

```python
# tests/test_recommender.py
import pytest
from datetime import datetime, timezone
from app.models import Item, User, Subscription, UserAction
from app.services.recommender.engine import RecommenderEngine

def test_recommender_returns_items(db):
    # 准备测试数据
    item = Item(source="arxiv", source_id="test001", title="Test Paper", url="http://example.com", score=0.9)
    user = User(email="test@example.com")
    db.add(item)
    db.add(user)
    db.commit()

    engine = RecommenderEngine()
    result = engine.get_daily_for_user(user.id, db)
    assert len(result) > 0
    assert result[0].id == item.id

def test_recommender_filters_read_items(db):
    item = Item(source="arxiv", source_id="test002", title="Read Paper", url="http://example.com", score=0.9)
    user = User(email="test2@example.com")
    db.add(item)
    db.add(user)
    db.commit()

    action = UserAction(user_id=user.id, item_id=item.id, action="view")
    db.add(action)
    db.commit()

    engine = RecommenderEngine()
    result = engine.get_daily_for_user(user.id, db)
    assert len(result) == 0  # 已读，不应返回
```

- [ ] **Step 3: 运行测试**

```bash
python -m pytest tests/test_recommender.py -v -x
```

Expected: PASS

- [ ] **Step 4: Commit**

```bash
git add engineering/apps/daily-digest/backend/app/services/recommender/
git add engineering/apps/daily-digest/backend/tests/test_recommender.py
git commit -m "feat(daily-digest): 添加推荐引擎"
```

---

### Task 8: 所有 API 路由

**Files:**
- Create: `engineering/apps/daily-digest/backend/app/routers/__init__.py`
- Create: `engineering/apps/daily-digest/backend/app/routers/daily.py`
- Create: `engineering/apps/daily-digest/backend/app/routers/search.py`
- Create: `engineering/apps/daily-digest/backend/app/routers/history.py`
- Create: `engineering/apps/daily-digest/backend/app/routers/auth.py`
- Create: `engineering/apps/daily-digest/backend/app/routers/subscriptions.py`
- Create: `engineering/apps/daily-digest/backend/app/routers/collections.py`
- Create: `engineering/apps/daily-digest/backend/app/routers/trigger.py`
- Create: `engineering/apps/daily-digest/backend/app/routers/stats.py`
- Create: `engineering/apps/daily-digest/backend/tests/test_api_daily.py`
- Create: `engineering/apps/daily-digest/backend/tests/test_api_subscriptions.py`
- Create: `engineering/apps/daily-digest/backend/tests/test_api_collections.py`
- Modify: `engineering/apps/daily-digest/backend/app/main.py`（注册路由）

**Interfaces:**
- Consumes: SQLAlchemy `Session`, 所有 Model, Pydantic Schemas
- Produces: FastAPI router 模块，每个路由函数带有 `@router.get/post/put/delete`

- [ ] **Step 1: 创建 daily.py**

```python
from fastapi import APIRouter, Depends, Query, HTTPException
from sqlalchemy.orm import Session
from sqlalchemy import desc
from app.database import get_db
from app.models.item import Item
from app.schemas.item import ItemResponse, ItemDetailResponse, ItemListResponse

router = APIRouter(prefix="/api/v1/daily", tags=["daily"])

@router.get("", response_model=ItemListResponse)
def get_daily(
    category: str = Query(None),
    page: int = Query(1, ge=1),
    size: int = Query(20, ge=1, le=50),
    db: Session = Depends(get_db),
):
    query = db.query(Item)
    if category:
        query = query.filter(Item.category == category)
    total = query.count()
    items = query.order_by(desc(Item.score), desc(Item.published)) \
                 .offset((page - 1) * size) \
                 .limit(size) \
                 .all()
    return ItemListResponse(
        items=[ItemResponse.model_validate(item) for item in items],
        total=total,
        page=page,
        size=size,
        total_pages=(total + size - 1) // size,
    )

@router.get("/{item_id}", response_model=ItemDetailResponse)
def get_item(item_id: int, db: Session = Depends(get_db)):
    item = db.query(Item).filter(Item.id == item_id).first()
    if not item:
        raise HTTPException(status_code=404, detail="Item not found")
    return ItemDetailResponse.model_validate(item)
```

- [ ] **Step 2: 创建 search.py**

```python
from fastapi import APIRouter, Depends, Query
from sqlalchemy.orm import Session
from sqlalchemy import desc, or_
from app.database import get_db
from app.models.item import Item
from app.schemas.item import ItemResponse, ItemListResponse

router = APIRouter(prefix="/api/v1/search", tags=["search"])

@router.get("", response_model=ItemListResponse)
def search(
    q: str = Query(..., min_length=1),
    category: str = Query(None),
    page: int = Query(1, ge=1),
    size: int = Query(20, ge=1, le=50),
    db: Session = Depends(get_db),
):
    query = db.query(Item).filter(
        or_(
            Item.title.ilike(f"%{q}%"),
            Item.summary.ilike(f"%{q}%"),
            Item.tags.any(q),
        )
    )
    if category:
        query = query.filter(Item.category == category)
    total = query.count()
    items = query.order_by(desc(Item.score)).offset((page - 1) * size).limit(size).all()
    return ItemListResponse(
        items=[ItemResponse.model_validate(item) for item in items],
        total=total,
        page=page,
        size=size,
        total_pages=(total + size - 1) // size,
    )
```

- [ ] **Step 3: 创建 history.py**

```python
from fastapi import APIRouter, Depends, Query
from sqlalchemy.orm import Session
from sqlalchemy import desc
from app.database import get_db
from app.models.user_action import UserAction
from app.models.item import Item
from app.schemas.item import ItemResponse, ItemListResponse

router = APIRouter(prefix="/api/v1/history", tags=["history"])

@router.get("", response_model=ItemListResponse)
def get_history(
    user_id: int = Query(...),
    page: int = Query(1, ge=1),
    size: int = Query(20, ge=1, le=50),
    db: Session = Depends(get_db),
):
    query = (
        db.query(Item)
        .join(UserAction, UserAction.item_id == Item.id)
        .filter(UserAction.user_id == user_id)
    )
    total = query.count()
    items = query.order_by(desc(UserAction.created_at)).offset((page - 1) * size).limit(size).all()
    return ItemListResponse(
        items=[ItemResponse.model_validate(item) for item in items],
        total=total,
        page=page,
        size=size,
        total_pages=(total + size - 1) // size,
    )
```

- [ ] **Step 4: 创建 auth.py**

```python
from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session
from app.database import get_db
from app.models.user import User
from app.schemas.user import WechatLoginRequest, EmailLoginRequest, UserResponse

router = APIRouter(prefix="/api/v1/auth", tags=["auth"])

@router.post("/wechat", response_model=UserResponse)
def wechat_login(req: WechatLoginRequest, db: Session = Depends(get_db)):
    # 简化版：用 code 作为 openid（实际需要调用微信接口验证）
    user = db.query(User).filter(User.wechat_openid == req.code).first()
    if not user:
        user = User(wechat_openid=req.code)
        db.add(user)
        db.commit()
        db.refresh(user)
    return UserResponse.model_validate(user)

@router.post("/email", response_model=UserResponse)
def email_login(req: EmailLoginRequest, db: Session = Depends(get_db)):
    user = db.query(User).filter(User.email == req.email).first()
    if not user:
        user = User(email=req.email)
        db.add(user)
        db.commit()
        db.refresh(user)
    user.last_active = None
    db.commit()
    return UserResponse.model_validate(user)
```

- [ ] **Step 5: 创建 subscriptions.py**

```python
from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session
from app.database import get_db
from app.models.subscription import Subscription
from app.schemas.subscription import (
    SubscriptionCreate, SubscriptionUpdate, SubscriptionResponse,
)

router = APIRouter(prefix="/api/v1/subscriptions", tags=["subscriptions"])

@router.get("", response_model=list[SubscriptionResponse])
def list_subscriptions(user_id: int, db: Session = Depends(get_db)):
    subs = db.query(Subscription).filter(Subscription.user_id == user_id).all()
    return [SubscriptionResponse.model_validate(s) for s in subs]

@router.post("", response_model=SubscriptionResponse, status_code=201)
def create_subscription(req: SubscriptionCreate, user_id: int, db: Session = Depends(get_db)):
    existing = db.query(Subscription).filter(
        Subscription.user_id == user_id,
        Subscription.category == req.category,
    ).first()
    if existing:
        raise HTTPException(status_code=409, detail="Subscription already exists")
    sub = Subscription(user_id=user_id, **req.model_dump())
    db.add(sub)
    db.commit()
    db.refresh(sub)
    return SubscriptionResponse.model_validate(sub)

@router.put("/{sub_id}", response_model=SubscriptionResponse)
def update_subscription(sub_id: int, req: SubscriptionUpdate, db: Session = Depends(get_db)):
    sub = db.query(Subscription).filter(Subscription.id == sub_id).first()
    if not sub:
        raise HTTPException(status_code=404, detail="Subscription not found")
    for key, val in req.model_dump(exclude_unset=True).items():
        setattr(sub, key, val)
    db.commit()
    db.refresh(sub)
    return SubscriptionResponse.model_validate(sub)

@router.delete("/{sub_id}", status_code=204)
def delete_subscription(sub_id: int, db: Session = Depends(get_db)):
    sub = db.query(Subscription).filter(Subscription.id == sub_id).first()
    if not sub:
        raise HTTPException(status_code=404, detail="Subscription not found")
    db.delete(sub)
    db.commit()
```

- [ ] **Step 6: 创建 collections.py**

```python
from fastapi import APIRouter, Depends, HTTPException, Query
from sqlalchemy.orm import Session
from sqlalchemy import func, desc
from app.database import get_db
from app.models.collection import Collection
from app.models.collection_item import CollectionItem
from app.models.item import Item
from app.schemas.collection import (
    CollectionCreate, CollectionUpdate, CollectionResponse,
    CollectionDetailResponse, CollectionItemCreate, CollectionItemResponse,
)
from app.schemas.item import ItemResponse

router = APIRouter(prefix="/api/v1/collections", tags=["collections"])

@router.get("", response_model=list[CollectionResponse])
def list_collections(user_id: int, db: Session = Depends(get_db)):
    cols = db.query(
        Collection,
        func.count(CollectionItem.id).label("item_count"),
    ).outerjoin(
        CollectionItem, CollectionItem.collection_id == Collection.id,
    ).filter(
        Collection.user_id == user_id,
    ).group_by(Collection.id).order_by(desc(Collection.created_at)).all()

    return [
        CollectionResponse(
            id=c.Collection.id,
            user_id=c.Collection.user_id,
            name=c.Collection.name,
            description=c.Collection.description,
            item_count=c.item_count,
            created_at=c.Collection.created_at,
        )
        for c in cols
    ]

@router.post("", response_model=CollectionResponse, status_code=201)
def create_collection(req: CollectionCreate, user_id: int, db: Session = Depends(get_db)):
    col = Collection(user_id=user_id, **req.model_dump())
    db.add(col)
    db.commit()
    db.refresh(col)
    return CollectionResponse(id=col.id, user_id=col.user_id, name=col.name, description=col.description, item_count=0, created_at=col.created_at)

@router.put("/{col_id}", response_model=CollectionResponse)
def update_collection(col_id: int, req: CollectionUpdate, db: Session = Depends(get_db)):
    col = db.query(Collection).filter(Collection.id == col_id).first()
    if not col:
        raise HTTPException(status_code=404, detail="Collection not found")
    for key, val in req.model_dump(exclude_unset=True).items():
        setattr(col, key, val)
    db.commit()
    db.refresh(col)
    return CollectionResponse(id=col.id, user_id=col.user_id, name=col.name, description=col.description, item_count=0, created_at=col.created_at)

@router.delete("/{col_id}", status_code=204)
def delete_collection(col_id: int, db: Session = Depends(get_db)):
    col = db.query(Collection).filter(Collection.id == col_id).first()
    if not col:
        raise HTTPException(status_code=404, detail="Collection not found")
    db.delete(col)
    db.commit()

@router.get("/{col_id}/items", response_model=list[ItemResponse])
def list_collection_items(col_id: int, db: Session = Depends(get_db)):
    items = (
        db.query(Item)
        .join(CollectionItem, CollectionItem.item_id == Item.id)
        .filter(CollectionItem.collection_id == col_id)
        .order_by(desc(CollectionItem.created_at))
        .all()
    )
    return [ItemResponse.model_validate(item) for item in items]

@router.post("/{col_id}/items", response_model=CollectionItemResponse, status_code=201)
def add_collection_item(col_id: int, req: CollectionItemCreate, db: Session = Depends(get_db)):
    col = db.query(Collection).filter(Collection.id == col_id).first()
    if not col:
        raise HTTPException(status_code=404, detail="Collection not found")
    existing = db.query(CollectionItem).filter(
        CollectionItem.collection_id == col_id,
        CollectionItem.item_id == req.item_id,
    ).first()
    if existing:
        raise HTTPException(status_code=409, detail="Item already in collection")
    ci = CollectionItem(collection_id=col_id, **req.model_dump())
    db.add(ci)
    db.commit()
    db.refresh(ci)
    return CollectionItemResponse.model_validate(ci)

@router.delete("/{col_id}/items/{item_id}", status_code=204)
def remove_collection_item(col_id: int, item_id: int, db: Session = Depends(get_db)):
    ci = db.query(CollectionItem).filter(
        CollectionItem.collection_id == col_id,
        CollectionItem.item_id == item_id,
    ).first()
    if not ci:
        raise HTTPException(status_code=404, detail="Collection item not found")
    db.delete(ci)
    db.commit()
```

- [ ] **Step 7: 创建 trigger.py**

```python
from fastapi import APIRouter, Depends, HTTPException, BackgroundTasks
from sqlalchemy.orm import Session
from app.database import get_db
from app.services.aggregator.orchestrator import AggregationOrchestrator
from app.services.processor import Summarizer, Classifier, Scorer
from app.models.item import Item
from app.config import settings

router = APIRouter(prefix="/api/v1/trigger", tags=["trigger"])

@router.post("/refresh")
async def trigger_refresh(background_tasks: BackgroundTasks, db: Session = Depends(get_db)):
    background_tasks.add_task(run_aggregation, db)
    return {"message": "Aggregation started"}

async def run_aggregation(db: Session):
    # 1. 聚合
    orchestrator = AggregationOrchestrator()
    raw_items = await orchestrator.run_all()
    if not raw_items:
        print("[Aggregation] no items fetched")
        return

    # 2. 分类 + 评分（无 LLM 降级模式）
    classifier = Classifier()
    scorer = Scorer()
    raw_items = classifier.classify(raw_items)
    raw_items = scorer.score(raw_items)

    # 3. 去重入库
    existing_ids = {
        row[0] for row in db.query(Item.source_id).filter(Item.source == raw_items[0].source).distinct().all()
    }
    for raw in raw_items:
        if raw.source_id in existing_ids:
            continue
        item = Item(
            source=raw.source,
            source_id=raw.source_id,
            title=raw.title,
            url=raw.url,
            summary=raw.summary,
            raw_content=raw.raw_content,
            category=raw.category,
            tags=raw.tags,
            score=raw.score,
            published=raw.published,
        )
        db.add(item)

    db.commit()
    print(f"[Aggregation] saved {len(raw_items)} items")
```

- [ ] **Step 8: 创建 stats.py**

```python
from fastapi import APIRouter, Depends
from sqlalchemy.orm import Session
from sqlalchemy import func, cast, Date
from app.database import get_db
from app.models.item import Item
from app.models.user import User
from app.models.subscription import Subscription

router = APIRouter(prefix="/api/v1/stats", tags=["stats"])

@router.get("")
def get_stats(db: Session = Depends(get_db)):
    today_count = db.query(func.count(Item.id)).filter(
        cast(Item.created_at, Date) == func.current_date()
    ).scalar()
    total_items = db.query(func.count(Item.id)).scalar()
    total_users = db.query(func.count(User.id)).scalar()
    total_subs = db.query(func.count(Subscription.id)).scalar()

    return {
        "today_new": today_count,
        "total_items": total_items,
        "total_users": total_users,
        "total_subscriptions": total_subs,
    }
```

- [ ] **Step 9: 创建 routers/__init__.py**

```python
from app.routers import daily, search, history, auth, subscriptions, collections, trigger, stats

__all__ = ["daily", "search", "history", "auth", "subscriptions", "collections", "trigger", "stats"]
```

- [ ] **Step 10: 更新 main.py 注册路由**

```python
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from app.config import settings
from app.routers import daily, search, history, auth, subscriptions, collections, trigger, stats

app = FastAPI(title="DailyDigest API", version="0.1.0")

app.add_middleware(
    CORSMiddleware,
    allow_origins=settings.cors_origins.split(","),
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(daily.router)
app.include_router(search.router)
app.include_router(history.router)
app.include_router(auth.router)
app.include_router(subscriptions.router)
app.include_router(collections.router)
app.include_router(trigger.router)
app.include_router(stats.router)

@app.get("/health")
def health():
    return {"status": "ok", "version": "0.1.0"}
```

- [ ] **Step 11: 创建 API 测试**

```python
# tests/test_api_daily.py
from fastapi.testclient import TestClient
from app.main import app

client = TestClient(app)

def test_health():
    resp = client.get("/health")
    assert resp.status_code == 200
    assert resp.json()["status"] == "ok"

def test_get_daily():
    resp = client.get("/api/v1/daily?page=1&size=10")
    assert resp.status_code == 200
    data = resp.json()
    assert "items" in data
    assert "total" in data
```

```python
# tests/test_api_subscriptions.py
from fastapi.testclient import TestClient
from app.main import app

client = TestClient(app)

def test_create_subscription():
    resp = client.post(
        "/api/v1/subscriptions",
        params={"user_id": 1},
        json={"category": "db", "keywords": ["LSM-Tree", "Raft"]},
    )
    assert resp.status_code == 201
    data = resp.json()
    assert data["category"] == "db"
```

```python
# tests/test_api_collections.py
from fastapi.testclient import TestClient
from app.main import app

client = TestClient(app)

def test_create_collection():
    resp = client.post(
        "/api/v1/collections",
        params={"user_id": 1},
        json={"name": "论文精读", "description": "值得细读的论文"},
    )
    assert resp.status_code == 201
    data = resp.json()
    assert data["name"] == "论文精读"
```

- [ ] **Step 12: 运行所有 API 测试**

```bash
python -m pytest tests/test_api_daily.py tests/test_api_subscriptions.py tests/test_api_collections.py -v -x
```

Expected: 所有测试 PASS

- [ ] **Step 13: Commit**

```bash
git add engineering/apps/daily-digest/backend/app/routers/
git add engineering/apps/daily-digest/backend/tests/test_api_*.py
git add engineering/apps/daily-digest/backend/app/main.py
git commit -m "feat(daily-digest): 添加所有 API 路由与测试"
```

---

### Task 9: 定时任务调度器

**Files:**
- Create: `engineering/apps/daily-digest/backend/app/scheduler.py`
- Modify: `engineering/apps/daily-digest/backend/app/main.py`（启动时注册定时任务）

**Interfaces:**
- Consumes: `run_aggregation` 函数（来自 trigger.py）
- Produces: APScheduler 在 FastAPI 启动时注册，每天 08:00 执行一次聚合

- [ ] **Step 1: 创建 scheduler.py**

```python
from apscheduler.schedulers.asyncio import AsyncIOScheduler
from apscheduler.triggers.cron import CronTrigger
from app.database import SessionLocal
from app.services.aggregator.orchestrator import AggregationOrchestrator
from app.services.processor import Classifier, Scorer
from app.models.item import Item

scheduler = AsyncIOScheduler()

async def daily_aggregation_job():
    print(f"[Scheduler] starting daily aggregation at ...")
    db = SessionLocal()
    try:
        orchestrator = AggregationOrchestrator()
        raw_items = await orchestrator.run_all()
        if not raw_items:
            print("[Scheduler] no items fetched")
            return

        classifier = Classifier()
        scorer = Scorer()
        raw_items = classifier.classify(raw_items)
        raw_items = scorer.score(raw_items)

        # 去重
        existing_ids = set()
        for raw in raw_items:
            row = db.query(Item.id).filter(
                Item.source == raw.source,
                Item.source_id == raw.source_id,
            ).first()
            if row:
                existing_ids.add(raw.source_id)

        count = 0
        for raw in raw_items:
            if raw.source_id in existing_ids:
                continue
            item = Item(
                source=raw.source,
                source_id=raw.source_id,
                title=raw.title,
                url=raw.url,
                summary=raw.summary,
                raw_content=raw.raw_content,
                category=raw.category,
                tags=raw.tags,
                score=raw.score,
                published=raw.published,
            )
            db.add(item)
            count += 1

        db.commit()
        print(f"[Scheduler] saved {count} new items")
    except Exception as e:
        print(f"[Scheduler] job failed: {e}")
    finally:
        db.close()

def init_scheduler(app):
    @app.on_event("startup")
    async def start_scheduler():
        scheduler.add_job(
            daily_aggregation_job,
            CronTrigger(hour=8, minute=0),
            id="daily_aggregation",
            replace_existing=True,
        )
        scheduler.start()
        print("[Scheduler] started, daily aggregation at 08:00")

    @app.on_event("shutdown")
    async def stop_scheduler():
        scheduler.shutdown()
```

- [ ] **Step 2: 更新 main.py 注册调度器**

```python
from app.scheduler import init_scheduler

# ...（之前代码）

init_scheduler(app)
```

- [ ] **Step 3: 验证启动**

```bash
python -c "from app.scheduler import scheduler; print('Scheduler OK')"
```

Expected: `Scheduler OK`

- [ ] **Step 4: Commit**

```bash
git add engineering/apps/daily-digest/backend/app/scheduler.py
git add engineering/apps/daily-digest/backend/app/main.py
git commit -m "feat(daily-digest): 添加 APScheduler 定时任务"
```

---

## Phase 2：Vue 3 Web 前端

### Task 10: 前端项目脚手架

**Files:**
- Create: `engineering/apps/daily-digest/web/package.json`
- Create: `engineering/apps/daily-digest/web/vite.config.ts`
- Create: `engineering/apps/daily-digest/web/tsconfig.json`
- Create: `engineering/apps/daily-digest/web/index.html`
- Create: `engineering/apps/daily-digest/web/src/main.ts`
- Create: `engineering/apps/daily-digest/web/src/App.vue`
- Create: `engineering/apps/daily-digest/web/src/api/index.ts`
- Create: `engineering/apps/daily-digest/web/src/router/index.ts`
- Create: `engineering/apps/daily-digest/web/src/styles/main.css`

**Interfaces:**
- Consumes: 后端 API（`http://localhost:8000/api/v1/...`）
- Produces: Vue 3 应用骨架，包含路由和 API 客户端

- [ ] **Step 1: 创建 package.json**

```json
{
  "name": "daily-digest-web",
  "version": "0.1.0",
  "type": "module",
  "scripts": {
    "dev": "vite",
    "build": "vue-tsc --noEmit && vite build",
    "preview": "vite preview"
  },
  "dependencies": {
    "vue": "^3.4.0",
    "vue-router": "^4.3.0"
  },
  "devDependencies": {
    "@vitejs/plugin-vue": "^5.0.0",
    "typescript": "^5.4.0",
    "vite": "^5.4.0",
    "vue-tsc": "^2.0.0"
  }
}
```

- [ ] **Step 2: 创建 vite.config.ts**

```typescript
import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

export default defineConfig({
  plugins: [vue()],
  server: {
    port: 5173,
    proxy: {
      '/api': {
        target: 'http://localhost:8000',
        changeOrigin: true,
      },
    },
  },
})
```

- [ ] **Step 3: 创建 tsconfig.json**

```json
{
  "compilerOptions": {
    "target": "ES2020",
    "module": "ESNext",
    "moduleResolution": "bundler",
    "strict": true,
    "jsx": "preserve",
    "resolveJsonModule": true,
    "isolatedModules": true,
    "esModuleInterop": true,
    "lib": ["ES2020", "DOM", "DOM.Iterable"],
    "skipLibCheck": true,
    "noEmit": true,
    "paths": {
      "@/*": ["./src/*"]
    }
  },
  "include": ["src/**/*.ts", "src/**/*.vue"]
}
```

- [ ] **Step 4: 创建 index.html**

```html
<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>DailyDigest — 每日数据库 & AI 速览</title>
  <link rel="stylesheet" href="/src/styles/main.css" />
</head>
<body>
  <div id="app"></div>
  <script type="module" src="/src/main.ts"></script>
</body>
</html>
```

- [ ] **Step 5: 创建 main.ts**

```typescript
import { createApp } from 'vue'
import App from './App.vue'
import router from './router'

const app = createApp(App)
app.use(router)
app.mount('#app')
```

- [ ] **Step 6: 创建 App.vue**

```vue
<template>
  <div id="app-root">
    <NavBar />
    <main class="main-content">
      <router-view />
    </main>
  </div>
</template>

<script setup lang="ts">
import NavBar from './components/NavBar.vue'
</script>
```

- [ ] **Step 7: 创建 api/index.ts**

```typescript
const API_BASE = '/api/v1'

async function request<T>(path: string, options?: RequestInit): Promise<T> {
  const resp = await fetch(`${API_BASE}${path}`, {
    headers: { 'Content-Type': 'application/json' },
    ...options,
  })
  if (!resp.ok) throw new Error(`API error: ${resp.status}`)
  return resp.json()
}

export interface Item {
  id: number
  source: string
  source_id: string
  title: string
  url: string
  summary: string | null
  category: string | null
  tags: string[] | null
  score: number
  published: string | null
  created_at: string
}

export interface PaginatedResponse<T> {
  items: T[]
  total: number
  page: number
  size: number
  total_pages: number
}

export const api = {
  daily: {
    list: (params?: { category?: string; page?: number; size?: number }) => {
      const q = new URLSearchParams()
      if (params?.category) q.set('category', params.category)
      q.set('page', String(params?.page ?? 1))
      q.set('size', String(params?.size ?? 20))
      return request<PaginatedResponse<Item>>(`/daily?${q}`)
    },
    detail: (id: number) => request<Item>(`/daily/${id}`),
  },
  search: (q: string, category?: string) => {
    const params = new URLSearchParams({ q })
    if (category) params.set('category', category)
    return request<PaginatedResponse<Item>>(`/search?${params}`)
  },
  history: (userId: number, page = 1) => {
    return request<PaginatedResponse<Item>>(`/history?user_id=${userId}&page=${page}`)
  },
  subscriptions: {
    list: (userId: number) => request<any[]>(`/subscriptions?user_id=${userId}`),
    create: (userId: number, data: { category: string; keywords?: string[] }) =>
      request<any>(`/subscriptions?user_id=${userId}`, { method: 'POST', body: JSON.stringify(data) }),
    delete: (id: number) => request<void>(`/subscriptions/${id}`, { method: 'DELETE' }),
  },
  collections: {
    list: (userId: number) => request<any[]>(`/collections?user_id=${userId}`),
    create: (userId: number, data: { name: string; description?: string }) =>
      request<any>(`/collections?user_id=${userId}`, { method: 'POST', body: JSON.stringify(data) }),
    delete: (id: number) => request<void>(`/collections/${id}`, { method: 'DELETE' }),
    items: (colId: number) => request<Item[]>(`/collections/${colId}/items`),
    addItem: (colId: number, itemId: number) =>
      request<any>(`/collections/${colId}/items`, { method: 'POST', body: JSON.stringify({ item_id: itemId }) }),
    removeItem: (colId: number, itemId: number) =>
      request<void>(`/collections/${colId}/items/${itemId}`, { method: 'DELETE' }),
  },
  stats: () => request<any>('/stats'),
}
```

- [ ] **Step 8: 创建 router/index.ts**

```typescript
import { createRouter, createWebHistory } from 'vue-router'
import DailyView from '../pages/DailyView.vue'
import ItemDetail from '../pages/ItemDetail.vue'
import Search from '../pages/Search.vue'
import Collections from '../pages/Collections.vue'
import CollectionDetail from '../pages/CollectionDetail.vue'
import Subscriptions from '../pages/Subscriptions.vue'
import History from '../pages/History.vue'
import Settings from '../pages/Settings.vue'

const routes = [
  { path: '/', name: 'daily', component: DailyView },
  { path: '/item/:id', name: 'item-detail', component: ItemDetail },
  { path: '/search', name: 'search', component: Search },
  { path: '/collections', name: 'collections', component: Collections },
  { path: '/collections/:id', name: 'collection-detail', component: CollectionDetail },
  { path: '/subscriptions', name: 'subscriptions', component: Subscriptions },
  { path: '/history', name: 'history', component: History },
  { path: '/settings', name: 'settings', component: Settings },
]

export default createRouter({
  history: createWebHistory(),
  routes,
})
```

- [ ] **Step 9: 创建 main.css（基础样式，UI 设计后会替换）**

```css
* { margin: 0; padding: 0; box-sizing: border-box; }
body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif; background: #f5f5f5; color: #333; }
a { color: #1a73e8; text-decoration: none; }
.main-content { max-width: 960px; margin: 0 auto; padding: 20px; }
```

- [ ] **Step 10: 安装依赖并验证**

```bash
cd web
npm install
npx vue-tsc --noEmit
npm run build
```

Expected: 构建成功，输出到 `web/dist/`

- [ ] **Step 11: Commit**

```bash
git add engineering/apps/daily-digest/web/
git commit -m "feat(daily-digest): 初始化 Vue 3 前端脚手架"
```

---

### Task 11: 前端核心组件（NavBar、ItemCard、CategoryTabs、Pagination）

**Files:**
- Create: `engineering/apps/daily-digest/web/src/components/NavBar.vue`
- Create: `engineering/apps/daily-digest/web/src/components/ItemCard.vue`
- Create: `engineering/apps/daily-digest/web/src/components/CategoryTabs.vue`
- Create: `engineering/apps/daily-digest/web/src/components/Pagination.vue`

**Interfaces:**
- Consumes: `api/index.ts` 中的类型
- Produces: 四个可复用组件，供各页面使用

- [ ] **Step 1: 创建 NavBar.vue**

```vue
<template>
  <nav class="navbar">
    <div class="navbar-inner">
      <router-link to="/" class="brand">DailyDigest</router-link>
      <div class="nav-links">
        <router-link to="/">每日速览</router-link>
        <router-link to="/search">搜索</router-link>
        <router-link to="/collections">收藏夹</router-link>
        <router-link to="/subscriptions">订阅</router-link>
        <router-link to="/history">历史</router-link>
        <router-link to="/settings">设置</router-link>
      </div>
    </div>
  </nav>
</template>

<style scoped>
.navbar { background: #fff; border-bottom: 1px solid #e0e0e0; position: sticky; top: 0; z-index: 100; }
.navbar-inner { max-width: 960px; margin: 0 auto; display: flex; align-items: center; padding: 0 20px; height: 56px; }
.brand { font-size: 18px; font-weight: 700; color: #1a73e8; margin-right: 32px; }
.nav-links { display: flex; gap: 16px; }
.nav-links a { color: #555; font-size: 14px; padding: 4px 8px; border-radius: 4px; }
.nav-links a:hover { background: #f0f0f0; }
.nav-links a.router-link-active { color: #1a73e8; font-weight: 500; }
</style>
```

- [ ] **Step 2: 创建 ItemCard.vue**

```vue
<template>
  <div class="item-card" @click="$router.push(`/item/${item.id}`)">
    <div class="card-header">
      <span class="source-badge">{{ sourceLabel }}</span>
      <span class="category-badge" v-if="item.category">{{ item.category }}</span>
      <span class="score" v-if="item.score">★ {{ item.score.toFixed(2) }}</span>
    </div>
    <h3 class="card-title">{{ item.title }}</h3>
    <p class="card-summary" v-if="item.summary">{{ truncate(item.summary, 120) }}</p>
    <div class="card-footer">
      <span class="date">{{ formatDate(item.published) }}</span>
      <div class="tags" v-if="item.tags && item.tags.length">
        <span v-for="tag in item.tags.slice(0, 3)" :key="tag" class="tag">{{ tag }}</span>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue'
import type { Item } from '../api'

const props = defineProps<{ item: Item }>()

const sourceLabels: Record<string, string> = {
  arxiv: 'arXiv',
  semantic_scholar: 'Semantic Scholar',
  dblp: 'DBLP',
  huggingface: 'HuggingFace',
  blog: '博客',
  github: 'GitHub',
}

const sourceLabel = computed(() => sourceLabels[props.item.source] || props.item.source)

function truncate(text: string, max: number): string {
  return text.length > max ? text.slice(0, max) + '...' : text
}

function formatDate(dateStr: string | null): string {
  if (!dateStr) return ''
  return new Date(dateStr).toLocaleDateString('zh-CN')
}
</script>

<style scoped>
.item-card { background: #fff; border-radius: 8px; padding: 16px; margin-bottom: 12px; box-shadow: 0 1px 3px rgba(0,0,0,0.08); cursor: pointer; transition: box-shadow 0.2s; }
.item-card:hover { box-shadow: 0 2px 8px rgba(0,0,0,0.12); }
.card-header { display: flex; align-items: center; gap: 8px; margin-bottom: 8px; }
.source-badge { font-size: 11px; background: #e8f0fe; color: #1a73e8; padding: 2px 8px; border-radius: 4px; }
.category-badge { font-size: 11px; background: #e6f4ea; color: #1e7e34; padding: 2px 8px; border-radius: 4px; }
.score { margin-left: auto; font-size: 12px; color: #f9ab00; }
.card-title { font-size: 16px; font-weight: 600; margin-bottom: 6px; line-height: 1.4; }
.card-summary { font-size: 14px; color: #666; line-height: 1.5; margin-bottom: 8px; }
.card-footer { display: flex; align-items: center; gap: 8px; }
.date { font-size: 12px; color: #999; }
.tags { display: flex; gap: 4px; }
.tag { font-size: 11px; background: #f0f0f0; padding: 2px 6px; border-radius: 4px; color: #555; }
</style>
```

- [ ] **Step 3: 创建 CategoryTabs.vue**

```vue
<template>
  <div class="category-tabs">
    <button v-for="tab in tabs" :key="tab.key" :class="['tab', { active: active === tab.key }]" @click="$emit('change', tab.key)">
      {{ tab.label }}
    </button>
  </div>
</template>

<script setup lang="ts">
defineProps<{ active: string }>()
defineEmits<{ change: [key: string] }>()

const tabs = [
  { key: '', label: '全部' },
  { key: 'db', label: '数据库' },
  { key: 'ai', label: 'AI' },
  { key: 'ml', label: '机器学习' },
  { key: 'infra', label: '基础设施' },
  { key: 'sys', label: '系统' },
]
</script>

<style scoped>
.category-tabs { display: flex; gap: 4px; margin-bottom: 16px; overflow-x: auto; }
.tab { padding: 8px 16px; border: none; background: #f0f0f0; border-radius: 20px; cursor: pointer; font-size: 14px; white-space: nowrap; }
.tab.active { background: #1a73e8; color: #fff; }
.tab:hover:not(.active) { background: #e0e0e0; }
</style>
```

- [ ] **Step 4: 创建 Pagination.vue**

```vue
<template>
  <div class="pagination" v-if="totalPages > 1">
    <button :disabled="page <= 1" @click="$emit('change', page - 1)">上一页</button>
    <span class="page-info">{{ page }} / {{ totalPages }}</span>
    <button :disabled="page >= totalPages" @click="$emit('change', page + 1)">下一页</button>
  </div>
</template>

<script setup lang="ts">
defineProps<{ page: number; totalPages: number }>()
defineEmits<{ change: [page: number] }>()
</script>

<style scoped>
.pagination { display: flex; justify-content: center; align-items: center; gap: 12px; padding: 20px 0; }
button { padding: 8px 16px; border: 1px solid #ddd; background: #fff; border-radius: 4px; cursor: pointer; }
button:disabled { opacity: 0.5; cursor: not-allowed; }
.page-info { font-size: 14px; color: #666; }
</style>
```

- [ ] **Step 5: 验证组件编译**

```bash
cd web
npx vue-tsc --noEmit
```

Expected: 无类型错误

- [ ] **Step 6: Commit**

```bash
git add engineering/apps/daily-digest/web/src/components/
git commit -m "feat(daily-digest): 添加前端核心组件（NavBar/ItemCard/CategoryTabs/Pagination）"
```

---

### Task 12: 前端页面（每日速览 + 详情页 + 搜索）

**Files:**
- Create: `engineering/apps/daily-digest/web/src/pages/DailyView.vue`
- Create: `engineering/apps/daily-digest/web/src/pages/ItemDetail.vue`
- Create: `engineering/apps/daily-digest/web/src/pages/Search.vue`

**Interfaces:**
- Consumes: `api` 客户端、`ItemCard` 等组件
- Produces: 三个核心页面

- [ ] **Step 1: 创建 DailyView.vue**

```vue
<template>
  <div class="daily-view">
    <h1 class="page-title">今日速览</h1>
    <CategoryTabs :active="category" @change="switchCategory" />
    <div v-if="loading" class="loading">加载中...</div>
    <div v-else-if="items.length === 0" class="empty">今日暂无更新</div>
    <template v-else>
      <ItemCard v-for="item in items" :key="item.id" :item="item" />
      <Pagination :page="page" :total-pages="totalPages" @change="changePage" />
    </template>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, watch } from 'vue'
import { api, type Item } from '../api'
import ItemCard from '../components/ItemCard.vue'
import CategoryTabs from '../components/CategoryTabs.vue'
import Pagination from '../components/Pagination.vue'

const items = ref<Item[]>([])
const loading = ref(false)
const category = ref('')
const page = ref(1)
const totalPages = ref(1)

async function loadDaily() {
  loading.value = true
  try {
    const data = await api.daily.list({ category: category.value || undefined, page: page.value })
    items.value = data.items
    totalPages.value = data.total_pages
  } finally {
    loading.value = false
  }
}

function switchCategory(cat: string) {
  category.value = cat
  page.value = 1
  loadDaily()
}

function changePage(p: number) {
  page.value = p
  loadDaily()
}

onMounted(loadDaily)
</script>

<style scoped>
.page-title { font-size: 24px; font-weight: 700; margin-bottom: 16px; }
.loading, .empty { text-align: center; padding: 60px 0; color: #999; }
</style>
```

- [ ] **Step 2: 创建 ItemDetail.vue**

```vue
<template>
  <div class="item-detail" v-if="item">
    <router-link to="/" class="back-link">← 返回速览</router-link>
    <div class="detail-header">
      <span class="source-badge">{{ sourceLabel }}</span>
      <span class="category-badge" v-if="item.category">{{ item.category }}</span>
    </div>
    <h1 class="detail-title">{{ item.title }}</h1>
    <div class="detail-meta">
      <span>{{ formatDate(item.published) }}</span>
      <span v-if="item.score">评分：{{ item.score.toFixed(2) }}</span>
    </div>
    <div class="detail-summary" v-if="item.summary">
      <h3>摘要</h3>
      <p>{{ item.summary }}</p>
    </div>
    <div class="tags" v-if="item.tags && item.tags.length">
      <span v-for="tag in item.tags" :key="tag" class="tag">{{ tag }}</span>
    </div>
    <div class="detail-actions">
      <a :href="item.url" target="_blank" class="btn btn-primary">查看原文</a>
      <button class="btn btn-secondary" @click="showSearch = true">深度搜索</button>
    </div>
    <div v-if="showSearch" class="deep-search">
      <h3>深度搜索</h3>
      <p>正在搜索与 "{{ item.title }}" 相关的更多内容...</p>
      <a :href="`https://scholar.google.com/scholar?q=${encodeURIComponent(item.title)}`" target="_blank">Google Scholar</a>
      |
      <a :href="`https://arxiv.org/search/?query=${encodeURIComponent(item.title)}`" target="_blank">arXiv</a>
      |
      <a :href="`https://www.semanticscholar.org/search?q=${encodeURIComponent(item.title)}`" target="_blank">Semantic Scholar</a>
    </div>
  </div>
  <div v-else class="loading">加载中...</div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted } from 'vue'
import { useRoute } from 'vue-router'
import { api, type Item } from '../api'

const route = useRoute()
const item = ref<Item | null>(null)
const showSearch = ref(false)

const sourceLabels: Record<string, string> = {
  arxiv: 'arXiv', semantic_scholar: 'Semantic Scholar', dblp: 'DBLP',
  huggingface: 'HuggingFace', blog: '博客', github: 'GitHub',
}
const sourceLabel = computed(() => sourceLabels[item.value?.source || ''] || item.value?.source)

function formatDate(dateStr: string | null): string {
  if (!dateStr) return ''
  return new Date(dateStr).toLocaleDateString('zh-CN')
}

onMounted(async () => {
  const id = Number(route.params.id)
  item.value = await api.daily.detail(id)
})
</script>
```

- [ ] **Step 3: 创建 Search.vue**

```vue
<template>
  <div class="search-page">
    <h1 class="page-title">搜索</h1>
    <div class="search-bar">
      <input v-model="query" @keyup.enter="doSearch" placeholder="搜索技术内容..." class="search-input" />
      <button @click="doSearch" class="btn btn-primary">搜索</button>
    </div>
    <CategoryTabs :active="category" @change="c => { category = c; doSearch() }" />
    <div v-if="loading" class="loading">搜索中...</div>
    <div v-else-if="results.length > 0">
      <ItemCard v-for="item in results" :key="item.id" :item="item" />
    </div>
    <div v-else-if="searched" class="empty">未找到相关内容</div>
  </div>
</template>

<script setup lang="ts">
import { ref } from 'vue'
import { api, type Item } from '../api'
import ItemCard from '../components/ItemCard.vue'
import CategoryTabs from '../components/CategoryTabs.vue'

const query = ref('')
const category = ref('')
const results = ref<Item[]>([])
const loading = ref(false)
const searched = ref(false)

async function doSearch() {
  if (!query.value.trim()) return
  loading.value = true
  searched.value = true
  try {
    const data = await api.search(query.value, category.value || undefined)
    results.value = data.items
  } finally {
    loading.value = false
  }
}
</script>

<style scoped>
.search-bar { display: flex; gap: 8px; margin-bottom: 16px; }
.search-input { flex: 1; padding: 10px 16px; border: 1px solid #ddd; border-radius: 6px; font-size: 16px; }
.loading, .empty { text-align: center; padding: 60px 0; color: #999; }
</style>
```

- [ ] **Step 4: 验证编译**

```bash
cd web
npx vue-tsc --noEmit
```

Expected: 无错误

- [ ] **Step 5: Commit**

```bash
git add engineering/apps/daily-digest/web/src/pages/DailyView.vue
git add engineering/apps/daily-digest/web/src/pages/ItemDetail.vue
git add engineering/apps/daily-digest/web/src/pages/Search.vue
git commit -m "feat(daily-digest): 添加前端页面（每日速览/详情页/搜索）"
```

---

### Task 13: 前端页面（收藏夹/订阅/历史/设置）

**Files:**
- Create: `engineering/apps/daily-digest/web/src/pages/Collections.vue`
- Create: `engineering/apps/daily-digest/web/src/pages/CollectionDetail.vue`
- Create: `engineering/apps/daily-digest/web/src/pages/Subscriptions.vue`
- Create: `engineering/apps/daily-digest/web/src/pages/History.vue`
- Create: `engineering/apps/daily-digest/web/src/pages/Settings.vue`

**Interfaces:**
- Consumes: `api` 客户端
- Produces: 五个功能页面

- [ ] **Step 1: 创建 Collections.vue**

```vue
<template>
  <div class="collections-page">
    <h1 class="page-title">收藏夹</h1>
    <button class="btn btn-primary" @click="showCreate = true">+ 新建收藏夹</button>
    <div v-if="loading" class="loading">加载中...</div>
    <div v-else-if="collections.length === 0" class="empty">还没有收藏夹</div>
    <div v-else class="collection-grid">
      <div v-for="col in collections" :key="col.id" class="collection-card" @click="$router.push(`/collections/${col.id}`)">
        <h3>{{ col.name }}</h3>
        <p v-if="col.description">{{ col.description }}</p>
        <span class="item-count">{{ col.item_count }} 条内容</span>
      </div>
    </div>
    <div v-if="showCreate" class="modal">
      <input v-model="newName" placeholder="收藏夹名称" />
      <input v-model="newDesc" placeholder="描述（可选）" />
      <button @click="createCollection">创建</button>
      <button @click="showCreate = false">取消</button>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { api } from '../api'

const collections = ref<any[]>([])
const loading = ref(false)
const showCreate = ref(false)
const newName = ref('')
const newDesc = ref('')

async function loadCollections() {
  loading.value = true
  try {
    const userId = Number(localStorage.getItem('user_id') || 1)
    collections.value = await api.collections.list(userId)
  } finally {
    loading.value = false
  }
}

async function createCollection() {
  if (!newName.value.trim()) return
  const userId = Number(localStorage.getItem('user_id') || 1)
  await api.collections.create(userId, { name: newName.value, description: newDesc.value })
  showCreate.value = false
  newName.value = ''
  newDesc.value = ''
  loadCollections()
}

onMounted(loadCollections)
</script>
```

- [ ] **Step 2: 创建 CollectionDetail.vue**

```vue
<template>
  <div class="collection-detail">
    <router-link to="/collections" class="back-link">← 返回收藏夹</router-link>
    <h1 class="page-title">{{ collection?.name }}</h1>
    <div v-if="loading" class="loading">加载中...</div>
    <div v-else-if="items.length === 0" class="empty">收藏夹为空</div>
    <ItemCard v-for="item in items" :key="item.id" :item="item" />
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { useRoute } from 'vue-router'
import { api, type Item } from '../api'
import ItemCard from '../components/ItemCard.vue'

const route = useRoute()
const collection = ref<any>(null)
const items = ref<Item[]>([])
const loading = ref(false)

onMounted(async () => {
  const colId = Number(route.params.id)
  loading.value = true
  try {
    items.value = await api.collections.items(colId)
  } finally {
    loading.value = false
  }
})
</script>
```

- [ ] **Step 3: 创建 Subscriptions.vue**

```vue
<template>
  <div class="subscriptions-page">
    <h1 class="page-title">订阅管理</h1>
    <div class="subscription-list">
      <div v-for="sub in subscriptions" :key="sub.id" class="sub-item">
        <span class="sub-category">{{ categoryLabel(sub.category) }}</span>
        <span class="sub-keywords" v-if="sub.keywords?.length">{{ sub.keywords.join(', ') }}</span>
        <button class="btn btn-danger btn-sm" @click="deleteSub(sub.id)">取消订阅</button>
      </div>
    </div>
    <div class="add-sub">
      <select v-model="newCategory">
        <option value="db">数据库</option>
        <option value="ai">AI</option>
        <option value="ml">机器学习</option>
        <option value="infra">基础设施</option>
      </select>
      <input v-model="newKeywords" placeholder="关键词（逗号分隔）" />
      <button class="btn btn-primary" @click="addSubscription">添加订阅</button>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { api } from '../api'

const subscriptions = ref<any[]>([])
const newCategory = ref('db')
const newKeywords = ref('')

const categoryLabels: Record<string, string> = { db: '数据库', ai: 'AI', ml: '机器学习', infra: '基础设施' }
const categoryLabel = (cat: string) => categoryLabels[cat] || cat

async function loadSubs() {
  const userId = Number(localStorage.getItem('user_id') || 1)
  subscriptions.value = await api.subscriptions.list(userId)
}

async function addSubscription() {
  const userId = Number(localStorage.getItem('user_id') || 1)
  const keywords = newKeywords.value.split(',').map(k => k.trim()).filter(Boolean)
  await api.subscriptions.create(userId, { category: newCategory.value, keywords })
  newKeywords.value = ''
  loadSubs()
}

async function deleteSub(id: number) {
  await api.subscriptions.delete(id)
  loadSubs()
}

onMounted(loadSubs)
</script>
```

- [ ] **Step 4: 创建 History.vue**

```vue
<template>
  <div class="history-page">
    <h1 class="page-title">阅读历史</h1>
    <div v-if="loading" class="loading">加载中...</div>
    <div v-else-if="items.length === 0" class="empty">还没有阅读记录</div>
    <ItemCard v-for="item in items" :key="item.id" :item="item" />
    <Pagination :page="page" :total-pages="totalPages" @change="changePage" />
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { api, type Item } from '../api'
import ItemCard from '../components/ItemCard.vue'
import Pagination from '../components/Pagination.vue'

const items = ref<Item[]>([])
const loading = ref(false)
const page = ref(1)
const totalPages = ref(1)

async function loadHistory() {
  loading.value = true
  try {
    const userId = Number(localStorage.getItem('user_id') || 1)
    const data = await api.history(userId, page.value)
    items.value = data.items
    totalPages.value = data.total_pages
  } finally {
    loading.value = false
  }
}

function changePage(p: number) {
  page.value = p
  loadHistory()
}

onMounted(loadHistory)
</script>
```

- [ ] **Step 5: 创建 Settings.vue**

```vue
<template>
  <div class="settings-page">
    <h1 class="page-title">设置</h1>
    <div class="setting-section">
      <h3>账号</h3>
      <p>用户 ID: {{ userId }}</p>
    </div>
    <div class="setting-section">
      <h3>推送偏好</h3>
      <label>推送时间：<input type="time" v-model="pushTime" /></label>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref } from 'vue'

const userId = ref(Number(localStorage.getItem('user_id') || 1))
const pushTime = ref('08:00')
</script>

<style scoped>
.setting-section { background: #fff; border-radius: 8px; padding: 16px; margin-bottom: 12px; }
.setting-section h3 { margin-bottom: 8px; }
</style>
```

- [ ] **Step 6: 验证编译**

```bash
cd web
npx vue-tsc --noEmit && npm run build
```

Expected: 构建成功

- [ ] **Step 7: Commit**

```bash
git add engineering/apps/daily-digest/web/src/pages/
git commit -m "feat(daily-digest): 添加前端页面（收藏夹/订阅/历史/设置）"
```

---

## Phase 3：微信小程序

### Task 14: 微信小程序项目初始化

**Files:**
- Create: `engineering/apps/daily-digest/miniapp/app.json`
- Create: `engineering/apps/daily-digest/miniapp/project.config.json`
- Create: `engineering/apps/daily-digest/miniapp/app.js`
- Create: `engineering/apps/daily-digest/miniapp/app.wxss`
- Create: `engineering/apps/daily-digest/miniapp/pages/index/index.js`
- Create: `engineering/apps/daily-digest/miniapp/pages/index/index.wxml`
- Create: `engineering/apps/daily-digest/miniapp/pages/index/index.wxss`
- Create: `engineering/apps/daily-digest/miniapp/pages/discover/discover.js`
- Create: `engineering/apps/daily-digest/miniapp/pages/discover/discover.wxml`
- Create: `engineering/apps/daily-digest/miniapp/pages/discover/discover.wxss`
- Create: `engineering/apps/daily-digest/miniapp/pages/collections/collections.js`
- Create: `engineering/apps/daily-digest/miniapp/pages/collections/collections.wxml`
- Create: `engineering/apps/daily-digest/miniapp/pages/collections/collections.wxss`
- Create: `engineering/apps/daily-digest/miniapp/pages/mine/mine.js`
- Create: `engineering/apps/daily-digest/miniapp/pages/mine/mine.wxml`
- Create: `engineering/apps/daily-digest/miniapp/pages/mine/mine.wxss`

**Interfaces:**
- Consumes: 后端 API（需要配置 HTTPS 域名）
- Produces: 微信小程序 4 个 Tab 页面

- [ ] **Step 1: 创建 app.json**

```json
{
  "pages": [
    "pages/index/index",
    "pages/discover/discover",
    "pages/collections/collections",
    "pages/mine/mine"
  ],
  "window": {
    "navigationBarTitleText": "DailyDigest",
    "navigationBarBackgroundColor": "#ffffff"
  },
  "tabBar": {
    "list": [
      { "pagePath": "pages/index/index", "text": "每日", "iconPath": "images/daily.png" },
      { "pagePath": "pages/discover/discover", "text": "发现", "iconPath": "images/discover.png" },
      { "pagePath": "pages/collections/collections", "text": "收藏", "iconPath": "images/collections.png" },
      { "pagePath": "pages/mine/mine", "text": "我的", "iconPath": "images/mine.png" }
    ]
  },
  "style": "v2",
  "sitemapLocation": "sitemap.json"
}
```

- [ ] **Step 2: 创建 project.config.json**

```json
{
  "description": "DailyDigest 微信小程序",
  "packOptions": { "ignore": [] },
  "setting": {
    "urlCheck": false,
    "es6": true,
    "postcss": true,
    "minified": true
  },
  "compileType": "miniprogram",
  "libVersion": "3.3.0",
  "appid": "your-appid-here",
  "projectname": "daily-digest"
}
```

- [ ] **Step 3: 创建 app.js**

```javascript
App({
  globalData: {
    baseUrl: 'https://your-api-domain.com/api/v1',
    userId: null,
  },
  onLaunch() {
    // 获取用户信息
    wx.login({
      success: (res) => {
        if (res.code) {
          wx.request({
            url: `${this.globalData.baseUrl}/auth/wechat`,
            method: 'POST',
            data: { code: res.code },
            success: (resp) => {
              this.globalData.userId = resp.data.id
              wx.setStorageSync('user_id', resp.data.id)
            }
          })
        }
      }
    })
  }
})
```

- [ ] **Step 4: 创建 app.wxss**

```css
page { background: #f5f5f5; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif; }
```

- [ ] **Step 5: 创建 ** 首页（每日速览）**

pages/index/index.wxml:
```xml
<view class="daily-page">
  <view class="tabs">
    <view class="tab {{activeCategory === '' ? 'active' : ''}}" bindtap="switchCategory" data-category="">全部</view>
    <view class="tab {{activeCategory === 'db' ? 'active' : ''}}" bindtap="switchCategory" data-category="db">数据库</view>
    <view class="tab {{activeCategory === 'ai' ? 'active' : ''}}" bindtap="switchCategory" data-category="ai">AI</view>
    <view class="tab {{activeCategory === 'ml' ? 'active' : ''}}" bindtap="switchCategory" data-category="ml">ML</view>
  </view>
  <view wx:for="{{items}}" wx:key="id" class="item-card" bindtap="goDetail" data-id="{{item.id}}">
    <view class="card-header">
      <text class="source">{{item.source}}</text>
      <text class="score" wx:if="{{item.score}}">★ {{item.score}}</text>
    </view>
    <text class="title">{{item.title}}</text>
    <text class="summary" wx:if="{{item.summary}}">{{item.summary.slice(0, 100)}}...</text>
  </view>
  <view wx:if="{{loading}}" class="loading">加载中...</view>
</view>
```

pages/index/index.js:
```javascript
const app = getApp()
Page({
  data: { items: [], activeCategory: '', loading: false, page: 1 },
  onLoad() { this.loadDaily() },
  loadDaily() {
    this.setData({ loading: true })
    const baseUrl = app.globalData.baseUrl
    let url = `${baseUrl}/daily?page=${this.data.page}&size=20`
    if (this.data.activeCategory) url += `&category=${this.data.activeCategory}`
    wx.request({
      url,
      success: (res) => {
        this.setData({
          items: res.data.items,
          loading: false,
        })
      },
      fail: () => this.setData({ loading: false }),
    })
  },
  switchCategory(e) {
    this.setData({ activeCategory: e.currentTarget.dataset.category, page: 1 })
    this.loadDaily()
  },
  goDetail(e) {
    wx.navigateTo({ url: `/pages/detail/detail?id=${e.currentTarget.dataset.id}` })
  },
})
```

- [ ] **Step 6: 创建 ** 发现页（搜索）**

pages/discover/discover.wxml:
```xml
<view class="discover-page">
  <view class="search-bar">
    <input placeholder="搜索技术内容..." bindinput="onQueryInput" bindconfirm="doSearch" />
    <button bindtap="doSearch">搜索</button>
  </view>
  <view wx:for="{{results}}" wx:key="id" class="item-card" bindtap="goDetail" data-id="{{item.id}}">
    <text class="title">{{item.title}}</text>
  </view>
</view>
```

- [ ] **Step 7: 创建 ** 收藏夹页**

pages/collections/collections.wxml:
```xml
<view class="collections-page">
  <button class="create-btn" bindtap="showCreate">+ 新建收藏夹</button>
  <view wx:for="{{collections}}" wx:key="id" class="collection-card" bindtap="goCollection" data-id="{{item.id}}">
    <text class="name">{{item.name}}</text>
    <text class="count">{{item.item_count}} 条</text>
  </view>
</view>
```

- [ ] **Step 8: 创建 ** 我的页（订阅/历史/设置入口）**

pages/mine/mine.wxml:
```xml
<view class="mine-page">
  <view class="menu-item" bindtap="goSubscriptions">订阅管理</view>
  <view class="menu-item" bindtap="goHistory">阅读历史</view>
  <view class="menu-item" bindtap="goSettings">设置</view>
</view>
```

- [ ] **Step 9: 验证小程序配置**

在微信开发者工具中导入 `engineering/apps/daily-digest/miniapp/` 目录，确认编译无报错。

- [ ] **Step 10: Commit**

```bash
git add engineering/apps/daily-digest/miniapp/
git commit -m "feat(daily-digest): 初始化微信小程序（4 个 Tab 页面）"
```

---

## 范围检查

检查 spec 每一节是否有关联的任务：

| Spec 章节 | 对应任务 |
|-----------|---------|
| 数据模型（6 张表） | Task 2 |
| 数据源（arXiv/Semantic Scholar/DBLP/RSS/GitHub/HuggingFace） | Task 4, 5 |
| 处理管道（摘要/分类/评分） | Task 6 |
| 推荐引擎（热门层/订阅层/探索层） | Task 7 |
| API（daily/search/history/auth/subscriptions/collections/trigger/stats） | Task 8 |
| 定时任务（APScheduler 08:00） | Task 9 |
| Vue 3 前端（8 个页面 + 4 个组件） | Task 10, 11, 12, 13 |
| 微信小程序（4 个 Tab 页面） | Task 14 |
| 错误处理与降级 | 内嵌在 Task 6（summarizer 降级）、Task 4/5（聚合器降级） |
| 测试策略 | 分布在 Task 4-8 的测试步骤中 |

**未覆盖的（后续优化阶段补充）：**
- 微信推送服务（pusher/wechat.py）— 需要微信模板消息审核
- 真正的用户认证（JWT/OAuth）— 当前用简化版 user_id 参数
- 前端 UI 美化 — 将调用 `frontend-design` 技能完成

一切正常，无遗漏。