# Reading Radar 2.0 技术设计方案

## Context

### 背景

Reading Radar 2.0 是对现有 reading-radar 项目的全面重构，目标：
- 合并 Reading Radar 和 Coolearn 的功能优势
- 采用现代化技术栈（React + Taro + Tailwind）
- 支持网页 + 微信小程序双端
- 保留本地优先存储，同时为云端同步预留扩展能力

### 现状分析

**Reading Radar 现有资产**：
- 296 个技术知识点（items-registry.js）
- C/C++/数据结构/数据库/Algo 五大技术栈
- 题库系统（~500+ 题目）
- 读书雷达图、看板、测验、仪表盘、摘抄、面试追踪等页面
- Node.js 服务端（server.js）用于数据持久化

**Coolearn 设计参考**：
- 深色毛玻璃 UI（glassmorphism）
- 玻璃卡片：bg-white/10 + backdrop-blur-xl
- 主色：#6366F1 (Indigo)
- 组件库：Lucide React 图标、Framer Motion 动画
- 雷达图：Recharts
- 知识图谱：@xyflow/react

### 约束条件

1. **本地优先**：数据存储在 localStorage/IndexedDB，无需服务端
2. **多端统一**：一次开发，编译到 H5 + 小程序
3. **性能优先**：首屏 < 2s，动画 60fps
4. **可扩展**：为未来云端同步预留 API 接口设计

## Goals / Non-Goals

**Goals：**
- 构建完整的学习追踪平台（参考 Coolearn 功能）
- 迁移现有 Reading Radar 所有功能
- 实现深色毛玻璃 UI 设计语言
- 完成 H5 + 小程序双端适配
- 建立可维护的组件库架构

**Non-Goals：**
- 云端同步功能（预留接口，暂不实现）
- 用户登录系统（单人使用）
- 实时协作功能
- 支付/订阅系统

## Decisions

### 决策 1：Taro vs UniApp vs 原生双端

| 方案 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| **Taro** | React 生态成熟、社区活跃、文档完善 | 小程序包体积较大 | ✅ 选用 |
| UniApp | Vue 支持好 | React 支持较弱 | ❌ |
| 原生双端 | 性能最优 | 代码重复、维护成本高 | ❌ |

**选定**：Taro 4.x + React 18

---

### 决策 2：Tailwind CSS vs CSS Modules vs Styled Components

| 方案 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| **Tailwind CSS** | 与 Coolearn 一致、响应式内置、减少 CSS 文件 | 需要构建时处理、class 较长 | ✅ 选用 |
| CSS Modules | 原生支持、无学习成本 | 需要额外配置、样式分散 | ❌ |
| Styled Components | 样式封装好、主题支持 | 运行时开销、不利于小程序 | ❌ |

**选定**：Tailwind CSS 3 + Coolearn 设计系统

---

### 决策 3：状态管理方案

| 方案 | 适用场景 | 结论 |
|------|----------|------|
| **Zustand** | 轻量、无 Provider 嵌套、支持持久化 | ✅ 选用 |
| Redux | 复杂大型应用 | 过度设计 |
| Recoil | Facebook 官方 | 社区活跃度低 |
| Jotai | 原子化状态 | 与 React 绑定 |

**选定**：Zustand + persist middleware（支持 localStorage）

---

### 决策 4：数据持久化方案

```
┌─────────────────────────────────────────────────────────────┐
│                    本地存储层                               │
│                                                             │
│  ┌─────────────┐     ┌─────────────┐     ┌─────────────┐ │
│  │ localStorage │ ←→  │  IndexedDB  │     │  Cloud API  │ │
│  │  (配置/小数据)│     │  (大文件缓存)│     │  (预埋)     │ │
│  └──────┬──────┘     └─────────────┘     └──────┬──────┘ │
│         │                                       │          │
│         └─────────────────┬───────────────────┘          │
│                           ▼                                │
│                  ┌─────────────────┐                       │
│                  │   DataService   │                       │
│                  │   (统一接口层)   │                       │
│                  └────────┬────────┘                       │
│                           ▼                                │
│                  ┌─────────────────┐                       │
│                  │   Store (Zustand)│                       │
│                  └────────┬────────┘                       │
│                           ▼                                │
│                  ┌─────────────────┐                       │
│                  │   React 组件     │                       │
│                  └─────────────────┘                       │
└─────────────────────────────────────────────────────────────┘
```

**策略**：
- 简单数据（配置、状态）：localStorage
- 大数据（题库、知识图谱）：IndexedDB（通过 Dexie.js）
- 云端 API：预留 `CloudService` 接口，暂不使用

---

### 决策 5：页面路由设计

```
┌─────────────────────────────────────────────────────────────┐
│                    应用结构                                 │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │                   Layout (根布局)                    │   │
│  │  ┌─────────┬───────────────────────────────────┐   │   │
│  │  │ Sidebar │        Content Area               │   │   │
│  │  │ (侧边栏) │   (主内容区，路由渲染)            │   │   │
│  │  │         │                                    │   │   │
│  │  │ • 首页   │   <Dashboard />                    │   │   │
│  │  │ • 雷达图 │   <TechRadar />                   │   │   │
│  │  │ • 看板   │   <Kanban />                      │   │   │
│  │  │ • 测验   │   <Quiz />                        │   │   │
│  │  │ • 复习   │   <Review />                      │   │   │
│  │  │ • 图谱   │   <KnowledgeGraph />              │   │   │
│  │  │ • 差距   │   <GapAnalysis />                 │   │   │
│  │  │ • 摘抄   │   <Excerpt />                     │   │   │
│  │  │ • 面试   │   <InterviewTracker />             │   │   │
│  │  │ • 路径   │   <LearningPath />                │   │   │
│  │  │ • 设置   │   <Settings />                    │   │   │
│  │  └─────────┴───────────────────────────────────┘   │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

**路由配置**（Taro）：
```typescript
const routes = [
  { path: '/pages/index', name: '首页/仪表盘' },
  { path: '/pages/radar', name: '技术雷达' },
  { path: '/pages/kanban', name: '知识看板' },
  { path: '/pages/quiz', name: '刷题测验' },
  { path: '/pages/review', name: '复习系统' },
  { path: '/pages/graph', name: '知识图谱' },
  { path: '/pages/gap', name: '差距分析' },
  { path: '/pages/excerpt', name: '摘抄管理' },
  { path: '/pages/interview', name: '面试追踪' },
  { path: '/pages/path', name: '学习路径' },
  { path: '/pages/settings', name: '设置' },
];
```

---

### 决策 6：组件分层架构

```
┌─────────────────────────────────────────────────────────────┐
│                    组件分层                                 │
│                                                             │
│  L4: Pages（页面组件）                                      │
│      └─ 组合 L3 组件，整页编排                              │
│                                                             │
│  L3: Features（功能组件）                                   │
│      └─ QuizCard, RadarChart, KanbanBoard...               │
│                                                             │
│  L2: UI（基础 UI 组件）                                    │
│      └─ Button, Card, Badge, Input, Modal...               │
│                                                             │
│  L1: Primitives（原子组件）                                 │
│      └─ Icon, Text, Divider, Avatar...                    │
└─────────────────────────────────────────────────────────────┘
```

**组件库目录**：
```
src/
├── components/
│   ├── ui/              # L1 + L2: Card, Button, Modal...
│   ├── features/        # L3: RadarChart, QuizCard...
│   └── layouts/         # Layout, Sidebar, Header...
├── pages/               # L4: 各页面组件
├── hooks/               # 业务逻辑 Hooks
├── stores/              # Zustand stores
└── utils/               # 工具函数
```

---

### 决策 7：知识数据模型

```typescript
// 知识领域
interface KnowledgeDomain {
  id: string;              // 'A', 'B', 'C', 'D'...
  name: string;            // '编程基础', '后端工程'...
  icon: string;            // Lucide 图标名
  color: string;           // '#3b82f6'
  weight: number;          // 权重（影响雷达图）
  description: string;     // 描述
  children?: KnowledgeItem[];
}

// 知识项
interface KnowledgeItem {
  id: string;              // 'A1', 'A2'...
  name: string;            // '数据类型'...
  level: 1 | 2 | 3;       // 难度等级
  status: 'mastered' | 'learning' | 'weak';
  children?: KnowledgeItem[]; // 子知识点（可选，支持树形）
}

// 读书卡片
interface BookCard {
  id: string;
  title: string;
  author: string;
  domain: string;          // 所属领域
  quadrant: string;        // 象限：language/systems/algorithms/engineering
  status: 'unread' | 'reading' | 'read' | 'mastered';
  progress: number;        // 0-100
  rating?: number;         // 1-5 星
  hasPdf?: boolean;
  tags: string[];
  notes?: string;          // 笔记摘要
  createdAt: string;
  updatedAt: string;
}

// 测验题目
interface QuizQuestion {
  id: string;
  domain: string;          // 所属领域
  category: '知识题' | '理解题' | '场景题' | '架构题' | '代码题';
  difficulty: 1 | 2 | 3 | 4;
  topic?: string;          // 所属主题
  question: string;
  answer: string;
  explanation?: string;
}

// 学习进度
interface LearningProgress {
  questionId: string;
  correctCount: number;   // 累计答对次数
  revealed: boolean;
  lastReviewed: string;
  userAnswers: AnswerRecord[];
}

// 复习记录（SM-2 算法）
interface ReviewRecord {
  topicId: string;
  easiness: number;        // 难度因子 (2.5 默认)
  interval: number;        // 间隔天数
  repetitions: number;     // 重复次数
  nextReview: string;      // 下次复习时间
}
```

---

### 决策 8：小程序特殊处理

```typescript
// 需要条件编译的组件
// #ifdef MP-WEIXIN
import { useNativeShare } from '@/utils/wechat';
// #endif

// 需要适配的样式
// 小程序不支持 backdrop-filter，需要用图片模拟或条件隐藏
// 小程序不支持 CSS 动画，需要用 animation API

// 路由跳转差异
// H5: React Router
// 小程序: Taro.navigateTo / Taro.switchTab
```

---

## Risks / Trade-offs

### 风险 1：Taro 包体积

[风险] 小程序包体积可能超过 2MB 限制
[缓解]
- 使用 Taro 的 Tree Shaking
- 按需加载页面组件
- 图片资源压缩
- 代码分割（dynamic import）

---

### 风险 2：小程序 CSS 限制

[风险] 小程序不支持 `backdrop-filter`、`CSS 动画`、`vmax` 等
[缓解]
- 毛玻璃效果：降级为纯色半透明背景
- 动画：使用 Taro.createAnimation() 或 Lottie
- 响应式单位：统一使用 rem/rpx，避免 vw/vh/vmax

---

### 风险 3：@xyflow/react 小程序兼容

[风险] @xyflow/react 主要面向 Web，小程序可能不支持
[缓解]
- 知识图谱功能仅在 H5 端启用
- 小程序端降级为列表视图
- 使用条件编译 `#ifndef MP-WEIXIN`

---

### 风险 4：数据迁移复杂度

[风险] 从 Vanilla JS 迁移到 TypeScript，数据结构变化大
[缓解]
- 分阶段迁移，先迁移数据结构
- 保留原项目作为参考
- 使用 TypeScript strict mode 强制类型检查

---

### 风险 5：学习曲线

[风险] React + Taro + Tailwind + TypeScript 组合学习成本
[缓解]
- 先完成项目脚手架搭建
- 组件开发前先编写设计规范
- 参考 Coolearn 源码实现

---

## Migration Plan

### Phase 0：准备工作（1 周）
```
1. 创建 Taro 项目脚手架
2. 配置 Tailwind CSS
3. 配置 ESLint + Prettier
4. 配置 TypeScript strict mode
5. 搭建组件目录结构
6. 配置 Git + CI/CD
```

### Phase 1：基础设施（1 周）
```
1. 设计系统组件（Button, Card, Input...）
2. Layout 布局组件（Sidebar, Header）
3. 路由配置
4. 状态管理配置（Zustand + persist）
5. 数据服务层（DataService）
```

### Phase 2：核心页面（2-3 周）
```
Week 1-2:
- 仪表盘（Dashboard）
- 技术雷达图（TechRadar）
- 知识看板（Kanban）

Week 2-3:
- 测验系统（Quiz）
- 复习系统（Review）
```

### Phase 3：功能完善（2-3 周）
```
Week 1-2:
- 摘抄管理（Excerpt）
- 差距分析（GapAnalysis）
- 学习路径（LearningPath）

Week 2-3:
- 面试追踪（InterviewTracker）
- 知识图谱（KnowledgeGraph）
```

### Phase 4：多端适配（1 周）
```
1. 小程序适配
2. 响应式优化
3. 性能优化
4. 兼容性测试
```

### Phase 5：上线准备（1 周）
```
1. 数据迁移脚本
2. 文档编写
3. 发布配置
4. 灰度发布
```

---

## Open Questions

### Q1: 是否需要 SSR？

当前方案是纯客户端渲染。如果未来需要 SEO，可能需要引入 SSR。
**建议**：MVP 阶段纯 CSR，后续按需添加。

---

### Q2: 图标方案？

Coolearn 使用 Lucide React。但 Lucide 在小程序中有兼容性问题。
**备选**：
- `@iconify/react` + icones.js.org（推荐）
- Vexip UI（Vue 生态，不适用）
- 直接使用 SVG 内联

---

### Q3: 是否需要 PWA 支持？

渐进式 Web 应用可以提升移动端体验。
**建议**：MVP 暂不支持，后续作为增强功能。

---

### Q4: 数据导出格式？

用户可能需要导出数据。
**建议**：
- JSON 格式（默认）
- Markdown 格式（摘抄）
- CSV 格式（统计数据）

---

### Q5: 面试追踪状态机扩展？

现有状态机是否满足需求？
**当前**：未读 → 一面 → 二面 → 三面 → 体检 → Offer → 入职
**可能需要**：添加「面试公司」「面试官信息」「薪资信息」等字段
