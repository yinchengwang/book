# RR2 UI Bugfix Round 2 — 任务清单

## 阶段 1：核心 Bug 修复（4h）

### 1.1 学习路径 / 摘抄 length 崩溃兜底
- [x] 1.1.1 修 `learningPathStore.ts:migrate`：强制 `activePath = null, activePathId = null, currentStep = 0`
- [x] 1.1.2 修 `excerptStore.ts:migrate`：老 tags 兼容（`Array.isArray(e.tags) ? e.tags : (e.tags ? [e.tags] : [])`）
- [x] 1.1.3 兜底 5 页面通用：LearningPath.tsx / Excerpt.tsx / LearnDeep.tsx / Radar.tsx / GapAnalysis.tsx 加可选链
- [x] 1.1.4 浏览器实测 5 页面，控制台无 length 错

### 1.2 Review 4 统计卡可点击
- [x] 1.2.1 `Review.tsx`: 4 个 stat-card 加 onClick → setTab 对应 tab
- [x] 1.2.2 点击后滚动到 tab 内容（scrollIntoView）
- [x] 1.2.3 激活态样式（卡片高亮 + tab 自动选中）

### 1.3 Quiz list 模式折叠卡
- [x] 1.3.1 `Quiz.tsx`: list 模式卡片改为可展开
- [x] 1.3.2 默认显示：title + 难度 + 分类 + domain + 已答次数
- [x] 1.3.3 展开后：description 前 100 字 + options 预览（选择题）+ 收藏按钮 + 直接答题
- [x] 1.3.4 `Quiz.scss`: 新增 `.question-list-item--expanded` 样式
- [x] 1.3.5 list 模式顶部加统计：当前 stack 题数 / 已答 / 答对

## 阶段 2：浅色主题全局覆盖（3h）

### 2.1 写 light-fixes.scss 全局覆盖
- [x] 2.1.1 创建 `web/styles/light-fixes.scss`
- [x] 2.1.2 全局：卡片/按钮/输入框/进度条/徽章/标签/进度卡/统计卡
- [x] 2.1.3 Sidebar 导航项 + TopBar 头像 + 通知下拉
- [x] 2.1.4 各页面特定修复（learning-path / radar / knowledge-graph / quiz / review）
- [x] 2.1.5 `main.tsx` 引入 light-fixes.scss

### 2.2 浅色主题验证
- [x] 2.2.1 切换 light 后截图：仪表盘 / Quiz / 复习 / 学习路径 / 雷达
- [x] 2.2.2 修复残留的白字/看不清问题

## 阶段 3：E2E 自动化测试覆盖（7h）

### 3.1 仪表盘按钮补全
- [x] 3.1.1 `e2e/dashboard.spec.ts` 扩：打卡项 4 按钮 / 查看详细计划 / 4 统计卡 / 7 stack 进度 / 5 年路线
- [x] 3.1.2 验证每个按钮点击后导航正确

### 3.2 新增 11 个 spec
- [x] 3.2.1 `e2e/quiz.spec.ts` — 4 模式 + 折叠卡 + stack/category/difficulty 三维筛选
- [x] 3.2.2 `e2e/radar.spec.ts` — 8 主题切换 + 圆点点击 + 详情弹窗
- [x] 3.2.3 `e2e/excerpt.spec.ts` — 摘抄 CRUD + 三维筛选 + 状态切换
- [x] 3.2.4 `e2e/learning-path.spec.ts` — 路径切换 + 折叠展开 + sub-step 勾选
- [x] 3.2.5 `e2e/learn-deep.spec.ts` — 分类筛选 + 搜索 + 详情 + 上下篇
- [x] 3.2.6 `e2e/gap-analysis.spec.ts` — 空状态 + 有数据切换
- [x] 3.2.7 `e2e/settings.spec.ts` — API key 显示/隐藏/复制 + 主题切换 + 导出
- [x] 3.2.8 `e2e/interview-tracker-full.spec.ts` — 创建/删除/状态切换/详情
- [x] 3.2.9 `e2e/mock-interview-full.spec.ts` — 配置 + 答题 + 反馈
- [x] 3.2.10 `e2e/interview-bank-full.spec.ts` — 分类 + 详情 + 复制
- [x] 3.2.11 `e2e/topbar-layout.spec.ts` — Sidebar 导航 + TopBar 主题切换 + 通知 + 用户下拉

### 3.3 全量回归
- [x] 3.3.1 `bun run test:e2e` 全部 14 个 spec 通过
- [x] 3.3.2 `test-pages.cjs` 12/12 通过

## 阶段 4：构建验证 + 文档（2h）

### 4.1 构建铁律（按 CLAUDE.md）
- [x] 4.1.1 `bunx tsc --noEmit` 0 错误
- [x] 4.1.2 `bun run build:h5` 0 错误
- [x] 4.1.3 `bun run build:weapp` 0 错误

### 4.2 文档同步
- [x] 4.2.1 `docs/TEST_CASES.md` 补 14 个 E2E spec 的 TC 编号
- [x] 4.2.2 `openspec/specs/e2e-coverage/spec.md` 补 Scenario

### 4.3 归档
- [x] 4.3.1 归档到 `openspec/changes/archive/2026-06-28-rr2-ui-bugfix-round-2/`
- [x] 4.3.2 同步 delta spec 到 main specs

## 总任务数

| 阶段 | 任务数 |
|------|--------|
| 阶段 1 核心 Bug | 11 |
| 阶段 2 浅色主题 | 7 |
| 阶段 3 E2E 测试 | 14 |
| 阶段 4 构建+文档+归档 | 7 |
| **合计** | **39** |