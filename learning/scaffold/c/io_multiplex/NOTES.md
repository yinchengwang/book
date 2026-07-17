# io_multiplex 学习笔记

## 概念地图

`select / poll / epoll / kqueue / IOCP` 是 OS 提供的高并发 IO 复用机制：

- **select**：POSIX 全平台；fd_set 位图，1024 上限；O(n) 扫描
- **poll**：POSIX；无 fd 数量限制；仍是 O(n) 扫描
- **epoll**：Linux 专属；回调式 O(1)；支持 LT/ET 两种触发模式
- **kqueue**：BSD/macOS；与 epoll 等价
- **IOCP**：Windows 专属；完成端口模型，与 epoll 思路不同

epoll 优势在大量并发连接 + 少数活跃连接的"长尾场景"（如 web server）。本 demo 用 LT（level-triggered）模式，比 ET 简单——未读完会继续触发。

非阻塞 IO + epoll 写回（write）也是 LT 控制——只有 write 返回 EAGAIN 时才放入 epoll 的 OUT 事件集合。本 demo 简化只读不写回严格 EAGAIN 模拟。

## 踩坑记录

1. **MinGW 缺 `<sys/epoll.h>`**：本 demo 在 MinGW-w64 14.2 下直接 `#error`。已在源码中明确报错，并写 README 标注"Linux/WSL only"。
2. **LT vs ET 选型**：LT 默认模式容易写但性能低；ET 性能高但必须一次读完。生产级 HTTP server（nginx、envoy）用 ET；学习场景 LT 足够。
3. **EPOLLRDHUP 是 Linux 2.6.17+ 才有的**：仅 EPOLLIN + EPOLLHUP/EPOLLERR 已可用。本 demo 用 `(EPOLLERR | EPOLLHUP)` 检查对端关闭。
4. **accept 与 read 必须处理 EINTR**：被信号打断要循环重试，本 demo 用 `if (errno == EINTR) continue;`。
5. **EPOLL_CTL_DEL 与 close 顺序**：先 epoll_ctl(EPOLL_CTL_DEL)，再 close(fd)——否则内核 epoll 内部 fd 句柄泄漏。
6. **accept4 是 Linux 专属**：跨平台写法是 `accept()` + `fcntl(O_NONBLOCK)`。本 demo 用了 accept4 因为只在 Linux 跑。

## 工程对照（≥100 字硬约束）

epoll 在 rag server 与 rag-remote 链路有以下直接迁移价值：

1. **rag server HTTP 长连接调度**：你的 `rag/` 与 `engineering/src/db/api/rest_api.c` 未来要走长连接（HTTP/1.1 keep-alive 或 WebSocket）；epoll 同时管理 listen + 多个 client 是核心技术。nginx 的 main event loop 思路与本 demo 完全同构。
2. **rag-remote-index-backend OpenSpec**：执行席正在推进的 §3 REST handlers 接入 VectorAPI，本卡刷过后读者能读懂 `handlers.c` 中 pthread + epoll 是否替换的讨论。
3. **engineering/src/algo-prod/coordinator.c**：分布式协调器的 RPC server 要处理 multi-shard 心跳、metadata 同步、LLM 请求回写——单线程 epoll 足够上千并发。
4. **engineering/src/db/storage/wal_writer.c**：WAL writer 监听 client 请求追加 WAL entry，client fd 数随连接数线性增长但绝大多数空闲——epoll 是教科书匹配。

**学完本卡后能动手的事**：实现一个有 graceful shutdown（SIGINT 后 epoll 不再添加新 fd，让存量跑完再退出）的 HTTP echo server。这正是 daemon（§7）的伏笔。
