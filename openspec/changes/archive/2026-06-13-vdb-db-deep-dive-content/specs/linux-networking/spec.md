## ADDED Requirements

### Requirement: Linux 网络栈深度文章

Linux 网络栈的每篇深度文章 SHALL 覆盖以下知识点（预计 ~16 篇）：

| 知识点 ID | 标题 | 难度 |
|-----------|------|------|
| `linux-tcp-state` | TCP 状态机 | basic |
| `linux-socket-buf` | Socket 缓冲区 | basic |
| `linux-tcpdump` | tcpdump 抓包分析 | basic |
| `linux-conn-pool` | 连接池管理 | basic |
| `linux-epoll` | epoll 事件驱动 | intermediate |
| `linux-zero-copy` | 零拷贝技术 | intermediate |
| `linux-cpu-affinity` | CPU 绑核与中断亲和性 | intermediate |
| `linux-unix-socket` | Unix Domain Socket | intermediate |
| `linux-so-reuseport` | SO_REUSEPORT 与多线程 | intermediate |
| `linux-iptables` | iptables/nftables 防火墙 | intermediate |
| `linux-namespace` | 网络命名空间 Netns | advanced |
| `linux-veth` | veth 与虚拟网络设备 | advanced |
| `linux-bridge` | Linux 网桥 bridge | advanced |
| `linux-xdp` | XDP 高速数据路径 | advanced |

每篇文章 SHALL 遵循 8 段式模版。

#### 特殊要求：网络栈文章 SHALL 额外包含

- 协议/机制的包处理流程（如 TCP 三次握手的完整报文交互）
- 关键内核参数（`net.core.*` / `net.ipv4.*`）及其对性能的影响
- 网络性能的观测方法（ss / netstat / ethtool / tcpdump 组合使用）
- 数据库/中间件的网络优化实践（如 Redis epoll、Nginx 零拷贝、容器网络 veth）

#### Scenario: 网络栈文章完整性

- **WHEN** 用户阅读网络栈文章
- **THEN** 每篇文章 SHALL 包含至少一个可观测的命令行示例（如 ss/tcpdump 输出）
