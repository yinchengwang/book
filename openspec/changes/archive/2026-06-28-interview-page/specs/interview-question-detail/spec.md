# 面试题详情展示

## Purpose

展示单道面试题的完整内容，包括回答思路、参考回答、示例辅助理解（代码块高亮）、追问方向（可折叠），使用 marked.js 渲染 MD 内容。

## ADDED Requirements

### Requirement: 详情内容渲染

详情区 SHALL 接收题目的 MD body 内容并使用 marked.js 渲染为 HTML 展示。渲染结果 SHALL 支持代码块、列表、表格、加粗等标准 Markdown 语法。

#### Scenario: MD 正确渲染

- **WHEN** 题目 body 包含 ` ## ` 标题和 ```c 代码块
- **THEN** 详情区 SHALL 正确渲染为 HTML 标题和带语法高亮的代码块

### Requirement: 四个内容板块展示

详情区 SHALL 按顺序展示四个内容板块：
- **📝 回答思路** — 对应 body 中 "## 回答思路" 后的内容
- **💡 参考回答** — 对应 body 中 "## 参考回答" 后的内容
- **🔧 示例辅助理解** — 对应 body 中 "## 示例辅助理解" 后的内容（如不存在则隐藏）
- **❓ 追问方向** — 对应 body 中 "## 追问方向" 后的内容

#### Scenario: 完整内容展示

- **WHEN** 选中一道包含所有四个板块的题目
- **THEN** 四个板块 SHALL 按顺序渲染显示

#### Scenario: 部分板块缺失

- **WHEN** 题目 body 不包含"## 示例辅助理解"板块
- **THEN** 该板块 SHALL 隐藏，不占用页面空间

### Requirement: 追问方向可折叠

"❓ 追问方向"板块中的每个追问 SHALL 默认仅显示标题，用户点击后展开显示完整内容。

#### Scenario: 展开/折叠追问

- **WHEN** 页面加载
- **THEN** 所有追问方向 SHALL 处于折叠状态
- **WHEN** 用户点击"追问 1"的行
- **THEN** 该追问 SHALL 展开显示完整内容
- **WHEN** 用户再次点击该追问
- **THEN** 该追问 SHALL 重新折叠

### Requirement: 代码语法高亮

详情区中的代码块 SHALL 使用 highlight.js 进行语法高亮。支持的语言至少包括 C、C++、Python、SQL、Shell、JavaScript。

#### Scenario: 代码块高亮

- **WHEN** 详情区渲染包含 ```c 的代码块
- **THEN** 该代码块 SHALL 带有 C 语言语法高亮样式
- **WHEN** 语言标识符无法识别
- **THEN** 代码块 SHALL 作为纯文本显示，不使用高亮

### Requirement: 题目标签展示

详情区顶部 SHALL 展示题目的元信息标签：
- 难度标签（入门/进阶/挑战），使用不同颜色区分
- 重要度标签（高频/中频/低频）

#### Scenario: 标签显示

- **WHEN** 用户选择一道难度为"进阶"、重要度为"高频"的题目
- **THEN** 详情顶部 SHALL 显示对应颜色样式的"进阶"标签和"🔥高频"标签

### Requirement: 访问次数记录

用户每次点击某一题目的列表项时，系统 SHALL 将该题目的访问计数 +1 并写入 localStorage。

#### Scenario: 访问计数递增

- **WHEN** 用户首次点击题目 A
- **THEN** localStorage 中 `interview_click_{questionId}` SHALL 值为 1
- **WHEN** 用户再次点击题目 A
- **THEN** localStorage 中 `interview_click_{questionId}` SHALL 值为 2
