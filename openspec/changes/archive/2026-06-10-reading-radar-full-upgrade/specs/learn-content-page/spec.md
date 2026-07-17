## ADDED Requirements

### Requirement: 学习内容页通过 URL hash 路由到具体知识点
`learn.html` SHALL 解析 `window.location.hash`（格式：`#<catId>/<itemId>`），加载对应技术栈的内容数据文件（`data/learn-content-<catId>.js`），并渲染该知识点的学习内容。

#### Scenario: 正确路由到 Python 知识点
- **WHEN** 用户访问 `learn.html#py/py-basic-types`
- **THEN** 页面 SHALL 显示 py-basic-types 知识点的学习内容，标题与 quiz-tech.js 中的 title 一致

#### Scenario: 兼容 file:// 协议
- **WHEN** 用户通过双击 HTML 文件（file:// 协议）打开
- **THEN** learn.html SHALL 正常工作，不依赖 History API 或服务端路由

#### Scenario: hash 无效时显示提示
- **WHEN** hash 为空或格式不合法
- **THEN** 页面 SHALL 显示"请选择一个知识点"引导提示，不崩溃

### Requirement: 学习内容覆盖入门到调优四个层次
每个知识点的内容 SHALL 包含四个 section：入门（概念与基本用法）、原理（底层机制）、实战（工程场景代码示例）、调优/面试（性能优化要点与高频面试题）。

#### Scenario: section 结构完整
- **WHEN** 加载任意知识点的内容数据
- **THEN** 该条目 SHALL 包含 `sections` 数组，数组长度 SHALL 不少于 3

#### Scenario: 实战 section 包含代码示例
- **WHEN** 渲染"实战"层 section
- **THEN** 内容 SHALL 包含至少一个 `<pre><code>` 代码块

### Requirement: 数据文件按技术栈分割
学习内容数据文件 SHALL 按技术栈独立存放：`data/learn-content-py.js`、`data/learn-content-c.js`、`data/learn-content-cpp.js`、`data/learn-content-ds.js`、`data/learn-content-db.js`，以 `const LEARN_CONTENT = {...}` 全局变量导出。

#### Scenario: 动态加载数据文件
- **WHEN** learn.html 解析到 catId 为 "py"
- **THEN** 页面 SHALL 动态插入 `<script src="data/learn-content-py.js">` 并在加载完成后渲染内容

### Requirement: 知识点页面与看板双向联通
learning-kanban.html 中每个知识点卡片 SHALL 新增"学习内容"链接（`learn.html#<catId>/<itemId>`），learn.html 顶部 SHALL 提供返回看板和去测验的导航链接。

#### Scenario: 卡片跳转学习页
- **WHEN** 用户点击看板卡片上的"学习内容"链接
- **THEN** 浏览器 SHALL 跳转到 `learn.html#<catId>/<itemId>`，显示该知识点内容
