## ADDED Requirements

### Requirement: 待办优先级管理
系统 SHALL 支持为每个待办设置优先级（P0-紧急、P1-高、P2-中、P3-低、P4-无）。
系统 SHALL 支持按优先级筛选待办列表。
系统 SHALL 支持按优先级排序待办列表。
系统 SHALL 在待办卡片上显示优先级标识（颜色/图标）。

#### Scenario: 创建待办时设置优先级
- **WHEN** 用户创建待办时指定优先级为 3（高）
- **THEN** 系统保存该待办并返回优先级为 3

#### Scenario: 按优先级筛选
- **WHEN** 用户请求 `GET /api/todos?priority=3`
- **THEN** 系统返回所有优先级为 3 的待办

#### Scenario: 优先级默认值
- **WHEN** 用户创建待办时未指定优先级
- **THEN** 系统默认设置优先级为 1（低）

#### Scenario: 更新优先级
- **WHEN** 用户更新待办优先级从 2 改为 4
- **THEN** 系统保存更新后的优先级