# NOTES - 工程对照

## 工程源码对照

`engineering/src/db/api/rest_api.c` 是我们 DB 存储引擎的 REST API 实现，其中包含了 socket 的完整使用：

### 1. Socket 创建与绑定

```c
// rest_api.c:645-648
server->listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
if (server->listen_socket == INVALID_SOCKET) {
    LOG_ERROR("Failed to create socket");
    // 错误处理并退出
}
```

我们的 DB 服务器使用 `AF_INET + SOCK_STREAM + IPPROTO_TCP` 创建 TCP socket，与 socket/main.c 一致。

### 2. 地址绑定

```c
// rest_api.c:666-668
if (bind(server->listen_socket, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
    LOG_ERROR("Failed to bind to port %d", server->config.port);
    closesocket(server->listen_socket);
    // 清理并返回错误
}
```

### 3. 监听与接受连接

```c
// rest_api.c:675-677
if (listen(server->listen_socket, SOMAXCONN) == SOCKET_ERROR) {
    LOG_ERROR("Failed to listen");
    closesocket(server->listen_socket);
    // ...
}

// rest_api.c:452-454
SOCKET client = accept(server->listen_socket, (struct sockaddr *)&client_addr, &addr_len);
if (client == INVALID_SOCKET) {
    // 处理错误
}
```

### 4. 关闭连接

```c
// rest_api.c:466, 482, 503, 557
closesocket(client);  // 关闭客户端连接
// ...
closesocket(server->listen_socket);  // 关闭监听 socket
```

### 5. 跨平台兼容

rest_api.c 使用了 `SOCKET` 类型和 `INVALID_SOCKET`/`SOCKET_ERROR` 宏，这是 Windows 兼容写法。

Linux 版本使用 `int` 类型和 `-1` 作为错误值。

```c
// Windows 风格（我们的代码）
typedef int SOCKET;
#define INVALID_SOCKET ((SOCKET)-1)
#define SOCKET_ERROR (-1)

// Linux 风格（scaffold 示例）
int fd = socket(AF_INET, SOCK_STREAM, 0);
if (fd < 0) { /* 错误 */ }
```

## 工程实践要点

| 模式 | 说明 |
|------|------|
| 错误传播 | 所有 socket 操作失败都要记录日志并清理资源 |
| 资源释放 | 使用 goto 模式统一清理：close -> mutex -> free |
| 非阻塞 I/O | DB 服务器使用 `pthread` 而非 epoll，accept 线程独立 |
| 超时设置 | `connection_timeout` 配置控制连接超时 |

## 与 DB 存储引擎的关系

DB 存储引擎通过 REST API 暴露网络接口，socket 编程是实现客户端-服务器通信的基础。

理解 socket 流程后，可以进一步学习：
- **epoll** - 高性能事件驱动（下一张卡）
- **连接池** - 复用连接减少开销
- **TLS/SSL** - 加密通信
