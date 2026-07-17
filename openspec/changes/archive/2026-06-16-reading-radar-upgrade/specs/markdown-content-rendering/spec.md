# 学习内容 Markdown 渲染

## Purpose

learn.html 通过 Markdown 解析器渲染 `.md` 文件作为学习内容源，替代原有的 JS 模板引擎（`data/learn-content-<catId>.js`），支持原生 Markdown 语法（标题、代码块、列表、图片、表格等）。

## ADDED Requirements

### Requirement: Markdown 解析渲染

`learn.html` SHALL 使用 Markdown 解析器（marked.js）将 `.md` 源文件解析为 HTML 并渲染到内容区域。解析器 SHALL 支持以下 Markdown 语法：标题（h1-h6）、代码块（带语言标注）、无序/有序列表、粗体/斜体、链接、图片、表格。

#### Scenario: 解析 Markdown 标题

- **WHEN** 用户访问包含 `## 二级标题` 的知识点
- **THEN** 页面 SHALL 渲染为 `<h2>二级标题</h2>` 元素，样式与页面主题一致

#### Scenario: 解析代码块

- **WHEN** 用户访问包含 C 语言代码块（`\`\`\`c`）的知识点
- **THEN** 页面 SHALL 渲染为带语法高亮的 `<pre><code class="language-c">` 代码块

#### Scenario: 解析表格

- **WHEN** 用户访问包含 Markdown 表格的知识点
- **THEN** 页面 SHALL 渲染为带样式的 `<table>` 元素，表头背景色区分

### Requirement: Markdown 文件加载

`learn.html` SHALL 根据 URL hash 中的 `catId/itemId` 解析，从 `data/learn-deep/{stack}/{quadrant}/{itemId}.md` 路径加载对应的 Markdown 文件。当 stack 为 "vdb" 时（向量数据库），文件路径中的目录前缀为 "db"（即 `data/learn-deep/db/vdb-*.md` 对应向量数据库知识点）。

#### Scenario: 加载 C 语言知识点

- **WHEN** 用户访问 `learn.html#c/c-pointer`
- **THEN** 页面 SHALL 从 `data/learn-deep/c/language/c-pointer.md` 加载 Markdown 文件

#### Scenario: 加载数据库通用知识点

- **WHEN** 用户访问 `learn.html#db/db-acid`
- **THEN** 页面 SHALL 从 `data/learn-deep/db/language/db-acid.md` 加载 Markdown 文件

#### Scenario: 加载向量数据库知识点

- **WHEN** 用户访问 `learn.html#vdb/db-hnsw`
- **THEN** 页面 SHALL 从 `data/learn-deep/db/systems/db-hnsw.md` 加载 Markdown 文件（vdb 栈映射到 db 目录）

#### Scenario: 文件加载失败

- **WHEN** 请求的 Markdown 文件不存在或 404
- **THEN** 页面 SHALL 显示"该知识点内容暂未创建"提示，不崩溃

### Requirement: 兼容 file:// 协议

当通过双击 HTML 文件（`file://` 协议）打开 `learn.html` 时，页面 SHALL 通过 `<script src>` 方式加载 marked.js，通过 `fetch` 加载 `.md` 文件（受浏览器安全限制），并优雅降级提示用户"请使用本地服务器访问"。

#### Scenario: 本地服务器正常加载

- **WHEN** 用户通过 `node server.js` 或 `python -m http.server` 启动的服务器访问 `learn.html`
- **THEN** 页面 SHALL 正常加载 marked.js 和 Markdown 文件并渲染内容

#### Scenario: file:// 协议降级

- **WHEN** 用户通过双击 `learn.html` 文件（file:// 协议）打开
- **THEN** 页面 SHALL 显示友好提示："请使用本地服务器访问（运行 `node server.js`），file:// 协议下 Markdown 文件加载受限。"

## REMOVED Requirements

### Requirement: 学习内容覆盖入门到调优四个层次

**Reason**: 已由自由格式 Markdown 内容替代固定四段式模板

**Migration**: 每个知识点的 MD 文件可自由组织章节结构，不再强制要求包含入门/原理/实战/调优四个 section

### Requirement: 数据文件按技术栈分割

**Reason**: 已由 `data/learn-deep/{stack}/{quadrant}/{id}.md` 的扁平 MD 文件结构替代

**Migration**: `data/learn-content-<catId>.js` 文件不再使用，新内容统一存放在 `data/learn-deep/` 目录下
