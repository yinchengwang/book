---
stack: c
difficulty: easy
status: done
links: []
ref: []
created: 2026-07-10
updated: 2026-07-10
tags: [index, home]
---

# 学习轨道总索引

> 双轨制下的 β 目标（学习日志）入口。132 个学习笔记按 6 大学习轨道组织。

## 学习轨道索引

| 轨道 | 简介 | 索引 |
|---|---|---|
| **linux** | Linux 内核、文件系统、网络、调度 | [[linux]] |
| **db** | 数据库（PG/MySQL/SQLite/向量库） | [[db]] |
| **c** | C 语言核心（指针、内存、编译） | [[c]] |
| **ds** | 数据结构与算法理论 | [[ds]] |
| **cpp** | C++ 现代特性（11/14/17/20） | [[cpp]] |
| **python** | Python 工具链与 AI/ML | [[python]] |

## 仪表盘

- 📊 [_dashboard/progress.md](_dashboard/progress.md) —— 学习进度仪表盘

## 元数据

- 📐 [_meta/schema.md](_meta/schema.md) —— Frontmatter Schema 规范
- 📝 [_templates/](_templates/) —— 4 类笔记模板（theory/practice/question/summary）

## 个人分类（独立轨道）

- 📓 [_diary/](../_diary/) —— 私人日记
- 📄 [_papers/](../_papers/) —— 论文笔记
- 📚 [_excerpts/](../_excerpts/) —— 摘录
- 🖥️ [_hardware/](../_hardware/) —— 硬件笔记

## 数据统计

```dataview
TABLE length(rows) as "笔记数", 
       length(filter(rows, (r) => r.status = "done")) as "已完成"
FROM ""
GROUP BY stack
SORT length(rows) DESC
```