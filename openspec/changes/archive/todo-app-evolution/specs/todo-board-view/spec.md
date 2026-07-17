## ADDED Requirements

### Requirement: 看板视图
前端 SHALL 提供看板路由 `/board`。
看板视图 SHALL 支持按分组分组展示待办。
看板视图 SHALL 支持按优先级分组展示待办。
看板视图 SHALL 在每个分组列中显示待办卡片。
看板视图 SHALL 在每个分组列顶部显示分组名称和待办数量。

#### Scenario: 按分组看板
- **WHEN** 用户访问 `/board?group_by=group`
- **THEN** 前端展示按分组分列的看板，每列包含该分组下的待办卡片

#### Scenario: 按优先级看板
- **WHEN** 用户访问 `/board?group_by=priority`
- **THEN** 前端展示按优先级分列的看板（紧急/高/中/低/无）

#### Scenario: 空分组列
- **WHEN** 某分组下无待办
- **THEN** 该分组列依然显示，顶部显示名称和 "0"