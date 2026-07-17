## 1. Phase 1：基础设施（items-registry + 技术栈元数据）

- [x] 1.1 items-registry.js 中将向量 DB 知识条目的 `stack:"db"` 改为 `stack:"vdb"`
- [x] 1.2 审查并调整 vdb 条目象限归属，确保与 `QUADRANT_LABELS.vdb` 对齐
- [x] 1.3 items-registry.js 中新增 SQLite 知识点条目（`stack:"db"`）
- [x] 1.4 quiz-tech.js 确认 `VDB_TECH_DATA` 自动派生（基于 `stack:"vdb"` 过滤）
- [x] 1.5 quiz-static.js 新建 `QUADRANT_LABELS.vdb`（向量基础/索引算法/量化与压缩/系统与工程）
- [x] 1.6 quiz-static.js 新建 `CATEGORIES.vdb` 条目（含 id/label/icon/quadrantLabels 等）
- [x] 1.7 nav-component.js 的 `NAV_CONFIG.items` 中新增 vdb 导航项（无需修改——vdb 是技术栈非独立页面，已通过 CATEGORIES 自动渲染）
- [x] 1.8 quiz-system.html 的技术栈选择器区域确认 vdb 入口可见（renderHome 用 Object.entries(CATEGORIES) 动态渲染，自动生效）
- [x] 1.9 同步更新了相关 8 个文件以支持 vdb：radar-tech.js、kanban-data.js、verify-data.js、dashboard.html、index.html、learning-kanban.html、learn.html

## 2. Phase 2a：传统 DB 题库文件拆分（产品粒度）

- [x] 2.1 从 quiz-questions-db.js 提取 MySQL/InnoDB 知识点题目到 `quiz-questions-db-mysql.js`
- [x] 2.2 从 quiz-questions-db.js 提取 Redis 知识点题目到 `quiz-questions-db-redis.js`
- [x] 2.3 从 quiz-questions-db.js 提取 PostgreSQL 知识点题目到 `quiz-questions-db-postgres.js`
- [x] 2.4 从 quiz-questions-db.js 提取 LSM-Tree/RocksDB 知识点题目到 `quiz-questions-db-rocksdb.js`
- [x] 2.5 从 quiz-questions-db.js 提取 Elasticsearch 知识点题目到 `quiz-questions-db-elasticsearch.js`
- [x] 2.6 从 quiz-questions-db.js 提取跨产品通用概念题目到 `quiz-questions-db-general.js`
- [x] 2.7 各文件内按象限分组，每道题补充 `quadrant` 和 `ring` 字段
- [x] 2.8 验证：各产品文件题目总和 = 原 `quiz-questions-db.js` 题目总数

## 3. Phase 2b：向量 DB 题库文件拆分（产品粒度）

- [x] 3.1 从 quiz-questions-db.js 提取 Faiss 系题目到 `quiz-questions-vdb-faiss.js`
- [x] 3.2 从 quiz-questions-db.js 提取 Milvus 题目到 `quiz-questions-vdb-milvus.js`
- [x] 3.3 从 quiz-questions-db.js 提取 DiskANN 题目到 `quiz-questions-vdb-diskann.js`
- [x] 3.4 从 quiz-questions-db.js 提取 Chroma 题目到 `quiz-questions-vdb-chroma.js`
- [x] 3.5 创建 `quiz-questions-vdb-pinecone.js`（框架文件，标明待填充）
- [x] 3.6 从 quiz-questions-db.js 提取跨产品通用概念到 `quiz-questions-vdb-general.js`
- [x] 3.7 各文件内按象限分组，每道题补充 `quadrant` 和 `ring` 字段
- [x] 3.8 验证：各产品文件题目总和 = 原 `quiz-questions-db.js` 向量 DB 题目总数

## 4. Phase 2c：其他技术栈题库按象限拆分

- [x] 4.1 将 `quiz-questions-c.js` 拆为 4 个象限文件（c-language / c-systems / c-algorithms / c-engineering），每道题补充 `quadrant` + `ring`
- [x] 4.2 将 `quiz-questions-cpp.js` 拆为 4 个象限文件，每道题补充 `quadrant` + `ring`
- [x] 4.3 将 `quiz-questions-ds.js` 拆为 4 个象限文件，每道题补充 `quadrant` + `ring`
- [x] 4.4 将 `quiz-questions-py.js` 拆为 4 个象限文件，每道题补充 `quadrant` + `ring`
- [x] 4.5 删除旧的 5 个大文件（quiz-questions-c/cpp/ds/db/py.js）
- [x] 4.6 更新 `quiz-system.html` 的 `<script>` 标签列表（移除旧文件引用，添加新文件引用）
- [x] 4.7 验证：`selectQuestions("c", "language", null, 10)` 只返回 `quadrant:"language"` 的题目，结果不为空
- [x] 4.8 验证：`selectQuestions("db", "systems", "basic", 10)` 返回正确的象限+难度组合

## 5. Phase 3：看板产品分组

- [x] 5.1 items-registry.js 中 db/vdb 条目增加 `product` 字段（mysql/redis/postgres/es/rocksdb/faiss/milvus/diskann/chroma/general）
- [x] 5.2 kanban-render.js 新增 `groupBy: "product"` 双层分组渲染模式
- [x] 5.3 kanban-render.js 新增分组模式切换器 UI（象限分组/产品分组）
- [x] 5.4 验证：看板切换到产品分组模式，按产品显示知识点区域，区域内按象限排列 [手动浏览器验证]

## 6. Phase 4a：Linux 题库内容补充

- [x] 6.1 填充 `quiz-questions-linux-observability.js`（至少 5 个知识点 × 2+ 题，基于 56 个 Linux 知识点）
- [x] 6.2 填充 `quiz-questions-linux-os-internals.js`
- [x] 6.3 填充 `quiz-questions-linux-storage.js`
- [x] 6.4 填充 `quiz-questions-linux-networking.js`
- [x] 6.5 每个文件确保至少 2 种题型，题目携带 `quadrant` + `ring` 字段
- [x] 6.6 验证：`loadQuestionBank("linux")` 返回非空题库

## 7. Phase 4b：SQLite 新题 + Pinecone 框架填充

- [x] 7.1 创建 `quiz-questions-db-sqlite.js`（至少 2 个知识点 × 2+ 题）
- [x] 7.2 后续：为 Pinecone 补充题目内容
- [x] 7.3 更新 CLAUDE.md 中文件分类表和读取策略表

## 8. 全局验证

- [x] 8.1 浏览器打开 `quiz-system.html`，检查所有技术栈的题库选择和答题流程正常 [手动浏览器验证]
- [x] 8.2 浏览器打开 `index.html` 雷达图，确认 db 和 vdb 知识点正确显示 [手动浏览器验证]
- [x] 8.3 浏览器打开 `learning-kanban.html`，确认象限分组和产品分组模式正常 [手动浏览器验证]
- [x] 8.4 验证：象限筛选能正确过滤出非空结果（修复隐藏 bug） [手动浏览器验证]
