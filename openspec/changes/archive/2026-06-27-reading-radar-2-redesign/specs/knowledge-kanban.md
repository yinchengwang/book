# Knowledge Kanban 知识看板

## Overview

看板用于管理读书/学习资源的状态，通过拖拽实现状态流转。

## Features

### F3.1 四象限看板

**描述**：按象限分类的卡片看板

**象限定义**：
| 象限 | 颜色 | 说明 |
|------|------|------|
| 语言基础 | #43a047 | 语法、标准库、语言特性 |
| 系统底层 | #1e88e5 | 内存、网络、系统编程 |
| 算法数据 | #ef6c00 | 数据结构、算法、复杂度 |
| 工程实践 | #8e24aa | 设计模式、架构、工程化 |

**列配置**：
| 状态 | 标题 | 颜色 |
|------|------|------|
| unread | 待读 | #b0b0b0 |
| reading | 阅读中 | #5c9ce6 |
| read | 已读 | #67c23a |
| mastered | 掌握 | #e6a23c |

---

### F3.2 卡片组件

**描述**：单个资源/书籍的卡片

**卡片字段**：
```typescript
interface KanbanCard {
  id: string;
  title: string;          // 书名/资源名
  author?: string;        // 作者
  quadrant: string;      // 所属象限
  stack: string;          // 技术栈
  status: 'unread' | 'reading' | 'read' | 'mastered';
  progress: number;       // 0-100
  rating?: number;        // 1-5 星
  hasPdf?: boolean;       // 是否有 PDF
  tags: string[];         // 标签
  notes?: string;         // 笔记摘要
  createdAt: string;
  updatedAt: string;
}
```

**卡片 UI**：
```
┌────────────────────────────────┐
│ [PDF] 书名标题                 │ ← 右上角 PDF 图标
│                                │
│ [语言基础] [C] [系统编程]     │ ← 标签
│                                │
│ ⭐⭐⭐☆☆                     │ ← 星级评分
│ ████████░░░░░ 80%            │ ← 进度条
│                                │
│ [删除] [编辑] [详情]          │ ← 操作按钮（悬停显示）
└────────────────────────────────┘
```

**卡片样式**：
- 背景：`bg-white/10 backdrop-blur-xl`
- 边框：左侧 3px 状态色
- 悬停：`translateY(-2px) + shadow-card-hover`
- 拖拽中：`opacity: 0.5 + rotate(2deg)`

---

### F3.3 拖拽交互

**描述**：卡片在不同状态列之间拖拽

**实现**：
- 使用 `@dnd-kit/core` 库
- 支持鼠标拖拽 + 触摸拖拽

**交互流程**：
1. 长按/鼠标按下 → 卡片进入拖拽态
2. 拖动 → 跟随鼠标/手指
3. 进入目标列 → 目标列高亮
4. 释放 → 卡片插入目标列，更新状态

**状态更新**：
```typescript
const handleDragEnd = (event) => {
  const { active, over } = event;
  if (active.id !== over.id) {
    updateCardStatus(active.id, over.status);
  }
};
```

---

### F3.4 看板分组切换

**描述**：切换分组维度

**模式**：
| 模式 | 描述 |
|------|------|
| 按象限 | 4 列 × 4 状态 = 16 列 |
| 按技术栈 | 6 列 × 4 状态 = 24 列 |
| 按标签 | 动态列 |

**UI 要求**：
- 顶部切换按钮组
- 切换动画：淡入淡出

---

### F3.5 卡片操作菜单

**描述**：卡片的操作菜单

**操作列表**：
| 操作 | 图标 | 说明 |
|------|------|------|
| 编辑 | Pencil | 编辑卡片信息 |
| 删除 | Trash | 删除卡片（确认弹窗） |
| 复制 | Copy | 复制卡片 |
| 移动 | ArrowRight | 移动到其他列 |
| 笔记 | FileText | 查看/编辑笔记 |
| PDF | FileText | 打开 PDF |

---

### F3.6 新增卡片

**描述**：添加新的读书资源

**表单字段**：
```typescript
interface NewCardForm {
  title: string;          // 必填
  author?: string;
  quadrant: 'language' | 'systems' | 'algorithms' | 'engineering';
  stack: string;          // 技术栈
  tags: string[];
  hasPdf?: boolean;
  url?: string;
  notes?: string;
}
```

**触发方式**：
- 空列占位符点击
- 顶部「+」按钮
- 快捷键 `N`

## Data Models

```typescript
// 看板列
interface KanbanColumn {
  id: string;
  title: string;
  cards: KanbanCard[];
}

// 看板数据
interface KanbanData {
  columns: KanbanColumn[];
  lastUpdated: string;
}
```

## Technical Notes

- 拖拽库：`@dnd-kit/core` + `@dnd-kit/sortable`
- 小程序端：使用 Taro 的 `movable-view` 组件
- 状态持久化：localStorage
