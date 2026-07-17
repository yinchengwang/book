# UDP 高性能套接字

本示例演示 Linux UDP 套接字的高性能特性，包括 SO_REUSEPORT、UDP GRO 和批量处理。

## 核心特性

### SO_REUSEPORT

SO_REUSEPORT 允许多个套接字绑定到相同的 IP:端口，实现多进程/多线程的负载均衡：

```c
int optval = 1;
setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
```

**优势：**
- 内核自动进行负载均衡
- 无需额外的代理或负载均衡器
- 适合高性能 DNS 服务器

**使用场景：**
- DNS 服务器（如 BIND、CoreDNS）
- QUIC/HTTP3 服务器
- 高性能 UDP 应用服务器

### UDP GRO (Generic Receive Offload)

UDP GRO 是 LRO（Large Receive Offload）在 UDP 协议上的实现，允许将多个 UDP 数据包合并成一个大的数据包，减少中断次数和协议栈开销：

```c
int udp_gro = 1;
setsockopt(sockfd, IPPROTO_UDP, UDP_GRO, &udp_gro, sizeof(udp_gro));
```

### 批量处理

通过非阻塞 I/O 和批量接收，减少系统调用次数：

```c
for (int i = 0; i < max_batch; i++) {
    n = recvfrom(sockfd, ...);
    if (n < 0) break;
    // 处理数据包
}
```

## 构建与运行

```bash
make          # 编译
make run      # 运行服务器
make clean    # 清理
```

## 使用方法

### 服务器模式
```bash
./udp_socket server 8888
```

### 客户端模式
```bash
./udp_socket client 127.0.0.1 8888 10
```

### 查看统计
```bash
./udp_socket stats
```

## 性能优化建议

1. **增大缓冲区**：使用 SO_RCVBUF/SO_SNDBUF 调整缓冲区大小
2. **启用 GRO**：减少协议栈开销
3. **批量处理**：减少系统调用次数
4. **SO_REUSEPORT**：多进程水平扩展

## 相关文件

- `main.c` - 完整实现源码
- `Makefile` - 编译配置
- `NOTES.md` - 工程关联与深度解析
