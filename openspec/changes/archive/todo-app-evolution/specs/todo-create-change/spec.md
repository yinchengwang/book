## ADDED Requirements

### Requirement: OPSX 变更创建
系统 SHALL 提供 `POST /api/todos/:id/create-change` 端点。
系统 SHALL 根据待办的标题和描述作为 OPSX 变更的标题和描述。
系统 SHALL 通过子进程调用 `opsx propose` CLI 创建变更。
系统 SHALL 返回创建的变更 ID 和 URL。
系统 SHALL 在 OPSX CLI 不可用时返回明确的错误信息。

#### Scenario: 成功创建变更
- **WHEN** 用户 POST `/api/todos/5/create-change`，系统上 opsx CLI 可用
- **THEN** 系统调用 `opsx propose` 创建变更，返回 change_id 和 url

#### Scenario: OPSX CLI 不可用
- **WHEN** 用户 POST `/api/todos/5/create-change`，系统上 opsx CLI 不可用
- **THEN** 系统返回 code=9002，msg="创建变更失败"

#### Scenario: 待办不存在
- **WHEN** 用户 POST `/api/todos/999/create-change`，待办不存在
- **THEN** 系统返回 code=2001，msg="not found"