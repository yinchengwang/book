# tree 学习笔记

## 概念地图

树是非线性数据结构的基石——BTree、LSM Tree、HNSW Graph、AST、DOM 全是树的变体：

- **二叉树遍历**：前序（根→左→右）用于序列化/反序列化；中序（左→根→右）BST 得有序序列；后序（左→右→根）用于"先处理子节点再处理父节点"的场景（如删除整个树）；层序（BFS）用于最短路径、打印树形
- **二叉搜索树（BST）**：`left < root < right`。平均 O(log n)，但退化到链表时 O(n)。删除节点分三种情况：无子节点（直接删）、一个子节点（子节点替代）、两个子节点（找后继/前驱替代）
- **二叉堆**：完全二叉树，数组存储。最小堆 `parent ≤ children`。push=尾部插入+sift-up，pop=取堆顶+末尾移到堆顶+sift-down。堆排序和优先级队列的基础
- **堆与栈的关系**：堆（heap）数据结构 ≠ 堆（heap）内存。本卡讲的是数据结构堆

与 `ds_basic` 的关系：树的链表实现（left/right 指针）是链表结构的递归扩展。与 `stack_queue` 的关系：层序遍历用队列，前/中/后序的递归本质是栈

## 踩坑记录

1. **BST 删除双子节点**：后继一定是"右子树的最小节点"（一路往左走到底）——不是随便一个右子树节点。如果找错后继会破坏 BST 性质
2. **堆的 sift-down 边界**：`left < size` 和 `right < size` 都要检查——堆的最后几个节点可能没有右子节点。少检查一个导致数组越界
3. **堆 pop 后的 sift-down**：必须判断 `size > 0` 再做——pop 最后一个元素后堆为空，sift-down 会越界
4. **递归深度的隐形成本**：二叉树的递归遍历在深度很大时可能栈溢出——生产代码中用迭代+显式栈替代递归。本 demo 的树只有 3 层，递归安全

## 工程对照（≥100 字硬约束）

树在工程侧的直接复用点：

1. **BTree 索引**：`engineering/src/db/index/btree/btreeam.c` 中的 BTree 是 BST 的多路平衡扩展——每个节点含 m 个 key 和 m+1 个子指针。本卡 BST 的 insert/search/delete 三件套是理解 BTree 分裂/合并的前置。BTree 的叶子层序遍历用队列——本卡的 levelorder 是原型
2. **HNSW 图结构**：`rag/src/rag/index/hnsw_index.cpp` 中 HNSW 的层次图本质是"每层一个近似 k-NN 图"——图的构建用贪心搜索（类似 BST search 的过程），候选集维护用优先级队列（本卡的堆）。本卡树遍历是读懂 HNSW 递归插入的入口
3. **SQL AST**：`engineering/src/db/sql/sql_parser.c` 中 SQL 解析后的 AST 是树——SelectStmt→FromClause→WhereClause→...。AST 的前序遍历用于语义分析、后序遍历用于代码生成
4. **二叉堆在排序中的应用**：`engineering/src/algo-prod/` 中 Top-K 查询用最小堆维护"当前最大的 K 个"——堆大小固定 K，新元素比堆顶大就 pop+push。本卡的 heap_pop/heap_push 是最小实现

学完本卡后能动手的事：在 `engineering/src/db/index/btree/` 下加一个 `btree_walker.c`，用队列实现 BTree 的层序遍历——验证 BTree 所有叶子在同一层。本卡的 levelorder 是这功能的最小原型
