# compaction - Compaction 与空间回收

## 策略概述

本模块演示 LSM-Tree 的两种核心 Compaction 策略。

### Leveled Compaction（层级压缩）

- L0 层接受所有新写入
- 每层只有一个 SSTable 文件
- 向下合并时读取一层的全部数据
- **特点**：低空间放大，写放大较高

### Tiered Compaction（分层压缩）

- 每层可以有多个 SSTable 文件
- 只有当层满时才与下一层合并
- 合并时读取整个层的所有文件
- **特点**：低写放大，空间放大较高

## 编译运行

```bash
make && ./test
```

## 输出示例

```
[compaction] ===== Leveled Compaction 模拟 =====
[compaction] Write #1: Compaction L0 -> L1 (读 5 MB, 写 5 MB)
...
[compaction] Leveled 统计结果:
[compaction]   写放大: 10.50x
[compaction]   空间放大: 1.11x
```
