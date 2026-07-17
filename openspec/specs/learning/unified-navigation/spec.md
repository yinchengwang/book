# 统一导航栏

## Purpose

在 reading-radar 的 6 个 HTML 页面中建立统一的导航栏组件，用户可在任意页面间自由跳转，消除 index.html 孤立岛屿问题。导航栏通过共享的配置文件和渲染函数实现，支持桌面端水平导航和移动端汉堡菜单。

## Requirements

### Requirement: 导航配置集中定义

系统 SHALL 在 `data/nav-component.js` 中定义 `NAV_CONFIG` 对象，包含所有页面的导航入口（标签、链接、图标）。`initGlobalNav(currentPageId)` 函数 SHALL 根据配置动态渲染导航栏 DOM。

#### Scenario: NAV_CONFIG 包含所有页面

- **WHEN** 页面加载 `nav-component.js`
- **THEN** `NAV_CONFIG` SHALL 包含 6 个导航项：雷达图、看板、测验、学习、仪表盘、五年计划
- **THEN** 每个导航项 SHALL 具备 `id`、`label`、`href`、`icon` 字段

### Requirement: 所有页面显示统一导航栏

6 个 HTML 页面（index.html、learning-kanban.html、quiz-system.html、learn.html、dashboard.html、five-year-plan.html）SHALL 在 `<body>` 顶部包含 `<nav id="global-nav"></nav>` 容器，并调用 `initGlobalNav()` 渲染。

#### Scenario: 导航栏在任意页面可见

- **WHEN** 用户打开任意 reading-radar 页面
- **THEN** 页面顶部 SHALL 显示水平导航栏，包含所有 6 个入口

### Requirement: 当前页面高亮

`initGlobalNav(currentPageId)` SHALL 为当前页面对应的导航项添加 `.active` CSS 类，使其在视觉上区别于其他导航项。

#### Scenario: 当前页高亮

- **WHEN** 用户在 index.html（currentPageId="radar"）
- **THEN** 导航栏中"雷达图"项 SHALL 有 `.active` 样式
- **THEN** 其他导航项 SHALL NOT 有 `.active` 样式

### Requirement: 移动端汉堡菜单

当视口宽度 < 768px 时，导航栏 SHALL 折叠为汉堡菜单图标（☰）。点击图标 SHALL 展开垂直导航下拉列表。

#### Scenario: 移动端导航折叠

- **WHEN** 用户在宽度 < 768px 的设备上打开任意页面
- **THEN** 导航项 SHALL 隐藏，仅显示汉堡图标
- **WHEN** 用户点击汉堡图标
- **THEN** 导航项 SHALL 以垂直列表形式展开
