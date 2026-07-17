# 网络诊断工具

## 概述

本卡演示 Linux 常用网络诊断工具，包括 tcpdump、netstat/ss 等。

## 常用诊断命令

### tcpdump（抓包）

```bash
# 基本抓包
sudo tcpdump -i eth0 port 8888

# 详细信息
sudo tcpdump -i any -nnvv -X port 80

# 只抓 TCP SYN
sudo tcpdump -i any 'tcp[tcpflags] == tcp-syn'
```

### netstat / ss（连接状态）

```bash
# 查看 TCP 连接
ss -ant

# 查看监听端口
ss -tlnp

# 查看统计
ss -s
```

## TCP 连接状态

| 状态 | 说明 |
|------|------|
| LISTEN | 服务端监听中 |
| ESTABLISHED | 连接已建立 |
| TIME_WAIT | 主动关闭，等待 2MSL |
| CLOSE_WAIT | 被动关闭，等待 close() |

## TIME_WAIT 处理

```bash
# 允许重用 TIME_WAIT 端口
echo 1 | sudo tee /proc/sys/net/ipv4/tcp_tw_reuse

# 调高本地端口范围
sysctl -w net.ipv4.ip_local_port_range="1024 65535"
```

## CLOSE_WAIT 处理

CLOSE_WAIT 过多说明代码问题：
- 读取返回 0 时必须调用 close()
- 设置读取超时
- 实现心跳检测

## 编译运行

```bash
make run          # 运行诊断脚本
```

## 学习要点

1. **tcpdump** 是抓包神器，必备技能
2. **ss** 是 netstat 的现代替代，更快更清晰
3. **TIME_WAIT** 是正常现象，可以通过参数调优
4. **CLOSE_WAIT** 过多通常是代码 bug
