# etcd 项目关联

## 学习目标

- 分析 etcd 设计对项目的启发
- 对比项目中 Raft 实现的异同

## 项目中已有的 Raft 实现

项目在 `engineering/src/db/core/` 中已有 Raft 共识实现：

| 组件 | 状态 | 说明 |
|------|------|------|
| Raft 核心 | ✅ 完成 | `src/db/core/raft.h/c` |
| Leader 选举 | ✅ 完成 | 基于 Raft 论文实现 |
| 日志复制 | ✅ 完成 | AppendEntries |
| 安全性 | ✅ 完成 | 选举限制 + 提交规则 |

## Raft 实现对比

| 特性 | etcd Raft | 项目 Raft |
|------|-----------|-----------|
| 语言 | Go | C |
| PreVote | ✅ | 待实现 |
| CheckQuorum | ✅ | 待实现 |
| 异步 Apply | ✅ | 部分 |
| Pipeline | ✅ | 待实现 |
| Snapshot | ✅ | 部分 |

## 可借鉴的设计

```c
// etcd 的 Raft 日志结构
type Entry struct {
    Term        uint64    // 任期
    Index       uint64    // 日志索引
    Type        EntryType // Normal/ConfChange
    Data        []byte    // 数据
}

// 项目中的 Raft 日志
// raft.h
typedef struct {
    uint64_t term;
    uint64_t index;
    uint8_t  type;    // 0: Normal, 1: ConfChange
    uint8_t *data;
    size_t   len;
} raft_entry_t;
```

## MVCC 借鉴

```c
// etcd 的 MVCC 思路
// 每个 key 有多个版本 (revision)
// 最新版本用于读，旧版本用于历史查询

// 项目借鉴
// 在存储引擎中增加 Revision
// 支持时间点查询
// 定期压缩清理
```

## 要点总结

- 项目中已实现基本的 Raft 核心
- 可借鉴 etcd 的 PreVote 和 Pipeline 优化
- MVCC 思路可用于项目的多版本支持
- etcd 的存储限制策略值得参考

## 思考题

1. 项目中的 Raft 实现与 etcd 的 raft 库在 API 设计上有何异同？
2. 如何将 etcd 的 PreVote 机制移植到项目的 Raft 实现中？
3. 项目中是否需要实现类似 etcd 的 MVCC？适用场景是什么？