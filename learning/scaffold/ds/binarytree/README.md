# Binary Tree Scaffold

二叉树基础算法练习。

## 目录结构

```
binarytree/
├── main.c     # 演示代码
├── Makefile   # 编译脚本
├── README.md  # 本文件
└── NOTES.md   # 工程对照说明
```

## 功能清单

- 二叉树节点创建与销毁
- 前序遍历（Preorder）：根 -> 左 -> 右
- 中序遍历（Inorder）：左 -> 根 -> 右
- 后序遍历（Postorder）：左 -> 右 -> 根
- 节点计数、叶子计数、树高计算

## 编译运行

```bash
make        # 编译
make run    # 编译并运行
make clean  # 清理
```

## 示例输出

```
===== 二叉树遍历演示 =====

前序遍历 (Preorder): 1 2 4 5 3 6
中序遍历 (Inorder):  4 2 5 1 3 6
后序遍历 (Postorder): 4 5 2 6 3 1

===== 统计信息 =====
节点总数:   6
叶子节点:   3
树的高度:   3
```

## 树结构

```
        1
       / \
      2   3
     / \   \
    4   5   6
```
