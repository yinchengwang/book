# tree scaffold

树三件套：二叉树四种遍历、BST 增删查、二叉堆 push/pop。

## 复现命令

```bash
cd learning/scaffold/tree
gcc -Wall -Wextra -O2 -std=c11 -o tree_demo main.c
./tree_demo
```

## 预期输出

```
=== 二叉树遍历 ===
[pre]  1 2 4 5 3 6 7
[in]   4 2 5 1 6 3 7
[post] 4 5 2 6 7 3 1
[level]1 2 3 4 5 6 7

=== 二叉搜索树 ===
[bst] 中序: 2 3 4 5 6 7 8 (应升序)
[bst] search(4): yes
[bst] search(9): no
[bst] delete(3)
[bst] 中序: 2 4 5 6 7 8 (3已删除)

=== 二叉堆（最小堆）===
[heap] push(5) → [heap] size=1 | 5
[heap] push(3) → [heap] size=2 | 3 5
[heap] push(8) → [heap] size=3 | 3 5 8
[heap] push(1) → [heap] size=4 | 1 3 8 5
[heap] push(2) → [heap] size=5 | 1 2 8 5 3
[heap] push(7) → [heap] size=6 | 1 2 7 5 3 8
[heap] pop 序列: 1 2 3 5 7 8 (应升序)

=== PASS ===
```

## 关键点

- 前序（根左右）用于序列化/反序列化树；中序（左根右）BST 输出有序序列
- 层序遍历用队列——BFS 的本质
- BST 删除三个子节点：找到右子树最小节点（后继）替换，然后递归删除后继
- 二叉堆用数组存储，父子索引 `parent=(i-1)/2`, `left=2i+1`

详见 NOTES.md 工程对照段。
