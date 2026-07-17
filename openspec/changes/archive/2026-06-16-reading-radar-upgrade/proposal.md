## Why

reading-radar 的学习内容页（learn.html）当前使用模板化的四段式结构（入门/原理/实战/调优），内容枯燥、缺乏可视化，学习体验远不如小林 coding 的"说人话+手绘图解"风格。同时数据库内容混放了产品专属知识（MySQL/Redis 等）和通用架构知识，向量数据库内容也散落在 db 栈中未独立分类。需要通过 Markdown 驱动的内容架构、图解穿插展示、数据库和向量数据库内容重新组织，全面提升学习内容的可读性和结构性。

## What Changes

- **Markdown 驱动的学习内容**：learn.html 改为解析 `.md` 文件渲染学习内容，支持原生 Markdown 语法（标题、代码块、列表、图片），300 个知识点的 MD 文件存放在 `data/learn-deep/{stack}/{quadrant}/{id}.md`
- **图解穿插展示**：每个知识点的 MD 文件可内嵌图片引用 `![说明](路径)`，渲染后图片居中显示并附带说明文字，穿插在章节之间辅助理解
- **内容风格改造**：MD 内容从教科书风格改为"说人话+图解"表达风格，每篇知识点包含人话开场白、图解辅助、代码示例、面试追问
- **数据库内容重组**：`data/learn-deep/db/` 仅保留通用数据库架构知识（11 个知识点：SQL/ACID/事务/分片/高可用/性能调优等），产品专属内容迁移到 `illustrate/` 目录
- **图解系列新增**：新建 `data/learn-deep/illustrate/` 目录，按产品/主题分类存放 MySQL、Redis、PostgreSQL、Elasticsearch、RocksDB、SQLite、Faiss、DiskANN、Milvus、Pinecone、openGauss、网络、操作系统、Agent、Claude Code 的图解文章
- **向量数据库迁移**：faiss、diskann、milvus、pinecone 相关知识点从 db 栈迁移到 illustrate/ 下的独立产品目录
- **网页视觉升级**：learn.html 内容区加宽至 ~900px，去掉固定四模块卡片布局改为流式长文，侧边栏增加滚动高亮，保留 [上一页][下一页] 导航
- **index.html 精简**：去掉嵌入的看板部分，只保留知识点选择器、知识点卡片、四象限雷达图

## Capabilities

### New Capabilities
- `markdown-content-rendering`: learn.html 通过 Markdown 解析器渲染 `.md` 文件作为学习内容源，支持标题、代码块、列表、图片、表格等 Markdown 语法
- `illustrated-articles`: 知识点文章内容支持内嵌图解图片，渲染时居中显示并附带说明文字，穿插在章节之间辅助理解
- `illustrate-series`: 图解系列（MySQL、Redis、网络、操作系统、Agent、Claude Code 等）独立于技术栈知识体系，按产品/主题分类存放和浏览
- `database-content-organization`: 数据库内容按"通用架构"与"产品专属"分离，db 栈仅保留通用概念，产品专属内容进入 illustrate 目录
- `vector-database-illustrate`: 向量数据库（Faiss、DiskANN、Milvus、Pinecone）作为独立图解系列，从 db 栈迁移至 illustrate/vdb/
- `learning-page-redesign`: learn.html 视觉升级：Markdown 渲染、流式长文布局、图解居中展示、侧边栏滚动高亮

### Modified Capabilities
- `learn-content-page`: 从 `data/learn-content-<catId>.js` 模板引擎改为 `data/learn-deep/{stack}/{quadrant}/{id}.md` Markdown 文件，四段式模板改为自由 Markdown 格式

## Impact

- **受影响文件**:
  - `project/reading-radar/learn.html` — 大幅重写（Markdown 解析 + 视觉升级）
  - `project/reading-radar/index.html` — 精简（去掉嵌入看板）
  - `project/reading-radar/data/app/items-registry.js` — 修正 vdb 产品映射
  - `project/reading-radar/data/learn-deep/` — 内容重组（db 精简、illustrate 新建）
  - 300 个 `.md` 文件 — 内容风格改造（从教科书 → 说人话+图解）
- **新增依赖**: `marked.js`（~30KB Markdown 解析库）
- **构建变更**: 无，纯静态 HTML/JS 项目无需构建工具
- **兼容性**: learn.html 改为 fetch 加载 .md 文件，file:// 协议下需调整加载方式或使用本地服务器
