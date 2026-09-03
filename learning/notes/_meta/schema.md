# 笔记 Frontmatter Schema

本目录的所有笔记**必须**包含以下 frontmatter 字段：

```yaml
---
stack: linux              # 必填：linux | db | c | ds | cpp | python
difficulty: medium        # 必填：easy | medium | hard
status: in-progress       # 必填：todo | in-progress | done
links: []                 # 必填：相关链接数组
ref: []                   # 可选：与 knowledge_hub/data/kanban-data.js 的 ID 对齐
created: 2026-07-10       # 必填：创建日期
updated: 2026-07-10       # 必填：最后修改日期
tags: []                  # 必填：自由标签
---
```

## 字段说明

| 字段 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `stack` | enum | ✅ | 所属学习轨道：`linux` / `db` / `c` / `ds` / `cpp` / `python` |
| `difficulty` | enum | ✅ | 难度等级：`easy`（2 天内掌握）/ `medium`（3-5 天）/ `hard`（6 天+） |
| `status` | enum | ✅ | 完成状态：`todo`（未开始）/ `in-progress`（进行中）/ `done`（完成） |
| `links` | array | ✅ | 相关链接（论文、官网、文档） |
| `ref` | array | ⬜ | 与 knowledge_hub 的 kanban ID 对齐（可选） |
| `created` | date | ✅ | 首次创建日期（YYYY-MM-DD） |
| `updated` | date | ✅ | 最后修改日期（YYYY-MM-DD） |
| `tags` | array | ✅ | 自由标签（Dataview 查询用） |

## 笔记类型

| 类型 | 模板 | frontmatter 差异 |
|---|---|---|
| `theory` | `_templates/theory.md` | `status` 默认 `in-progress` |
| `practice` | `_templates/practice.md` | `tags` 必填 `[practice, experiment]` |
| `question` | `_templates/question.md` | `status` 默认 `in-progress`，加 `unresolved: true` |
| `summary` | `_templates/summary.md` | `status` 默认 `done` |

## Dataview 查询示例

```dataview
TABLE stack, difficulty, status
FROM ""
WHERE stack = "linux"
SORT updated DESC
```

## 渐进式迁移策略

- **Phase A（已完成）**：建骨架（`_index/`、`_templates/`、`_meta/`、`_dashboard/` + `.obsidian/`）
- **Phase B**：手工迁移 20 个代表性笔记（每 stack 至少 2 个）+ 补 frontmatter
- **Phase C**：脚本批量处理剩余 112 个笔记
- **Phase D**：双向链接 + 仪表盘

详见 `_dashboard/progress.md`。