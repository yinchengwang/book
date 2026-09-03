# 备份与恢复规范

## Requirements

### Requirement: 文件级备份
MiniVecDB SHALL support file-level backup for data protection.

#### Scenario: 创建备份
- **WHEN** 用户执行备份命令或 API
- **THEN** 系统 SHALL 创建包含以下内容的备份：
  - 数据目录完整快照
  - WAL 日志文件
  - 配置文件
  - 备份元数据（时间、版本等）

#### Scenario: 备份存储
- **WHEN** 备份创建时
- **THEN** 备份文件 SHALL 存储在用户指定目录
- **AND** SHALL 支持压缩以节省空间
- **AND** SHALL 生成校验和用于完整性验证

### Requirement: 增量备份
系统 SHALL 支持增量备份以减少存储和备份时间。

#### Scenario: 增量备份
- **WHEN** 用户执行增量备份时
- **THEN** 系统 SHALL 仅备份自上次备份以来变更的数据
- **AND** SHALL 记录备份基点信息
- **AND** 恢复时 SHALL 支持从多个增量备份重建

### Requirement: 备份恢复
系统 SHALL 支持从备份恢复数据。

#### Scenario: 完整恢复
- **WHEN** 用户执行恢复操作并指定备份
- **THEN** 系统 SHALL：
  - 停止服务
  - 清空数据目录
  - 解压备份文件
  - 验证数据完整性
  - 重启服务

#### Scenario: 恢复验证
- **WHEN** 恢复完成后
- **THEN** 系统 SHALL 验证：
  - 数据文件完整性
  - 向量数量一致性
  - 集合结构正确性

### Requirement: 备份管理
系统 SHALL 提供备份列表和删除功能。

#### Scenario: 列出备份
- **WHEN** 用户请求备份列表时
- **THEN** 系统 SHALL 返回：
  - 备份时间
  - 备份大小
  - 备份类型（完整/增量）
  - 校验和

#### Scenario: 删除备份
- **WHEN** 用户删除指定备份时
- **THEN** 系统 SHALL 安全删除备份文件
- **AND** 更新备份元数据

### Requirement: 自动备份
系统 SHALL 支持配置定时自动备份。

#### Scenario: 自动备份配置
- **WHEN** 用户配置自动备份（每天凌晨 2 点）
- **THEN** 系统 SHALL：
  - 按配置时间自动执行备份
  - 保留最近 N 个备份（默认 7）
  - 清理过期备份