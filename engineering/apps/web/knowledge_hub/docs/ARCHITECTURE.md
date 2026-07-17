# Reading Radar 2.0 - 架构设计文档

> 最后更新：2026-06-23 · 版本：2.0

## 一、技术栈总览

| 层级 | 技术 | 版本 |
|------|------|------|
| 框架 | Taro | 3.6.40 |
| UI 库 | React | 18.3.1 |
| 语言 | TypeScript | 5.x |
| 状态管理 | Zustand + persist | 4.5.5 |
| 路由 | React Router DOM (H5) / Taro Router (小程序) | v6 |
| 样式 | SCSS + CSS Variables | - |
| 构建 | Taro CLI (小程序) / Vite 5 (H5) | 5.4.21 |
| 测试 | Playwright | 1.61 |
| 包管理 | Bun | 1.3.13 |

## 二、目录结构

```
knowledge_hub/
├── web/                          # H5 端入口（Vite）
│   ├── App.tsx                  # 路由 + 错误边界
│   ├── main.tsx                 # React 启动 + 全局错误捕获
│   ├── components/Layout/       # 布局组件（TopBar / Sidebar / Layout）
│   ├── adapters/                # Taro 组件 → H5 适配
│   ├── styles/                  # 全局样式（global.scss + 主题变量）
│   ├── config/routes.ts         # 路由配置 + 动态 badge
│   └── scripts/                 # 迁移 + 测试脚本
├── src/                         # 跨端共享代码（Taro）
│   ├── pages/                   # 页面（19 个）
│   │   ├── index/                # 仪表盘
│   │   ├── quiz/                 # 题库练习
│   │   ├── review/               # 复习计划
│   │   ├── interview_tracker/    # 面试追踪
│   │   ├── interview_bank/       # 面试题库
│   │   ├── mock_interview/       # 模拟面试
│   │   ├── knowledge_graph/      # 知识图谱
│   │   ├── learning_path/       # 学习路径
│   │   ├── learn_deep/           # 图解系列
│   │   ├── excerpt/              # 摘抄管理
│   │   ├── gap_analysis/         # 差距分析
│   │   ├── radar/                # 技术雷达
│   │   ├── five_year_plan/       # 五年计划
│   │   ├── project_roadmap/      # 项目路线
│   │   ├── kanban/               # 看板
│   │   ├── dashboard/            # 老仪表盘（备用）
│   │   ├── settings/             # 设置
│   │   └── not-found/            # 404
│   ├── stores/                   # Zustand store（12 个）
│   ├── data/                     # 数据文件（题库 / 摘抄 / 雷达 / 学习路径）
│   ├── hooks/                    # 自定义 hooks
│   └── services/                 # 服务（导出 / 数据同步）
├── e2e/                         # Playwright e2e 测试
├── docs/                        # 项目文档
├── project.config.json          # Taro 小程序配置
└── package.json
```

## 三、核心架构

### 3.1 数据流

```
┌─────────────────────────────────────────────────────────┐
│                    Page (React 组件)                       │
│  ↓ useStore()                                            │
├─────────────────────────────────────────────────────────┤
│              Zustand Store (with persist)                 │
│  - in-memory state                                       │
│  - actions: 修改状态的方法                                  │
│  - persist: 自动写入 localStorage                          │
├─────────────────────────────────────────────────────────┤
│                  Data Layer (TS)                          │
│  - 静态数据：题库 / 摘抄 / 雷达 / 学习路径                   │
│  - 动态生成：store 派生数据                                 │
└─────────────────────────────────────────────────────────┘
```

### 3.2 Store 列表

| Store | 持久化 Key | 用途 |
|-------|-----------|------|
| `userStore` | `rr2-user` | 用户昵称/头像 |
| `settingsStore` | `settings-storage` | 主题/API 配置/通知 |
| `notificationStore` | (无持久化) | 全局通知（动态派生） |
| `quizStore` | `quiz-storage` | 题库 + 答题记录 + 错题本 |
| `reviewStore` | `review-storage` | SM-2 复习记录 |
| `knowledgeStore` | `knowledge-storage` | 知识图谱节点 + 缺口分析（v2 清空初始 gaps） |
| `learningPathStore` | `learning_path-storage` | 学习路径 + sub-step 完成状态（v2） |
| `excerptStore` | `excerpt-storage` | 摘抄（v2 合并老项目 149 条） |
| `interviewStore` | `interview_tracker-storage` | 面试追踪公司 + 阶段 |
| `interviewBankStore` | `rr2-interview_bank` | 面试题库浏览记录 |
| `mockInterviewStore` | (无) | 模拟面试状态 |
| `fiveYearPlanStore` | `rr2-five_year_plan` | 五年计划打卡数据 |
| `dailyDataStore` | (无) | 每日数据聚合 |
| `projectStore` | (无) | 项目路线 |

### 3.3 主题系统

通过 `:root[data-theme="light|dark"]` 切换 CSS 变量：

```scss
:root,
:root[data-theme="dark"] {
  --bg-0: #0a0a1a;
  --primary: #6366f1;
  --text: #fff;
  /* ... */
}

:root[data-theme="light"] {
  --bg-0: #f1f5f9;
  --primary: #6366f1;
  --text: #0f172a;
  /* ... */
}
```

触发机制：
1. `settingsStore.theme` 变化
2. `useThemeEffect` hook 在 `Layout` 中订阅，写 `document.documentElement.dataset.theme`
3. CSS 变量自动响应

### 3.4 路由系统

`web/config/routes.ts` 中定义所有路由：

```typescript
export const NAV_ITEMS: NavItem[] = [
  {
    path: '/quiz',
    title: '题库练习',
    icon: '📝',
    group: '练习',
    badgeGetter: () => useQuizStore.getState().dueReviews.length  // 动态 badge
  },
  // ...
]
```

动态 badge：
- `Sidebar` 组件每秒调用 `badgeGetter()` 重算
- 避免硬编码 badge（之前是 `10`、`32` 等错误数字）

懒加载：
```typescript
'/quiz': lazy(() => import('@/pages/quiz/Quiz'))
```

每个页面独立 chunk，错误隔离由 `PageErrorBoundary` 包裹。

### 3.5 数据迁移脚本

位于 `web/scripts/`：
- `migrate-interview_bank.cjs` - 老 project/reading-radar/data/ → src/data/interview_bank/
- `migrate-excerpt.cjs` - 老 project/reading-radar/data/excerpt/ → src/data/excerpt/initialExcerpts.ts
- `migrate-quiz.cjs` - 老 quiz-static.js → src/data/quiz-*.ts
- `migrate-learn_deep.cjs` - 老 learn_deep → src/data/learn_deep-*.ts
- `test-pages.cjs` - 12 页冒烟测试

### 3.6 错误处理

三层错误捕获：
1. **全局 window 错误**：`window.addEventListener('error')` + `'unhandledrejection')`（`web/main.tsx`）
2. **React ErrorBoundary**：每个页面独立 `PageErrorBoundary`（`web/App.tsx`）
3. **局部 try/catch**：store actions / API 调用

错误上报：
- 控制台打印详细堆栈
- 暴露 `window.__lastError` 供测试断言

### 3.7 Taro H5 双端适配

Taro 组件（View/Text/Input）在 H5 端通过 `web/adapters/` 转换为原生 HTML 元素：

```typescript
// web/adapters/Input.tsx
export const Input = forwardRef<HTMLInputElement, InputProps>((props, ref) => {
  return <input ref={ref} {...props} />
})
```

跨端跳转：
```typescript
function navigateTo(path: string) {
  if (typeof window !== 'undefined') {
    window.location.href = path  // H5
  } else {
    Taro.navigateTo({ url: path })  // 小程序
  }
}
```

## 四、关键页面设计

### 4.1 仪表盘（index）

- 5 个总览卡片（总题/已测/平均分/已掌握/累计）
- 7 个技术栈进度卡
- 5 年路线图（当前年高亮）
- 热力图 + 趋势图

### 4.2 题库（quiz）

- 4 种模式：今日计划 / 题库浏览 / 答题中 / 错题本 / 统计
- 三维筛选：技术栈（7）+ 分类（5）+ 难度（5）

### 4.3 学习路径（learning_path）

- 3 条路径（C++/算法/系统设计）
- 大点拆小点（每 step 3-7 个 sub-step）
- Sub-step 类型：knowledge / practice / project

### 4.4 雷达图（radar）

- 8 个主题（书籍 + 7 stack）
- 4 象限 × 3 环 = 12 槽位（书籍）
- 8 维度多边形（stack）
- 8 轮力导向碰撞检测

### 4.5 知识图谱（knowledge_graph）

- SVG 树状布局（按 domain 分列）
- 节点可点击 → 弹详情（掌握度 / 关联节点 / 学习数据）
- SVG line + arrow marker 绘制有向边

## 五、性能与质量

### 5.1 性能优化

- 路由懒加载：`React.lazy(() => import(...))`
- 状态局部订阅：`useQuizStore(s => s.answers)`
- 派生数据 `useMemo` 缓存
- SVG 自适应：`window.innerWidth` 监听

### 5.2 持久化迁移策略

每个 store 用 `version` + `migrate` 函数处理老数据：

```typescript
{
  name: 'knowledge-storage',
  version: 2,
  migrate: (persistedState, version) => {
    if (version < 2) {
      return { ...persistedState, gaps: {} }  // 清空硬编码初始值
    }
    return persistedState
  }
}
```

### 5.3 测试覆盖

详见 [TEST_CASES.md](TEST_CASES.md)。

## 六、部署

详见 [h5-deploy.md](h5-deploy.md) 和 [weapp-deploy.md](weapp-deploy.md)。

## 七、变更历史

| 版本 | 日期 | 变更 |
|------|------|------|
| 2.0 | 2026-06 | H5 Vite 重构 + 完整页面 + 主题切换 + 多雷达 |
| 1.x | 2025 | 原 Taro H5 + 基础页面 |