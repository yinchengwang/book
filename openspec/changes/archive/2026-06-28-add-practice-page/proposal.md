## Why

现有的 Reading Radar 项目覆盖了读书雷达、看板、测验、学习等丰富功能，但缺少一个系统化的刷题计划页面。用户有清晰的 LeetCode/牛客刷题需求（刷题计划.md），需要一个按知识点分类、难度递进的刷题课程页面，支持完成状态跟踪，帮助用户有计划地提升算法能力。

## What Changes

- 新增 `data/app/practice-data.js` 数据文件：包含约 20+ 个知识分类、共 200+ 道 LeetCode/牛客题目的完整题库，仅含 Easy/Medium 难度
- 新增 `practice.html` 页面：左侧分阶段折叠导航 + 右侧按分类展示题目列表（严格 Easy→Medium 排序），支持复选框跟踪完成状态（localStorage 持久化）
- 修改 `data/app/nav-component.js`：在导航栏中注册「刷题计划」入口项

## Capabilities

### New Capabilities
- `practice-curriculum`: 刷题课程体系，涵盖所有数据结构与算法的题目编排，按 6 个阶段、20+ 知识分类组织，每类 10+ 题，严格 Easy→Medium 排序

### Modified Capabilities

无。本变更不修改现有功能的需求。

## Impact

- 新建 1 个 JS 数据文件（`data/app/practice-data.js`），无运行时依赖
- 新建 1 个 HTML 页面（`practice.html`），遵循现有页面模式
- 修改 1 个现有文件（`nav-component.js`），新增 1 个导航项
- 不修改任何现有功能的行为
