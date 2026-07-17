# SO_REUSEPORT 负载均衡

## 概述

SO_REUSEPORT 是 Linux 3.9+ 引入的 socket 选项，允许**多个 socket 绑定到相同的 IP:Port 组合**。内核会在这多个 socket 之间自动进行负载均衡，将新连接分配给不同的 socket。

## 核心优势

1. **多核扩展**：每个 socket 可由不同 CPU 核心处理，减少锁竞争
2. **负载均衡**：内核自动将入站连接分发到各 socket
3. **平滑重启**：新旧 socket 可同时存在，实现零 downtime 部署
4. **简单实现**：无需额外轮询或锁机制

## 核心 API

```c
#include <sys/socket.h>

// 启用端口复用
int opt = 1;
setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
```

## 负载均衡策略

内核使用 **source IP hash** 策略分配连接：
- 同一客户端 IP 的连接倾向于分配到同一 socket
- 保证会话亲和性，避免同一连接的包乱序

## 编译与运行

```bash
# 编译
gcc -std=c11 -Wall -O2 -o so_reuseport main.c -lpthread

# 运行
./so_reuseport

# 测试（多终端执行）
nc 127.0.0.1 8888
```

## 测试观察

运行后观察：
1. 多个 listener 线程各自接受的连接数是否均衡分布
2. 同一客户端多次连接是否可能被分配到不同 socket
3. 连接分布是否随连接数增加趋于均衡
