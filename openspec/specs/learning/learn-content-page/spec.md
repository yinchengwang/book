# 学习内容页

## Purpose

提供通过 URL hash 路由的知识点学习内容页（learn.html），通过 Markdown 文件渲染学习内容，与看板和评测系统双向联通。

## Requirements

### Requirement: 学习内容页通过 URL hash 路由到具体知识点

`learn.html` SHALL 解析 `window.location.hash`（格式：`#<catId>/<itemId>`），从 `data/learn-deep/{stack}/{quadrant}/{itemId}.md` 路径加载 Markdown 源文件并解析渲染。stack 到目录的映射规则：`c`→`c`、`cpp`→`cpp`、`ds`→`ds`、`db`→`db`、`py`→`py`、`linux`→`linux`、`vdb`→`db`（向量数据库文件存放在 db 目录下）。

#### Scenario: 正确路由到 Python 知识点

- **WHEN** 用户访问 `learn.html#py/py-basic-types`
- **THEN** 页面 SHALL 从 `data/learn-deep/py/language/py-basic-types.md` 加载 Markdown 文件并渲染内容

#### Scenario: 兼容 file:// 协议

- **WHEN** 用户通过双击 HTML 文件（file:// 协议）打开
- **THEN** learn.html SHALL 显示"请使用本地服务器访问"提示，不崩溃

#### Scenario: hash 无效时显示提示

- **WHEN** hash 为空或格式不合法
- **THEN** 页面 SHALL 显示"请选择一个知识点"引导提示，不崩溃

### Requirement: 学习内容自由格式 Markdown

每个知识点的内容 SHALL 以 Markdown 文件格式存放在 `data/learn-deep/{stack}/{quadrant}/{itemId}.md`。内容格式自由，不再强制要求包含固定四个 section（入门/原理/实战/调优）。

#### Scenario: 知识点包含人话开场白

- **WHEN** 知识点 MD 文件首行为人话开场白段落
- **THEN** 页面 SHALL 渲染为带左侧边框和渐变背景的 intro 区块

#### Scenario: 知识点包含图解图片

- **WHEN** 知识点 MD 文件中包含 `![说明](./diagrams/xxx.png)`
- **THEN** 页面 SHALL 渲染为居中图片，附带说明文字 caption

### Requirement: 知识点页面与看板双向联通

learning-kanban.html 中每个知识点卡片 SHALL 新增"学习内容"链接（`learn.html#<catId>/<itemId>`），learn.html 顶部 SHALL 提供返回看板和去测验的导航链接。

#### Scenario: 卡片跳转学习页

- **WHEN** 用户点击看板卡片上的"学习内容"链接
- **THEN** 浏览器 SHALL 跳转到 `learn.html#<catId>/<itemId>`，显示该知识点内容
