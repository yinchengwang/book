# Build My DB - 自建数据库内核

## Why

通过亲手实现一个数据库内核，系统性地学习数据库的核心原理（存储引擎、索引、事务、查询优化）。相比阅读源码或理论学习，动手实现能更深刻地理解这些概念，同时产出的是一个实际可用的工具。

**为什么现在做：**
- 业余时间充裕，无截止日期压力
- 项目已有丰富的索引实现（ART、B+Tree、LSM 等），可直接复用
- 分布式索引代码提供了很好的架构参考

## What Changes

### 第1期：MVP（最小可行产品）

- **存储引擎**：基于磁盘文件的页面存储系统
  - 页面（Page）作为基本读写单元，默认 4KB 或 8KB
  - 缓存池（Buffer Pool）管理内存中的页面
  - 页面淘汰策略（LRU）
  - 脏页刷盘机制

- **索引系统**：复用项目已有实现
  - 以 ART（Adaptive Radix Tree）作为主索引
  - 支持范围查询和前缀扫描

- **KV API**：提供简洁的键值接口
  - `db_put(key, value)` - 插入/更新
  - `db_get(key)` - 单键查询
  - `db_delete(key)` - 删除
  - `db_scan(start_key, end_key)` - 范围扫描

- **文件布局**：清晰的磁盘文件结构
  - 数据文件：存储实际页面数据
  - 元数据文件：存储数据库头部信息

### 第2期：基本款

- **事务支持**：WAL（预写日志）
  - `db_begin()` - 事务开始
  - `db_commit()` - 事务提交
  - `db_rollback()` - 事务回滚
  - 崩溃恢复能力

- **简单查询层**
  - WHERE 条件解析（等于、大于、小于、BETWEEN、IN）
  - 简单聚合（COUNT、SUM、AVG、MIN、MAX）
  - GROUP BY 基本支持

- **基础 SQL 支持**
  - SELECT 语句（简单投影和筛选）
  - INSERT 语句
  - UPDATE 语句
  - DELETE 语句
  - CREATE TABLE（基本数据类型）

### 第3期：增强款

- **MVCC 并发控制**
  - 多版本快照
  - 读不阻塞写，写不阻塞读
  - 垃圾回收机制

- **多种索引支持**
  - B+Tree 索引
  - Hash 索引（等值查询优化）
  - Bitmap 索引（低基数列）

- **查询优化器**
  - 规则优化（RBO）
  - 代价优化（CBO）基础版
  - 索引选择
  - 执行计划可视化

### 第4期：完整款

- **分布式支持**
  - 主从复制
  - 分片策略
  - 简单共识机制

- **高级特性**
  - 约束检查（主键、唯一、外键）
  - 触发器基础
  - 视图

- **性能优化**
  - 查询编译（JIT）
  - 向量化和 SIMD
  - 并行查询

## Capabilities

### New Capabilities

- `storage-engine`: 磁盘文件存储引擎，包含页面管理、缓存池、刷盘策略
- `kv-api`: 键值 API 接口，封装底层存储细节
- `art-index`: 基于 Adaptive Radix Tree 的索引实现
- `transaction-wal`: 基于 WAL 的事务系统，支持 ACID
- `sql-parser`: SQL 解析器，将文本 SQL 转为抽象语法树
- `query-executor`: 查询执行引擎，遍历算子树执行查询
- `mvcc`: 多版本并发控制，支持快照隔离
- `btree-index`: B+Tree 索引实现
- `hash-index`: Hash 索引实现
- `bitmap-index`: Bitmap 索引实现
- `query-optimizer`: 查询优化器，支持 RBO 和基础 CBO
- `distributed-replication`: 主从复制和分片支持

### Modified Capabilities

<!-- 无修改现有能力 -->

## Impact

### 新增目录结构

```
src/db/
├── storage/          # 存储引擎
│   ├── page.c/h      # 页面管理
│   ├── buffer.c/h    # 缓存池
│   └── disk.c/h      # 磁盘文件操作
├── index/            # 索引系统
│   └── art.c/h       # 复用 src/index/tree/art/
├── api/              # KV API
│   └── kv.c/h        # 键值接口
├── transaction/      # 事务系统
│   └── wal.c/h       # WAL 实现
├── sql/              # SQL 处理
│   ├── parser.c/h    # 解析器
│   ├── planner.c/h   # 查询规划器
│   └── executor.c/h  # 执行器
├── concurrency/      # 并发控制
│   └── mvcc.c/h      # MVCC 实现
└── main.c            # 入口

include/db/           # 公共头文件
test/db/              # 测试代码
```

### 依赖关系

- 复用 `src/index/tree/art/` 作为主索引
- 复用 `src/algo/ds/` 中的数据结构
- 无外部依赖，纯 C/C++ 实现