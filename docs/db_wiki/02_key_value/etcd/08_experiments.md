# etcd 动手实验

## 学习目标

- 掌握 etcd 的部署和基本操作
- 通过实验验证 Raft 一致性和故障恢复

## 环境准备

```bash
# Docker 启动单节点 etcd
docker run -d --name etcd-test \
  -p 2379:2379 -p 2380:2380 \
  quay.io/coreos/etcd:v3.5 \
  etcd --name node1 \
  --advertise-client-urls http://0.0.0.0:2379 \
  --listen-client-urls http://0.0.0.0:2379 \
  --listen-peer-urls http://0.0.0.0:2380 \
  --initial-advertise-peer-urls http://0.0.0.0:2380 \
  --initial-cluster node1=http://0.0.0.0:2380

# 安装 etcdctl
# Windows: 下载二进制
# Linux: apt install etcd-client
```

## 实验 1：基本 KV 操作

```bash
# 写入
etcdctl put /key1 "value1"
etcdctl put /key2 "value2"

# 读取
etcdctl get /key1
etcdctl get /key1 --print-value-only

# 前缀查询
etcdctl get /key --prefix

# 删除
etcdctl del /key1
etcdctl del /key --prefix

# 事务
# 乐观锁: 基于 Revision
etcdctl txn --interactive
compares:
value("/key") = "old_value"
success:
put /key "new_value"
failure:
get /key
```

## 实验 2：Revision 与历史版本

```bash
# 多次写入同一 key
etcdctl put /version "v1"
etcdctl put /version "v2"
etcdctl put /version "v3"

# 查看当前版本
etcdctl get /version -w json

# 查看历史版本
etcdctl get /version --rev=1

# 压缩
etcdctl compaction 2
etcdctl get /version --rev=1  # 应该报错
```

## 实验 3：Watch 监听

```bash
# 终端 1：监听
etcdctl watch /key1

# 终端 2：写入
etcdctl put /key1 "watch_test"

# 终端 1 应收到 PUT 事件

# 前缀监听
etcdctl watch /prefix/ --prefix

# 历史事件监听
etcdctl watch /key1 --rev=1
```

## 实验 4：Lease 租约

```bash
# 创建租约
etcdctl lease grant 10
# 输出: lease 694d71e0a8e7f0ef granted with TTL(10s)

# 关联键
etcdctl put /lease-key "test" --lease=694d71e0a8e7f0ef
etcdctl get /lease-key  # 可以获取

# 等待 10 秒后
etcdctl get /lease-key  # 应该返回空

# 续约
etcdctl lease keep-alive 694d71e0a8e7f0ef
```

## 实验 5：集群与故障转移

```bash
# 启动 3 节点 etcd 集群
docker network create etcd-net

# Node 1
docker run -d --name etcd1 --network etcd-net \
  -p 12379:2379 -p 12380:2380 \
  quay.io/coreos/etcd:v3.5 \
  etcd --name etcd1 \
  --initial-advertise-peer-urls http://etcd1:2380 \
  --listen-peer-urls http://0.0.0.0:2380 \
  --advertise-client-urls http://etcd1:2379 \
  --listen-client-urls http://0.0.0.0:2379 \
  --initial-cluster etcd1=http://etcd1:2380,etcd2=http://etcd2:2380,etcd3=http://etcd3:2380

# 类似启动 Node 2 和 Node 3

# 验证集群健康
etcdctl endpoint health --cluster

# 模拟故障：停止 Leader
docker stop etcd1

# 验证自动选举
etcdctl endpoint status --cluster
# 应该看到新的 Leader

# 恢复节点
docker start etcd1
```

## 实验结果记录

| 实验项目 | 预期结果 | 实际结果 |
|---------|---------|---------|
| 基本 KV 操作 | 读写成功 | |
| 历史版本查询 | 可查询压缩前版本 | |
| Watch 事件 | 写入时触发通知 | |
| Lease 过期 | 10 秒后自动删除 | |
| 故障转移 | Leader 故障后自动选举 | |

## 要点总结

- etcdctl 是主要的交互工具
- Revision 实现多版本控制
- Watch 实现实时事件通知
- 集群测试验证 Raft 故障转移

## 思考题

1. 实验中观察到的 Leader 切换时间大约是多少？
2. 3 节点集群允许同时故障几个节点？
3. 如何验证 etcd 的线性一致性读？