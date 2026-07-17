# Reading Radar 2.0 全面重构提案

## Why

当前 Reading Radar 基于纯 HTML/JS 技术栈，存在以下限制：
1. **技术债务**：无法复用组件、状态管理混乱、维护成本高
2. **多端缺失**：无法生成微信小程序，限制移动端使用场景
3. **UI 落后**：缺乏现代交互体验（动画、手势、玻璃拟态）
4. **扩展性差**：新增功能需要大量重复代码

与此同时，Coolearn 项目展示了更现代的设计方向（React + Tailwind + 深色毛玻璃），但功能范围与 Reading Radar 不同。

**机会**：合并两者优势，构建一个功能完整、体验现代、多端覆盖的学习追踪平台。

## What Changes

### 技术架构
- **框架迁移**：从 Vanilla HTML/JS → Taro 4.x + React 18
- **样式系统**：从内联 CSS → Tailwind CSS 3（参考 Coolearn 设计语言）
- **构建目标**：一次开发，编译为 H5 网页 + 微信小程序

### 功能重构
| 模块 | 现状 | 目标 |
|------|------|------|
| 仪表盘 | 简单统计卡片 | 综合仪表盘 + 成长趋势 + 热力图 |
| 雷达图 | Canvas 手绘 | Recharts 雷达 + 交互增强 |
| 看板 | 拖拽卡片 | 保留核心，UI 升级 |
| 测验系统 | 基础题目练习 | 智能出题 + AI 批改 + 语音输入 |
| 摘抄管理 | Markdown 笔记 | 富文本编辑 + Obsidian 双向链接 |
| 学习路径 | 无 | Step-by-Step 路径导航 |
| 知识图谱 | 无 | 节点可视化 + 关联展示 |
| 差距分析 | 无 | 薄弱点诊断 + 优先推荐 |
| 复习系统 | 无 | 间隔重复 + 记忆曲线 |
| 面试追踪 | 基础状态机 | 完整面试流程 + 复盘 |
| 模拟面试 | 无 | AI 面试官（预埋） |
| 项目路线 | 无 | 项目驱动学习规划（预埋） |

### UI/UX 升级
- **设计语言**：深色毛玻璃（glassmorphism）+ 透明边框
- **动画系统**：Framer Motion 交互动画
- **响应式**：移动端优先，支持手势操作
- **图标**：统一使用 Lucide React

### 数据架构
- **存储层**：本地 localStorage + IndexedDB（可扩展为云端）
- **同步预埋**：数据模型设计支持未来云端同步

## Capabilities

### New Capabilities

- `tech-radar`: 技术雷达图 — 可视化技术栈掌握度，支持多维度评分
- `knowledge-kanban`: 知识看板 — 拖拽管理读书/学习状态（未读/阅读中/已读/掌握）
- `quiz-system`: 测验系统 — 题库练习、智能出题、错题本、AI 批改（预埋）
- `excerpt-manager`: 摘抄管理 — Markdown 笔记、标签分类、Obsidian 双向链接
- `learning-path`: 学习路径 — Step-by-Step 路径导航，进度追踪
- `knowledge-graph`: 知识图谱 — 节点可视化、关联展示、@xyflow/react
- `gap-analysis`: 差距分析 — 薄弱点诊断、优先推荐学习方向
- `review-system`: 复习系统 — 间隔重复算法、记忆曲线、SM-2 算法
- `interview-tracker`: 面试追踪 — 完整面试流程管理、状态机、复盘记录
- `mock-interview`: 模拟面试 — AI 面试官交互（预埋 API 接口）
- `project-roadmap`: 项目路线 — 项目驱动学习规划（预埋）
- `dashboard`: 仪表盘 — 全局统计、成长趋势、热力图、日历视图
- `multi-platform`: 多端适配 — Taro 编译层，支持 H5 + 小程序双端

### Modified Capabilities

- 无需修改现有 capabilities（全新项目）

## Impact

### 新增文件结构
```
reading-radar-2/
├── src/
│   ├── components/     # React 组件库
│   ├── pages/          # 页面组件
│   ├── hooks/          # 自定义 Hooks
│   ├── stores/         # Zustand 状态管理
│   ├── data/           # 静态数据
│   ├── utils/          # 工具函数
│   └── styles/         # Tailwind 配置
├── config/             # Taro 配置
├── project/reading-radar/  # 保留原项目（可选删除）
└── ...
```

### 依赖变更
- 新增：React 18, Taro 4.x, Tailwind CSS 3, Recharts, @xyflow/react, Framer Motion, Zustand, Lucide React
- 移除：Vanilla JS, 内联 CSS

### 数据迁移
- 将 `items-registry.js` → `data/knowledgeData.ts`
- 将 `BOOK_DATA` → 知识卡片数据模型
- 将题库 → TypeScript 常量

### 平台目标
- H5 网页：现代浏览器
- 微信小程序：基础库 2.20+
