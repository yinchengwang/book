# 虚拟以太网设备

## 概述

veth（Virtual Ethernet）pair 是一对虚拟以太网设备，数据从一端进入，从另一端流出。它是 Linux 内核提供的虚拟网络设备，常用于容器网络、网络命名空间隔离等场景。

## 核心概念

### veth pair

veth pair 由两个虚拟端口组成：
- 总是成对出现
- 数据双向转发
- 可分配到不同网络命名空间

```
  命名空间 A                命名空间 B
  +---------+              +---------+
  |  veth0  |<-----+----->|  veth1  |
  +---------+      |      +---------+
                   |
              (内核转发)
```

## 与容器网络的关系

Docker、Podman、Kubernetes 等容器运行时底层依赖 veth pair：

1. **创建阶段**：为每个容器创建一对 veth
2. **分配阶段**：容器内一端命名为 eth0，宿主机的另一端接入网桥
3. **通信阶段**：同主机容器通过网桥通信，跨主机通过 overlay 网络（VXLAN）

```
  容器内                         宿主机
  +--------+                    +--------+
  |  eth0  |<-- veth pair -->|  vethX  |
  +--------+                    +---+----+
                                   |
                              +----+----+
                              |  docker0 |
                              |  (bridge) |
                              +----+----+
                                   |
                              +----+----+
                              |   eth0   |
                              +----+----+
```

## 常用配置命令

```bash
# 创建 veth pair
ip link add veth0 type veth peer name veth1

# 配置 IP 并启用
ip addr add 10.0.0.1/24 dev veth0
ip link set veth0 up
ip link set veth1 up

# 将 veth 移动到网络命名空间
ip netns add myns
ip link set veth1 netns myns
ip netns exec myns ip addr add 10.0.0.2/24 dev veth1
ip netns exec myns ip link set veth1 up

# 接入网桥
ip link set veth0 master br0

# 查看设备
ip link show type veth
bridge link show

# 查看网桥转发表
bridge fdb show
```

## 编译运行

```bash
# 编译
make

# 运行（需要 root 权限）
sudo ./veth_demo
```

## 扩展阅读

- `ip netns` - 网络命名空间管理
- `bridge` - 网桥配置工具
- `tc` - 流量控制工具
- Docker bridge 驱动、Kubernetes CNI 插件
