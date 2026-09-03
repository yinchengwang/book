# numpy 学习笔记

## 概念地图

NumPy 是 Python 科学计算的基石，核心是 ndarray：

- **ndarray**：N 维数组，同质（所有元素类型相同）
- **向量化**：避免 Python 循环，C 级性能
- **广播**：不同形状数组自动扩展为相同形状
- **ufunc**：通用函数，逐元素运算

## 踩坑记录

1. **广播规则**：从右向左比较维度，必须有一个为 1 或相等
2. **视图 vs 副本**：切片返回视图，修改影响原数组；copy() 返回副本
3. **dtype 精度**：注意 float32 vs float64 的精度差异
4. **矩阵乘法**：`@` 是矩阵乘法，`*` 是逐元素乘法

## 工程对照（≥100 字硬约束）

### 1. 科学计算场景

NumPy 在向量索引和数据库引擎中的应用：

```python
# 向量检索库中的距离计算
# faiss 使用 numpy 进行批量计算
import numpy as np

def cosine_similarity(a, b):
    return np.dot(a, b) / (np.linalg.norm(a) * np.linalg.norm(b))
```

### 2. 性能关键路径

NumPy 向量化 vs Python 循环：

```python
# Python 循环（慢）
result = [x * 2 + 1 for x in data]

# NumPy 向量化（快 10-100X）
result = np.array(data) * 2 + 1
```

### 3. 与项目 C 代码的关系

本工程 `engineering/src/algo/` 中的 SIMD 距离计算与 NumPy 向量化理念一致：

| 实现 | 语言 | 优势 |
|------|------|------|
| NumPy | Python + C | 快速开发 |
| SIMD | C | 极致性能 |

学完本卡能动手的事：用 NumPy 替代 Python 循环处理数值计算任务。
