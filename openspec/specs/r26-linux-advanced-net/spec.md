# R26 Linux 高级网络 — 能力规格

## 概述

R26 Linux 高级网络是 R25 Linux 网络基础的后续专题，深入学习 Linux 网络栈的高级特性。

## 8 张高级网络卡

### 1. namespace — 网络命名空间(Netns)

| 属性 | 值 |
|------|-----|
| 技术栈 | linux |
| 象限 | engineering |
| 难度 | advanced |
| 核心概念 | netns / veth pair / 网络隔离 / 路由隔离 |
| 工程关联 | 容器网络隔离 / Docker 网络模型 / K8s 网络 |

### 2. veth — veth 与虚拟网络设备

| 属性 | 值 |
|------|-----|
| 技术栈 | linux |
| 象限 | engineering |
| 难度 | advanced |
| 核心概念 | veth pair / MACVLAN / IPVLAN / offload |
| 工程关联 | 容器网络性能 / GRO/GSO / TSO |

### 3. bridge — Linux 网桥(bridge)

| 属性 | 值 |
|------|-----|
| 技术栈 | linux |
| 象限 | engineering |
| 难度 | advanced |
| 核心概念 | FDB / STP / Open vSwitch / VXLAN |
| 工程关联 | Docker bridge 模式 / SDN 网络 |

### 4. xdp — XDP 高速数据路径

| 属性 | 值 |
|------|-----|
| 技术栈 | linux |
| 象限 | engineering |
| 难度 | advanced |
| 核心概念 | XDP / AF_XDP / BPF / 零拷贝 |
| 工程关联 | DDoS 防护 / 高性能负载均衡 / eBPF |

### 5. tcp_congestion — TCP 拥塞控制算法

| 属性 | 值 |
|------|-----|
| 技术栈 | linux |
| 象限 | engineering |
| 难度 | advanced |
| 核心概念 | CUBIC / BBR / cwnd / 拥塞窗口 |
| 工程关联 | 高 BDP 网络 / 跨地域数据库复制 |

### 6. tcp_fastopen — TCP Fast Open

| 属性 | 值 |
|------|-----|
| 技术栈 | linux |
| 象限 | engineering |
| 难度 | advanced |
| 核心概念 | TFO / Cookie / 三次握手优化 |
| 工程关联 | 短连接优化 / Serverless 冷启动 |

### 7. udp_socket — UDP 高性能套接字

| 属性 | 值 |
|------|-----|
| 技术栈 | linux |
| 象限 | engineering |
| 难度 | advanced |
| 核心概念 | SO_REUSEPORT / UDP_GRO / 批量处理 |
| 工程关联 | DNS 服务器 / QUIC 协议 |

### 8. so_reuseport — SO_REUSEPORT 多线程

| 属性 | 值 |
|------|-----|
| 技术栈 | linux |
| 象限 | engineering |
| 难度 | advanced |
| 核心概念 | per-CPU 负载均衡 / eBPF redirect |
| 工程关联 | 数据库连接池 / 多核扩展 / Nginx |

## 能力矩阵

| 能力 | 级别 | 说明 |
|------|------|------|
| 网络命名空间 | 掌握 | 能够创建/管理 netns，配置网络隔离 |
| 虚拟网络设备 | 掌握 | 理解 veth/MACVLAN/IPVLAN 的差异与适用场景 |
| 网桥与 SDN | 理解 | 了解 bridge/FDB/STP/OVS 的工作原理 |
| XDP/eBPF | 了解 | 理解 XDP 在高速数据包处理中的应用 |
| TCP 拥塞控制 | 理解 | 能够根据网络环境选择合适的拥塞控制算法 |
| TCP 优化 | 掌握 | 配置 TFO、SO_REUSEPORT 等优化选项 |
| UDP 高性能 | 掌握 | 实现高性能 UDP 应用，理解 QUIC 基础 |
| 多核扩展 | 掌握 | 使用 SO_REUSEPORT 实现多核负载均衡 |

## 技术栈覆盖

- **Linux 网络内核**：netns / veth / bridge / XDP
- **传输层优化**：TCP 拥塞控制 / TFO / UDP 优化
- **多核扩展**：SO_REUSEPORT / eBPF 负载均衡
- **SDN**：Open vSwitch / VXLAN

## 工程实践

1. **容器网络**：使用 netns/veth 模拟容器网络
2. **数据库优化**：TCP 参数调优提升跨地域复制性能
3. **高性能服务**：使用 SO_REUSEPORT 实现高并发
4. **网络诊断**：综合运用 tcpdump/ss/ethtool

## 后续学习

- R27: 数据结构基础（DS basics）
- 深入 eBPF/XDP 实现高性能网络应用
- 学习 Kubernetes CNI 插件实现
