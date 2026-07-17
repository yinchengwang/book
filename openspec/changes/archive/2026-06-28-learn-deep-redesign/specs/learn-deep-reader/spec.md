# learn-deep-reader

右侧文章阅读器规格

## ADDED Requirements

### Requirement: 文章内容渲染

系统 SHALL 使用 Markdown 渲染器显示文章内容，支持标题、代码块、图片、表格。

#### Scenario: Markdown 渲染
- **WHEN** 用户选择文章
- **THEN** 右侧区域渲染 Markdown 内容，包括标题层级、代码高亮、图片居中

### Requirement: 文章元信息

文章内容区 SHALL 显示文章标题、分类标签、阅读时长。

#### Scenario: 显示元信息
- **WHEN** 渲染文章详情
- **THEN** 显示标题（h1）、分类 badge、预估阅读时间

### Requirement: 上/下篇导航

文章底部 SHALL 显示上/下篇导航按钮，允许用户在同分类内切换文章。

#### Scenario: 上篇导航
- **WHEN** 用户点击"← 上一篇"
- **THEN** 加载并显示同分类中的上一篇文章，右侧内容区滚动到顶部

#### Scenario: 下篇导航
- **WHEN** 用户点击"下一篇 →"
- **THEN** 加载并显示同分类中的下一篇文章，右侧内容区滚动到顶部

#### Scenario: 无上/下篇
- **WHEN** 当前文章是分类中的第一篇或最后一篇
- **THEN** 对应方向导航按钮不显示或禁用

### Requirement: 返回分类

文章顶部 SHALL 显示返回列表或分类的链接。

#### Scenario: 返回操作
- **WHEN** 用户点击面包屑中的分类名
- **THEN** 侧边栏滚动到该分类并展开

### Requirement: 滚动行为

文章内容区 SHALL 支持垂直滚动，与侧边栏独立。

#### Scenario: 独立滚动
- **WHEN** 用户滚动右侧内容区
- **THEN** 左侧侧边栏位置保持不变
