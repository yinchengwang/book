## MODIFIED: dict-trie-optimize

### 概述

优化 Trie 顶层（root 节点）的边存储方式：将 uthash 动态哈希替换为固定大小数组的直接索引，消除首层字符查找的哈希计算开销。

### 数据结构变更

```c
#define DICT_TRIE_ROOT_CHILDREN_SIZE 65536  // BMP 全覆盖，可编译期配置

typedef struct dict_node_t {
    bool is_word;
    int32_t freq;
    // 仅 root 节点（level 0）使用：
    struct dict_node_t **root_children;  // 65536 槽位指针数组，NULL = 无该边
    // level ≥ 1 的节点继续使用 uthash：
    dict_edge_t *edges;                  // uthash 句柄
} dict_node_t;
```

判定逻辑：`node->root_children != NULL` → 走数组索引；否则走 uthash。

### 内存开销

- 每个 root_children 槽位为 8 字节（64 位指针）
- 总计：65536 × 8 = 512 KB
- 只有 root 节点分配，其他节点不分配
- 相比当前 uthash 方案（每个 root 出边至少 40 字节的 uthash 内部开销），实际词库 > 12000 词时数组方案更省内存

### 替换范围

仅替换 root 节点的查找/插入路径。`dict_find_edge()` 和 `dict_get_or_create_edge()` 内部：

```c
if (node->root_children != NULL) {
    // 数组路径：O(1) 直接索引
    uint32_t idx = (uint32_t)codepoint;
    if (idx < DICT_TRIE_ROOT_CHILDREN_SIZE)
        return node->root_children[idx];
    return NULL;
}
// 否则：现有 uthash 路径
```

### 向后兼容性

以下外部行为完全不变：
- Trie 的遍历顺序（数组索引 0..65535 等效于按码点升序遍历）
- `dict_add_word()` 的语义和返回值
- `dict_cut()` 的分词结果不受影响（边的存在性判定逻辑与 uthash 等价）
- `dict_reset()` 释放所有 root_children 指向的子节点

### 约束

- 仅在 root 节点启用数组优化，子节点保持 uthash（避免指数级内存膨胀）
- `DICT_TRIE_ROOT_CHILDREN_SIZE` 可通过编译宏覆盖，默认 65536
- 若未来需要非 BMP 字符支持（码点 > U+FFFF），数组索引需映射或回退到 uthash
