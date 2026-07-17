# Linux 网络命名空间

## 概述

Linux 网络命名空间 (Network Namespace) 是 Linux 内核提供的网络资源隔离机制。通过网络命名空间，可以在同一个主机上创建多个相互隔离的网络视图，每个命名空间拥有独立的网络设备、路由表、防火墙规则、端口绑定等网络资源。

网络命名空间是容器技术的核心基础之一，Docker、Kubernetes 等容器平台都依赖网络命名空间来实现容器间的网络隔离。

## 核心概念

### 命名空间内隔离的资源

| 资源类型 | 说明 |
|---------|------|
| 网络接口 | eth0, veth, lo 等网络设备 |
| IP 地址 | 每个接口可绑定独立 IP |
| 路由表 | 独立的路由决策 |
| 防火墙规则 | iptables/nftables 规则 |
| 端口绑定 | 可绑定相同端口而不冲突 |
| 网络协议栈 | 独立的 TCP/IP 配置 |

### 常用 API

```c
#include <sched.h>

// 创建新的网络命名空间
int unshare(int flags);
// flags: CLONE_NEWNET

// 查看当前进程所属命名空间
int setns(int fd, int nstype);

// 读取 /proc/self/ns/net 获取命名空间文件描述符
```

## 与容器的关联

网络命名空间是容器网络隔离的基础：

1. **Docker 容器**
   - 每个容器拥有独立的网络命名空间
   - 容器内只能看到自己的网络接口
   - 不同容器可绑定相同端口而不冲突

2. **Kubernetes Pod**
   - 每个 Pod 一个网络命名空间
   - Pod 内所有容器共享同一网络命名空间 (通过 pause 容器实现)
   - CNI 插件负责 Pod 网络配置

3. **网络隔离效果**
   - 容器间网络完全隔离
   - 容器可访问外部网络 (通过 NAT)
   - 可配置容器间网络策略

## 常用命令

### 创建和管理命名空间

```bash
# 创建网络命名空间
sudo ip netns add myns

# 列出所有网络命名空间
ip netns list

# 删除网络命名空间
sudo ip netns del myns

# 在命名空间内执行命令
sudo ip netns exec myns ip link
sudo ip netns exec myns ip addr

# 使用 nsenter 进入命名空间
sudo nsenter --target=<PID> --net ip link
```

### veth pair (虚拟网线)

```bash
# 创建 veth pair
sudo ip link add veth0 type veth peer name veth1

# 将一端移到命名空间
sudo ip link set veth1 netns myns

# 配置主机端
sudo ip addr add 10.0.0.1/24 dev veth0
sudo ip link set veth0 up

# 配置命名空间内端
sudo ip netns exec myns ip addr add 10.0.0.2/24 dev veth1
sudo ip netns exec myns ip link set veth1 up
```

### Docker 网络

```bash
# 查看 Docker 网络
docker network ls

# 创建自定义网络 (bridge 模式)
docker network create --driver bridge mynet

# 运行容器并指定网络
docker run --network=mynet -it ubuntu bash

# 查看容器网络命名空间
ls /var/run/docker/netns/
```

## 编译运行

```bash
# 编译
make

# 运行 (需要 root 权限)
sudo make run

# 清理
make clean
```

## 注意事项

1. 需要 root 权限或 `CAP_SYS_ADMIN` capability 才能创建网络命名空间
2. 删除命名空间前需确保其中没有进程
3. veth pair 一端移到命名空间后，主机端需要手动管理
4. 网络命名空间删除后，其中的网络资源也会被清理
