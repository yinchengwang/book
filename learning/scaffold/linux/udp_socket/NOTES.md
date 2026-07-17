# UDP 高性能套接字 - 工程关联

## DNS 服务器 UDP 优化

在生产环境的 DNS 服务器（如 BIND、CoreDNS、kube-dns）中，UDP 优化是性能关键。DNS 协议天生依赖 UDP（特别是 512 字节以下的查询），其性能直接影响域名解析延迟和吞吐量。

**SO_REUSEPORT 在 DNS 中的应用：**
Linux 3.9+ 引入的 SO_REUSEPORT 允许多个 DNS 服务进程绑定同一端口，内核自动进行负载均衡。这解决了传统方案（如 ip_vs/Nginx）的单点瓶颈和额外跳转开销。在 Kubernetes 集群中，CoreDNS 每秒处理数万次 DNS 查询，SO_REUSEPORT 使得多个副本真正实现无状态水平扩展，无需额外的负载均衡层。

**批量接收优化：**
DNS 服务器通常需要处理突发的大量查询（如服务启动、故障恢复时的集中查询）。通过 recvmsg() 的 MSG_PEEK 配合批量处理框架，可以显著减少系统调用次数，提升单核处理能力。

**EDNS0 与缓冲区调整：**
现代 DNS 响应可能超过 512 字节（如 DNSSEC 签名），需要 EDNS0 扩展。服务器需要调整 SO_RCVBUF 以容纳大包，避免丢包。Linux 内核默认值（208KB）对于高性能 DNS 场景往往不够。

## QUIC 协议（基于 UDP）

QUIC 是 Google 提出的新一代传输协议，已作为 HTTP/3 的底层协议标准化。QUIC 选择在用户态实现而非内核，是因为它需要细粒度的拥塞控制、0-RTT 连接建立、流级别的流量控制——这些在 TCP 内核实现中难以高效达成。

**为什么 QUIC 选择 UDP：**
- **防火墙/中间件穿透性**：UDP 能穿越几乎所有 NAT 和防火墙，而新传输协议很难部署
- **部署灵活性**：企业网络常阻止非常规端口和未知协议，但 443 端口的 UDP 几乎总是开放的
- **快速迭代**：用户态协议栈可以独立于内核更新，支持 TLS 1.3、0-RTT 等最新特性
- **拥塞控制可定制**：不同业务场景（直播、游戏、金融）需要不同的拥塞控制策略，用户态实现更灵活

**QUIC 的 UDP 层优化：**
QUIC 虽然基于 UDP，但其可靠性完全在用户态实现。这要求应用层正确处理 UDP 的乱序、丢包、重传。生产环境中的 QUIC 实现（如 quiche、mvfst）通常：
- 禁用 UDP 校验和（对于可信网络），减少 CPU 开销
- 使用 UDP_GRO 合并入站数据包
- 使用 SO_REUSEPORT 支持多核并行处理
- 绑定到特定 CPU 核心，避免缓存失效

**工程实践：**
在 Redis Cluster、Kafka、TiDB 等分布式数据库中，如果引入 QUIC，需要考虑：
- 丢包率敏感场景（高并发写入）：UDP 可靠性需精心设计
- 跨地域复制：QUIC 的 RTT 优化显著优于 TCP
- 监控告警：UDP 层面的统计（/proc/net/udp）需与 QUIC 层统计结合

## 性能调优参数汇总

| 参数 | 位置 | 说明 |
|------|------|------|
| net.core.rmem_max | sysctl | UDP 接收缓冲区上限 |
| net.core.wmem_max | sysctl | UDP 发送缓冲区上限 |
| net.ipv4.udp_rmem_min | sysctl | UDP 最小接收缓冲区 |
| net.ipv4.udp_wmem_min | sysctl | UDP 最小发送缓冲区 |
| SO_RCVBUF | setsockopt | 单个套接字接收缓冲 |
| SO_SNDBUF | setsockopt | 单个套接字发送缓冲 |
| SO_REUSEPORT | setsockopt | 多进程负载均衡 |
| UDP_GRO | setsockopt | UDP 批量接收（需内核 4.18+） |

## 与存储引擎的关联

本项目的 PostgreSQL 风格存储引擎在实现复制和同步时，可能涉及网络传输优化。QUIC 协议的低延迟特性可用于：
- 物理复制优化：减少主从同步延迟
- 在线索引构建：减少复制阻塞时间
- 分布式事务：降低两阶段提交的网络开销

理解 UDP 高性能特性，为未来引入 QUIC 或其他 UDP 协议栈打下基础，是存储工程师网络素养的重要组成。
