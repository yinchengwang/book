# iptables 工程实践笔记

## 网络安全场景

在生产环境中，iptables 是保障服务器安全的基石。常见的网络安全场景包括：

### 1. 数据库服务器防火墙

数据库服务器（如 PostgreSQL、MySQL）通常只允许应用服务器访问，不对公网暴露。以下是典型的数据库防火墙策略：

```bash
# 仅允许应用层网段访问数据库
iptables -A INPUT -p tcp -s 10.0.1.0/24 --dport 5432 -j ACCEPT  # PostgreSQL
iptables -A INPUT -p tcp -s 10.0.1.0/24 --dport 3306 -j ACCEPT  # MySQL

# 允许本地回环和管理网段
iptables -A INPUT -i lo -j ACCEPT
iptables -A INPUT -p tcp -s 10.0.0.0/8 --dport 22 -j ACCEPT    # SSH 管理

# 允许健康检查和监控
iptables -A INPUT -p tcp --dport 9100 -j ACCEPT                 # node_exporter
iptables -A INPUT -p icmp -m limit --limit 1/s -j ACCEPT       # Ping 健康检查
```

### 2. 连接白名单机制

在金融、政务等高安全要求场景下，采用白名单策略是最佳实践：

```bash
# 白名单模式：先 DROP 全部，再 ACCEPT 特定 IP
iptables -P INPUT DROP
iptables -P FORWARD DROP

# 仅允许受信任的 IP 段
iptables -A INPUT -p tcp -s 10.10.0.0/16 -j ACCEPT              # 内部办公网
iptables -A INPUT -p tcp -s 192.168.100.0/24 -j ACCEPT          # 运维网段

# 允许 VPN 客户端
iptables -A INPUT -p udp --dport 1194 -j ACCEPT                  # OpenVPN
```

### 3. 端口限制与最小化攻击面

每个暴露的端口都是潜在的攻击向量。应遵循最小权限原则：

```bash
# 禁止所有非必要端口
# 只开放业务必需端口
iptables -A INPUT -p tcp --dport 80  -j ACCEPT   # HTTP
iptables -A INPUT -p tcp --dport 443 -j ACCEPT   # HTTPS
iptables -A INPUT -p tcp --dport 22 -j ACCEPT    # SSH（建议改为非标准端口）

# 禁止源端口小于 1024 的非管理员连接（部分安全策略）
iptables -A INPUT -p tcp --syn -m recent --set --name TCPFLOOD
iptables -A INPUT -p tcp --syn -m recent --update --seconds 60 --hitcount 10 --name TCPFLOOD -j DROP
```

### 4. 应用场景：K8s 节点防火墙

Kubernetes 集群中的 Node 节点需要特殊防火墙配置：

```bash
# K8s 节点必需端口
# API Server (若 Node 需要直接访问)
iptables -A INPUT -p tcp -s 10.0.0.0/8 --dport 6443 -j ACCEPT
# Kubelet
iptables -A INPUT -p tcp --dport 10250 -j ACCEPT   # 限制为同集群 CIDR
# NodePort 范围
iptables -A INPUT -p tcp --dport 30000:32767 -j ACCEPT
# Calico/CNI
iptables -A INPUT -p gre -j ACCEPT
```

### 5. Web 服务器防护实践

```bash
# 防止 SYN Flood 攻击
iptables -A INPUT -p tcp --dport 80 -m conntrack --ctstate NEW -m limit --limit 10/s -j ACCEPT

# 防止 HTTP Flood
iptables -A INPUT -p tcp --dport 80 -m hashlimit --hashlimit-above 20/sec --hashlimit-burst 100 --hashlimit-mode srcip --hashlimit-name http_flood -j DROP

# 允许 Let's Encrypt ACME 验证
iptables -A INPUT -p tcp --dport 80 -s 0.0.0.0/0 -j ACCEPT
```

### 6. 多层防火墙架构

在企业网络中，通常采用多层防护策略：

```
互联网 → 边界防火墙（硬件） → DMZ 区 → 应用防火墙（WAF）
  ↓
核心交换机 → 内网区 → 数据库防火墙（iptables/安全组）
  ↓
核心交换机 → 备份网络 → 离线存储区
```

### 7. iptables 与云安全组的关系

在云环境（AWS、阿里云等）中，iptables 与云安全组协同工作：

| 层次       | 工具              | 作用范围            |
|------------|-------------------|---------------------|
| 边界层     | 云安全组/ACL      | VPC 入口/出口控制   |
| 主机层     | iptables          | 单机网络策略        |
| 应用层     | AppArmor/Selinux  | 程序行为控制        |

通常建议：
- 云安全组负责粗粒度控制（如允许整个网段访问 80/443）
- iptables 负责细粒度控制（如特定 IP 的特定端口）

### 8. 运维注意事项

1. **修改前先备份**：`iptables-save > /tmp/iptables.bak`
2. **使用脚本管理**：将规则写入 `/etc/iptables/rules.v4`，使用 `iptables-restore` 恢复
3. **测试环境验证**：生产修改前在测试环境验证
4. **考虑性能影响**：规则过多会降低转发性能，考虑使用 ipset 管理大量 IP 白名单
5. **日志审计**：定期审计防火墙日志，检测异常访问

### 9. iptables 持久化

```bash
# Debian/Ubuntu
apt install iptables-persistent
netfilter-persistent save

# RHEL/CentOS
service iptables save

# 手动保存
iptables-save > /etc/sysconfig/iptables
iptables-restore < /etc/sysconfig/iptables
```

## 总结

iptables 是 Linux 网络安全的核心工具，合理配置可以有效防止未授权访问、DDoS 攻击和数据泄露。在实际工程中，应结合业务场景，采用最小权限原则，定期审计和更新防火墙规则。
