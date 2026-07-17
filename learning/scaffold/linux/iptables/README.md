# iptables 防火墙

## 概述

iptables 是 Linux 内核 netfilter 框架的用户空间管理工具，用于配置 IPv4 数据包的过滤、网络地址转换（NAT）和端口转发规则。

## 四表五链结构

iptables 将规则组织为 **四表** 和 **五链** 的二维结构：

```
┌─────────────────────────────────────────────────────────────┐
│                      数据包流向                              │
│   PREROUTING ─→ ROUTING ─→ FORWARD ─→ POSTROUTING          │
│        ↓            ↓         ↓            ↓                │
│   [NAT 表]    [路由决策]  [Filter 表]  [NAT 表]              │
│                         ↓                                    │
│                   INPUT ─→ [Filter 表] ─→ OUTPUT            │
│                     ↓                      ↓                │
│                应用进程               输出数据包              │
└─────────────────────────────────────────────────────────────┘
```

### 四表

| 表名    | 用途                           | 优先级 |
|---------|--------------------------------|--------|
| filter  | 包过滤（接受/拒绝）             | 高     |
| nat     | 网络地址转换                   | 中     |
| mangle  | 包修改（TTL、TOS 等）          | 低     |
| raw     | 绕过连接追踪                   | 最高   |

### 五链

| 链名        | 位置                           | 用途                     |
|-------------|--------------------------------|--------------------------|
| PREROUTING  | 路由前                         | DNAT（目标地址转换）     |
| INPUT       | 进入本地应用前                 | 入站过滤                 |
| FORWARD     | 转发数据包时                   | 转发过滤                 |
| OUTPUT      | 本地输出前                     | 出站过滤                 |
| POSTROUTING | 路由后                         | SNAT/MASQUERADE（源地址转换）|

## 连接追踪状态

| 状态       | 含义                                           |
|------------|------------------------------------------------|
| NEW        | 新建立的连接请求（如 TCP SYN）                 |
| ESTABLISHED| 已完成三次握手的连接                           |
| RELATED    | 关联现有连接的新连接（如 FTP 数据通道、ICMP 错误）|
| INVALID    | 无法识别的数据包                                |

## 常见规则示例

### 基本包过滤

```bash
# 允许已建立连接的流量
iptables -A INPUT -m state --state ESTABLISHED,RELATED -j ACCEPT

# 允许 SSH（源 IP 限制）
iptables -A INPUT -p tcp -s 192.168.1.0/24 --dport 22 -j ACCEPT

# 允许 HTTP/HTTPS
iptables -A INPUT -p tcp --dport 80 -j ACCEPT
iptables -A INPUT -p tcp --dport 443 -j ACCEPT

# 默认拒绝
iptables -P INPUT DROP
```

### NAT 地址转换

```bash
# SNAT：内网访问外网时替换源 IP
iptables -t nat -A POSTROUTING -s 192.168.1.0/24 -o eth0 -j MASQUERADE

# DNAT：外部请求转发到内网服务器
iptables -t nat -A PREROUTING -p tcp -i eth0 --dport 8080 -j DNAT --to-destination 192.168.1.100:80

# 端口重定向（透明代理）
iptables -t nat -A PREROUTING -p tcp --dport 80 -j REDIRECT --to-port 8080
```

### 连接追踪

```bash
# 使用 conntrack 模块（比 state 更精确）
iptables -A INPUT -m conntrack --ctstate ESTABLISHED -j ACCEPT
iptables -A INPUT -m conntrack --ctstate RELATED -j ACCEPT
iptables -A INPUT -m conntrack --ctstate NEW -p tcp --dport 22 -j ACCEPT
```

### 限流与日志

```bash
# 限制 ICMP 流量（防止 ping flood）
iptables -A INPUT -p icmp -m limit --limit 1/s -j ACCEPT

# 记录被丢弃的包
iptables -A INPUT -m limit --limit 5/min -j LOG --log-prefix 'iptables-DROP: '

# 限速 SYN
iptables -A INPUT -p tcp --dport 80 -m conntrack --ctstate NEW -m limit --limit 10/s -j ACCEPT
```

### Docker 容器网络

```bash
# Docker 默认规则
iptables -t nat -A POSTROUTING -s 172.17.0.0/16 ! -o docker0 -j MASQUERADE
iptables -A FORWARD -i docker0 -o eth0 -j ACCEPT
iptables -A FORWARD -i eth0 -o docker0 -m state --state RELATED,ESTABLISHED -j ACCEPT
```

## 查看和管理规则

```bash
# 查看规则（详细）
iptables -L -n -v --line-numbers

# 查看 NAT 表
iptables -t nat -L -n -v

# 清空规则
iptables -F
iptables -X
iptables -t nat -F

# 删除指定规则
iptables -D INPUT 3

# 保存规则
iptables-save > /etc/sysconfig/iptables

# 恢复规则
iptables-restore < /etc/sysconfig/iptables
```

## 编译与运行

```bash
# 编译
make

# 模拟模式运行（仅打印命令）
make run

# 实际模式运行（需要 root 权限）
sudo make apply
```

## 参考资料

- [iptables 官方文档](https://www.netfilter.org/documentation/)
- [Linux 防火墙 HOWTO](https://www.tldp.org/HOWTO/Linux-iFirewall/)
