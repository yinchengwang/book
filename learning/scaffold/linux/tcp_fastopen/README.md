# TCP Fast Open

## 概述

TCP Fast Open (TFO) 是 Linux 内核 3.7 引入的 TCP 协议扩展，旨在减少短连接的延迟开销。传统 TCP 需要完成三次握手才能传输数据，而 TFO 允许在第一次 SYN 包中携带应用数据，从而节省 1 个 RTT（Round-Trip Time）。

## 工作原理

### 传统 TCP 三次握手

```
客户端                              服务端
  |------- SYN ------------------->|  RTT 1
  |<------ SYN-ACK ---------------|  RTT 2
  |------- ACK ------------------>|
  |======= HTTP请求数据 ==========>|  RTT 3
  |<====== HTTP响应数据 ===========|
```

### TFO 优化后的握手

```
客户端                              服务端
  |------- SYN + Cookie + Data --->|  RTT 1 (已携带数据!)
  |<------ SYN-ACK + 响应数据 -----|  RTT 2
  |------- ACK ------------------>|
```

## Cookie 机制

TFO 使用 Cookie 来确保安全性，防止放大攻击：

1. **首次连接**：客户端发送带空 Cookie 的 SYN+Data
2. **Cookie 协商**：服务端生成 Cookie 并通过 SYN-ACK 返回
3. **后续连接**：客户端携带有效 Cookie，服务端验证后接受数据
4. **Cookie 格式**：`HMAC(密钥, 客户端IP + 端口 + 时间戳)`

Cookie 有效期通常为 2 天，且与 (IP, Port) 对绑定。

## 启用方法

```bash
# 检查当前状态
cat /proc/sys/net/ipv4/tcp_fastopen

# 启用 TFO (3 = 客户端|服务端)
sudo sysctl -w net.ipv4.tcp_fastopen=3

# 永久生效
echo "net.ipv4.tcp_fastopen = 3" | sudo tee -a /etc/sysctl.conf
sudo sysctl -p
```

## 适用场景

| 场景 | 收益 | 说明 |
|------|------|------|
| HTTP API 短连接 | 高 | 高频 HTTP 请求，每次节省 1 RTT |
| 数据库短连接 | 高 | 连接池不可用时的最佳选择 |
| DNS 查询 | 中 | DNS 递归查询通常只需一次往返 |
| 长连接 | 低 | 连接复用时 TFO 优势消失 |

## 使用限制

1. **首次连接无收益**：需要协商 Cookie
2. **需要应用支持**：应用程序需使用 TFO API
3. **需要 root 权限**：修改 sysctl 参数
4. **不支持重传**：SYN+Data 在重传时会被丢弃

## curl 测试示例

```bash
# 检查 curl TFO 支持
curl --version | grep -i fastopen

# 使用 TFO 发起请求
curl -v --tcp-fastopen https://example.com/

# 服务端启用示例 (使用 nc 模拟)
nc -l -p 8080 &
curl --tcp-fastopen http://localhost:8080/
```

## 内核参数说明

`/proc/sys/net/ipv4/tcp_fastopen` 是 3 位掩码：

| 位 | 值 | 说明 |
|----|----|------|
| bit 0 | 1 | 客户端启用 TFO |
| bit 1 | 2 | 服务端启用 TFO |
| bit 2 | 4 | 禁用 TFO 旁路黑名单 |

值 3 表示客户端+服务端都启用。
