# net_diag — 网络诊断接口

## 简介

演示 Linux 网络诊断接口 `/proc/net/snmp`、`/proc/net/tcp`，分析 TCP/UDP 统计和连接状态。

## 编译

```bash
gcc -std=c11 -Wall -o net_diag main.c
```

## 运行

```bash
./net_diag
```

## 核心概念

### /proc/net/snmp

内核 SNMP（Simple Network Management Protocol）统计，包含 IP/TCP/UDP 三层计数。

### /proc/net/tcp

当前 TCP 连接表，列出所有套接字及其状态。

### TCP 连接状态

| 状态 | 说明 |
|------|------|
| LISTEN | 监听 |
| ESTABLISHED | 已建立连接 |
| TIME_WAIT | 等待 2MSL 后关闭 |
| CLOSE_WAIT | 对端已关闭，本端未 close |

## 诊断要点

- 重传率高 → 网络丢包或拥塞
- TIME_WAIT 过多 → 短连接频繁
- CLOSE_WAIT 堆积 → 应用未正确关闭套接字

## 扩展

- 使用 `ss -i` 查看套接字详细统计
- 使用 `netstat -s` 查看历史统计
