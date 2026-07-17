# 统一导航集成

## Purpose

面试页面与 reading-radar 全局导航栏的集成决策。

## ADDED Requirements

### Requirement: 独立页面（不集成导航栏）

面试页面 SHALL 是独立页面，不加载 `nav-component.js`，不显示 reading-radar 项目的顶部导航栏。页面完全自包含。

#### Rationale

面试页面与 reading-radar 主项目（文章索引/标签页）在风格和使用场景上存在差异。独立页面避免了导航栏占用垂直空间，提供了更专注的面试题浏览体验。

#### Scenario: 页面无导航栏

- **WHEN** 用户打开 `interview.html`
- **THEN** 页面顶部 SHALL NOT 显示 reading-radar 导航栏
- **THEN** 页面 SHALL 显示自定义的深色渐变 header
- **THEN** 页面 SHALL 自行处理所有内部导航和路由

### Requirement: nav-component.js 不修改

`data/app/nav-component.js` SHALL NOT 因本变更而修改。没有任何入口需要注册到 reading-radar 导航栏中。
