# 守护进程 (Daemon)

## 什么是守护进程

守护进程（Daemon）是运行在后台、不受终端控制的长生命周期进程。

## 核心特征

- 父进程通常是 init (PID=1) 或 systemd
- 脱离控制终端，在后台运行
- 通常在系统启动时启动，系统关闭时关闭

## 创建步骤

```c
1. fork()        // 创建子进程，父进程退出
2. setsid()      // 创建新会话，脱离控制终端
3. chdir("/")    // 改变工作目录
4. umask(0)      // 重置文件权限掩码
5. close()       // 关闭标准输入/输出/错误
6. dup2()        // 重定向到 /dev/null
```

## 信号处理

| 信号 | 用途 |
|------|------|
| SIGTERM | 优雅退出（默认） |
| SIGHUP | 配置重载 |
| SIGUSR1 | 用户自定义 |

## 工程对照

### 数据库服务
- mysqld、postgres、redis-server 都是守护进程
- 使用 SIGTERM 优雅关闭，刷新数据后退出

### Web 服务器
- nginx worker 进程由 master 管理
- 支持 HUP 信号热重载配置

### 定时任务
- crond 按计划执行任务
- atd 处理一次性定时任务

### 系统服务
- sshd、systemd-logind
- 监控服务（Prometheus node_exporter）

## 参考

- `learning/scaffold/c/signal/` — 信号处理
- `learning/scaffold/c/process/` — 进程管理
