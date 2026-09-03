# buffer_pool - Buffer Pool 缓存管理

## 核心算法

- **Hash 表**：`page_id % hash_size` 映射页面到 Buffer frame
- **Clock-Sweep**：近似 LRU 置换算法
  - 跳过 refcount > 0 的 Buffer（被 pin 住）
  - 递减 usage_count > 0 的 Buffer
  - 置换 usage_count == 0 的 Buffer
- **脏页管理**：pin/unpin + 标记脏 + 刷盘

## 数据结构

| 结构 | 说明 |
|------|------|
| `BufferDesc` | Buffer 描述符（状态、使用计数、引用计数） |
| `BufferPool` | 整个缓存池（descriptors + pages + hash_table） |

## 状态标志

| 标志 | 说明 |
|------|------|
| `BM_VALID` | Buffer 有效（已加载页面） |
| `BM_DIRTY` | Buffer 脏（已修改，需刷盘） |
| `BM_PINNED` | Buffer 被 pin 住（不可置换） |

## 编译运行

```bash
make && ./test
```

## 输出示例

```
=== Buffer Pool 演示程序 ===
[buffer] Buffer Pool 初始化完成: 4 个 Buffer

--- 演示 1: Hash 表查找 ---
[buffer] Hash 未命中: page_id=100
[buffer] Clock: 选中 Buffer 0 进行置换
[buffer] 加载页面: page_id=100 -> Buffer 0
[buffer] Unpin Buffer 0 (refcount=0)
[buffer] Hash 命中: page_id=100 -> Buffer 0
[buffer] Unpin Buffer 0 (refcount=0)

--- 演示 2: 脏页标记 ---
[buffer] Hash 未命中: page_id=200
...
```
