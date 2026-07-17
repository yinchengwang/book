## ADDED Requirements

### Requirement: 截止日期管理
系统 SHALL 支持为每个待办设置截止日期（时间戳，0=无截止日期）。
系统 SHALL 支持按截止日期范围筛选待办（due_before / due_after）。
系统 SHALL 支持按截止日期排序待办。
系统 SHALL 在待办卡片上高亮显示过期项（当前时间 > 截止日期）。

#### Scenario: 设置截止日期
- **WHEN** 用户创建/更新待办时指定 due_date 为 1700000000
- **THEN** 系统保存该截止日期并返回

#### Scenario: 按截止日期范围筛选
- **WHEN** 用户请求 `GET /api/todos?due_before=1700000000&due_after=1600000000`
- **THEN** 系统返回截止日期在 1600000000-1700000000 之间的待办

#### Scenario: 过期高亮标识
- **WHEN** 待办截止日期小于当前时间且状态为 open
- **THEN** 系统在待办卡片上显示过期标识（红色高亮）

#### Scenario: 无截止日期
- **WHEN** 用户创建待办时未指定截止日期
- **THEN** 系统默认设置 due_date 为 0