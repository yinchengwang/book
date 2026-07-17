# sort_basic scaffold

三种基础排序：冒泡、选择、插入——每轮打印中间数组状态 + 统计比较/交换次数。

## 复现命令

```bash
cd learning/scaffold/sort_basic
gcc -Wall -Wextra -O2 -std=c11 -o sortbasic_demo main.c
./sortbasic_demo
```

## 预期输出

```
原始数组:       5 3 8 4 2 7 1 6

=== 冒泡排序 (Bubble Sort) ===
  [bubble pass 1] 3 5 4 2 7 1 6 8
  [bubble pass 2] 3 4 2 5 1 6 7 8
  [bubble pass 3] 3 2 4 1 5 6 7 8
  [bubble pass 4] 2 3 1 4 5 6 7 8
  [bubble pass 5] 2 1 3 4 5 6 7 8
  [bubble pass 6] 1 2 3 4 5 6 7 8
  [bubble pass 7] 1 2 3 4 5 6 7 8
最终:           1 2 3 4 5 6 7 8
比较 28 次, 交换 18 次 | 稳定: 是 | O(n^2)
...
```

## 关键点

- 冒泡每轮将最大值"浮"到末尾；加 swapped 标志可提前终止
- 选择每轮找到最小值放到前面；交换次数最少（O(n)），但不稳定
- 插入排序在"几乎有序"时接近 O(n)——Timsort 的基础
- 三种排序都 O(n^2) 时间，但常数因子不同：插入 < 选择 < 冒泡

详见 NOTES.md 工程对照段。
