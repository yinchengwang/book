# 监控与日志规范

## Requirements

### Requirement: Prometheus Metrics 端点
MiniVecDB SHALL expose metrics in Prometheus format.

#### Scenario: Metrics 端点
- **WHEN** 用户访问 GET `/metrics` 端点
- **THEN** 系统 SHALL 返回 Prometheus text 格式的指标
- **AND** 响应 SHALL 设置 Content-Type 为 `text/plain; version=0.0.4`

#### Scenario: 核心指标暴露
- **WHEN** Prometheus 抓取指标时
- **THEN** 系统 SHALL 暴露以下指标：
  - `minivecdb_vectors_total` (gauge): 当前向量总数
  - `minivecdb_collections_total` (gauge): 当前集合数
  - `minivecdb_search_duration_seconds` (histogram): 搜索延迟分布
  - `minivecdb_insert_duration_seconds` (histogram): 插入延迟分布
  - `minivecdb_request_total` (counter): 请求总数（带 status 标签）
  - `minivecdb_buffer_hit_ratio` (gauge): Buffer Pool 命中率

### Requirement: 结构化日志
MiniVecDB SHALL output structured logs for easier parsing and analysis.

#### Scenario: 日志格式
- **WHEN** 系统输出日志时
- **THEN** 日志 SHALL 包含 JSON 格式字段：
  - `timestamp`: ISO 8601 格式时间戳
  - `level`: 日志级别 (DEBUG/INFO/WARN/ERROR)
  - `message`: 日志消息
  - `module`: 来源模块名
  - `request_id`: 请求追踪 ID（可选）

#### Scenario: 日志级别配置
- **WHEN** 用户配置 `log_level=DEBUG|INFO|WARN|ERROR`
- **THEN** 系统 SHALL 仅输出该级别及以上的日志

#### Scenario: 日志输出目标
- **WHEN** 用户配置日志输出
- **THEN** 系统 SHALL 支持输出到：
  - stdout（默认）
  - 文件（指定路径）
  - syslog（Linux）

### Requirement: 请求追踪
系统 SHALL 支持请求级别的追踪以便于调试。

#### Scenario: 请求 ID 生成
- **WHEN** HTTP 请求到达时
- **THEN** 系统 SHALL 生成唯一请求 ID
- **AND** SHALL 在响应头中返回 `X-Request-ID`
- **AND** SHALL 在日志中记录请求 ID

#### Scenario: 请求日志记录
- **WHEN** HTTP 请求处理完成时
- **THEN** 系统 SHALL 记录包含以下信息的日志：
  - 请求 ID
  - HTTP 方法和路径
  - 响应状态码
  - 处理耗时
  - 客户端 IP

### Requirement: 性能日志
系统 SHALL 提供关键性能指标的可选日志输出。

#### Scenario: 慢查询日志
- **WHEN** 请求处理时间超过阈值（默认 1 秒）
- **THEN** 系统 SHALL 记录 WARN 级别日志
- **AND** 日志 SHALL 包含查询详情和耗时

#### Scenario: 内存使用日志
- **WHEN** 系统配置了内存监控且内存使用超过阈值
- **THEN** 系统 SHALL 定期记录内存使用情况