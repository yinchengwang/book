# etcd 学习资源

## 学习目标

- 收集 etcd 学习的优质资源
- 提供源码阅读路径

## 官方资源

- **GitHub**：[etcd-io/etcd](https://github.com/etcd-io/etcd)
- **官方文档**：[etcd.io/docs](https://etcd.io/docs/)
- **API 参考**：[etcd.io/docs/v3.5/learning/api/](https://etcd.io/docs/v3.5/learning/api/)
- **Raft 论文**：In Search of an Understandable Consensus Algorithm

## 源码阅读路径

```
server/
├── embed/              # 服务嵌入
├── etcdserver/         # 服务端核心
│   ├── raft.go         # Raft 集成
│   ├── server.go       # 服务主逻辑
│   └── v3_server.go    # v3 API 实现
├── mvcc/
│   ├── kvstore.go      # MVCC 存储
│   ├── watchable_store.go  # Watch 实现
│   └── index.go        # 索引
├── raft/
│   ├── raft.go         # Raft 算法核心
│   ├── log.go           # 日志管理
│   └── node.go          # 节点通信
├── wal/
│   ├── wal.go          # WAL 日志
│   └── decoder.go      # 解码器
├── lease/
│   └── lessor.go       # 租约管理
├── client/
│   └── v3/             # 客户端
├── proxy/              # 代理
├── snapshot/           # 快照
└── auth/               # 认证
```

## 推荐书籍

| 书名 | 作者 | 说明 |
|------|------|------|
| 《分布式系统：Raft 与 etcd》 | 华为 | etcd 实现详解 |
| 《Raft 共识算法》 | 张磊 | Raft 原理精讲 |
| 《云原生分布式存储》 | 耿晏 | 分布式存储全景 |

## 推荐项目

| 项目 | 说明 | 链接 |
|------|------|------|
| Raft.js | JS 实现的 Raft | [raft/raft.js](https://github.com/raft/raft.js) |
| LogCabin | C++ 实现 Raft | [logcabin/logcabin](https://github.com/logcabin/logcabin) |
| TiKV | 分布式 KV，Raft 实现 | [tikv/tikv](https://github.com/tikv/tikv) |
| braft | Baidu 的 C++ Raft | [baidu/braft](https://github.com/baidu/braft) |

## 学习路径建议

1. **入门**：阅读 Raft 论文 + 动画演示（raft.github.io）
2. **深入**：阅读 etcd raft 包源码，理解选举和日志复制
3. **实践**：搭建 3 节点集群，验证故障转移
4. **进阶**：研究 MVCC 和 Watch 实现
5. **扩展**：对比 TiKV 的 Raft 实现，了解 multi-group Raft

## 要点总结

- 官方文档包含详细的设计文档
- 源码阅读推荐从 raft/raft.go 开始
- Raft 论文必须阅读，动画演示辅助理解
- etcd 是理解分布式一致性的最佳入口点