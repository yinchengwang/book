# Markdown 渲染组件规范

## Purpose

为 knowledge_hub 项目提供跨平台的 Markdown 渲染能力，支持 H5 端和微信小程序端。

## ADDED Requirements

### Requirement: H5 端 Markdown 渲染

H5 端 SHALL 使用 `react-markdown` + `remark-gfm` + `react-syntax-highlighter` 渲染 Markdown 内容，支持以下语法：

| 语法 | 渲染结果 |
|------|----------|
| `# 标题` | h1-h6 标题，带样式 |
| `` `code` `` | 行内代码，背景高亮 |
| ` ```lang\ncode\n``` ` | 代码块，语法高亮 |
| `![alt](url)` | 图片，居中 + caption |
| `> 引用` | 引用块，左边框样式 |
| `- item` | 无序列表 |
| `1. item` | 有序列表 |
| `**bold**` | 粗体 |
| `*italic*` | 斜体 |
| `[text](url)` | 链接 |
| `\| col1 \| col2 \|` | 表格（GFM） |

#### Scenario: 渲染标题层级

- **WHEN** Markdown 内容包含 `#`、`##`、`###` 等标题
- **THEN** 渲染为对应层级的 h1-h6 元素，样式层级分明

#### Scenario: 渲染代码块

- **WHEN** Markdown 内容包含 C/C++ 代码块
- **THEN** 渲染为带语法高亮的代码块，支持语言标注（如 ` ```c `）

#### Scenario: 渲染图片

- **WHEN** Markdown 内容包含 `![图解](https://example.com/img.png)`
- **THEN** 图片居中显示，下方附带说明文字（alt），最大宽度 90%

#### Scenario: 渲染表格

- **WHEN** Markdown 内容包含 GFM 表格
- **THEN** 渲染为带表头样式的表格，单元格边框清晰

#### Scenario: 渲染外部链接

- **WHEN** Markdown 内容包含外部链接 `[text](https://example.com)`
- **THEN** 链接新窗口打开（`target="_blank"`），添加 `rel="noopener noreferrer"`

### Requirement: 小程序端 Markdown 渲染

小程序端 SHALL 实现兼容的 Markdown 渲染方案，在 Taro 环境下工作。

#### Scenario: 渲染标题

- **WHEN** 小程序端渲染包含 `#` 的 Markdown
- **THEN** 使用 `Text` 组件配合字号样式（h1=28px, h2=24px, h3=20px）

#### Scenario: 渲染代码块

- **WHEN** 小程序端渲染代码块
- **THEN** 使用 `View` + `Text` 组件，代码区带背景色（`#1e1e1e`），支持横向滚动

#### Scenario: 渲染图片

- **WHEN** 小程序端渲染 `![alt](url)`
- **THEN** 使用 `Image` 组件，支持图片懒加载

#### Scenario: 渲染表格

- **WHEN** 小程序端渲染 GFM 表格
- **THEN** 使用嵌套 `View` 模拟表格，行背景色交替

### Requirement: Markdown 解析器组件接口

渲染组件 SHALL 提供统一的接口，屏蔽双端差异。

#### Scenario: 组件接受 Markdown 字符串

- **WHEN** 调用 `<MarkdownRenderer content={mdString} />`
- **THEN** 组件返回完整渲染后的 HTML/React 元素

#### Scenario: 组件处理空内容

- **WHEN** 传入空字符串或 null
- **THEN** 组件显示"暂无内容"提示，不崩溃

#### Scenario: 组件处理大型内容

- **WHEN** 传入超过 50KB 的 Markdown 内容
- **THEN** 组件分段渲染或使用虚拟列表，避免渲染阻塞

### Requirement: 代码块语法高亮

代码块 SHALL 支持以下编程语言的语法高亮：

- C / C++
- Python
- JavaScript / TypeScript
- SQL
- Shell / Bash
- JSON / YAML

#### Scenario: 高亮 C 语言代码

- **WHEN** 渲染 ` ```c\nint main() { ... }\n``` `
- **THEN** 关键字（`int`, `return`）、类型、字符串等使用不同颜色区分

#### Scenario: 高亮 JSX/TSX

- **WHEN** 渲染包含 React 组件的代码
- **THEN** JSX 标签、属性、插值表达式使用对应颜色
