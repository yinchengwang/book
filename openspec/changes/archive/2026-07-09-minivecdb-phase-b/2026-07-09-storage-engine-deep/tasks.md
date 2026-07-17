# B1: 存储引擎深度 - 任务清单

## 任务概述

完善存储引擎核心功能：MVCC 隔离级别、完整检查点、死锁检测、事务回滚。

## 当前状态

### 已完成

- [x] **thread_local 兼容性修复**
  - 在 `include/db/storage/txn/mvcc.h` 中添加 Windows 兼容性宏
  - 使用 `__declspec(thread)` 替代 `thread_local` 关键字
  - 创建 `thread_local_compat.h` 兼容层

### 待修复

- [ ] **A.1** 修复 mvcc.h 前向声明问题 (TxnContext)
- [ ] **A.2** 编译存储引擎模块
- [ ] **B.1** 实现检查点触发条件判断
- [ ] **B.2** 实现检查点数据收集
- [ ] **C.1** 实现等待图构建
- [ ] **D.1** 实现 UNDO 日志记录

## 问题分析

现有存储引擎存在以下编译问题：

1. **前向声明冲突**: `mvcc.h` 中引用 `TxnContext` 但未包含其定义
2. **隐式函数声明**: `is_active_in_readview` 等函数未声明
3. **Windows 兼容性**: `thread_local` 关键字

## 建议方案

1. 重构 txn 模块的依赖关系
2. 创建统一的头文件包含
3. 使用 pthread 或 Windows TLS API

## 状态

- 创建时间: 2026-07-09
- 状态: 部分完成 (thread_local 修复)
- 阻塞: 依赖 txn 模块重构
- 所属大变更: 2026-07-09-minivecdb-end-to-end
