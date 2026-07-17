# Dashboard 仪表盘

## Overview

仪表盘是应用首页，提供全局学习概览、统计数据和快捷入口。

## Features

### F1.1 全局统计卡片

**描述**：展示核心学习指标的统计卡片

**数据指标**：
| 指标 | 计算方式 | 显示格式 |
|------|----------|----------|
| 总知识点 | `KNOWLEDGE_DATA` 长度 | 数字 + "个知识点" |
| 已掌握 | `status === 'mastered'` 计数 | 数字 + "已掌握" |
| 学习中 | `status === 'learning'` 计数 | 数字 + "进行中" |
| 薄弱点 | `status === 'weak'` 计数 | 数字 + "待加强" |

**UI 要求**：
- 卡片数量：4 个（桌面 4 列，移动端 2 列）
- 卡片样式：`bg-white/10 backdrop-blur-xl rounded-2xl border border-white/20`
- 动画：数字递增动画（Framer Motion）

---

### F1.2 技术栈进度条

**描述**：展示各技术栈的学习进度

**技术栈列表**：
| 栈名 | 颜色 | 图标 |
|------|------|------|
| C | #2e8b57 | Code |
| C++ | #2f6db3 | Cpp |
| 数据结构 | #dd7a1f | Database |
| 数据库 | #7d4cc2 | Server |
| Python | #d65245 | Terminal |
| Linux | #555 | Terminal |

**计算方式**：
```typescript
const progress = (masteredCount / totalCount) * 100;
```

**UI 要求**：
- 进度条颜色：渐变色（stack color）
- 悬停显示：详细数据弹窗
- 点击跳转：对应技术栈的雷达图

---

### F1.3 成长趋势图

**描述**：展示学习进度的历史变化趋势

**数据来源**：
- 每日快照数据（由 `SkillAssessment` 页面保存）
- 存储位置：`localStorage.skillHistory`

**图表要求**：
- 类型：柱状图（Recharts BarChart）
- X 轴：日期（最近 14 天）
- Y 轴：综合掌握度百分比
- 颜色：`#6366F1` (primary)
- 空状态：无数据时显示锁定图标 + 引导文案

---

### F1.4 热力图日历

**描述**：展示每日学习活动的热力图

**数据来源**：
- `localStorage.dailyLog`
- 记录每日学习题目数、正确数

**颜色等级**：
| 学习时长 | 等级 | 颜色 |
|----------|------|------|
| 0 题 | lv0 | #1a1a1a |
| 1-5 题 | lv1 | #c6efce |
| 6-10 题 | lv2 | #70c97a |
| 11-20 题 | lv3 | #27ae60 |
| 20+ 题 | lv4 | #1a7a44 |

---

### F1.5 快捷入口

**描述**：常用功能的快速入口

**入口列表**：
| 入口 | 图标 | 颜色 | 描述 |
|------|------|------|------|
| 开始测验 | Brain | primary | 进入测验系统 |
| 继续学习 | BookOpen | accent.cyan | 继续上次学习 |
| 查看差距 | AlertTriangle | accent.amber | 进入差距分析 |
| 复习计划 | RefreshCw | accent.green | 进入复习系统 |

**UI 要求**：
- 网格布局：2x2 或 4x1
- 悬停效果：scale(1.02) + 阴影增强

---

## Data Models

```typescript
// 快照数据
interface Snapshot {
  date: string;           // 'YYYY-MM-DD'
  scores: Record<string, number>; // domain -> score
}

// 每日日志
interface DailyLog {
  [date: string]: {
    studied: string[];     // 题目 ID 列表
    correct: string[];
    wrong: string[];
  };
}
```

## Technical Notes

- 使用 Recharts 绘制图表
- 数字动画使用 Framer Motion 的 `animate` 属性
- 小程序端：图表降级为静态图片或列表
