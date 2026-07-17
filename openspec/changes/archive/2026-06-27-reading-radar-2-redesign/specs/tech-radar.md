# Tech Radar 技术雷达图

## Overview

雷达图是技术雷达的核心可视化组件，以雷达形式展示各技术栈的掌握度。

## Features

### F2.1 雷达图可视化

**描述**：以雷达图展示技术领域掌握度

**数据源**：
```typescript
interface RadarData {
  domain: string;         // 领域名称
  score: number;          // 掌握度 0-100
  fullMark: number;       // 满分（固定 100）
  color: string;          // 领域颜色
}
```

**图表配置**（Recharts RadarChart）：
```javascript
<RadarChart cx={cx} cy={cy} outerRadius={r}>
  <PolarGrid stroke="#E5E7EB" />
  <PolarAngleAxis dataKey="domain" tick={{ fontSize: 11 }} />
  <PolarRadiusAxis angle={90} domain={[0, 100]} />
  <Radar dataKey="score" fill={color} fillOpacity={0.15} />
</RadarChart>
```

**UI 要求**：
- 中心点：应用主色
- 轴线：白色/20% 透明度
- 数据区域：半透明填充 + 描边
- 悬停：显示详细数据 tooltip

---

### F2.2 技术栈切换

**描述**：切换不同技术栈视图

**模式**：
| 模式 | 描述 | 数据 |
|------|------|------|
| 全部 | 显示所有技术栈 | 合并数据 |
| C | C 语言栈 | C_TECH_DATA |
| C++ | C++ 语言栈 | CPP_TECH_DATA |
| 数据结构 | 数据结构与算法 | DS_TECH_DATA |
| 数据库 | 数据库技术 | DB_TECH_DATA |

**UI 要求**：
- 顶部 Tab 切换
- 胶囊按钮样式
- 激活态：bg-white/20 + 白色文字

---

### F2.3 知识详情弹窗

**描述**：点击雷达节点显示知识详情

**触发**：点击雷达图上的数据点

**弹窗内容**：
```
┌────────────────────────────────────────┐
│ [领域图标] 编程基础                    │
│                                        │
│ 描述：Python 核心编程能力...           │
│                                        │
│ 掌握度：85%                            │
│ ████████████░░░░░                     │
│                                        │
│ 子知识点：                             │
│ • 数据类型    ⭐ 已掌握                │
│ • 函数       ⭐ 已掌握                │
│ • 类        ⭐ 已掌握                │
│ • 装饰器    🔵 进行中                 │
│ • 生成器    🔵 进行中                 │
│ • 异步编程  ⚠️ 薄弱                   │
│                                        │
│ [查看详情] [开始学习]                  │
└────────────────────────────────────────┘
```

---

### F2.4 雷达图 Legend

**描述**：图例说明

**内容**：
| 状态 | 颜色 | 说明 |
|------|------|------|
| 已掌握 | #27ae60 | 答对 5+ 次 |
| 进行中 | #409eff | 开始学习 |
| 薄弱点 | #f59e0b | 正确率 < 50% |
| 未开始 | #e4e4e7 | 尚未学习 |

---

## Data Models

```typescript
// 知识领域
interface KnowledgeDomain {
  id: string;
  name: string;
  icon: string;
  color: string;
  weight: number;
  description: string;
  children: KnowledgeItem[];
}

// 知识项
interface KnowledgeItem {
  id: string;
  name: string;
  level: 1 | 2 | 3;
  status: 'mastered' | 'learning' | 'weak';
  children?: KnowledgeItem[];
}
```

## Technical Notes

- 使用 Recharts `RadarChart` 组件
- 响应式：`ResponsiveContainer`
- 小程序端：降级为列表展示
