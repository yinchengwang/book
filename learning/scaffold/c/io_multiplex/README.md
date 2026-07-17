# io_multiplex scaffold

epoll LT（level-triggered）模式 echo server，单线程管理一个 listen fd 与多个 client fd。

## 复现命令

仅 Linux/WSL：

```bash
# 终端 A
cd learning/scaffold/io_multiplex
gcc -Wall -Wextra -O2 -std=c11 -o epoll_demo main.c
./epoll_demo 9000
# 输出: [epoll] listening on port 9000

# 终端 B
echo "hello epoll" | nc 127.0.0.1 9000
# 服务端会原样回显输入
```

## 预期输出

服务端：
```
[epoll] listening on port 9000
[epoll] client fd=4 connected
[epoll] client fd=4 closed
```

多次连接：每次 connect 会触发一段。

## 在本机（MinGW-w64）的现状

- `#error` 强制要求 POSIX 平台（`<sys/epoll.h>`、`<sys/socket.h>`）
- 当前会话已直接验证 `#error` 触发；Linux 用户请在 WSL 或原生 Linux 下跑

## 关键点

- **LT vs ET**：本 demo 用 LT（默认），未读完会在下次 epoll_wait 重新触发。ET（edge-triggered）需要一次性读完直到 EAGAIN——性能更好但 bug 更多。
- **non-blocking IO 必不可少**：`EAGAIN/EWOULDBLOCK` 不是错误而是常态。
- **accept4 + SOCK_NONBLOCK**：比 `accept()` + `fcntl()` 少一次 syscall。
- **EPOLLERR / EPOLLHUP 必查**：peer 强关会触发，错误处理不可省。
- **EPOLL_CTL_DEL 别漏**：close 之前先 epoll_ctl del，否则内核报 `EBADF`。

详见 NOTES.md 工程对照段。
