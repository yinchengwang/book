# 守护进程 (Daemon)

## 运行说明

```bash
gcc -std=c11 -Wall -Wextra -O2 -o daemon_demo main.c
./daemon_demo
# 5 秒后自动退出演示
```

## 预期输出

```
[信号处理演示]
  注册信号处理器:
    SIGTERM (15) -> 优雅退出
    SIGHUP  (1) -> 配置重载
  等待信号（5秒后自动退出演示）...
  [SIGHUP 收到] 配置重载触发
  收到 SIGTERM，优雅退出

[守护进程创建模式]
  父进程: 子进程 PID=xxxx 已创建
  守护进程将在后台运行
```

## 注意事项

- 守护进程会创建 `/tmp/daemon_demo.pid` PID 文件
- 完整守护进程需在 Linux 环境运行
