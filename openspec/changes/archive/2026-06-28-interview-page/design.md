## Context

reading-radar 项目目前覆盖"学 → 测"场景（雷达图/看板用于学习规划，quiz-system 用于知识测评），缺少"面"这一环节。新增面试页面，让用户在同一个平台内完成面试题库浏览和学习。

项目架构特征：
- 纯静态 HTML/JS，无构建工具（编译脚本除外），file:// 和 http:// 双模式
- 全局变量挂载（`window` 下共享数据），localStorage 持久化
- 面试页面为独立页面，不继承 reading-radar 全局导航栏

## Goals / Non-Goals

**Goals:**
- 提供一个可按分类和技术栈浏览的面试题库页面
- 支持按类别（全部/语言/数据库/AI/计算机基础/其他）过滤技术栈
- 支持重要度（高频/中频/低频）和访问次数排序
- MD 源文件 → 编译脚本 → JS 数据文件的发布流水线
- UI 风格与 reading-radar 项目一致（蓝色主题、深色渐变 header）

**Non-Goals:**
- 不实现用户登录/权限系统
- 不实现 AI 模拟面试功能
- 不做面试答题/评分功能（纯题库浏览）

## Decisions

### D1: 数据源格式 — YAML frontmatter + Markdown 正文

每道面试题一个独立 `.md` 文件，使用 YAML frontmatter 存储结构化元数据，正文存储富文本内容。

**对比：纯 JSON 作为源**
- JSON 可读性差，不适合直接在编辑器中浏览和修改
- MD 文件在 GitHub/Obsidian 等工具中可直接预览

**对比：单一巨量 MD 文件**
- 可维护性差，多人协作冲突概率高
- 编译脚本无法利用文件系统目录结构做分类

### D2: MD 渲染方案 — marked.js（CDN 加载）

使用 `marked.js` 将 MD body 渲染为 HTML。

**选择理由：**
- 项目已有 quiz-system.html 作为先例——quiz-system 也是纯静态 HTML + CDN 脚本模式
- marked.js 成熟稳定，仅 30KB gzip，对首次加载影响小
- 支持 GFM（表格、代码块、列表等），满足面试题内容展示需求

**对比：手写 MD 解析器**
- 实现成本高，需要处理代码块高亮、嵌套列表等边界情况
- 纯属重复造轮子，无明显收益

### D3: 排序实现方案

三种排序模式：

| 排序 | 实现方式 |
|------|----------|
| 默认 | 按 `data/interview/interview-questions.js` 中数组的自然顺序（即编译时目录+文件名的顺序） |
| 重要度 | 按 MD frontmatter 中 `priority` 字段值排序（频率 → 高频/中频/低频映射为数字权重） |
| 访问次数 | 每次用户点击题目时，`localStorage` 记录 `click_count_{questionId}`，排序时读取 |

**选择理由：**
- 不引入外部排序库，全部手写 comparator
- 访问次数持久化到 localStorage，不依赖服务器

### D4: 页面布局 — 三栏响应式（牛客风格）

```
桌面端（≥ 768px）：侧栏（190px）| 列表（300px）| 详情（flex）
移动端（768px ↓）：顶部侧栏（水平滚动）| 列表 | 详情（纵向堆叠）
窄屏（480px ↓）：列表全屏 → 点击题目切详情
```

**对比：原两栏布局**
- 20 个技术栈用水平 Tab 栏太长，不适合横向滚动
- 侧栏分类 + 技术栈选择更接近牛客的 UI 风格
- 侧栏顶部放置类别药丸（全部/语言/数据库/AI/计算机基础/其他），筛选技术栈列表

### D5: 类别与技术栈定义

侧栏顶部显示类别药丸筛选器，选中类别后下方技术栈列表仅显示该类别下的栈。类别和技术栈在 `data/interview/interview-questions.js` 中一起定义：

```js
const CATEGORIES = [
  { id: "all", label: "全部" },
  { id: "lang", label: "语言" },
  { id: "db", label: "数据库" },
  { id: "cs", label: "计算机基础" },
  { id: "ai", label: "AI" },
  { id: "other", label: "其他" },
];

const INTERVIEW_STACKS = [
  { id: "c-language", label: "C 语言", icon: "🔧", category: "lang" },
  { id: "cpp-language", label: "C++", icon: "⚙️", category: "lang" },
  { id: "redis", label: "Redis", icon: "🗄️", category: "db" },
  { id: "mysql", label: "MySQL", icon: "🗃️", category: "db" },
  { id: "os", label: "操作系统", icon: "💻", category: "cs" },
  { id: "network", label: "计算机网络", icon: "🌐", category: "cs" },
  { id: "design-pattern", label: "设计模式", icon: "🏗️", category: "cs" },
  { id: "python", label: "Python", icon: "🐍", category: "lang" },
  { id: "ai-rag", label: "AI知识工程与RAG", icon: "📚", category: "ai" },
  { id: "ai-agent", label: "AI Agent理论与框架", icon: "🤖", category: "ai" },
  { id: "ai-prompt", label: "AI Prompt工程与应用", icon: "✍️", category: "ai" },
  { id: "ai-multimodal", label: "AI多模态与AIGC", icon: "🎨", category: "ai" },
  { id: "ai-llm", label: "AI大模型", icon: "🧠", category: "ai" },
  { id: "ai-deploy", label: "AI模型部署与推理优化", icon: "🚀", category: "ai" },
  { id: "ai-distributed", label: "AI分布式训练与大规模系统", icon: "⚡", category: "ai" },
  { id: "ai-recsys", label: "AI推荐与搜索算法", icon: "🎯", category: "ai" },
  { id: "docker", label: "Docker", icon: "🐳", category: "other" },
  { id: "elasticsearch", label: "Elasticsearch", icon: "🔍", category: "db" },
  { id: "mongodb", label: "MongoDB", icon: "🍃", category: "db" },
  { id: "claude-code", label: "Claude Code", icon: "📎", category: "other" },
];
```

### D6: 代码块语法高亮

使用 `highlight.js`（CDN 加载）作为 marked.js 的 `highlight` 扩展，支持 C/C++/Python/Redis 协议/Mysql 等语法高亮。

## Data Flow

```
data/interview-questions/             scripts/                     data/interview/
  ├── c-language/              ──▶    build-interview-    ──▶      interview-questions.js
  │   ├── static-keyword.md            questions.js                  ├── CATEGORIES
  │   └── ...                          ├── 扫描 .md 文件              ├── INTERVIEW_STACKS
  ├── cpp-language/                    ├── 解析 frontmatter            └── INTERVIEW_QUESTIONS
  │   └── ...                          ├── 按 stack 分组
  ├── redis/                           └── 输出 JS 文件
  └── ...                                                      │
                                                                ▼
                                                         interview.html
                                                           ├── 侧栏：类别药丸 + 技术栈列表
                                                           ├── 中栏：题目列表 + 排序
                                                           ├── 右栏：marked.js 渲染详情
                                                           └── localStorage 记访问
```

## Risks / Trade-offs

- **[SEO]** 面试页面内容在 JS 中渲染，搜索引擎不可见 → 不影响，本项目是个人工具，非公 website
- **[marked.js 版本兼容]** marked.js 各版本 API 有差异 → 锁版本号，写入注释
- **[编译脚本依赖]** 编译脚本需要 Node.js 环境 → 加入 package.json scripts，同时提供 README 说明
- **[MD 格式一致性]** 多人维护 MD 可能格式不一致 → 编译脚本增加 frontmatter 校验，缺少必填字段时报错

## Migration Plan

无迁移需求。这是一个全新的页面，不涉及对现有功能的改造。
