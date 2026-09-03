# sort_advanced scaffold

四种 O(n log n) 排序：快排（三数取中）、归并排序、堆排序 + 线性时间计数排序。

## 复现命令

```bash
cd learning/scaffold/sort_advanced
gcc -Wall -Wextra -O2 -std=c11 -o sortadv_demo main.c
./sortadv_demo
```

## 关键点

- **快排**：每轮 partition 选 pivot，左边 ≤ pivot ≤ 右边。三数取中避免退化。小数组回退插入排序（Introsort 核心）
- **归并**：分治到单个元素，merge 时比较+复制。稳定，但需 O(n) 辅助数组。外部排序的基础（多路归并）
- **堆排**：建最大堆 O(n)，取堆顶 O(log n)。原地，不稳定。Linux 内核排序用堆排（最坏可控）
- **计数排序**：O(n+k)，非比较排序。适合范围有限的整数。基数排序的前置——按每位分别计数排

详见 NOTES.md 工程对照段。
