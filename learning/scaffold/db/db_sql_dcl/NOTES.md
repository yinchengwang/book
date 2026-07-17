# SQL DCL 工程对照笔记

## 学习轨与工程轨的对应关系

本文档对比学习轨的 SQL DCL 演示代码与工程轨 `engineering/src/db/sql/` 中的实际实现。

---

## 1. 事务状态机

### 学习轨实现
```c
typedef enum {
    TXN_IDLE,           /* 空闲状态，无活跃事务 */
    TXN_ACTIVE,         /* 事务活跃中 */
    TXN_PARTIAL_ROLLBACK /* 部分回滚到 Savepoint */
} TxnState;
```

### 工程轨实现（对应文件）
- `engineering/src/db/sql/sql_exec.c` — 执行器层事务状态管理
- `engineering/src/db/sql/transaction.c` — 事务子系统核心实现

工程轨使用更精细的状态机，涵盖：
- `TRANS_DEFAULT` — 默认状态
- `TRANS_STARTED` — 已开始
- `TRANS_COMMIT` / `TRANS_ABORT` — 提交/中止
- 保存点管理通过 `PortalDrop` 和 `AbortedSubTransaction` 实现

---

## 2. Savepoint 实现

### 学习轨实现
使用链表存储 Savepoint：
```c
typedef struct Savepoint {
    char name[64];
    int statement_count;
    struct Savepoint *next;
} Savepoint;
```

### 工程轨实现
工程轨的 Savepoint 管理更加复杂：
- 使用 `SubTransactionId` 进行层级管理
- 支持命名和非命名 Savepoint
- 通过 `PushTransaction()` / `PopTransaction()` 管理事务栈
- 参考：`src/backend/access/transam/xact.c`（PostgreSQL 风格的两阶段提交）

---

## 3. Autocommit 机制

### 学习轨实现
```c
if (txn->autocommit) {
    txn_begin(txn);
}
```
简单的每语句自动提交。

### 工程轨实现
- `engineering/include/db/guc.h` 中的 `AutoCommit` 参数
- 通过 `guc.c` 配置系统管理
- 支持会话级和全局级配置
- DDL 语句自动提交行为（`CalciteExecutor` 中处理）

---

## 4. 实际 SQL 执行流程

```
用户 SQL
   ↓
CalciteExecutor::execute()      ← 解析 + 重写 + 优化
   ↓
TxnBeginStmt / TxnCommitStmt   ← 事务控制语句识别
   ↓
PortalRun()                     ← 执行器驱动
   ↓
TransactionId GetCurrentTransactionId()
```

---

## 5. 关键学习点

| 概念 | 学习轨演示 | 工程轨实现 |
|------|-----------|-----------|
| 事务状态 | 简化枚举 | TRANS_xxx 状态机 |
| Savepoint | 链表模拟 | SubTransactionId 栈 |
| Autocommit | 布尔开关 | GUC 参数 + 规则系统 |
| 语句计数 | 简单计数器 | 语句序列号 + XID |

---

## 6. 扩展阅读

- PostgreSQL 事务系统：`src/backend/access/transam/xact.c`
- Savepoint 语义：SQL Standard SQL/Foundation 4.33
- 两阶段提交协议：用于分布式事务（engineering/src/db/dist_txn.c）

---

## 7. 相关工程轨文件

```
engineering/src/db/sql/
├── sql_exec.c          # SQL 执行器入口
├── txn_stmt.c          # 事务控制语句处理（BEGIN/COMMIT/ROLLBACK/SAVEPOINT）
├── CalciteExecutor.cpp # Calcite 查询引擎集成
└── (其他执行器文件)

engineering/include/db/
├── sql_exec.h          # 执行器公共接口
├── transaction.h       # 事务子系统接口
└── guc.h               # GUC 配置参数（AutoCommit 等）
```
