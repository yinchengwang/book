# NOTES.md — veth 工程对照

## 虚拟网络设备场景

在现代分布式系统中，虚拟网络设备是基础设施的重要组成部分。veth pair 作为 Linux 内核提供的虚拟以太网设备，主要应用于以下场景：

1. **容器网络隔离**：每个容器拥有独立的网络命名空间，通过 veth pair 与宿主机通信
2. **服务网格**：Sidecar 代理通过 veth 或 tap 设备拦截流量
3. **网络虚拟化**：OpenStack Neutron、VMware NSX 等使用 veth 构建虚拟网络
4. **安全隔离**：将敏感服务置于独立命名空间，通过 veth 控制网络访问

## Docker/容器网络实现

Docker 默认使用 bridge 驱动，底层实现如下：

```
容器 namespace              宿主机
     |                          |
     | eth0                     |
     |<-> vethXXXXXXXX@ifN      |
     |                          |
     +-------- docker0 ---------+
              (bridge)
```

关键点：
- 容器内的 eth0 是 veth pair 的宿主端
- 宿主机端的 vethXXXXXXXX 接入 docker0 网桥
- 容器间通过网桥二层转发通信
- 容器访问外网通过 NAT 实现

## Pod 网络与 veth 的关系

Kubernetes 中 Pod 网络模型与 veth 紧密相关：

| CNI 插件 | 网络模型 | veth 角色 |
|----------|----------|-----------|
| Flannel | VXLAN overlay | host-gw 模式下 veth 接入 host |
| Calico | BGP 直连 | veth 接入宿主机网络 |
| Cilium | eBPF | 替代部分 veth 功能，但仍需 veth 连接 namespace |
| Host-local | 透传 | 直接使用宿主机网络命名空间 |

Cilium 等新型 CNI 通过 eBPF 在内核数据路径上实现更高效的转发，减少了传统 veth + bridge 的软转发开销，但在容器创建时仍需建立 veth pair 作为连接通道。

## 工程实践

在设计分布式数据库或中间件的容器化部署时，需要考虑：

1. **网络模式选择**：bridge 模式简单但有性能开销，host 模式性能好但端口冲突
2. **MTU 配置**：veth 默认 MTU 1500，需与物理网络和 overlay 隧道协调
3. **性能监控**：通过 `ip -s link show vethXXX` 观察丢包和错包
4. **安全策略**：结合 network policy 限制 Pod 间通信

理解 veth 机制有助于排查容器网络问题，优化服务间通信延迟。
