# CLAUDE.md

此文件为 Claude Code 在 reading-radar 项目中工作时提供指导。

## 语言规范

1. 所有对话、解释、建议使用**简体中文**。
2. 代码注释使用中文。
3. Commit message 使用中文。

## 项目概述

纯静态 HTML/JS 学习站点，无构建工具。覆盖 C/C++/数据结构/数据库内核/Python/Linux 六大技术栈。

6 个 HTML 页面 + 1 个 Node 服务 + ~30 个数据/逻辑 JS 文件。全局变量挂载模式（`window` 下共享数据），localStorage 持久化状态。

## 🚨 文件读取纪律 —— 所有场景通用（每次必读）

**不区分"开发"和"排查"，任何文件操作都强制遵守以下流程：**

1. **先 grep 定位目标代码**（函数名 / 关键字 / 变量名 / 错误特征）
2. **再用 Read(offset, limit≤50) 精确读取目标段**
3. **一次只读一个文件**，读完确认再读下一个
4. **读前必须自问：我 grep 了吗？**
5. **碰到 Edit 需要 old_string，先 grep 定位，按需 offset 读 5-10 行取上下文，不要为此完整读文件**

> 教训（持续更新）：
> - 第 1 次：排查 3 个问题，完整读了 quiz-system.html(2916行) + index.html(2141行)，撑爆上下文
> - 第 2 次：Phase 3 开发完整读了 items-registry.js(367行) + kanban-render.js(304行)，同样撑爆上下文
> 根源都是没有先 grep。先 grep 找 `product`/`groupBy`/`groupMode` 等关键字，每个只读 30 行就能完成相同工作。

### 🟡 按需局部读（所有文件默认使用此方式）

| 文件 | 行数 | grep 定位关键字参考 |
|------|------|-------------------|
| `quiz-system.html` | ~3000 | `loadQuestionBank`、`selectQuestions`、`CATEGORIES`、`<script` |
| `index.html` | ~2100 | `mode-bar`、`kanban-embed`、`radar`、`<script`、`DRAW_TYPE` |
| `learning-kanban.html` | ~650 | `renderKanban`、`product-board`、`groupToggle`、`mode-btn` |
| `learn.html` | ~1200 | `hash`、`render`、`LEARN_CONTENT`、`<script` |
| `five-year-plan.html` | ~850 | `renderAll`、`打卡`、`LEARNING_ROADMAP` |
| `dashboard.html` | ~800 | `quiz-tech`、`掌握率`、`趋势` |
| `data/app/items-registry.js` | ~370 | 知识点 ID（如 `db-btree-idx`）或 `product`、`stack`、`quadrant` |
| `data/app/kanban-render.js` | ~330 | `groupBy`、`renderProductBoard`、`PRODUCT_LABELS` |
| `data/quiz/static.js` | ~730 | `CATEGORIES`、`QUADRANT_LABELS`、`每日提示` |
| `data/quiz/questions/quiz-questions-*-*.js` | ~30-500 | 知识点 ID（如 `db-btree-idx`） |

```
# 正确做法（所有文件通用）
Grep(pattern, file)                              # 定位目标代码行
Read(offset=matched_line-5, limit=30)            # 精确读取目标段
Edit(new_string=..., old_string=...)             # 修改
```

### 🟢 例外：仅在确认需要文件全部内容时完整读

以下文件结构清晰、≤300 行，可在 **确认需要完整内容**（如全局搜索/重构范围确认）时完整读。**如果只是找某个函数或字段，仍然优先 grep+offset。**

| 文件 | 行数 | 何时需要完整读 |
|------|------|--------------|
| `data/quiz/core.js` | 155 | 理解完整加载链路 |
| `data/quiz/ui.js` | 340 | 理解完整渲染流程（优先 grep UI 函数名） |
| `data/content/learn-roadmap.js` | 242 | 理解完整路线图结构 |
| `data/app/nav-component.js` | 242 | 新增导航项 |
| `data/app/radar-tech.js` | 77 | 任何时候 |
| `data/app/kanban-data.js` | 28 | 任何时候 |
| `data/quiz/tech.js` | 20 | 任何时候 |
| `data/content/learn-content-*.js` | 120-161 | 补充新内容 |
| `server.js` | ~100 | 任何时候 |

> ⚠️ **常见陷阱**：`items-registry.js`(370行) 和 `kanban-render.js`(330行) 经常被不必要地完整读。如果只是找 `product`、`groupBy`、某个知识点 ID，grep 一行定位即可，限 30 行。

## 数据架构（Phase 0 重构后）

```
ITEMS_REGISTRY (items-registry.js)     ← 唯一定义源，296 个知识点
    │
    ├── quiz-tech.js   ──→ C_TECH_DATA / CPP_TECH_DATA / ... (派生)
    ├── radar-tech.js  ──→ BOOK_DATA (独立) + C_TECH_DATA / ... (派生)
    ├── kanban-data.js ──→ C_DATA / CPP_DATA / ... (派生)
    └── quiz-static.js ──→ CATEGORIES / QUADRANT_LABELS (独立配置)
```

**核心原则**：新增知识点只需在 `items-registry.js` 中添加一次，三个消费者文件自动生效。

## 实施纪律

### 规则 1：先 grep，后读（不区分开发/排查）
```
❌ Grep(关键字, file) → 觉得不够 → Read(full_file) → 找到修改点 → 上下文已浪费100+行
❌ "这是开发不是排查" → 完整读文件 → 上下文撑爆
✅ Grep(pattern, file) → Read(offset=matched_line-5, limit=30) → Edit
✅ 如果 Edit 需要 old_string 上下文，grep 附近关键字再读 5-10 行，绝不为此读完整文件
```

### 规则 2：Edit 的 old_string 不超过 20 行
```
❌ old_string = 40 行函数体（匹配失败风险 + 上下文浪费）
✅ 用 Write 覆盖整个文件（前提：文件 < 200 行或已理解其完整内容）
```

### 规则 3：数据迁移用脚本
```
❌ 手工逐条复制 N 个条目到新文件
✅ 写 Python/Bash 脚本自动转换，一行命令完成
```

### 规则 4：每次 `/opsx:apply` 最多 2 个 Phase
```
❌ 一口气跑 84 个任务 6 个 Phase
✅ 分 3-4 次 apply，每次 1-2 个 Phase
```

### 规则 5：手动验证任务默认跳过
```
❌ 读到"验证：打开浏览器..." → Read 全部相关文件 → 人工验证不了
✅ 标注为 [手动]，跳过，不读文件
```

## 添加新功能/文件时的约定

### 新增知识点
1. 在 `data/app/items-registry.js` 中添加条目（id/title/stack/quadrant/ring/desc/tags）
2. quiz-tech.js / radar-tech.js / kanban-data.js 会自动派生
3. 无需修改任何其他文件

### 新增题库文件
1. 创建 `data/quiz/questions/quiz-questions-<stack>-<product|quadrant>.js`
   - DB/VDB 技术栈用产品名：`quiz-questions-db-mysql.js`、`quiz-questions-vdb-faiss.js`
   - 其他技术栈用象限名：`quiz-questions-c-language.js`、`quiz-questions-cpp-systems.js`
2. 使用 `QUESTION_BANK.<stack> = Object.assign(QUESTION_BANK.<stack> || {}, {...})` 合并
3. 在 `quiz-system.html` 中添加 `<script>` 加载标签

### 新增 HTML 页面
1. 在 `<body>` 顶部插入 `<nav id="global-nav"></nav>`
2. 加载 `items-registry.js` + `nav-component.js`
3. 调用 `initGlobalNav("pageId")`
4. 在 `data/app/nav-component.js` 的 `NAV_CONFIG.items` 中注册

### 修改现有 HTML/JS 文件
1. grep 定位目标区块的关键字（函数名 / 变量名 / 字符串）
2. offset 读取目标代码段（前后 20-30 行）
3. Edit 精确替换，old_string 控制在 20 行以内
4. 绝不完整读取 quiz-system.html、index.html、learning-kanban.html、items-registry.js、kanban-render.js

## 测试/验证

- 无自动化测试框架，所有功能验证在浏览器中手动进行
- `node server.js` 启动本地服务（默认 8080 端口）
- 浏览器控制台检查全局变量：`C_TECH_DATA.length`、`getItemsByStack("linux").length` 等

## 相关记忆文件

- [[reading-radar-context-optimization]] — 上下文爆炸防护规则（详细版）
