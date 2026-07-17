---
stack: c
difficulty: easy
status: done
links: []
ref: []
created: 2026-07-10
updated: 2026-07-10
tags: [dashboard]
---

# 学习进度仪表盘

> Dataview 驱动的实时统计。每次打开 vault 自动渲染。

## 各轨道完成度

```dataview
TABLE length(rows) as "总数",
       length(filter(rows, (r) => r.status = "done")) as "已完成",
       length(filter(rows, (r) => r.status = "in-progress")) as "进行中",
       length(filter(rows, (r) => r.status = "todo")) as "未开始"
FROM ""
WHERE stack != null
GROUP BY stack
SORT length(rows) DESC
```

## 难度分布

```dataview
TABLE length(rows) as "笔记数"
FROM ""
WHERE difficulty != null
GROUP BY difficulty
```

## 最近更新（30 天）

```dataview
LIST
FROM ""
WHERE date(updated) >= date(today) - dur(30 days)
SORT updated DESC
LIMIT 20
```

## 待解决疑问

```dataview
LIST
FROM ""
WHERE unresolved = true
SORT created DESC
```

## 高难度笔记（hard 状态分布）

```dataview
TABLE stack, file.link as "标题", status
FROM ""
WHERE difficulty = "hard"
SORT updated DESC
```

---

## 进度指标（β 目标）

- **当前笔记总数**：见"各轨道完成度"表的 SUM
- **目标**：≥ 500 笔记（覆盖 6 轨道 × ~80 笔记/轨道）
- **进度**：当前 / 500 × 100%

返回 [[home|总索引]]