# Linux 网桥配置

本项目演示 Linux 网桥（Bridge）的配置和使用方法，包括传统 `brctl` 命令和现代 `ip link` 命令。

## 概述

Linux 网桥是一种二层虚拟交换机设备，运行在数据链路层（OSI 第二层）。它能够：

- 连接多个网络接口，形成一个共享介质
- 学习 MAC 地址，构建转发表
- 基于 MAC 地址进行帧转发
- 支持 VLAN 隔离
- 支持 STP 生成树协议防止环路

## 核心概念

### 二层转发原理

```
                    网桥（Bridge）
    +----------------------------------------+
    |                                        |
    |   MAC 地址表                           |
    |   +--------+-----------------+         |
    |   | MAC    | Port            |         |
    |   +--------+-----------------+         |
    |   | AA:BB  | eth0            |         |
    |   | CC:DD  | eth1            |         |
    |   | ...    | ...             |         |
    |   +--------+-----------------+         |
    |                                        |
    +--------+--------+--------+
         |        |        |
       eth0     eth1     veth0
```

**转发流程：**
1. 收到帧后学习源 MAC 地址（建立 MAC 表）
2. 查找目标 MAC 所在的端口
3. 如果找到则单播转发，否则泛洪到所有端口
4. 广播帧泛洪到所有端口（除来源）

## 命令工具

### brctl（传统方式）

```bash
# 安装
apt-get install bridge-utils

# 创建网桥
brctl addbr br0

# 添加端口
brctl addif br0 eth0

# 启用 STP
brctl stp br0 on

# 查看状态
brctl show
brctl showmacs br0

# 删除网桥
brctl delbr br0
```

### ip link（现代方式）

```bash
# 创建网桥
ip link add br0 type bridge

# 添加端口
ip link set eth0 master br0

# 设置 STP
ip link set br0 type bridge stp_state 1

# 查看网桥
ip link show type bridge
bridge link
bridge fdb show

# 删除网桥
ip link del br0
```

## VLAN 网桥

```
                    +----------+     +----------+
    VLAN 100  ----> |   br100  |     |   br200  | <---- VLAN 200
                    +----+-----+     +----+-----+
                         |                |
                    eth0.100         eth0.200
                         |                |
                    +----+-----+     +----+-----+
                         |                |
                      eth0 (Trunk)
```

## 编译运行

```bash
# 编译
make

# 运行（需要 root 权限）
sudo ./bridge_demo

# 清理
make clean
```

## 参考资料

- Linux 官方文档：https://www.kernel.org/doc/html/latest/networking/bridge.html
- iproute2 文档：https://www.man7.org/linux/man-pages/man8/ip-link.8.html
