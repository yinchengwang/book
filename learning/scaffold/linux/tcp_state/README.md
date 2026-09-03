# Linux TCP 状态学习卡片

本学习卡片演示 Linux TCP 连接状态的查看方法，包括读取 /proc/net/tcp、解析 TCP 11 种状态、统计连接数、以及故障排查思路。

## 学习目标

1. 理解 TCP 11 种状态及其含义
2. 掌握 /proc/net/tcp 的数据格式
3. 理解 TCP 状态机转换
4. 能够进行基本的连接故障排查

## 核心概念

### TCP 11 种状态

| 状态 | 含义 | 典型场景 |
|------|------|----------|
| LISTEN | 监听中 | 服务器等待连接 |
| SYN_SENT | 已发送 SYN | 客户端发起连接 |
| SYN_RECV | 已收到 SYN | 服务器回复后 |
| ESTABLISHED | 已建立 | 正常通信 |
| FIN_WAIT1 | 等待 FIN ACK | 主动关闭 |
| FIN_WAIT2 | 等待对方 FIN | 收到 ACK 后 |
| CLOSE_WAIT | 等待关闭 | 被动关闭 |
| CLOSING | 双方同时关闭 | - |
| TIME_WAIT | 等待超时 | 确保对方收到 |
| LAST_ACK | 最后 ACK | 被动关闭方 |
| CLOSE | 关闭 | 连接结束 |

### /proc/net/tcp 格式

```
sl  local_address rem_address   st tx_queue rx_queue tr tm->when retrnsmt   uid timeout inode
0: 0100007F:0035 00000000:0000 0A 00000000:00000000 00:00000000 00000000     0        0 12345
```

字段说明：
- `local_address`: 本地 IP:Port（十六进制）
- `rem_address`: 远程 IP:Port（十六进制）
- `st`: TCP 状态（十六进制）
- `tx_queue/rx_queue`: 发送/接收队列大小
- `uid`: 用户 ID
- `inode`: Socket inode

## 编译与运行

### 编译

```bash
make
# 或直接使用 gcc
gcc -std=c11 -Wall -o tcp_state main.c
```

### 运行

```bash
./tcp_state
# 或使用 make
make check
```

### 清理

```bash
make clean
```

## 输出示例

```
========================================
  Linux TCP 状态学习演示
========================================

--- Demo 1: TCP 连接统计 ---
[tcp_state]   本地地址         远程地址         状态
[tcp_state]   0.0.0.0:22      -> 0.0.0.0:0      LISTEN
[tcp_state]   127.0.0.1:5432  -> 0.0.0.0:0      LISTEN
[tcp_state]   192.168.1.10:80 -> 10.0.0.5:45678 ESTABLISHED

[TCP 连接状态统计]
[tcp_state]   ESTABLISHED=5 LISTEN=2

========================================
=== PASS ===
========================================
```

## 相关资源

- `man netstat` — 网络统计工具
- `ss -t -a` — Socket 统计
- `cat /proc/net/tcp` — 原始数据
