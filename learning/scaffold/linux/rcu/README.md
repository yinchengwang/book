# rcu — RCU 无锁同步机制

## 简介

演示 Linux 内核 RCU（Read-Copy-Update）机制原理，展示读侧无锁和写侧宽限期的设计思想。

## 编译

```bash
gcc -std=c11 -Wall -o rcu main.c -lpthread
```

## 运行

```bash
./rcu
```

## 核心概念

### RCU 三步走

1. **Read**：读者无需锁，直接访问共享数据
2. **Copy**：写者拷贝数据，在新副本上修改
3. **Update**：原子更新指针，等待宽限期后回收旧数据

### 适用场景

- 读多写少（比例 >1000:1）
- 读操作延迟敏感
- 写操作可以容忍延迟

## 关键 API

- `rcu_read_lock()` / `rcu_read_unlock()` — 读侧
- `synchronize_rcu()` — 等待宽限期
- `rcu_assign_pointer()` — 更新指针
- `rcu_dereference()` — 读取指针

## 扩展

- 阅读内核文档 `Documentation/RCU/whatisRCU.rst`
- 了解用户空间 liburcu 库
