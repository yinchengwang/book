# 回溯算法 (Backtracking)

## 测试数据

- N 皇后: 4 皇后（2 个解）
- 全排列: 数组 [1,2,3,4]
- 子集生成: 数组 [1,2,3]

## 预期输出

```
[N 皇后] 解 1: 1 3 0 2
         解 2: 2 0 3 1
  共 2 个解

[全排列] 1: 1 2 3 4
         2: 1 2 4 3
         ...

[子集生成] { } { 1 } { 2 } { 1 2 } ...
```

## 运行

```bash
gcc -std=c11 -Wall -Wextra -O2 -o backtrack_demo main.c && ./backtrack_demo
```
