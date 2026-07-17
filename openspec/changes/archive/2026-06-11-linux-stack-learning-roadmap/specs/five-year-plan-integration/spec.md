# 五年计划集成（增量变更）

## ADDED Requirements

### Requirement: dashboard.html 显示当月学习进度

dashboard.html 的五年计划摘要区块 SHALL 新增当月学习进度显示，从 `LEARNING_ROADMAP` 数据中读取当前月份的已掌握/总知识点数，并展示在计划卡片中。

#### Scenario: 当月学习进度可见

- **WHEN** 用户打开 dashboard.html
- **THEN** 五年计划摘要卡片 SHALL 显示当前月份的学习进度（如 "6月学习进度: 3/8"）
- **THEN** 进度数据 SHALL 从 localStorage 中对应看板状态计算得出

### Requirement: dashboard.html 五年路线摘要包含 Linux

dashboard.html 的五年路线摘要 SHALL 在 2026 筑基年阶段的要点中包含 Linux 技术栈相关的学习目标。

#### Scenario: 筑基年摘要包含 Linux

- **WHEN** 用户查看 dashboard.html 的"五年成长路线"区块
- **THEN** 2026 筑基年阶段卡片 SHALL 包含至少一条与 Linux 系统学习相关的要点

## MODIFIED Requirements

### Requirement: dashboard.html 嵌入五年计划摘要区块

dashboard.html 底部 SHALL 新增"五年成长路线"区块，展示各阶段名称、时间范围、核心目标，数据硬编码在 dashboard.html 内（不新增数据文件）。该区块 SHALL 额外包含当前月份的看板学习进度摘要。

#### Scenario: 五年计划区块可见

- **WHEN** 用户打开 dashboard.html 并滚动到底部
- **THEN** 页面 SHALL 显示至少 3 个阶段的摘要卡片，每个卡片包含阶段名、时间区间、核心目标

#### Scenario: 五年计划区块链接到详情页

- **WHEN** 用户点击"查看完整计划"链接
- **THEN** 浏览器 SHALL 跳转到 five-year-plan.html

### Requirement: five-year-plan.html 增加返回仪表盘的导航

five-year-plan.html 顶部 SHALL 新增返回 dashboard.html 的链接，实现导航闭环。

#### Scenario: 返回链接显示

- **WHEN** 用户在 five-year-plan.html 页面
- **THEN** 页面顶部 SHALL 显示"← 返回仪表盘"链接，点击后跳转 dashboard.html

### Requirement: 五年计划数据不修改 localStorage 键名

现有 five-year-plan.html 使用的 localStorage 键名 SHALL 保持不变，确保用户已保存的自定义数据不丢失。学习路线图相关数据 SHALL 使用独立的 `learning-roadmap-state` 键名，与现有五年计划数据分离。

#### Scenario: 现有用户数据兼容

- **WHEN** 已有用户打开融合后的 dashboard.html
- **THEN** 现有 localStorage 中的五年计划相关数据 SHALL 仍然可以被 five-year-plan.html 正常读取
- **THEN** 新学习路线图数据 SHALL 存储在 `learning-roadmap-state` 键名下，不与现有键名冲突
