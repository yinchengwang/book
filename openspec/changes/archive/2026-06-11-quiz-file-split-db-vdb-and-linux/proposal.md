## Why

当前 reading-radar 项目的题库文件存在三个问题：

1. **文件过大**：5 个题库文件均在 2000-4000 行（`db.js` 达 3998 行），后续补充题目会导致可维护性急剧下降。
2. **数据库知识混仓**：传统数据库（MySQL/PostgreSQL/Redis 等）与向量数据库（Faiss/Milvus/Pinecone 等）知识点混在一个 `db` 技术栈下，但两者技术路线、目标用户、象限定义都不同，应当独立为两个技术栈。
3. **Linux 题库为空**：4 个象限空壳文件等待填充。

同时存在一个隐藏 bug：题目对象缺少 `quadrant` 字段，导致象限筛选功能无法正常工作（永远命中「全部象限」回退路径）。

## What Changes

- 对 C/C++/数据结构/Python/传统 DB/向量 DB/Linux 七个技术栈的题库文件，按**象限维度**拆分为独立小文件（每文件约 300-600 行）
- 将 `db` 技术栈拆分为 `db`（传统数据库）和 `vdb`（向量数据库）两个独立技术栈，各自拥有独立的导航项、CATEGORIES、象限标签
- 题库文件按**产品粒度**组织：`quiz-questions-db-mysql.js`、`quiz-questions-vdb-faiss.js` 等
- 补充 Linux 题库内容（4 个象限文件，基于现有 56 个 Linux 知识点出题）
- 给所有迁移的题目补充 `quadrant` 字段，修复象限筛选逻辑
- 看板渲染按产品分组展示知识点（子章节功能）
- 新增 `SQLite` 知识点和题目

## Capabilities

### New Capabilities

- `quiz-file-quadrant-split`: 七个技术栈题库文件按象限拆分为独立小文件，每文件 ~300-600 行
- `db-vdb-stack-split`: `db` 技术栈拆分为 `db`（传统数据库）和 `vdb`（向量数据库），独立导航 / CATEGORIES / 象限标签体系
- `items-db-vdb-migration`: items-registry.js 中 db 知识点按内容类型迁移到 db + vdb 两个栈，包括向量 DB 知识点的象限重标定
- `quiz-product-file-org`: 题库文件按产品粒度组织命名（mysql/postgres/sqlite/redis/es/faiss/milvus/pinecone/chroma/diskann 等），产品内维持象限维度
- `quiz-quadrant-field-fix`: 给所有拆分后的题目补充 `quadrant` 字段（从注释分组推导），修复 `selectQuestions()` 象限筛选失效的隐藏 bug
- `linux-quiz-content`: 补充 Linux 4 个象限的题库内容（基于 items-registry.js 中 56 个 Linux 知识点）
- `kanban-product-grouping`: 看板渲染支持按产品分组的子章节展示，同一个技术栈下知识点先按产品归类再按象限排列

### Modified Capabilities

- 无（本次为新增能力，不修改现有能力规范）

## Impact

- **`data/quiz-questions-*.js`**：5 个大文件将被移除，替换为 ~20 个按象限/产品拆分的小文件
- **`data/items-registry.js`**：db 条目的 `stack` 字段部分改为 `vdb`，新增 SQLite 条目
- **`data/quiz-tech.js`**：新增 `VDB_TECH_DATA` 派生数组
- **`data/quiz-static.js`**：新增 `CATEGORIES.vdb` 和 `QUADRANT_LABELS.vdb`
- **`data/nav-component.js`**：新增 vdb 导航项
- **`data/quiz-core.js`**：无实质修改（selectQuestions 已支持多栈）
- **`quiz-system.html`**：`<script>` 加载标签从 5 个改为约 20 个
- **`data/kanban-render.js`**：新增产品分组渲染逻辑
- **`project/reading-radar/CLAUDE.md`**：更新文件分类和读取策略表
