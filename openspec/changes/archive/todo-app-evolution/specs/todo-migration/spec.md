## ADDED Requirements

### Requirement: 旧数据自动迁移
系统启动时 SHALL 自动检测旧数据文件 `issue_tool.json` 是否存在。
迁移时 SHALL 给每条旧记录添加默认值：priority=1、due_date=0、group_id=0、sort_order=0。
迁移后 SHALL 写入新文件 `todo_app.json`。
迁移后 SHALL 重命名旧文件为 `issue_tool.json.bak`。
迁移失败时 SHALL 回滚变更并保留原始数据。
迁移过程 SHALL 输出日志到控制台。

#### Scenario: 旧文件存在，迁移成功
- **WHEN** 系统启动时检测到 `issue_tool.json` 存在
- **THEN** 系统自动迁移数据到 `todo_app.json`，备份旧文件为 `issue_tool.json.bak`，控制台输出迁移日志

#### Scenario: 旧文件不存在
- **WHEN** 系统启动时未检测到 `issue_tool.json`
- **THEN** 系统正常启动，使用空数据

#### Scenario: 迁移失败回滚
- **WHEN** 迁移过程中写入 `todo_app.json` 失败
- **THEN** 系统保留原始 `issue_tool.json` 不变，输出错误日志并退出