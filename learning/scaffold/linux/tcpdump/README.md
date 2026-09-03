# Linux tcpdump 原理学习卡片

本学习卡片演示 tcpdump 的工作原理，包括 Raw Socket 创建、BPF 过滤器、以及数据包解析（Ethernet + IP + TCP）。

## 学习目标

1. 理解 Raw Socket 的工作原理
2. 掌握 BPF（Berkeley Packet Filter）概念
3. 理解数据包封装结构（Ethernet -> IP -> TCP）
4. 能够解析简单的网络数据包

## 核心概念

### Raw Socket

```c
// 创建原始套接字
int sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP));
```

- `AF_PACKET`: 链路层访问
- `SOCK_RAW`: 原始数据包
- `ETH_P_IP`: 接收 IPv4 数据包

### BPF 过滤器

tcpdump 使用 BPF 在内核态过滤数据包：

```
用户空间: tcpdump "port 80"
         ↓ 编译
BPF 字节码: [load 12], [jeq 0x86DD], ...
         ↓ 加载
内核空间: 内核执行过滤，减少数据拷贝
```

### 数据包结构

```
+-------------------+
| Ethernet Header   | 14 字节
| - Dst MAC (6B)    |
| - Src MAC (6B)    |
| - EtherType (2B)  |
+-------------------+
| IP Header         | 20 字节
| - Version (4b)    |
| - TTL (8b)        |
| - Protocol (8b)   |
| - Src IP (32b)    |
| - Dst IP (32b)    |
+-------------------+
| TCP Header        | 20+ 字节
| - Src Port (16b)  |
| - Dst Port (16b)  |
| - Flags (SYN/ACK) |
+-------------------+
| Payload           |
+-------------------+
```

## 编译与运行

### 编译

```bash
make
# 或直接使用 gcc
gcc -std=c11 -Wall -o tcpdump main.c
```

### 运行（需要 root）

```bash
sudo ./tcpdump
# 或使用 make（自动添加 sudo）
make check
```

### 清理

```bash
make clean
```

## 输出示例

```
========================================
  Linux tcpdump 原理学习演示
========================================

--- Demo 1: Raw Socket 创建 ---
[tcpdump]   Raw Socket 创建成功

--- Demo 2: 抓包演示 ---
[tcpdump] ===== 数据包 #1 =====
[tcpdump]   Ethernet: AA:BB:CC:DD:EE:FF -> 00:11:22:33:44:55
[tcpdump]   EtherType: 0x0800 (IPv4)
[tcpdump]   IP: 192.168.1.100 -> 93.184.216.34
[tcpdump]   TCP: 192.168.1.100:54321 -> 93.184.216.34:80
[tcpdump]   Flags: SYN

========================================
=== PASS ===
========================================
```

## 相关资源

- `man pcap` — libpcap 库文档
- `man bpf` — BPF 说明
- tcpdump 源码 — https://github.com/the-tcpdump-group/tcpdump
- libpcap 源码 — https://github.com/the-tcpdump-group/libpcap
