# Reading Radar 2.0 使用指南

> 基于 Taro + React + TypeScript + TailwindCSS 的多端学习追踪平台

## 📋 目录

- [快速开始](#快速开始)
- [核心功能](#核心功能)
- [页面介绍](#页面介绍)
- [数据管理](#数据管理)
- [自定义配置](#自定义配置)
- [常见问题](#常见问题)

---

## 快速开始

### 安装

```bash
cd knowledge_hub
npm install
```

### 开发

```bash
# H5 开发
npm run dev:h5

# 小程序开发
npm run dev:weapp
```

### 构建

```bash
# 构建 H5
npm run build:h5

# 构建小程序
npm run build:weapp
```

---

## 核心功能

### 📊 仪表盘

- 全局学习进度统计
- 技术栈掌握度可视化
- 快捷入口导航

### 🧠 知识雷达

- 四象限知识分类
- 技术掌握度雷达图
- 知识详情查看

### 📝 复习系统

基于 **SM-2 间隔重复算法**：

| 评级 | 说明 | 间隔倍数 |
|------|------|----------|
| 5 - 完美 | 立即回忆 | ×1.3 |
| 4 - 良好 | 稍后回忆 | ×1.2 |
| 3 - 一般 | 需复习 | ×1.0 |
| 1 - 模糊 | 需强化 | ×0.5 |
| 0 - 忘记 | 重置 | 1天 |

### 💼 面试追踪

14 状态面试流程：

```
投递 → 简历筛选 → 笔试 → 一面 → 二面 → 三面 → HR → offer
                                                    ↓
                                              拒绝/待定
```

### 📚 摘抄管理

- Markdown 格式摘抄
- 书籍/标签分类
- Obsidian 双向链接

---

## 页面介绍

### 首页 `/`

学习数据总览，包含统计卡片和快捷入口。

### 复习 `/pages/review/Review`

- **待复习**：根据 SM-2 算法筛选的卡片
- **复习统计**：学习进度和历史记录
- **复习设置**：调整每日复习量

### 知识雷达 `/pages/knowledge_graph/KnowledgeGraph`

> H5 端：交互式图谱  
> 小程序端：列表视图

展示知识点之间的关系网络。

### 差距分析 `/pages/gap_analysis/GapAnalysis`

- 各领域掌握度分析
- 薄弱点识别
- 学习目标设定

### 面试追踪 `/pages/interview_tracker/InterviewTracker`

- 公司列表管理
- 面试状态追踪
- 面试记录

### 学习路径 `/pages/learning_path/LearningPath`

预设学习路径：

- C++ 进阶路线
- 算法与数据结构
- 系统设计

### 摘抄管理 `/pages/excerpt/Excerpt`

- 摘抄列表（支持书籍/标签视图）
- Markdown 编辑器
- 搜索与筛选

### 设置 `/pages/settings/Settings`

- 主题切换（深色/浅色）
- LLM API 配置
- 数据导出/导入

---

## 数据管理

### 数据导出

```typescript
// 在设置页面点击「导出数据」
// 导出格式：JSON
```

### 数据导入

```typescript
// 在设置页面点击「导入数据」
// 导入格式：JSON
```

### 本地存储

使用 `localStorage` + Zustand persist 中间件：

- 复习记录
- 面试追踪
- 用户设置
- 摘抄笔记

---

## 自定义配置

### 修改知识数据

编辑 `src/data/knowledgeData.ts`：

```typescript
export const KNOWLEDGE_DATA: DomainConfig = {
  algorithm: {
    name: '算法与数据结构',
    items: [
      { id: 'algo-1', name: '数组', category: 'algorithm', level: 3, mastery: 85 },
      // ...
    ]
  }
}
```

### 修改题库

编辑 `src/data/quizQuestions.ts`：

```typescript
export const QUIZ_QUESTIONS: QuizQuestion[] = [
  {
    id: 'q-1',
    question: '什么是时间复杂度？',
    answer: '...',
    category: 'algorithm',
    difficulty: 'easy'
  }
]
```

### 添加学习路径

编辑 `src/stores/learningPathStore.ts`：

```typescript
const DEFAULT_PATHS: LearningPath[] = [
  {
    id: 'cpp-advanced',
    title: 'C++ 进阶路线',
    steps: [
      { id: 's1', title: '内存管理', status: 'pending', resources: [] }
    ]
  }
]
```

---

## 常见问题

### Q: 如何添加新的知识分类？

1. 在 `knowledgeData.ts` 中添加新的领域配置
2. 在 `app.config.ts` 中注册页面（如需要）

### Q: 如何修改复习间隔？

编辑 `src/stores/reviewStore.ts` 中的 SM-2 参数：

```typescript
// SM-2 算法参数
const SM2_CONFIG = {
  initialInterval: 1,    // 初始间隔（天）
  minInterval: 1,        // 最小间隔
  maxInterval: 365,     // 最大间隔
  easeFactor: 2.5       // 难度系数
}
```

### Q: 如何禁用某个页面？

编辑 `app.config.ts`，注释掉对应页面路径。

### Q: 如何配置微信登录？

1. 在微信公众平台配置 AppID
2. 配置登录权限
3. 调用 `wx.login()` 获取 code

---

## 技术栈

| 技术 | 版本 | 用途 |
|------|------|------|
| Taro | 3.6.40 | 多端框架 |
| React | 18.2 | UI 库 |
| TypeScript | 5.4 | 类型系统 |
| TailwindCSS | 3.4 | 样式方案 |
| Zustand | 4.5 | 状态管理 |
| Recharts | 2.12 | 图表 |
| Framer Motion | 11.0 | 动画 |

---

## 许可证

MIT License
