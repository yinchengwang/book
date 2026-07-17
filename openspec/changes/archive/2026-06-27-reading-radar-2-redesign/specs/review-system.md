# Review System 复习系统

## Overview

复习系统基于间隔重复算法（SM-2），帮助用户科学地巩固记忆。

## Features

### F5.1 复习卡片

**描述**：基于 SM-2 算法的复习卡片

**SM-2 算法**：
```
初始值：
- easiness: 2.5
- interval: 1 (天)
- repetitions: 0

答题后更新：
if (quality >= 3) {  // 正确
  if (repetitions === 0) interval = 1;
  else if (repetitions === 1) interval = 6;
  else interval = Math.round(interval * easiness);
  repetitions++;
} else {  // 错误
  repetitions = 0;
  interval = 1;
}

easiness = max(1.3, easiness + (0.1 - (5 - quality) * (0.08 + (5 - quality) * 0.02)));
nextReview = today + interval;
```

**卡片 UI**：
```
┌─────────────────────────────────────────────────────┐
│                    复习中 (3/10)                    │
├─────────────────────────────────────────────────────┤
│                                                     │
│  [Python] 装饰器                                   │
│                                                     │
│  请简述 Python 装饰器的作用和使用场景               │
│                                                     │
│  ──────────────────────────────────────────────   │
│                                                     │
│  你的答案：                                        │
│  装饰器是在不改变原函数的情况下...                 │
│                                                     │
│  ──────────────────────────────────────────────   │
│                                                     │
│  参考答案：                                        │
│  装饰器是一个函数，接收一个函数作为参数...         │
│                                                     │
├─────────────────────────────────────────────────────┤
│  [没印象 😵]  [有点难 😐]  [还行 🙂]  [太简单 😎] │
└─────────────────────────────────────────────────────┘
```

---

### F5.2 复习队列

**描述**：待复习内容队列

**筛选**：
| 筛选 | 说明 |
|------|------|
| 今日待复习 | `nextReview <= today` |
| 逾期复习 | 过期未复习的 |
| 全部 | 所有复习记录 |

**队列管理**：
- 优先显示逾期项目
- 按 `nextReview` 排序
- 完成后自动出队

---

### F5.3 复习统计

**描述**：复习效果统计

**指标**：
| 指标 | 说明 |
|------|------|
| 今日复习 | 今日完成复习数 |
| 本周复习 | 本周累计复习数 |
| 连续天数 | 连续复习天数 |
| 记忆曲线 | 遗忘曲线图 |

**图表**：
- 使用 Recharts 折线图
- X 轴：日期
- Y 轴：复习完成数

---

### F5.4 复习设置

**描述**：复习参数配置

**可配置项**：
| 参数 | 默认值 | 说明 |
|------|--------|------|
| 每日复习上限 | 20 | 每日最大复习量 |
| 新词上限 | 10 | 每日新增学习量 |
| 提醒时间 | 20:00 | 每日提醒时间 |

---

### F5.5 复习历史

**描述**：复习记录历史

**记录内容**：
```typescript
interface ReviewHistory {
  date: string;
  reviewed: string[];      // 复习的题目 ID
  correct: string[];       // 正确的
  wrong: string[];         // 错误的
  newLearned: string[];    // 新学习的
}
```

## Data Models

```typescript
// 复习记录
interface ReviewRecord {
  topicId: string;
  easiness: number;        // 2.5 default
  interval: number;        // days
  repetitions: number;
  nextReview: string;      // YYYY-MM-DD
  lastReview?: string;
}

// 复习历史
interface ReviewHistory {
  date: string;
  entries: {
    topicId: string;
    quality: 0 | 1 | 2 | 3 | 4 | 5;
    interval: number;
  }[];
}
```

## Technical Notes

- SM-2 算法：参考 Anki 算法
- 本地存储：localStorage
- 提醒功能：Web Notifications API
- 小程序：使用微信订阅消息
