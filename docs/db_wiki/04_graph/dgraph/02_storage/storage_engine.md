# Dgraph 存储与事务

## 学习目标

- 理解 Dgraph 的 Badger LSM-Tree 存储引擎
- 掌握 Dgraph 的 DQL（GraphQL+-）查询语言

## Badger LSM-Tree

```go
// Badger 数据结构
// 基于 LSM-Tree 的 KV 存储

// 写入路径
// 1. 写入 WAL（预写日志）
// 2. 写入 MemTable
// 3. MemTable 刷盘为 L0 SSTable
// 4. 后台 compaction 合并

// 读取路径
// 1. MemTable
// 2. L0 SSTable（最新）
// 3. L1, L2, ...
// 4. Bloom Filter 加速

// 压缩策略
// Level: L0 → L1 → L2 → ... 每层大小翻倍
```

## DQL 查询

```graphql
# 基础查询
{
  query(func: eq(name, "Alice")) {
    uid
    name
    age
  }
}

# 图案匹配
{
  query(func: eq(name, "Alice")) {
    name
    knowsperson: knows {
      name
    }
  }
}

# 聚合
{
  query(func: has(Person.name)) {
    count(uid)
    avg(age)
  }
}
```

## 事务机制

```go
// Dgraph 事务特性
// 1. 乐观事务（无锁）
// 2. 快照隔离
// 3. 分布式事务支持

// 事务示例
txn := client.NewTxn()
defer txn.Discard(ctx)

// 读取
resp, err := txn.Query(ctx, `{ query(func: eq(name, "Alice")) { uid } }`)

// 写入
_, err = txn.Mutate(ctx, &api.Mutation{
  SetNquads: []byte(`<_:x> <name> "Bob" .
                      <_：x> <age> "25" .`),
})
txn.Commit(ctx)
```

## 要点总结

- Badger LSM-Tree 优化写入
- DQL 类似 GraphQL 语法
- 乐观事务无锁
- 快照隔离保证一致性