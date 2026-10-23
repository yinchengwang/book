---
name: learning-kanban-storage
description: reading-radar 看板的状态存储层架构（localStorage + localStorage key）+ R5 v1/v2 截断点
metadata: 
  node_type: memory
  type: project
  originSessionId: a33304ce-6587-423b-a488-1e6ebd17c5c6
---

`apps/web/reading-radar/data/app/items-registry.js`（468 行，约 250+ 条知识点，6 个技术栈 C/C++/DS/DB/...）是**唯一数据源**。`kanban-data.js`、`quiz-tech.js`、`radar-tech.js`、`quiz-static.js` **全部是派生文件**，不能直接改——改主源即可。

**看板状态机关键事实（与 R5 提案直接相关）**：

1. **状态字段不存在于数据层**：`items-registry.js` 没有 status/mastered/pending 字段。
2. **状态走 localStorage**：`kanban-render.js` 的 `syncStatusesFromStorage(items)` 从 `localStorage["learning-kanban-statuses"]`（或自定义 `storageKey + "-statuses"`）读 `{<cardId>: status}` 映射。
3. **状态枚举固定**：`{todo, learning, done, review}`（中文"待学习/学习中/已掌握/需复习"），不可扩展。
4. **`kanban-render.js` 377 行**（CLAUDE.md 纪律：避免完整读，先 grep 后 offset）。
5. **关键函数**：
   - `syncStatusesFromStorage(items)`（line 85-100）：从 localStorage 同步状态
   - `updateOneStatus(...)`（line 342-355）：写回 localStorage
   - 卡片点击切换：`nextStatus(current)` → 写 localStorage + 更新 DOM `data-status` 属性

**为什么 R5 v1 被推翻**：2026-07-11 /grill-me 达成的 F1 严格勾卡（commit 引用 + 产物 + 复现 + 工程对照四要素）与 localStorage 存储脱钩——任何 git commit 都**不会**自动反映在看板上。需要在 R5 v2 里二选一：

- **方案甲**：新增 `learning/data/statuses.json` 作为权威存储，git 跟踪；`kanban-render.js` 改读该文件（同时保留 localStorage 作为本地编辑缓冲）；新写 `syncStatusesToFile()` 与 `commitStatuses()` 工作流。
- **方案乙**：改 `items-registry.js` 主源 schema，加 `status` 字段（默认值 `todo`）；看板直接读静态数据；勾选变成"改文件 + git commit"——这就是 R5 想要的"git 必引"，但破坏了"前端编辑可立即生效"的 UX。

**Why:** 任何在 R5 之后涉及"勾卡 / 审计 / 跨设备同步"的需求都必须基于本记忆的事实，不能凭名字假设字段存在。

**How to apply:** 引用本卡的卡 ID 时用**裸 ID**（如 `c-pthread`、`c-daemon`、`c-gdb`、`c-makefile`），**不要**用 `c-sys-` / `c-pra-` 前缀；卡 ID 在 `items-registry.js` 与 `kanban-data.js`（第 25-32、47-48 行）映射，但派生文件别改。当前 R5 OPSX（`openspec/changes/learning-backlog-r5/`）的 4 份文件都带"v1 已被 Phase 3 探索推翻"头——v2 重写待启，不要直接基于 v1 实施。
