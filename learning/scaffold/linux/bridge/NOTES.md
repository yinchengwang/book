# 网桥配置 - 工程对照笔记

## 二层网络场景

在生产环境中，网桥主要用于以下二层网络场景：

1. **虚拟机网络隔离**
   - KVM/Xen 虚拟机通过 tap 设备连接到宿主机的网桥
   - 同一网桥上的虚拟机可以相互通信
   - 不同网桥实现网络隔离

2. **容器网络（Docker/Kubernetes）**
   - 容器通过 veth pair 连接到 Docker 网桥
   - 实现容器间通信和容器到宿主机的通信
   - 通过 iptables 实现 NAT 访问外网

3. **网络隔离与分段**
   - 将不同安全级别的网络通过网桥隔离
   - 结合 VLAN 实现更细粒度的网络分段

## 容器网络中的网桥模式

### Docker 默认网桥

Docker 安装后会创建一个默认的 `docker0` 网桥：

```
$ ip link show docker0
docker0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc noqueue state UP
    link/ether 02:42:7a:1b:3c:4d brd ff:ff:ff:ff:ff:ff
    inet 172.17.0.1/16 brd 172.17.255.255 scope global docker0
```

**特点：**
- 默认地址段：172.17.0.0/16
- 自动分配 IP 地址给容器
- 容器间可通过 IP 通信，但无法通过容器名解析
- 性能较好，适合单主机容器编排

### Docker 自定义网桥

```bash
# 创建自定义网桥
docker network create --driver bridge my_bridge

# 指定子网和网关
docker network create --driver bridge \
    --subnet=192.168.100.0/24 \
    --gateway=192.168.100.1 \
    my_bridge

# 连接容器到自定义网桥
docker run --network=my_bridge -it my_image

# 查看网桥详情
docker network inspect my_bridge
```

### Kubernetes 网络模型

Kubernetes 不使用 Docker 网桥，而是依赖 CNI（容器网络接口）插件：

- **Flannel**: 基于 VXLAN 的覆盖网络
- **Calico**: 基于 BGP 的纯三层网络
- **Cilium**: 基于 eBPF 的高性能网络

这些插件都依赖底层的 bridge 或 veth 设备，但工作在更复杂的网络拓扑中。

## 工程实践要点

1. **性能考虑**
   - 网桥工作在软中断，频繁转发会影响 CPU
   - 高速网络建议使用 hardware offload
   - eBPF 可以实现内核旁路转发

2. **安全考虑**
   - 开启 STP 防止网络环路攻击
   - 配合 VLAN 隔离不同租户流量
   - 监控 MAC 地址表防 MAC 泛洪攻击

3. **故障排查**
   - 使用 `bridge fdb show` 检查 MAC 表
   - 使用 `tcpdump -i br0` 抓包分析
   - 检查 `/sys/class/net/br0/bridge/` 下的参数
