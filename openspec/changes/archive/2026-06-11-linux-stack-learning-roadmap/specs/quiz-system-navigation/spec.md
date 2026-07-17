# 导航系统 Linux 入口

## Purpose

确保 Linux 技术栈在所有页面中可通过导航入口访问。得益于 Phase 0 P0（统一导航栏），页面间跳转已有统一入口。本规格聚焦于页面内部的 Linux 技术栈选择（雷达图模式切换、测验技术栈选择、看板切换等）。

## ADDED Requirements

### Requirement: 雷达图页面支持 Linux 模式

index.html 的 header mode-bar SHALL 新增 Linux 模式按钮（`<button data-mode="linux">🐧 Linux</button>`）。切换到 Linux 模式时 SHALL 从 `LINUX_TECH_DATA` 读取知识点数据渲染雷达图。

#### Scenario: Linux 模式按钮可见

- **WHEN** 用户打开 index.html
- **THEN** header mode-bar SHALL 显示 "🐧 Linux" 按钮
- **THEN** 按钮与其他技术栈模式按钮（📚 读书雷达 / 🔧 C 技术栈等）样式一致

#### Scenario: 点击 Linux 模式切换雷达图

- **WHEN** 用户点击 "🐧 Linux" 按钮
- **THEN** 雷达图 SHALL 切换为 Linux 技术栈的知识点布局（56 个节点，4 个象限）

### Requirement: 仪表盘包含 Linux 进度卡片

dashboard.html 的 stacks-grid SHALL 新增 Linux 进度卡片，展示 Linux 技术栈的已掌握数、测验覆盖率和平均分。卡片样式 SHALL 与其他五个技术栈保持一致。

#### Scenario: Linux 进度卡片显示

- **WHEN** 用户打开 dashboard.html
- **THEN** stacks-grid SHALL 包含 6 个技术栈卡片（C/C++/DS/DB/PY + Linux）
- **THEN** Linux 卡片的 border-color SHALL 使用专属颜色（如 `#2ecc71` 绿色）

### Requirement: 测验系统自动适配 Linux

quiz-system.html 通过遍历 `CATEGORIES` 对象渲染技术栈导航按钮。由于 `CATEGORIES.linux` 已在数据层注册（quiz-static.js），quiz-system.html SHALL 自动显示 Linux 导航按钮和知识点列表。

#### Scenario: Linux 在测验系统中可选

- **WHEN** 用户在 quiz-system.html 中查看技术栈选择
- **THEN** Linux（🐧）SHALL 作为可选技术栈出现在导航中

### Requirement: 看板系统自动适配 Linux

learning-kanban.html 通过 kanban-render.js 的共享渲染函数读取 `LINUX_KANBAN_DATA`（从 items-registry 自动派生），SHALL 自动显示 Linux 看板卡片。

#### Scenario: Linux 看板卡片可见

- **WHEN** 用户在 learning-kanban.html 中选择 Linux 技术栈
- **THEN** 56 张 Linux 看板卡片 SHALL 显示在对应的象限和 ring 分组中

### Requirement: 统一导航栏覆盖页面间跳转

得益于 Phase 0 P0 的统一导航栏，所有 6 个页面 SHALL 通过顶部导航栏实现页面间跳转。Linux 相关内容 SHALL 可通过页面内部的技术栈选择器访问，无需在导航栏中为每个技术栈设置独立入口。

#### Scenario: 导航闭环

- **WHEN** 用户依次在 index.html → quiz-system.html → dashboard.html → learning-kanban.html 之间导航
- **THEN** 每个页面顶部 SHALL 显示统一导航栏，提供到达其他页面的入口
- **THEN** Linux 技术栈相关的导航入口 SHALL 在各页面内部正常工作
