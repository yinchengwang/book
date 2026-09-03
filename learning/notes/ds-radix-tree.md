# Radix Tree 前缀树

## 概述

Radix Tree（基数树/压缩前缀树）是一种**前缀压缩**的字典树变体，用于高效存储和查找字符串键。

## 设计思想

普通字典树：每个字符一个节点，空间开销大
```
foo → f → o → o
foobar → f → o → o → b → a → r
```

Radix Tree：相同前缀合并
```
foo → f → o → o → (分支)
         ├── "bar"
         └── "baz"
```

## 数据结构

```c
typedef struct radix_node {
    char *prefix;           // 压缩的前缀
    uint32_t prefix_len;    // 前缀长度
    bool is_end;            // 是否是 key 结尾
    void *value;            // 关联值
    struct radix_node **children;  // 子节点（256 个 ASCII）
    uint8_t child_count;    // 子节点数量
} radix_node_t;
```

## 核心操作

### 插入
1. 遍历树，匹配尽可能长的前缀
2. 若前缀完全匹配：
   - 无后续字符 → 设置 is_end
   - 有后续字符 → 继续向下或创建新节点
3. 若前缀部分匹配 → **分裂节点**

### 分裂操作
```
插入 "foobar" 时遇到节点 "foo"：
  "foo" 分裂为 "fo" + "o"
         ├── "bar" (新 key)
         └── "o" → "obar" (旧 key 继续)
```

## 操作复杂度

| 操作 | 时间复杂度 | 说明 |
|------|------------|------|
| 精确查找 | O(k) | k 为 key 长度 |
| 前缀搜索 | O(k + m) | k 为前缀长度，m 为匹配数 |
| 最长前缀匹配 | O(k) | k 为 key 长度 |

## 应用场景

| 系统 | 用途 |
|------|------|
| IP 路由 | 路由表前缀匹配 |
| Redis | SDS（简单动态字符串）内部使用 |
| 内核 | 路由缓存、进程 PID 查找 |

## 代码实现

项目路径：`src/index/tree/radix_tree/`

核心模块：
- `radix_tree_core.c` - 创建、销毁、节点管理
- `radix_tree_insert.c` - 插入（含分裂）
- `radix_tree_delete.c` - 删除
- `radix_tree_lookup.c` - 查找、前缀搜索、最长前缀匹配

## 与普通 Trie 的对比

| 维度 | Radix Tree | 普通 Trie |
|------|------------|-----------|
| 空间效率 | 高（前缀压缩） | 低 |
| 节点数 | O(n) | O(n*k) |
| 插入复杂度 | O(k) + 可能分裂 | O(k) |

## 面试要点

1. **Radix Tree 与 Trie 的核心区别？** - 前缀压缩，减少空间
2. **什么时候需要分裂节点？** - 插入时前缀部分匹配
3. **Radix Tree 的时间复杂度？** - O(k)，k 为 key 长度
4. **Redis 中 Radix Tree 的应用？** - SDS 字符串实现，HyperLogLog

## 参考资料

- Linux 内核源码：lib/radix-tree.c
- Redis 源码：server.c (SDS), hyperloglog.c