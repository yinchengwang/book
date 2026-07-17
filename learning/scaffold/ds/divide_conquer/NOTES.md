# 分治算法 (Divide and Conquer)

## 核心思想

将原问题分解为若干个规模较小的相同问题，分别求解后合并。

## 经典算法

| 算法 | 时间复杂度 | 说明 |
|------|-----------|------|
| 归并排序 | O(N log N) | 稳定排序 |
| 快速排序 | O(N log N) | 原位排序 |
| Karatsuba | O(N^1.585) | 大整数乘法 |
| 最近点对 | O(N log N) | 二维平面 |

## 分治步骤

1. **分解**：将问题划分为子问题
2. **求解**：递归解决子问题
3. **合并**：合并子问题的解

## 工程对照

### 归并排序
- Java `Arrays.sort()` 底层实现
- 数据库外排序（外部归并排序）

### Karatsuba
- 大整数运算库（GMP）
- 多精度算术

### 并行计算
- MapReduce 范式类似分治
- fork-join 并行框架

## 参考

- `learning/scaffold/ds/sort_basic/` — 基础排序
- `learning/scaffold/ds/sort_advanced_ds/` — 高级排序
