# NOTES - 工程对照

## 高性能网络处理

XDP 代表了 Linux 高性能网络处理的最新方向，与 Cilium/DPDK 形成技术光谱。

## Cilium 对比

| 方案 | 位置 | 性能 | 灵活性 |
|------|------|------|--------|
| XDP | 驱动层 | 最高 | 中 |
| TC | qdisc 层 | 高 | 高 |
| iptables | netfilter | 中 | 高 |

## 工程实践

### DPDK vs XDP

DPDK 使用用户态轮询，绕过内核；XDP 在内核中实现类似性能。
- XDP：更简单，Linux 原生
- DPDK：性能更高，需要专用驱动

### 典型应用

1. **DDoS 防护** - Cloudflare 用 XDP 实现 10G+ 防御
2. **负载均衡** - Facebook Katran 使用 XDP
3. **网络监控** - Cilium 用 XDP 实现网络策略
4. **流量过滤** - 内核网络高速路径

## 数据库场景

高并发网络处理优化：
- 连接数极高时 XDP 可减少协议栈开销
- 网络密集型 OLTP 场景受益
- 内核网络栈成为瓶颈时的可选方案
