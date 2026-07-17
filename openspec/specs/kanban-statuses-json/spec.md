# kanban-statuses-json Specification

## Purpose
TBD - created by archiving change learning-backlog-r5. Update Purpose after archive.
## Requirements
### Requirement: JSON 文件作为权威状态存储

看板 SHALL 维护一个 git 跟踪的状态文件 `apps/web/reading-radar/data/statuses.json`，其结构 SHALL 为：

```json
{
  "<card-id>": "<status>",
  ...
}
```

- key：卡 ID（与 `items-registry.js` 主源一致，裸 ID 如 `c-pthread`）
- value：枚举 `{ "todo" | "learning" | "done" | "review" }` 之一
- 初始基线：所有已知卡 ID 都出现一次，值默认为 `todo`，缺失的视为未初始化

#### Scenario: 初始 statuses.json 包含所有派生卡 ID

- **WHEN** R5 首次提交 statuses.json 时
- **THEN** 该文件 SHALL 包含所有派生卡（C_DATA + CPP_DATA + DS_DATA + DB_DATA 等），每条 value = `"todo"`
- **AND** 文件 SHALL 进入 git 跟踪，commit message 形如 `[r5] kanban-statuses: 初始基线`

#### Scenario: JSON 与 localStorage 一致性——以 JSON 为准

- **WHEN** 看板运行时同时读到 statuses.json 与 localStorage["learning-kanban-statuses"]
- **THEN** JSON SHALL 覆盖 localStorage 中任何不一致的值
- **AND** 不一致项 SHALL 被 console.warn 输出，便于审计

### Requirement: 看板运行时先 fetch JSON，失败降级 localStorage

`kanban-render.js` SHALL 在 `syncStatusFromStorage(items, storageKey)` 调用前，新增 `await syncStatusFromJson(items, jsonUrl)`：

- 成功 fetch → 把 json 状态覆盖到 items、然后写入 localStorage（保持两者一致）
- 失败（404/网络错误）→ 静默降级，回退到原 localStorage 行为
- 加载过程中 SHALL 显示一个轻量级占位（避免数字闪烁，但**不阻塞**渲染）

#### Scenario: 静态部署下 fetch 200

- **WHEN** 看板从本地静态站加载 statuses.json
- **THEN** 全部卡的状态 SHALL 与文件一致
- **AND** localStorage SHALL 被同步刷新（以 json 为最新值）

#### Scenario: 离线或 fetch 失败

- **WHEN** `fetch('/data/statuses.json')` 返回 404 或网络错误
- **THEN** 看板 SHALL 落入"localStorage 模式"
- **AND** 不向用户显示错误（降级必须是安静的）

### Requirement: 勾选 → 写 localStorage 的同时反馈"待提交"

系统 SHALL 在用户 UI 触发状态切换时保留现有 localStorage + DOM 行为，且 SHALL 同时输出开发者日志以提示手工 git commit statuses.json。

#### Scenario: 用户点击卡片切换状态

- **WHEN** 用户在 UI 触发状态切换（click / Ctrl-Click）
- **THEN** 当前 SHALL 行为保留：写 localStorage + 更新 DOM `data-status` 属性
- **AND** 新增 console.info 输出 `[r5] 待提交：<card-id> → <status>`，提醒开发者手工 git commit

#### Scenario: 离线模式下不报错

- **WHEN** fetch 失败 + 用户勾选卡
- **THEN** localStorage 正常写入
- **AND** info 提示 SHALL 改为 "离线模式：勾选仅本地有效，联网后请 pull statuses.json"

