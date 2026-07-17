# 学习页视觉重设计

## Purpose

`learn.html` 视觉升级：Markdown 渲染、流式长文布局、图解居中展示、侧边栏滚动高亮，取代原有的四模块卡片布局。

## ADDED Requirements

### Requirement: 内容区宽度

`learn.html` 的内容区域最大宽度 SHALL 设为 900px（原 ~780px），以提升长文阅读的舒适度。侧边栏宽度调整为 280px（原 320px）。页面总最大宽度调整为 1360px。

#### Scenario: 内容区宽度适配

- **WHEN** 用户在 1920x1080 分辨率查看 learn.html
- **THEN** 内容区最大宽度 SHALL 为 900px，左右留白均匀

#### Scenario: 侧边栏宽度适配

- **WHEN** 用户在 1920x1080 分辨率查看 learn.html
- **THEN** 侧边栏宽度 SHALL 为 280px，与内容区通过 grid 布局排列

### Requirement: 流式长文布局

`learn.html` SHALL 放弃原有的四模块卡片（入门路径/核心原理/实战与工作/调优与面试）固定布局，改为流式长文布局。每个知识点 MD 文件的章节直接渲染为段落，段落之间通过间距区分，不再使用卡片容器。

#### Scenario: 章节标题渲染

- **WHEN** MD 文件包含 `## 二级标题`
- **THEN** 页面 SHALL 渲染为 `<h2>` 标题，下方带底线分隔，不包裹在卡片容器中

#### Scenario: 段落间距

- **WHEN** MD 文件包含多个段落
- **THEN** 相邻段落之间 SHALL 有 16px 垂直间距，行高 1.9

### Requirement: 侧边栏滚动高亮

`learn.html` 的侧边栏目录 SHALL 通过 `IntersectionObserver` 监听各章节的可视状态，当某个章节进入可视区域时，侧边栏中对应目录项高亮显示。

#### Scenario: 滚动时目录高亮

- **WHEN** 用户向下滚动 learn.html 页面
- **THEN** 当前可视区域的章节标题 SHALL 在侧边栏中对应高亮（背景色 + 左侧边框色）

#### Scenario: 点击目录跳转

- **WHEN** 用户点击侧边栏目录项
- **THEN** 页面 SHALL 平滑滚动（smooth scroll）至对应章节

### Requirement: 上一页/下一页导航

`learn.html` 底部 SHALL 保留 [上一页] [下一页] 导航，点击后跳转到同技术栈下相邻知识点。

#### Scenario: 下一页导航

- **WHEN** 用户点击底部"下一页"按钮
- **THEN** 页面 SHALL 跳转到当前技术栈下下一个知识点（如 `c-pointer` → `c-dynamic-memory`）

#### Scenario: 首尾导航禁用

- **WHEN** 当前为第一个知识点
- **THEN** [上一页] 按钮 SHALL 禁用（灰色不可点击）

- **WHEN** 当前为最后一个知识点
- **THEN** [下一页] 按钮 SHALL 禁用（灰色不可点击）
