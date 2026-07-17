# 面试题数据

## Purpose

定义面试题的数据规范，包括 MD 源文件、编译生成的 JS 数据文件、以及数据结构和字段约定。

## ADDED Requirements

### Requirement: CATEGORIES 数组

数据文件 SHALL 包含 `CATEGORIES` 常量数组，定义技术栈的分类体系：

```js
const CATEGORIES = [
  { id: "all", label: "全部" },
  { id: "lang", label: "语言" },
  { id: "db", label: "数据库" },
  { id: "cs", label: "计算机基础" },
  { id: "ai", label: "AI" },
  { id: "other", label: "其他" },
];
```

### Requirement: INTERVIEW_STACKS 数组

数据文件 SHALL 包含 `INTERVIEW_STACKS` 常量数组，每个技术栈对象必须包含 `id`、`label`、`icon`、`category` 字段。

- `id`：技术栈的唯一标识，用于技术栈切换和题目分组
- `label`：技术栈的中文展示名称
- `icon`：技术栈图标 emoji
- `category`：所属分类的 id，与 `CATEGORIES` 中的 id 对应

技术栈列表共 20 个：

| id | label | category |
|---|---|---|
| c-language | C 语言 | lang |
| cpp-language | C++ | lang |
| python | Python | lang |
| redis | Redis | db |
| mysql | MySQL | db |
| elasticsearch | Elasticsearch | db |
| mongodb | MongoDB | db |
| os | 操作系统 | cs |
| network | 计算机网络 | cs |
| design-pattern | 设计模式 | cs |
| ai-rag | AI知识工程与RAG | ai |
| ai-agent | AI Agent理论与框架 | ai |
| ai-prompt | AI Prompt工程与应用 | ai |
| ai-multimodal | AI多模态与AIGC | ai |
| ai-llm | AI大模型 | ai |
| ai-deploy | AI模型部署与推理优化 | ai |
| ai-distributed | AI分布式训练与大规模系统 | ai |
| ai-recsys | AI推荐与搜索算法 | ai |
| docker | Docker | other |
| claude-code | Claude Code | other |

### Requirement: INTERVIEW_QUESTIONS 对象

数据文件 SHALL 包含 `INTERVIEW_QUESTIONS` 对象，键为技术栈 id，值为该技术栈下的题目数组。

```js
const INTERVIEW_QUESTIONS = {
  "c-language": [
    {
      id: "static-keyword",
      title: "static 关键字在 C 语言中的作用？",
      difficulty: "beginner",
      priority: "high",
      tags: ["关键字", "存储类"],
      body: "## static 关键字的作用...\n\n...",
      followUps: [
        {
          question: "static 全局变量和普通全局变量有什么区别？",
          answer: "...",
        },
      ],
    },
  ],
  // ...
};
```

### Requirement: 题目字段

每道题目 SHALL 包含以下字段：

- `id`：题目唯一标识（同目录下唯一即可）
- `title`：题目标题，string
- `difficulty`：难度，枚举 `"beginner"` | `"intermediate"` | `"advanced"`
- `priority`：重要度，枚举 `"high"` | `"medium"` | `"low"`
- `tags`：标签数组，string[]
- `body`：Markdown 正文
- `followUps`：追问方向数组，可选，每项含 `question` 和 `answer`

### Requirement: MD 源文件规范

每个 MD 源文件 SHALL 以 YAML frontmatter 开头，包含 `title`、`difficulty`、`priority`、`tags`。编译脚本 SHALL 解析 frontmatter 生成 JS 数据文件的对应字段。

```yaml
---
title: static 关键字在 C 语言中的作用？
difficulty: beginner
priority: high
tags:
  - 关键字
  - 存储类
---
```

如果 frontmatter 缺少 `difficulty` 或 `priority`，编译脚本 SHALL 使用默认值（`"intermediate"` 和 `"medium"`）并输出控制台警告。

### Requirement: 追问方向

追问方向在 MD 源文件中 SHALL 以 `\d. ` 格式的编号列表标记，编译脚本 SHALL 按此格式拆分：

```markdown
## 追问方向

1. static 全局变量和普通全局变量有什么区别？
   static 全局变量的作用域限于当前文件...

2. static 局部变量的生命周期是怎样的？
   从程序开始到结束...
```

编译脚本 SHALL 将每个 `序号. 问题` + 下方文本解析为 `{ question, answer }` 对象。

### Requirement: 第二阶段覆盖机制

当第一阶段 mock 数据文件存在且编译脚本首次运行时，SHALL 保留 mock 数据不被空覆盖：

1. 脚本 SHALL 检查 `data/interview/interview-questions.js` 是否已存在
2. 如果不存在，SHALL 从 scratch 生成（含 header + 空数据占位）
3. 如果存在，SHALL 读取现有数据，合并 MD 源文件内容
4. 当 `data/interview-questions/` 目录为空或不含任何 `.md` 文件时，SHALL 保持现有数据不变
