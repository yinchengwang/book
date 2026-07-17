## ADDED Requirements

### Requirement: 统计看板
系统 SHALL 提供 `GET /api/todos/stats` 端点返回统计信息。
统计信息 SHALL 包含：总数、open/closed/archived 数量、过期数量、今日到期数量。
统计信息 SHALL 包含按优先级分布。
统计信息 SHALL 包含按分组分布。
统计信息 SHALL 包含完成率（closed / total）。

#### Scenario: 获取统计看板
- **WHEN** 用户请求 `GET /api/todos/stats`
- **THEN** 系统返回 JSON 包含 total、open、closed、archived、overdue、due_today、by_priority、by_group、completion_rate

#### Scenario: 空数据统计
- **WHEN** 数据库中无待办时请求统计
- **THEN** 系统返回 total=0，各项分布为空数组，completion_rate=0

#### Scenario: 过期统计
- **WHEN** 数据库中存在截止日期小于当前时间且状态为 open 的待办
- **THEN** 系统在 overdue 字段中返回过期数量