# heap_storage -- Heap 堆表存储

## 概述

Heap 堆表是 PostgreSQL 风格存储引擎的核心访问方法，负责管理表数据的物理存储。

## 页面结构

```
+------------------+ <- 0
|   PageHeader     |  24 字节
+------------------+ <- pd_lower (空闲空间起始)
|  LinePointer[0]  |   6 字节，元组指针
|  LinePointer[1]  |   6 字节
|      ...         |
+------------------+ <- pd_upper (空闲空间结束)
|                  |
|     Tuple Data    |  元组数据（从后向前增长）
|     (grows up)    |
|                  |
+------------------+ <- HEAP_PAGE_SIZE
```

### PageHeader

| 字段 | 大小 | 说明 |
|------|------|------|
| pd_lsn | 4B | 最后修改的日志序列号 |
| pd_checksum | 2B | 页面校验和 |
| pd_flags | 2B | 页面标志 |
| pd_lower | 2B | LinePointer 数组起始位置 |
| pd_upper | 2B | 空闲空间结束位置 |
| pd_special | 2B | 特殊空间起始位置 |
| pd_pagesize_version | 2B | 页面大小和版本 |
| pd_prune_xid | 4B | 可清理的最早事务 ID |
| pd_xid_base | 2B | 事务 ID 基准 |
| pd_multi_base | 2B | 多事务 ID 基准 |

### LinePointer

| 字段 | 大小 | 说明 |
|------|------|------|
| t_off | 4B | 元组数据在页面中的偏移 |
| t_flags | 1B | 标志位 |
| t_xmax | 1B | 删除事务 ID（非0表示已删除） |

## TID（元组标识符）

TID 用于唯一标识页面中的元组：
- `ip_blkno`: 块号（页面号）
- `ip_offset`: LinePointer 在页面中的偏移

## 元组组织

1. **插入**：元组数据从 `pd_upper` 向低地址增长，LinePointer 从 `pd_lower` 向高地址增长
2. **删除**：通过设置 `t_xmax != 0` 打墓碑，不立即清理空间
3. **定位**：通过 LinePointer 数组找到元组数据位置

## 编译运行

```bash
make && ./test
```

## 输出格式

使用 `[heap]` 前缀标记堆表输出，便于与其他组件区分。
