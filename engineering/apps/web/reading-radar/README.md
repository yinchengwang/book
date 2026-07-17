# Reading Radar

这是一个面向 C / C++ / 数据结构 / 数据库内核 / Python 的静态学习站点。项目把知识地图、学习看板、知识测评、学习内容页、全景仪表盘和五年计划放在同一套本地数据模型上，默认无构建工具，直接打开 HTML 即可使用；如果需要多设备同步，可以启动 `server.js`，把状态持久化到 `user-data/`。

## 快速开始

### 离线模式

直接打开页面文件即可，无需构建：

- `index.html`：经典雷达图 / 读书模式 / 技术栈雷达
- `learning-kanban.html`：技术栈学习看板
- `quiz-system.html`：知识测评系统
- `learn.html`：知识点学习内容页
- `dashboard.html`：学习全景仪表盘
- `five-year-plan.html`：五年建设计划

### 本地服务模式

```bash
node server.js
# 或指定端口
node server.js 9090
```

启动后可访问：

- 本机：`http://localhost:8080`
- 局域网设备：`http://<你的局域网IP>:8080`

本地服务模式同时提供两类能力：

1. 静态文件服务：托管当前目录下全部 HTML / JS / 数据文件。
2. REST API：把页面状态、上传文件、项目代码保存到 `user-data/`，便于 Git 追踪与备份。

## 四层联动架构

| Layer | 目标 | 入口页面 | 主要数据 | 输出结果 |
|------|------|------|------|------|
| Layer 1 | 知识地图与自评状态 | `index.html`、`learning-kanban.html` | `data/app/radar-tech.js`、`data/app/kanban-data.js` | 标记阅读/学习状态，建立知识地图 |
| Layer 2 | 题库测评与项目验收 | `quiz-system.html` | `data/quiz/tech.js`、`data/quiz/static.js`、`data/quiz/questions/quiz-questions-*.js` | 形成测验成绩、最近记录、项目评分 |
| Layer 3 | 单知识点学习内容 | `learn.html` | `data/learn-deep/`（Markdown 图解文章） | 提供”人话开场白 → ASCII 图解 → 对比表 → 面试追问 → 代码示例 → 一句话总结” |
| Layer 4 | 总览与长期计划 | `dashboard.html`、`five-year-plan.html` | localStorage / `user-data/` 状态 | 汇总掌握率、盲点、趋势和五年路线 |

推荐使用顺序：

1. 先在 `learning-kanban.html` 或 `index.html` 确定当前知识点。
2. 进入 `learn.html#<catId>/<itemId>` 学习该知识点内容。
3. 学完后跳转 `quiz-system.html#<catId>/<itemId>` 做测验。
4. 在 `dashboard.html` 看掌握率、盲点、趋势和五年进度，再回到计划页继续推进。

## 页面与目录职责

### 顶层页面

| 路径 | 作用 |
|------|------|
| `index.html` | 经典雷达主入口，兼容书籍阅读模式与技术栈雷达浏览 |
| `learning-kanban.html` | 技术栈学习看板，维护知识点状态，并跳转到 `learn.html` / `quiz-system.html` |
| `quiz-system.html` | 知识测评系统，支持分类进入题库，并包含项目验收逻辑 |
| `learn.html` | 学习内容页，支持 `#<catId>/<itemId>` hash 路由 |
| `dashboard.html` | 学习总览页，展示掌握率、盲点、最近测验、热力图、五年路线摘要 |
| `five-year-plan.html` | 五年建设计划页，按年度与日历管理长期打卡 |
| `server.js` | Node 本地服务器，负责静态托管和 `user-data/` API |

### `data/` 目录

| 路径 | 作用 |
|------|------|
| `data/app/radar-tech.js` | 经典雷达 / 阅读模式数据与层级配置 |
| `data/app/kanban-data.js` | 看板轻量卡片数据，供 `learning-kanban.html` 使用 |
| `data/quiz/tech.js` | 技术栈知识点元数据总表，定义 `id` / `title` / `quadrant` / `ring` / `desc` |
| `data/quiz/static.js` | 评测系统静态配置，如 `CATEGORIES`、每日提示、项目验收模板 |
| `data/quiz/questions/quiz-questions-*.js` | 按技术栈+象限分类的题库数据 |
| `data/quiz/static.js` | 评测系统静态配置，如 `CATEGORIES`、每日提示 |
| `data/quiz/tech.js` | 技术栈知识点元数据总表，定义 `id` / `title` / `quadrant` / `ring` / `desc` |

### `user-data/` 目录

服务模式下，用户状态和上传文件会写入该目录。当前可以按下面的职责理解：

```text
user-data/
├── state/      # 页面状态、看板状态、测验记录、五年计划状态等 JSON
├── books/      # PDF 或资料文件
└── projects/   # 项目验收模式上传的代码文件
```

## 数据规范

### 1. 知识点元数据

`data/quiz/tech.js` 中每个技术栈都维护一组知识点元数据，例如：

```js
const C_TECH_DATA = [
	{
		id: "pointer",
		title: "指针深度解析",
		quadrant: "language",
		ring: "intermediate",
		desc: "多级指针、指针与数组的互换、指针运算、const 指针、void* 泛型指针。"
	}
];
```

字段说明：

- `id`：知识点唯一 ID，同时用于路由、题库分组、localStorage key。
- `title`：页面标题。
- `quadrant`：象限。当前可选值为 `language`、`systems`、`algorithms`、`engineering`。
- `ring`：难度层级。当前可选值为 `basic`、`intermediate`、`advanced`。
- `desc`：知识点说明，用于评测页、学习页和文档展示。

### 2. 题库结构 `QUESTION_BANK`

题库文件统一采用以下结构：

```js
QUESTION_BANK.c = {
	c: {
		"pointer": [
			{
				id: "pointer-q1",
				type: "choice",
				difficulty: "intermediate",
				scenario: "真实工作或面试场景描述",
				stem: "题干",
				code: "可选代码片段",
				options: ["A. ...", "B. ..."],
				answer: "A",
				explanation: "解析"
			}
		]
	}
};
```

题目对象字段：

- `id`：题目唯一 ID，通常为 `<itemId>-q<序号>`。
- `type`：题型。
- `difficulty`：题目难度，取值通常为 `basic`、`intermediate`、`advanced`。
- `scenario`：题目背景或业务语境。
- `stem`：题干正文。
- `code`：可选，附加代码片段。
- `options`：选项数组；填空题通常为空数组。
- `answer`：标准答案。单选通常是字符串，多选通常是字符串数组，判断题通常是布尔值。
- `explanation`：答案解析。

当前已出现的题型枚举：

- `choice`
- `multi_choice`
- `true_false`
- `fill_blank`
- `predict_output`

### 3. 学习内容结构（Markdown 图解文章）

所有知识点的学习内容已迁移到 `data/learn-deep/` 目录下的独立 Markdown 文件，采用"说人话+图解"风格：

```text
data/learn-deep/
├── c/              # C 语言（44 个知识点）
│   ├── language/   # 语法、指针、内存管理等
│   ├── systems/    # 进程、线程、IPC、信号等
│   ├── algorithms/ # 排序、哈希、位运算等
│   └── engineering/ # GDB、Git、Valgrind 等工具
├── cpp/            # C++（42 个知识点）
├── ds/             # 数据结构（52 个知识点）
├── db/             # 数据库通用（11 个知识点）
├── illustrate/     # 数据库/向量数据库产品图解（52 个）
│   ├── mysql/
│   ├── redis/
│   ├── postgres/
│   ├── faiss/
│   └── ...
├── linux/          # Linux 系统（70 个知识点）
└── py/             # Python（43 个知识点）
```

**单篇文章结构：**

1. **一句话人话开场白** — 用生活类比解释概念本质
2. **ASCII 图解** — text 代码块画直观示意图/对比表
3. **对比表格** — 多方案优缺点对比
4. **面试追问** — 5-6 个高频问题及简洁回答
5. **代码示例** — 精简可运行的代码演示核心逻辑
6. **一句话总结** — 收尾概括

### 4. URL 规则

学习与测评统一采用 hash 路由：

- `learn.html#<catId>/<itemId>`
- `quiz-system.html#<catId>/<itemId>`

当前 `catId` 取值：

- `c` — C 语言
- `cpp` — C++
- `ds` — 数据结构
- `db` — 数据库通用
- `py` — Python
- `linux` — Linux 系统
- `vdb` — 向量数据库（映射到 illustrate/ 目录）

示例：

- `learn.html#cpp/raii`
- `learn.html#c/pointer`
- `quiz-system.html#db/db-hnsw`

## 当前技术栈覆盖情况

以下统计来自当前仓库数据文件：

| 技术栈 | 知识点数 | 题库分组数 | 当前题量范围 | 学习内容覆盖 |
|------|------:|------:|------|------|
| C | 44 | 44 | 每组 10-12 题 | 44 个 MD 图解文章 |
| C++ | 42 | 42 | 每组 10 题 | 42 个 MD 图解文章 |
| 数据结构 | 52 | 52 | 每组 10 题 | 52 个 MD 图解文章 |
| 数据库通用 | 11 | — | — | 11 个 MD 图解文章 |
| 数据库产品 (MySQL/Redis/PG) | 35 | — | — | 35 个 MD 图解文章 |
| 向量数据库 (Faiss/Milvus/...) | 14 | — | — | 14 个 MD 图解文章 |
| Python | 43 | 43 | 每组 10 题 | 43 个 MD 图解文章 |
| Linux | 70 | 4 | 每组若干 | 70 个 MD 图解文章 |

说明：

- 所有内容已迁移到 `data/learn-deep/` 下的 Markdown 文件，支持 Obsidian 直接阅读。
- 学习内容覆盖：314 个知识点全部完成"说人话+图解"风格改写。
- 题库目前每组约 10 题，后续计划扩容到 20+ 题（Phase 7d）。

## 页面联动关系

### 学习看板 → 学习内容

`learning-kanban.html` 的每张卡片都提供：

- `📚 学习内容`：跳转到 `learn.html#<catId>/<itemId>`
- `🎯 去测试`：跳转到 `quiz-system.html#<catId>/<itemId>`

### 仪表盘 → 五年计划

`dashboard.html` 包含：

- 当前年度计划摘要卡
- 五年成长路线摘要区块
- 指向 `five-year-plan.html` 的完整计划入口

`five-year-plan.html` 顶部提供返回 `dashboard.html` 的导航链接。

## 存储说明

| 模式 | 存储位置 | 多设备 |
|------|---------|--------|
| `file://` 离线模式 | 浏览器 localStorage / IndexedDB | 否 |
| `http://` 服务模式 | `user-data/` 目录（JSON + 文件） | 是，局域网可访问 |

如果需要备份学习过程，建议直接把 `user-data/` 纳入 Git，提交状态 JSON 即可追踪学习变化。
