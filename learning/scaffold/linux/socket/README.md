# Socket 编程

## 概述

本卡演示 Linux socket 编程基础，包括 TCP 服务器/客户端和 UDP 数据报通信。

## TCP 通信流程

### Server 端

```
socket() -> bind() -> listen() -> accept() -> recv/send -> close()
```

| 函数 | 说明 |
|------|------|
| `socket()` | 创建 socket 文件描述符 |
| `bind()` | 绑定地址和端口 |
| `listen()` | 开始监听连接 |
| `accept()` | 接受客户端连接 |
| `recv()/send()` | 收发数据 |

### Client 端

```
socket() -> connect() -> send/recv -> close()
```

| 函数 | 说明 |
|------|------|
| `socket()` | 创建 socket |
| `connect()` | 连接服务器 |
| `send()/recv()` | 收发数据 |

## UDP 通信流程

```
socket() -> bind() -> sendto/recvfrom -> close()
```

UDP 是无连接的，使用 `sendto()`/`recvfrom()` 指定目标地址。

## 常用 Socket 选项

| 选项 | 说明 |
|------|------|
| `SO_REUSEADDR` | 允许绑定已使用的地址（快速重启） |
| `SO_KEEPALIVE` | TCP 保活探测 |
| `TCP_NODELAY` | 禁用 Nagle 算法（低延迟） |
| `SO_RCVTIMEO` | 接收超时 |

## 编译运行

```bash
make              # 编译
make run          # 运行演示
make run-server   # 仅运行 TCP Server
make run-client   # 仅运行 TCP Client（需要 server 先运行）
```

## 代码结构

- `main.c` - 主程序
  - `demo_tcp_server()` - TCP 服务器演示
  - `demo_tcp_client()` - TCP 客户端演示
  - `demo_udp()` - UDP 演示

## 学习要点

1. **TCP 是面向连接的字节流协议**，保证可靠传输
2. **UDP 是无连接的数据报协议**，不保证可靠性和顺序
3. **Socket 是文件描述符**，可以使用 `read/write`/`send/recv`
4. **错误处理很重要**：`perror()` 查看系统错误信息
