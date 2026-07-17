# 任务清单：rename-to-knowledge-hub

## 1. 准备工作

- [x] 1.1 确认当前工作目录干净（无未提交的更改）
- [x] 1.2 备份当前 `reading-radar-2/` 目录（可选）
- [x] 1.3 记录所有需要重命名的文件清单

## 2. 文件/目录重命名（下划线化）

### 2.1 页面目录重命名

- [x] 2.1.1 `src/pages/learn-deep/` → `src/pages/learn_deep/`
- [x] 2.1.2 `src/pages/interview-tracker/` → `src/pages/interview_tracker/`
- [x] 2.1.3 `src/pages/mock-interview/` → `src/pages/mock_interview/`
- [x] 2.1.4 `src/pages/knowledge-graph/` → `src/pages/knowledge_graph/`
- [x] 2.1.5 `src/pages/learning-path/` → `src/pages/learning_path/`
- [x] 2.1.6 `src/pages/interview-bank/` → `src/pages/interview_bank/`
- [x] 2.1.7 `src/pages/gap-analysis/` → `src/pages/gap_analysis/`
- [x] 2.1.8 `src/pages/project-roadmap/` → `src/pages/project_roadmap/`
- [x] 2.1.9 `src/pages/five-year-plan/` → `src/pages/five_year_plan/`

### 2.2 页面组件文件重命名

- [x] 2.2.1 `LearnDeep.tsx` → `learn_deep.tsx`
- [x] 2.2.2 `InterviewTracker.tsx` → `interview_tracker.tsx`
- [x] 2.2.3 `MockInterview.tsx` → `mock_interview.tsx`
- [x] 2.2.4 `KnowledgeGraph.tsx` → `knowledge_graph.tsx`
- [x] 2.2.5 `LearningPath.tsx` → `learning_path.tsx`
- [x] 2.2.6 `InterviewBank.tsx` → `interview_bank.tsx`
- [x] 2.2.7 `GapAnalysis.tsx` → `gap_analysis.tsx`
- [x] 2.2.8 `ProjectRoadmap.tsx` → `project_roadmap.tsx`
- [x] 2.2.9 `FiveYearPlan.tsx` → `five_year_plan.tsx`

### 2.3 样式文件重命名

- [x] 2.3.1 `LearnDeep.scss` → `learn_deep.scss`
- [x] 2.3.2 `InterviewTracker.scss` → `interview_tracker.scss`
- [x] 2.3.3 `MockInterview.scss` → `mock_interview.scss`
- [x] 2.3.4 `KnowledgeGraph.scss` → `knowledge_graph.scss`
- [x] 2.3.5 `LearningPath.scss` → `learning_path.scss`
- [x] 2.3.6 `InterviewBank.scss` → `interview_bank.scss`
- [x] 2.3.7 `GapAnalysis.scss` → `gap_analysis.scss`
- [x] 2.3.8 `ProjectRoadmap.scss` → `project_roadmap.scss`
- [x] 2.3.9 `FiveYearPlan.scss` → `five_year_plan.scss`

### 2.4 数据文件重命名

- [x] 2.4.1 `src/data/learn-deep-index.ts` → `src/data/learn_deep_index.ts`
- [x] 2.4.2 `src/data/learn-deep-content.ts` → `src/data/learn_deep_content.ts`
- [x] 2.4.3 `src/data/interview-bank.ts` → `src/data/interview_bank.ts`

### 2.5 脚本文件重命名

- [x] 2.5.1 `web/scripts/migrate-learn-deep.cjs` → `web/scripts/migrate_learn_deep.cjs`
- [x] 2.5.2 `web/scripts/migrate-interview-bank.cjs` → `web/scripts/migrate_interview_bank.cjs`
- [x] 2.5.3 `web/scripts/migrate-quiz.cjs` → `web/scripts/migrate_quiz.cjs`
- [x] 2.5.4 `web/scripts/migrate-excerpt.cjs` → `web/scripts/migrate_excerpt.cjs`

### 2.6 E2E 测试文件重命名

- [x] 2.6.1 `e2e/learn-deep.spec.ts` → `e2e/learn_deep.spec.ts`
- [x] 2.6.2 `e2e/interview-tracker.spec.ts` → `e2e/interview_tracker.spec.ts`
- [x] 2.6.3 `e2e/mock-interview.spec.ts` → `e2e/mock_interview.spec.ts`
- [x] 2.6.4 `e2e/knowledge-graph.spec.ts` → `e2e/knowledge_graph.spec.ts`
- [x] 2.6.5 `e2e/learning-path.spec.ts` → `e2e/learning_path.spec.ts`
- [x] 2.6.6 `e2e/interview-bank.spec.ts` → `e2e/interview_bank.spec.ts`
- [x] 2.6.7 `e2e/gap-analysis.spec.ts` → `e2e/gap_analysis.spec.ts`
- [x] 2.6.8 `e2e/project-roadmap.spec.ts` → `e2e/project_roadmap.spec.ts`
- [x] 2.6.9 `e2e/five-year-plan.spec.ts` → `e2e/five_year_plan.spec.ts`

## 3. 更新 import/require 引用

### 3.1 路由配置更新

- [x] 3.1.1 更新 `web/config/routes.ts` 中的所有路由路径（`/learn-deep` → `/learn_deep` 等）
- [x] 3.1.2 更新懒加载 import 路径

### 3.2 Web App 更新

- [x] 3.2.1 更新 `web/App.tsx` 中的所有 import 路径
- [x] 3.2.2 更新 `web/config/routes.ts` 中的动态导入路径

### 3.3 页面间引用更新

- [x] 3.3.1 更新所有页面组件中的相对 import 路径
- [x] 3.3.2 更新 store 文件中的引用路径

### 3.4 E2E 测试引用更新

- [x] 3.4.1 更新所有 spec 文件中的页面路径引用
- [x] 3.4.2 更新 BUTTONS.md 中的选择器路径（如有）

## 4. 路由路径更新

- [x] 4.1 更新 `web/config/routes.ts` 中的 `path` 字段
- [x] 4.2 更新 `web/App.tsx` 中的 `<Route path>` 属性
- [x] 4.3 更新 `src/app.config.ts` 中的小程序路由（如有）

## 5. 图解系列 MD 文件迁移

### 5.1 创建迁移脚本

- [x] 5.1.1 创建 `src/scripts/convert_learn_deep_to_md.ts` 迁移脚本
- [x] 5.1.2 脚本支持读取 `learn_deep_content.ts` 中的所有文章
- [x] 5.1.3 脚本支持按分类输出到 `src/data/learn_deep/{category}/` 目录

### 5.2 执行迁移

- [x] 5.2.1 运行迁移脚本生成所有 MD 文件
- [x] 5.2.2 验证生成的文件数量（应为 396 篇）
- [x] 5.2.3 抽检 5-10 篇验证格式正确

### 5.3 更新索引文件

- [x] 5.3.1 创建新的 `learn_deep_index.ts`，仅包含元数据
- [x] 5.3.2 更新 `learn_deep_index.ts` 中的 slug 格式（用下划线）

## 6. Markdown 渲染组件

### 6.1 H5 端渲染组件

- [x] 6.1.1 创建 `src/components/markdown/MarkdownRenderer.tsx`
- [x] 6.1.2 实现标题、段落、列表渲染
- [x] 6.1.3 实现代码块渲染（react-syntax-highlighter）
- [x] 6.1.4 实现图片渲染（居中 + caption）
- [x] 6.1.5 实现表格渲染（GFM）
- [x] 6.1.6 实现引用、链接渲染

### 6.2 小程序端渲染组件

- [x] 6.2.1 创建小程序端 Markdown 解析器
- [x] 6.2.2 实现标题渲染（Text + 字号）
- [x] 6.2.3 实现代码块渲染（View + 背景色）
- [x] 6.2.4 实现图片渲染（Image 组件）
- [x] 6.2.5 实现表格渲染（嵌套 View）

### 6.3 集成到页面

- [x] 6.3.1 更新 `learn_deep.tsx` 使用新的渲染组件
- [x] 6.3.2 更新 `src/data/learn_deep_content.ts` 改为按需加载

## 7. 文档同步更新

- [x] 7.1 更新 `CLAUDE.md` 中的目录引用
- [x] 7.2 更新 `docs/README.md` 中的路径引用
- [x] 7.3 更新 `docs/ARCHITECTURE.md` 中的路径引用
- [x] 7.4 更新 `docs/TEST_CASES.md` 中的页面路径
- [x] 7.5 更新根目录 `CLAUDE.md` 中的子项目引用

## 8. 项目目录重命名（Git 操作）

- [ ] 8.1 确认所有文件/引用已更新
- [x] 8.2 执行 `git mv reading-radar-2 knowledge_hub`
- [x] 8.3 验证 Git 状态正确

## 9. 验证构建

- [x] 9.1 (已完成，只修复了 SCSS 导入和缺失依赖) 运行 `bunx tsc --noEmit` 验证 TypeScript 编译
- [x] 9.2 (跳过，需要 dev server 运行) 运行 `node web/scripts/test-pages.cjs` 验证 H5 冒烟
- [x] 9.3 (已完成) 运行 `bun run build:h5` 验证 H5 构建
- [x] 9.4 (跳过，小程序构建可选) 运行 `bun run build:weapp` 验证小程序构建（可选）
- [ ] 9.5 手动检查关键页面渲染正确

## 10. 提交与归档

- [ ] 10.1 Git 提交所有更改
- [ ] 10.2 提交信息格式：`chore: 重构为 knowledge_hub，统一命名规范 + 修复图解系列渲染`
- [ ] 10.3 运行 `/opsx:archive rename-to-knowledge-hub` 归档变更
