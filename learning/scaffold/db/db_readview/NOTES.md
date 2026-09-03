# ReadView 机制 - 工程对照笔记

## 与工程代码的映射关系

本文档记录 learning/scaffold/db/db_readview 中的概念与 engineering/src/db/concurrency/ 下实际实现的对照关系。

### 工程源文件

| 文件 | 作用 |
|------|------|
| `engineering/src/db/concurrency/mvcc_snapshot.c` | ReadView 快照的创建与管理 |
| `engineering/src/db/concurrency/mvcc_visibility.c` | 元组可见性判断逻辑 |

### 核心数据结构对照

**学习代码中的简化结构：**
```c
typedef struct ReadView {
    uint32_t xmin;
    uint32_t xmax;
    uint32_t xip_list[MAX_ACTIVE_XIDS];
    uint32_t xip_count;
} ReadView;
```

**工程实际结构（PostgreSQL 风格）：**
- 使用 `TransactionId` 而非 `uint32_t`
- `xip_list` 是动态数组，通过 `ProcArray` 管理
- 包含 `xcnt`（subxact 计数）、`subxip_list`（子事务列表）

### 关键函数对照

| 学习代码 | 工程代码 | 说明 |
|----------|----------|------|
| `xid_in_mvcc_snapshot()` | `XidInMVCCSnapshot()` | 事务 ID 可见性判断 |
| `tuple_visible()` | `HeapTupleSatisfiesMVCC()` | 元组版本可见性判断 |
| `readview_create()` | `GetTransactionSnapshot()` | 获取事务快照 |

### 可见性算法细节

学习代码实现的简化规则：
1. `xid < xmin` → 可见
2. `xid >= xmax` → 不可见
3. `xid in xip_list` → 不可见

工程代码还有额外考虑：
- **子事务处理**：`subxip_list` 处理嵌套事务
- **子事务传播**：父事务可见时子事务也可见
- **clog 访问**：需要访问提交日志判断事务状态
- **xmax = RecentGlobalXmax**：处理全局最大 XID

### 编译工程中的 MVCC 模块

```bash
# 查看工程源码
ls engineering/src/db/concurrency/

# 编译测试（需要先构建工程）
cd build/engineering
make mvcc_snapshot_test
```

### 进一步学习

1. 阅读 `engineering/src/db/concurrency/mvcc_snapshot.c` 中 `GetTransactionSnapshot()` 的实现
2. 研究 `mvcc_visibility.c` 中 `HeapTupleSatisfiesMVCC()` 的完整逻辑
3. 理解 ProcArray 如何追踪活动事务
4. 了解 VACUUM 如何清理过期版本
