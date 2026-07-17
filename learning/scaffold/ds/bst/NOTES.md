# BST 工程对照说明

## 与 engineering/src/db/storage/access/btree/btreeam.c 的联系

`engineering/src/db/storage/access/btree/btreeam.c` 实现了数据库 BTree 访问方法，其中许多设计思想与 BST 一脉相承：

### 1. 键比较与查找

BST 的 `find()` 函数通过键比较决定向左还是向右搜索。btreeam.c 中的 `bt_page_get_item()` 同样基于键比较进行页面内查找，只是 BTree 节点存储在固定大小的页面上（而非动态分配的树节点），并且采用二分查找定位 item。

```c
// BST 查找逻辑（main.c）
while (root && key != root->key) {
    root = (key < root->key) ? root->left : root->right;
}

// BTree 页面内查找（btreeam.c 简化版）
for (int i = 0; i < count; i++) {
    // 按索引顺序比较键值
}
```

### 2. 页面初始化

BST 节点通过 `create_node()` 动态分配，而 BTree 页面通过 `bt_page_init()` 初始化。两者都需要设置元数据：

| BST 节点 | BTree 页面 |
|---------|-----------|
| key | 键值数据 |
| left/right 指针 | btpo_prev/btpo_next（页面链表）|
| - | btpo_level（层级），btpo_flags（叶子/根标志）|

### 3. 删除操作的后继查找

BST 删除双子树节点时，需要找后继（左子树最大值或右子树最小值）。btreeam.c 在构建 BTree 时也需要处理页面分裂，这涉及到键的重新分配和定位，与 BST 的后继查找逻辑本质相同。

### 4. 平衡因子与 BTree 填充因子

BST 的平衡因子（balance_factor）检查树是否失衡。类似地，BTree 通过 `bt_page_get_free_space()` 监控页面空闲空间，当页面过满时触发分裂，保证 BTree 维持平衡特性。

### 5. 总结

BST 是理解 BTree 的基础。两者都满足"左小右大"的搜索树性质，只是 BTree 将这一思想扩展到了固定大小页面、多路分支的磁盘存储场景，更适合数据库索引的高效读写。
