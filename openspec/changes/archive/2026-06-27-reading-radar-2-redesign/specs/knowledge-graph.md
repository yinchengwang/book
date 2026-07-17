# Knowledge Graph 知识图谱

## Overview

知识图谱以可视化方式展示知识节点及其关联关系，帮助用户理解知识结构。

## Features

### F6.1 图谱视图

**描述**：交互式知识图谱

**节点类型**：
| 类型 | 形状 | 颜色 |
|------|------|------|
| 领域 | 大圆 | 领域主色 |
| 主题 | 中圆 | 父领域色 70% |
| 知识点 | 小圆 | 状态色 |

**边类型**：
| 关系 | 线型 | 说明 |
|------|------|------|
| 包含 | 实线 | 领域 → 主题 |
| 前置 | 虚线 | A 前置 B |
| 关联 | 点线 | 相关但非层级 |

---

### F6.2 节点交互

**描述**：节点的交互操作

**操作**：
| 操作 | 触发 | 行为 |
|------|------|------|
| 查看详情 | 点击 | 弹出详情面板 |
| 标记状态 | 右键菜单 | 修改为掌握/学习中/薄弱 |
| 添加关联 | 拖拽 | 添加节点关联 |
| 展开/折叠 | 双击 | 展开/折叠子节点 |

**详情面板**：
```
┌────────────────────────────────────────┐
│ 装饰器                                 │
│                                        │
│ [Python] [高级] ⭐⭐⭐               │
│                                        │
│ 描述：                                 │
│ 装饰器是 Python 的高级特性之一...      │
│                                        │
│ 状态：🔵 进行中                       │
│ 掌握度：60%                            │
│                                        │
│ 前置知识：函数、闭包                   │
│ 关联应用：Flask 路由、Web 框架        │
│                                        │
│ [开始学习] [查看题目]                  │
└────────────────────────────────────────┘
```

---

### F6.3 图谱布局

**描述**：图谱的布局算法

**布局类型**：
| 类型 | 说明 |
|------|------|
| 层级布局 | 按层级从上到下 |
| 力导向 | 节点自动分布 |
| 圆形布局 | 按领域圆形分布 |

**缩放/平移**：
- 鼠标滚轮：缩放
- 拖拽空白处：平移
- 双击：适应屏幕

---

### F6.4 筛选控制

**描述**：筛选图谱显示

**筛选维度**：
| 维度 | 选项 |
|------|------|
| 技术栈 | 全部 / C / C++ / 数据结构 / 数据库 |
| 状态 | 全部 / 已掌握 / 进行中 / 薄弱 |
| 难度 | 全部 / 入门 / 进阶 / 高级 |

**筛选效果**：
- 隐藏不匹配的节点和边
- 孤立节点合并到父节点

---

### F6.5 图谱缩略图

**描述**：全局缩略图导航

**功能**：
- 右下角小地图
- 显示当前视口位置
- 点击小地图快速跳转

## Data Models

```typescript
// 图谱节点
interface GraphNode {
  id: string;
  label: string;
  type: 'domain' | 'topic' | 'item';
  domain: string;
  level: number;
  status: 'mastered' | 'learning' | 'weak';
  position?: { x: number; y: number };
}

// 图谱边
interface GraphEdge {
  id: string;
  source: string;
  target: string;
  type: 'contains' | 'prerequisite' | 'related';
}

// 图谱数据
interface KnowledgeGraph {
  nodes: GraphNode[];
  edges: GraphEdge[];
}
```

## Technical Notes

- 图谱库：`@xyflow/react` (React Flow)
- 布局算法：dagre（层级）、d3-force（力导向）
- **小程序**：此功能仅在 H5 端启用
- 小程序端降级：列表视图

## Mobile Compatibility

> ⚠️ 注意：`@xyflow/react` 主要面向 Web，小程序端使用条件编译：
> ```typescript
> // #ifndef MP-WEIXIN
> import KnowledgeGraph from '@/features/knowledge-graph';
> // #else
> import KnowledgeList from '@/features/knowledge-list';
> // #endif
> ```
