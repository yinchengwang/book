# ACID 特性学习卡

## 概述

本卡片演示数据库事务的四个核心特性（ACID），通过银行转账示例展示：
- 原子性：Undo Log 回滚机制
- 一致性：约束检查验证
- 隔离性：并发控制概述
- 持久性：WAL 原则

## 编译与运行

```bash
# 编译（默认）
gcc -std=c11 -o acid main.c

# 运行
./acid

# 编译（模拟故障测试原子性）
gcc -std=c11 -DSIMULATE_FAILURE -o acid_failure main.c
./acid_failure  # 观察回滚行为
```

## 学习要点

1. **原子性实现**：通过 Undo Log 记录修改前状态，失败时回滚
2. **一致性检查**：余额约束、账户存在性验证
3. **隔离性问题**：脏读、不可重复读、幻读
4. **持久性保证**：WAL（Write-Ahead Logging）原则

## 工程对照

见 [NOTES.md](NOTES.md) 中 ACID 在存储引擎的实现分析。