# light-theme-coverage

## Purpose

为 Reading Radar 2.0 H5 端提供完整的浅色主题 UI 适配，确保切换到 light 模式后所有页面文字、按钮、卡片、进度条、徽章均清晰可读。

## Requirements

### Requirement: 全局浅色变量覆盖

`web/styles/light-fixes.scss` 必须通过 `:root[data-theme="light"]` 选择器覆盖 dark 模式硬编码的深色样式：

- 顶栏（TopBar）：背景 `var(--bg-1)` + 文字 `var(--text)` + 边框 `var(--border)`
- 侧栏（Sidebar）：导航项 hover 背景、激活态高亮、徽章对比度
- 卡片：背景 `var(--card-bg)` + 阴影 `var(--shadow)` + 边框 `var(--border)`
- 按钮：文字色 `var(--text)` + 禁用态半透明
- 输入框：背景 `var(--bg-1)` + 占位符 `var(--text-4)`

#### Scenario: 浅色模式加载覆盖样式
- **WHEN** 主题切换为 light 且页面加载
- **THEN** light-fixes.scss 立即生效，全局背景变浅、文字变深、按钮文字清晰可见
- **AND** dark 主题下 light-fixes 不应用（不影响 dark 默认样式）

### Requirement: 组件级浅色适配

针对 11 个核心页面，硬编码颜色必须被覆盖：

| 组件 | 修复点 |
|------|--------|
| 仪表盘 5 卡片 | 数字 / 标签 / 进度条背景 |
| Quiz 难度徽章 | 5 难度等级（入门/初级/中级/高级/专家）颜色对比 |
| Quiz 选项 hover | 当前选项高亮 |
| Quiz 折叠卡 | 展开/折叠背景色 |
| 复习计划 4 统计卡 | 数字 + 标签 + 状态徽章 |
| 面试追踪阶段徽章 | 14 个阶段（投递/简历筛选/.../已 offer）颜色 |
| 学习路径步骤卡 | 大步骤/小任务徽章 + 折叠展开按钮 |
| 雷达图 SVG | 圆点 + 连线 + 文字标签 |
| 知识图谱 SVG | 节点 + 边线 + 详情弹窗 |
| 摘抄卡片 | 状态徽章（4 状态）+ 收藏图标 + 笔记 |
| 差距分析 5 领域 | 进度条 + 警告红色 |

#### Scenario: 浅色下所有 11 页面无看不清文字
- **WHEN** 用户在 light 主题下访问每个页面
- **THEN** 文字与背景对比度 ≥ 4.5:1（WCAG AA 标准）
- **AND** 按钮可点击区域文字清晰

### Requirement: 主入口引入 light-fixes

`web/main.tsx` 必须在 dark 主题变量后引入 `light-fixes.scss`：

```ts
import './styles/global.scss'
import './styles/light-fixes.scss'  // 新增
```

#### Scenario: 主题切换 0 闪烁
- **WHEN** 用户点击 TopBar 🌙 按钮切换主题
- **THEN** light-fixes 立即生效，无白屏闪烁、无组件残留 dark 色