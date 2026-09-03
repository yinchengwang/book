# vector_basic - 向量基础原理

## 概述

向量是向量数据库和机器学习中的核心数据类型。本模块介绍向量的基本概念和距离计算方法。

## 向量表示

向量在内存中以连续数组形式存储：

```
Vector 结构体:
  - data: float* 指向数据数组
  - dim:  int    向量维度

内存布局: [v[0], v[1], v[2], ..., v[dim-1]]
```

## 距离度量

### 欧氏距离（L2 距离）

两点之间的直线距离：

```
L2(a, b) = sqrt(sum((ai - bi)^2))
```

### 余弦距离

衡量两个向量方向的差异：

```
cosine_dist(a, b) = 1 - (a·b) / (|a| * |b|)
                  = 1 - cosine_similarity
```

### 点积（内积）

向量对应元素相乘后求和：

```
dot(a, b) = sum(ai * bi)
```

## 向量归一化

将向量缩放为单位长度（模长为1）：

```
v_normalized = v / |v|
```

**归一化的意义：**
- 简化距离计算
- 余弦距离等于 1 - 点积
- 提高数值稳定性

## 编译运行

```bash
make && ./test
```

## 参考实现

参考 `engineering/src/algo-prod/distance/distance.c`

该文件实现了：
- L2 平方距离（`distance_l2sqr`）
- 余弦距离（`distance_cosine`）
- 内积（`distance_inner_product`）
- SIMD 优化（NEON/AVX/SSE）
- 批量计算（batch4）

注意：`engineering/src/db/vector/` 目录不存在，距离计算相关代码位于 `algo-prod/distance/` 模块中。
