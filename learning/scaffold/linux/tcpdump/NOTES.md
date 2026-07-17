# Linux tcpdump 原理学习笔记

本文档记录网络抓包在项目工程代码中的实际应用对照。

## 工程对照

### 1. 网络抓包在调试中的应用

工程代码中使用 Raw Socket 进行网络调试：

```c
// 工程中可能使用类似机制进行网络诊断
// engineering/src/db/network/ 中可能有网络监控
typedef struct {
    unsigned int packets_sent;
    unsigned int packets_received;
    unsigned int bytes_sent;
    unsigned int bytes_received;
    unsigned int errors;
} net_stats_t;
```

**应用场景：**

1. **连接问题诊断**：抓包分析 TCP 重传、握手失败
2. **性能分析**：测量延迟、带宽
3. **安全审计**：检测异常流量

### 2. libpcap 与 BPF

实际 tcpdump 使用 libpcap 库：

```c
// libpcap 典型用法
#include <pcap.h>

pcap_t *handle = pcap_open_live("eth0", 65535, 1, 1000, errbuf);
pcap_compile(handle, &fp, "tcp port 5432", 1, 0);
pcap_setfilter(handle, &fp);

pcap_loop(handle, 10, packet_handler, NULL);
```

**libpcap vs Raw Socket：**

| 特性 | libpcap | Raw Socket |
|------|---------|------------|
| 跨平台 | 支持 | 仅 Linux |
| 过滤器 | 内核执行 | 用户执行 |
| 性能 | 更高 | 较低 |
| 权限 | 通常需要 | 需要 root |

### 3. 数据包解析在工程中的应用

工程代码可能需要解析协议：

```c
// 解析以太网 + IP + TCP 头部
// 类似于 demo 中的 parse_eth_header()
typedef struct {
    unsigned char dst_mac[6];
    unsigned char src_mac[6];
    unsigned int src_ip;
    unsigned int dst_ip;
    unsigned short src_port;
    unsigned short dst_port;
    unsigned char flags;
} packet_info_t;

int parse_packet(const unsigned char *data, size_t len, packet_info_t *info) {
    if (len < sizeof(eth_header_t) + sizeof(struct ip)) {
        return -1;
    }
    // 解析逻辑...
    return 0;
}
```

### 4. 工程中的网络监控

```c
// 工程中的网络监控模块
// engineering/src/db/monitor/net_monitor.c
typedef struct {
    time_t timestamp;
    unsigned int established_conn;
    unsigned int time_wait_conn;
    unsigned int listen_conn;
    unsigned long rx_bytes;
    unsigned long tx_bytes;
} net_sample_t;
```

### 5. Wireshark 协议解析

工程代码调试时可以使用 Wireshark：

1. 抓包保存为 pcap 文件
2. 用 Wireshark 打开
3. 分析协议细节

**常用过滤器：**

```
tcp.port == 5432        # PostgreSQL 流量
ip.addr == 192.168.1.100  # 指定 IP
tcp.flags.syn == 1      # SYN 包
```

## 参考资料

- `man pcap` — libpcap 手册
- tcpdump 源码 — 学习完整实现
- Wireshark 源码 — 学习协议解析
- `/usr/include/netinet/` — 协议头部定义
