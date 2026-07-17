## Why

当前 LeetCode 代码组织混乱，39 道题目平铺在单一目录中，缺少按数据结构和算法分类的逻辑组织。同时，`src/self_made/` 和 `src/algo/ds/` 存在大量数据结构实现代码，结构不清晰，难以学习和查阅。需要统一重组，建立清晰的知识体系，便于刷题学习和面试准备。

## What Changes

1. **创建 `src/ds/` 目录**：将 `src/self_made/` 和 `src/algo/ds/` 中的数据结构实现迁移重组
2. **重组 `src/algo/` 目录**：按算法类型（排序、二分、滑动窗口等）组织，补充 `problems/` 目录
3. **重组 `leetcode/` 目录**：将题目按所属数据结构和算法分类到对应的 `problems/` 目录
4. **重组 `include/` 目录**：与 `src/` 结构镜像，创建 `include/ds/` 和 `include/algo/`
5. **重组 `test/` 目录**：与 `src/` 结构镜像，创建 `test/ds/` 和 `test/algo/`
6. **创建 README.md 文档**：每个数据结构和算法目录包含详细的 Mermaid 可视化文档
7. **更新 CMakeLists.txt**：同步更新构建配置
8. **删除过渡目录**：重组完成后删除 `src/self_made/`、`src/algo/ds/`

## Capabilities

### New Capabilities

- `ds-array`: 数组数据结构实现与文档
- `ds-linked-list`: 链表数据结构实现与文档
- `ds-stack`: 栈数据结构实现与文档
- `ds-queue`: 队列数据结构实现与文档
- `ds-tree`: 树数据结构实现与文档（含二叉树、BST、AVL、红黑树、B树、堆、Trie）
- `ds-hash`: 哈希表数据结构实现与文档
- `ds-graph`: 图数据结构实现与文档
- `ds-string`: 字符串数据结构实现与文档
- `ds-bit`: 位操作数据结构实现与文档
- `ds-segment-tree`: 线段树数据结构实现与文档
- `ds-fenwick-tree`: 树状数组数据结构实现与文档
- `ds-bitmap`: 位图数据结构实现与文档
- `ds-cache`: 缓存数据结构实现与文档（LRU/LFU）
- `algo-sorting`: 排序算法实现与文档
- `algo-binary-search`: 二分查找算法实现与文档
- `algo-sliding-window`: 滑动窗口算法实现与文档
- `algo-dp`: 动态规划算法实现与文档
- `algo-greedy`: 贪心算法实现与文档
- `algo-backtrack`: 回溯算法实现与文档
- `algo-prefix-sum`: 前缀和算法实现与文档
- `algo-two-pointers`: 双指针算法实现与文档

### Modified Capabilities

<!-- 无现有规格需要修改，所有规格均为新建 -->

## Impact

### 受影响代码

| 原始位置 | 目标位置 | 说明 |
|---------|---------|------|
| `src/self_made/` | `src/ds/<分类>/impl/` | 迁移数据结构实现 |
| `src/algo/ds/` | `src/ds/<分类>/impl/` | 迁移数据结构实现 |
| `src/algo/sort/` | `src/algo/sorting/impl/` | 迁移排序算法 |
| `src/algo/binary_search/` | `src/algo/binary_search/impl/` | 迁移二分查找 |
| `leetcode/*/` | `src/ds/<分类>/problems/` | 按题号分类迁移 |
| `include/algo/` | `include/ds/` + `include/algo/` | 重组头文件 |
| `test/self_made/` | `test/ds/<分类>/` | 重组测试 |

### 新增文件

- 每个 `src/ds/` 和 `src/algo/` 子目录的 `README.md`
- 每个题目的 `problems/<题号>/README.md`

### 构建系统

- 更新 `src/CMakeLists.txt` 及各子目录 CMakeLists.txt
- 确保构建路径更新后仍能正常编译
