# OPQ 算法原理

## 1. 算法概述

OPQ (Optimized Product Quantization，优化乘积量化) 是 PQ 的改进版本，由 Ge et al. 在 2013 年提出。通过学习最优旋转矩阵，使子空间划分更均衡，提升量化精度。

### 核心思想

在 PQ 的基础上增加**旋转优化**：
- **PQ 问题**：子空间划分固定，各子空间方差不均
- **OPQ 优化**：学习旋转矩阵 R，使旋转后的空间更适合均匀划分
- **目标**：最小化量化误差 `‖x - R·q(Rᵀx)‖`

### 适用场景

- 向量压缩存储（内存优化）
- 大规模近似最近邻搜索
- 与 IVF/HNSW 结合，加速距离计算
- 相似度过滤与重排序

### 优缺点

| 优点 | 缺点 |
|------|------|
| 压缩率高（32x） | 有损压缩，精度损失 |
| 距离计算快（ADC） | 需要训练数据 |
| 比 PQ 精度更高 | 参数选择敏感 |
| 支持批量处理 | 解码复杂度高 |

## 2. 数据结构

### 量化流程

```
原始向量 x [d 维]
     │
     ▼
┌─────────────────┐
│  旋转 Rᵀx       │  ← 学习得到的最优旋转
└────────┬────────┘
         │
         ▼
┌─────────────────────────────────────┐
│         子空间划分 (m 个)              │
│  x₁ [d/m]  x₂ [d/m]  ...  xₘ [d/m]  │
└───┬─────────┬─────────┬─────────┬───┘
    │         │         │         │
    ▼         ▼         ▼         ▼
  码本₁     码本₂     ...      码本ₘ
  (k 个)    (k 个)             (k 个)
    │         │         │         │
    ▼         ▼         ▼         ▼
  编码₁     编码₂     ...      编码ₘ
  (1 byte)  (1 byte)           (1 byte)
    │         │         │         │
    └─────────┴─────────┴─────────┘
              │
              ▼
        最终编码 [m bytes]
```

### 核心数据结构

```c
typedef struct opq_quantizer {
    int dims;           // 向量维度 d
    int m;              // 子空间数量
    int bits;           // 每子空间比特数（默认 8）
    int ks;             // 每子空间码本大小 = 2^bits
    int sub_dim;        // 子空间维度 = d / m

    float *codebooks;   // 码本 [m, ks, sub_dim]
    float *rotation;    // 旋转矩阵 [d, d]（优化后）
    float *centroids;   // 训练迭代中间质心

    /* Phase 2: 存储后端集成 */
    storage_backend_t *storage;
    heap_vector_store_t *heap_store;

    int trained;        // 是否已训练
} opq_quantizer_t;
```

### 旋转矩阵

OPQ 的关键创新：学习旋转矩阵 R

```c
// 原始向量
float x[dims];

// 旋转向量
float x_rotated[dims];
for (int i = 0; i < dims; i++) {
    x_rotated[i] = 0;
    for (int j = 0; j < dims; j++) {
        x_rotated[i] += rotation[i * dims + j] * x[j];
    }
}

// 划分子空间
for (int sub = 0; sub < m; sub++) {
    int start = sub * sub_dim;
    for (int j = 0; j < sub_dim; j++) {
        sub_vector[sub][j] = x_rotated[start + j];
    }
}
```

## 3. 核心算法

### 3.1 训练算法

OPQ 训练包含两部分：码本学习 + 旋转优化

```
Train(vectors, n, m, bits):
    1. 初始化
       - 随机初始化旋转矩阵 R (正交矩阵)
       - 划分子空间

    2. 迭代优化 (ITER 次):
       a) 旋转所有向量: x' = Rᵀx
       b) 对每个子空间做 K-Means:
          - 训练 k=2^bits 个质心
          - 得到码本 C₁, C₂, ..., Cₘ
       c) 更新旋转矩阵 R:
          - 目标: 最小化 ∑‖x - R·q(Rᵀx)‖²
          - 方法: Procrustes 分析
          - 解法: SVD 分解

    3. 返回码本和旋转矩阵
```

**Procrustes 分析**：

给定数据矩阵 X 和质心矩阵 Q，求解旋转 R：

```
min ||X - R·Q||_F
```

最优解：
```
R = argmin ||X - R·Q||_F
  = U·Vᵀ  (其中 X·Qᵀ = U·Σ·Vᵀ)
```

### 3.2 编码算法

```
Encode(x):
    1. 旋转向量
       x' = Rᵀx

    2. 对每个子空间:
       for sub = 1 to m:
           提取子向量 x'_sub
           在码本 C_sub 中找最近质心
           code[sub] = argmin_i ||x'_sub - C_sub[i]||

    3. 返回编码 [m bytes]
```

### 3.3 距离计算（ADC）

**Asymmetric Distance Computation (ADC)**：

查询向量不编码，只对数据库向量编码。

```
ADC_Distance(query, code):
    1. 旋转查询向量
       q' = Rᵀq

    2. 计算距离表
       for sub = 1 to m:
           for i = 1 to ks:
               table[sub][i] = ||q'_sub - C_sub[i]||²

    3. 查表求和
       dist = 0
       for sub = 1 to m:
           dist += table[sub][code[sub]]

    4. 返回距离
```

**优势**：
- 距离表只需计算一次（查询级）
- 数据库向量编码只需查表
- 复杂度：O(m·ks·sub_dim + m)

### 3.4 非对称距离优化

**SDC (Symmetric Distance Computation)**：查询和数据库都编码
- 精度更低
- 速度更快

**ADC (Asymmetric Distance Computation)**：只编码数据库
- 精度更高
- 推荐使用

| 方法 | 查询处理 | 精度 | 速度 |
|------|----------|------|------|
| ADC | 旋转+距离表 | 高 | 快 |
| SDC | 编码+查表 | 低 | 更快 |

## 4. 参数影响

### 参数选择

| 参数 | 作用 | 增大时影响 |
|------|------|-----------|
| `m` (子空间数) | 压缩率 | 压缩率↑ 精度↓ |
| `bits` (比特数) | 码本大小 | 精度↑ 内存↑ |
| `ks` (码本大小) | = 2^bits | 精度↑ 训练时间↑ |
| `sub_dim` (子空间维度) | = d / m | 维度均衡性 |

### 典型参数组合

| 向量维度 | m | bits | ks | 压缩率 | 相对误差 |
|----------|---|------|----|----|----------|
| 128 | 16 | 8 | 256 | 32x | ~10% |
| 256 | 32 | 8 | 256 | 32x | ~12% |
| 512 | 64 | 8 | 256 | 32x | ~15% |
| 960 | 96 | 8 | 256 | 40x | ~18% |

### 压缩率计算

```
原始向量: d × 4 bytes (float32)
编码向量: m × 1 byte (8-bit)
压缩率: d × 4 / m = 4d / m

示例 (d=128, m=16):
压缩率 = 128 × 4 / 16 = 32x
```

## 5. 时间/空间复杂度

### 训练复杂度

| 操作 | 时间复杂度 | 空间复杂度 |
|------|------------|------------|
| 旋转变换 | O(n·d²) | O(d²) |
| 子空间 K-Means | O(iter·n·ks·d/m) | O(m·ks·d/m) |
| Procrustes 更新 | O(n·d² + d³) | O(d²) |

**总体训练复杂度**：`O(iter·n·d² + iter·d³)`

### 编码复杂度

| 操作 | 时间复杂度 |
|------|------------|
| 旋转变换 | O(d²) |
| 子空间编码 | O(m·ks·d/m) = O(ks·d) |
| **总体** | **O(d² + ks·d)** |

### 距离计算复杂度

| 操作 | 时间复杂度 |
|------|------------|
| 旋转查询 | O(d²) |
| 计算距离表 | O(m·ks·d/m) = O(ks·d) |
| ADC 查表 | O(m) |
| **总体** | **O(d² + ks·d)** |

### 空间占用

```
码本大小: m × ks × (d/m) × 4 bytes
         = ks × d × 4 bytes

旋转矩阵: d × d × 4 bytes

距离表: m × ks × 4 bytes

编码向量: n × m bytes
```

**示例 (d=128, m=16, ks=256, n=1M)**：
- 码本: 256 × 128 × 4 = 128 KB
- 旋转矩阵: 128 × 128 × 4 = 64 KB
- 距离表: 16 × 256 × 4 = 16 KB
- 编码向量: 1M × 16 = 16 MB

## 6. 与其他量化方法对比

### OPQ vs PQ

| 特性 | PQ | OPQ |
|------|----|----|
| **旋转优化** | 无 | 有 |
| **子空间方差** | 不均衡 | 均衡 |
| **量化误差** | 高 | 低（~10% 降低） |
| **训练时间** | 快 | 慢（多旋转迭代） |
| **适用场景** | 均匀分布数据 | 任意分布数据 |

**OPQ 相对 PQ 的优势**：
- 旋转优化使子空间方差更均衡
- 对数据分布适应性更强
- 相同压缩率下精度更高

### OPQ vs LVQ

| 特性 | OPQ | LVQ |
|------|-----|-----|
| **量化粒度** | 子空间级 | 向量级 |
| **学习方式** | 无监督 K-Means | 有监督学习 |
| **精度** | 中等 | 高（有监督） |
| **训练复杂度** | 低 | 高 |

### OPQ vs SVD

| 特性 | OPQ | SVD |
|------|-----|-----|
| **降维方式** | 量化 | 投影 |
| **压缩率** | 固定（m 控制） | 可变（奇异值截断） |
| **精度** | 有损 | 近似无损 |
| **用途** | 向量压缩 | 降维 + 压缩 |

## 7. 实践建议

### 参数选择指南

1. **确定压缩率需求**：
   ```
   目标压缩率 = 原始大小 / 目标大小
   m = d × 4 / 目标压缩率
   ```

2. **选择 bits**：
   - 推荐 `bits = 8`（平衡精度和速度）
   - 高精度场景用 `bits = 10` 或 `12`
   - 内存受限用 `bits = 6` 或 `4`

3. **训练数据量**：
   ```
   n_train = max(100 × ks × m, 10000)
   ```
   至少需要每个码本 100 个样本。

### 性能优化技巧

1. **批量计算距离表**：
   - 多个查询共享码本距离计算
   - 使用矩阵乘法加速

2. **距离表预计算**：
   - 对热门查询缓存距离表
   - 适用于查询分布集中的场景

3. **GPU 加速**：
   - 旋转变换用 cuBLAS
   - 距离表计算用矩阵乘法

### 与索引结合

**IVF + OPQ**：
```
1. 用 OPQ 编码所有向量
2. 构建 IVF 索引（在编码空间）
3. 查询:
   - nprobe 个桶
   - ADC 距离过滤
   - Heap 重排序（取 top-k）
```

**HNSW + OPQ**：
```
1. 用 OPQ 编码向量
2. HNSW 图节点存储编码
3. 搜索:
   - ADC 距离导航图
   - Heap 重排序取 top-k
```

## 8. 算法变体

### 8.1 Residual VQ (RVQ)

**核心思想**：多级量化，每级量化残差。

```
level 1: q₁(x) → 编码₁
level 2: q₂(x - q₁(x)) → 编码₂
level 3: q₃(x - q₁(x) - q₂(x)) → 编码₃
...
```

**优势**：更高精度（相同压缩率）

### 8.2 Optimized RVQ (ORVQ)

结合旋转优化和多级量化：

```
x' = Rᵀx
编码 = [q₁(x'), q₂(residual₁), ...]
```

### 8.3 Deep Quantization

用深度网络学习量化器：

- 端到端训练
- 数据相关，精度更高
- 训练复杂度高

## 9. 参考资料

- Ge et al. (2013). "Optimized Product Quantization for Approximate Nearest Neighbor Search"
- Jegou et al. (2011). "Product Quantization for Nearest Neighbor Search"
- Babenko & Lempitsky (2014). "Additive Quantization"
- Martinez et al. (2018). "Revisiting Optimized Product Quantization"