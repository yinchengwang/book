# learn-deep-sidebar

左侧分类导航栏规格

## ADDED Requirements

### Requirement: 侧边栏布局

系统 SHALL 在 H5 端渲染左侧固定宽度（250px）的分类导航栏。

#### Scenario: 侧边栏渲染
- **WHEN** H5 端加载图解系列页面
- **THEN** 左侧显示固定宽度的分类导航栏，右侧显示内容区

### Requirement: 分类树形结构

侧边栏 SHALL 显示可折叠的分类列表，每个分类可展开显示文章列表。

#### Scenario: 展开分类
- **WHEN** 用户点击未展开的分类标题
- **THEN** 该分类展开，显示该分类下的文章列表

#### Scenario: 折叠分类
- **WHEN** 用户点击已展开的分类标题
- **THEN** 该分类折叠，隐藏文章列表

### Requirement: 文章选择

用户 SHALL 能够通过点击侧边栏中的文章标题选择文章，选中的文章 SHALL 在右侧高亮显示。

#### Scenario: 选择文章
- **WHEN** 用户点击侧边栏中的文章标题
- **THEN** 该标题高亮，右侧内容区加载并显示该文章内容

### Requirement: 当前位置指示

侧边栏 SHALL 显示当前选中的分类和文章。

#### Scenario: 显示当前位置
- **WHEN** 用户选择了某篇文章
- **THEN** 该分类自动展开，文章标题高亮显示

### Requirement: 分类标题

侧边栏顶部 SHALL 显示"图解系列"标题。

#### Scenario: 显示标题
- **WHEN** 侧边栏渲染
- **THEN** 顶部显示"📖 图解系列"标题

### Requirement: 小程序端布局

小程序端 SHALL 保持简单列表模式，不实现双栏布局。

#### Scenario: 小程序列表模式
- **WHEN** 小程序端加载图解系列页面
- **THEN** 显示分类列表和文章列表，与现有布局保持一致
