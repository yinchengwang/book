# TCP 拥塞控制

## 概述

TCP 拥塞控制是 TCP/IP 协议栈中的核心机制，用于防止发送方注入过多数据导致网络拥塞。本项目通过代码演示如何查询和设置 TCP 拥塞控制算法，理解 cwnd（拥塞窗口）的概念。

## 核心概念

### CUBIC vs BBR 算法

| 特性 | CUBIC | BBR |
|------|-------|-----|
| 提出者 | Linux 默认 | Google (2016) |
| 驱动方式 | 丢包驱动 | 模型驱动 |
| 核心思想 | 丢包即拥塞 | 同时测量带宽和 RTT |
| 适用场景 | 传统数据中心 | 高 BDP 网络 |
| 优势 | 成熟稳定 | 跨洲际传输高效 |
| 劣势 | 高延迟表现差 | 在丢包网络可能退化 |

### 拥塞窗口 (cwnd)

- **定义**: 发送方在未收到 ACK 前可发送的最大数据量
- **单位**: 字节或 MSS (Maximum Segment Size)
- **实际限制**: 发送量 = min(cwnd, rwnd)

### 工作流程

1. **慢启动**: cwnd 从小值开始，指数增长
2. **拥塞避免**: 达到阈值后，线性增长
3. **拥塞检测**: 丢包时减小 cwnd

## 查看拥塞控制算法

```bash
# 查看当前使用的算法
cat /proc/sys/net/ipv4/tcp_congestion_control

# 查看所有可用算法
cat /proc/sys/net/ipv4/tcp_available_congestion_control

# 查看当前设置
sysctl net.ipv4.tcp_congestion_control
```

## 设置拥塞控制算法

```bash
# 临时设置 (需要 root)
sysctl -w net.ipv4.tcp_congestion_control=bbr

# 永久设置
echo 'net.ipv4.tcp_congestion_control=bbr' >> /etc/sysctl.conf
sysctl -p
```

## 查看拥塞窗口 (cwnd)

使用 `ss` 命令查看连接状态和 cwnd：

```bash
# 查看所有 TCP 连接
ss -tn

# 查看详细连接信息 (含 cwnd)
ss -ti

# 查看特定目标的连接
ss -ti dst 192.168.1.100

# TCP 统计
ss -s
```

## 编译运行

```bash
make        # 编译
make run    # 运行演示
make clean  # 清理
```

## 输出示例

```
========================================
    TCP 拥塞控制演示程序
========================================

=== 查询当前拥塞控制算法 ===
文件: /proc/sys/net/ipv4/tcp_congestion_control

当前算法: cubic

=== 查询可用拥塞控制算法 ===
文件: /proc/sys/net/ipv4/tcp_available_congestion_control
可用算法: cubic reno

=== CUBIC vs BBR 算法对比 ===
...
```
