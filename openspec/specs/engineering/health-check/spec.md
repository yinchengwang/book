# 健康检查与优雅关闭规范

## Requirements

### Requirement: 健康检查端点
MiniVecDB SHALL provide a health check endpoint for monitoring service status.

#### Scenario: 健康状态检查
- **WHEN** 外部系统发送 GET `/health` 请求
- **THEN** 系统 SHALL 返回 HTTP 200 状态码
- **AND** 响应体 SHALL 包含 `{"status": "ok", "uptime": <seconds>}`

#### Scenario: 不健康状态检查
- **WHEN** 服务处于启动中或故障状态
- **THEN** 系统 SHALL 返回 HTTP 503 状态码
- **AND** 响应体 SHALL 包含 `{"status": "unhealthy", "reason": "<reason>"}`

### Requirement: 就绪检查
系统 SHALL 提供就绪检查以支持 Kubernetes 等编排系统。

#### Scenario: 就绪探测
- **WHEN** 编排系统发送 GET `/ready` 请求
- **THEN** 系统 SHALL 检查所有关键组件状态
- **AND** 仅在完全就绪时返回 200
- **AND** 未就绪时返回 503

### Requirement: 活跃检查
系统 SHALL 提供活跃检查以检测服务是否挂死。

#### Scenario: 活跃探测
- **WHEN** 编排系统发送 GET `/live` 请求
- **THEN** 系统 SHALL 检查进程是否存活
- **AND** 返回 200 表示存活，5xx 表示已挂死

### Requirement: SIGTERM 信号处理
MiniVecDB SHALL handle SIGTERM signal for graceful shutdown.

#### Scenario: 优雅关闭流程
- **WHEN** 进程收到 SIGTERM 信号
- **THEN** 系统 SHALL 设置关闭标志
- **AND** SHALL 停止接受新的 HTTP 连接
- **AND** SHALL 等待现有请求完成（最多 30 秒超时）
- **AND** SHALL 刷盘所有脏页
- **AND** SHALL 关闭所有打开的文件
- **AND** SHALL 正常退出（exit code 0）

#### Scenario: 关闭超时处理
- **WHEN** 优雅关闭超过 30 秒
- **THEN** 系统 SHALL 强制终止
- **AND** SHALL 记录警告日志

### Requirement: SIGINT 信号处理
系统 SHALL 支持 SIGINT（Ctrl+C）触发优雅关闭。

#### Scenario: Ctrl+C 关闭
- **WHEN** 用户在终端按 Ctrl+C
- **THEN** 系统 SHALL 触发与 SIGTERM 相同的优雅关闭流程

### Requirement: 关闭钩子
系统 SHALL 提供关闭钩子机制供其他模块注册清理回调。

#### Scenario: 关闭钩子执行
- **WHEN** 系统开始关闭流程
- **THEN** 系统 SHALL 按注册顺序调用所有关闭钩子
- **AND** 每个钩子 SHALL 有最多 5 秒执行时间
- **AND** 超时的钩子 SHALL 被强制终止