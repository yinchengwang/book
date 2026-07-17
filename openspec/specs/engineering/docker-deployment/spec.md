# Docker 容器化部署规范

## Requirements

### Requirement: 单节点 Docker 镜像构建
MiniVecDB SHALL support building as a Docker image for single-node deployment.

#### Scenario: Docker 镜像构建成功
- **WHEN** 用户执行 `docker build -t minivecdb .` 命令
- **THEN** 系统 SHALL 生成一个可运行的 Docker 镜像
- **AND** 镜像大小 SHOULD NOT 超过 100MB（Release 构建）

#### Scenario: 多阶段构建优化
- **WHEN** Docker 构建执行时
- **THEN** 系统 SHALL 使用多阶段构建分离编译环境和运行时环境
- **AND** 最终镜像 SHALL 仅包含运行时必需的依赖

### Requirement: 容器运行时配置
MiniVecDB Docker 容器 SHALL 支持通过环境变量和配置文件进行配置。

#### Scenario: 环境变量配置端口
- **WHEN** 用户启动容器时设置 `MINIVECDB_PORT=8080`
- **THEN** 服务 SHALL 监听 8080 端口而非默认端口

#### Scenario: 数据卷持久化
- **WHEN** 用户将宿主目录挂载到容器 `/data` 路径
- **THEN** 所有数据库文件 SHALL 存储在 `/data` 目录
- **AND** 容器重启后数据 SHALL 保持不变

### Requirement: 健康检查
Docker 容器 SHALL 提供健康检查机制以支持容器编排系统。

#### Scenario: 健康检查端点
- **WHEN** 容器编排系统执行健康检查
- **THEN** 系统 SHALL 响应 HTTP GET `/health` 请求
- **AND** 返回状态码 200 表示健康，5xx 表示不健康

### Requirement: 优雅关闭
MiniVecDB Docker 容器 SHALL 支持优雅关闭以确保数据一致性。

#### Scenario: SIGTERM 信号处理
- **WHEN** 容器收到 SIGTERM 信号（优雅关闭请求）
- **THEN** 系统 SHALL 停止接受新请求
- **AND** SHALL 等待当前请求完成或超时（最多 30 秒）
- **AND** SHALL 刷盘所有脏页
- **AND** SHALL 正常退出

### Requirement: docker-compose 集成
MiniVecDB SHALL 提供 docker-compose.yml 用于快速部署演示环境。

#### Scenario: 单节点部署
- **WHEN** 用户执行 `docker-compose up` 命令
- **THEN** 系统 SHALL 启动包含 MiniVecDB 的容器
- **AND** MiniVecDB SHALL 可通过 localhost 访问