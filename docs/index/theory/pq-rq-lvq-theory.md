# PQ/RQ/LVQ 量化器原理

## 1. 算法概述

PQ（Product Quantization，乘积量化）、RQ（Residual Quantization，残差量化）和 LVQ（Locally-adaptive Vector Quantization，局部自适应量化）是三种主流的**向量压缩**方法。它们共同的目的是将高维浮点向量映射到紧凑的码字（code），从而在保持可比较性的前提下大幅降低存储和距离计算成本。

这三种量化器在工程实现中常与 IVF、HNSW、DiskANN 等索引结构配合使用，承担「压缩后的向量表示 + 快速的非对称距离计算（ADC）」角色。

### 1.1 共同核心思想

三种量化器都建立在**非对称距离计算（Asymmetric Distance Computation, ADC）**的范式之上：

```
原始查询 q ──(保留为浮点)──┐
                          ├─→ ADC 距离估计
压缩码字 c ─(查表/解码)──┘
```

与对称距离计算（双方都解码再算距离）相比，ADC 保留了查询端的全部精度，只对数据库端做压缩，因此召回率更高。

### 1.2 适用场景总结

| 量化器 | 核心场景 | 训练开销 | 码字大小 (d=128, m=16) |
|--------|---------|---------|-----------------------|
| PQ | 大规模向量压缩、IVF-PQ、DiskANN | 高（K-Means×m） | 16 B (32× 压缩) |
| RQ (RabitQ) | 内存与精度的甜点位 | 中（K-Means×m + 残差统计） | 16~18 B |
| LVQ | 无训练场景、动态更新、DiskANN | 无 | 132~264 B |

### 1.3 优缺点对比

| 维度 | PQ | RQ (RabitQ) | LVQ |
|------|----|----|-----|
| 压缩率 | 极高 | 高 | 低 |
| 训练开销 | K-Means×m | K-Means×m + 残差统计 | 无 |
| 增量友好 | 差（改码本） | 差（改码本） | 好（无码本） |
| ADC 查表 | 距离表 + 索引 | 距离表 + 残差校正 | 仅解码向量 |
| 内存模式 | 共享码本 | 共享码本 + 每向量残差 | 每向量独立 |
| 典型精度 | 中 | 高 | 高 |

---

## 2. PQ（Product Quantization，乘积量化）

PQ 由 Jegou 等人在 2011 年提出，是向量压缩领域最经典的算法，几乎所有工业级 ANN 系统都会涉及。

### 2.1 数学原理

#### 2.1.1 向量子空间分解

设原始向量 `x ∈ ℝᵈ`，PQ 将其切分为 `m` 个互不重叠的子向量：

```
x = [x₁, x₂, ..., xₘ]，每个 xₛ ∈ ℝ^(d/m)
```

子空间维度 `d* = d / m` 通常取 4~16 之间的值，太大则码本规模爆炸，太小则子空间相关性丢失。

#### 2.1.2 子空间码本

对每个子空间 `s ∈ {1, ..., m}`，PQ 用 K-Means 训练一个码本（codebook）：

```
Cₛ = {cₛ,₁, cₛ,₂, ..., cₛ,k}，cₛ,k ∈ ℝ^(d/m)
```

码本大小 `k = 2^pq_bits`，典型 `pq_bits = 8` ⇒ `k = 256`。

#### 2.1.3 编码公式

对每个子空间独立量化：

```
codeₛ = argmin_{k ∈ [K]} ‖ xₛ − cₛ,k ‖²
```

最终码字是 `m` 个索引的拼接：

```
code(x) = [code₁, code₂, ..., codeₘ] ∈ {0,1,...,K-1}ᵐ
```

#### 2.1.4 ADC 距离计算

距离计算借助**预计算的距离表**实现高效查表：

```
预计算：
  table[s][k] = ‖ qₛ − cₛ,k ‖²

查表：
  d(q, x) ≈ Σₛ table[s][codeₛ]
```

由于 `m` 个子空间相互独立，距离表大小为 `m × K` 个浮点数，单次距离计算只需 `m` 次查表与 `m-1` 次加法。

#### 2.1.5 量化误差

PQ 的量化误差源于**笛卡尔积约束**——每个子向量独立量化，没有跨子空间的协调。理论误差：

```
E[‖x − q(x)‖²] ≈ m × E_sub[‖xₛ − qₛ(xₛ)‖²]
```

每个子空间的误差与 K-Means 失真（distortion）成正比。

### 2.2 编码与解码流程

```
原始向量 x [d 维]
     │
     ├─── 切成 m 个子向量 x₁, x₂, ..., xₘ
     │
     ▼
┌──────────────────────────────────────┐
│      m 个子空间的码本（已训练）       │
│  C₁[C₁,₁...C₁,K]  C₂[...]  ... Cₘ  │
└───┬──────────┬──────────┬───────┬───┘
    ▼          ▼          ▼       ▼
  argmin    argmin    ...     argmin   ← K-Means 最近邻
    │          │          │       │
    ▼          ▼          ▼       ▼
  code₁     code₂     ...     codeₘ   ← 各 1 字节
    │          │          │       │
    └──────────┴──────────┴───────┘
                │
                ▼
          最终码字 [m 字节]
```

### 2.3 参数选择指南

| 参数 | 取值范围 | 推荐 | 影响 |
|------|---------|------|------|
| `pq_subquantizers` (m) | 4~64 | d/4 ~ d/8 | 大则压缩率高、子空间独立性更强；小则每子空间码本更精确 |
| `pq_bits` | 4 / 6 / 8 | 8 | 大则码本大、精度高；小则码字短 |
| 训练样本数 | — | ≥ 1000 或 100×k | 不够则 K-Means 不收敛 |
| K-Means 迭代 | — | 100 次 + tol=1e-4 | 与 scikit-learn 默认值一致 |

**经验法则**：
- d=128、m=16、bits=8 是最常见的「PQ 黄金配置」
- d=768~1024（BERT 类）通常取 m=64~96
- 4-bit 仅在内存极度受限时使用，精度损失明显

### 2.4 适用场景

- **大规模向量压缩存储**：向量库内存占用降低 32×
- **IVF-PQ 索引**：倒排聚类后用 PQ 压缩每个倒排列表
- **DiskANN**：仅保留 PQ 压缩后的向量在内存中，图结构放 SSD
- **重排序前的粗筛**：先用 PQ ADC 过滤掉明显不相似的候选

---

## 3. RQ（Residual Quantization，残差量化）

### 3.1 概念溯源

RQ 的核心思想可追溯到「残差量化（Residual Quantization, RQ）」与 2023 年微软 RabitQ 论文（《RabitQ: Quantizing High-Dimensional Vectors with a Theoretical Guarantee》）。本项目中的 RQ 实现即对应**RabitQ 方案**——一种 PQ + 残差方向编码的两级量化。

### 3.2 与 PQ 的区别

| 维度 | PQ | RQ (RabitQ) |
|------|----|----|
| 量化级数 | 单级 | 两级（PQ + 残差） |
| 码字 | m 个字节索引 | m 个 PQ 字节 + 残差位 |
| 精度 | 中 | 高（接近全精度） |
| 存储开销 | 基准 | +12.5%~25%（仅多几个字节） |
| 训练 | K-Means×m | K-Means×m + 残差步长统计 |

PQ 的局限：**码字的离散性**——每个子空间只有 256 个候选位置；落在两个码心之间的点无法被更精确地表达。

RQ 的解决思路：**用少量额外位编码残差方向（甚至大致幅度）**，把每个候选点拓展为一个微小的「方向球」。

### 3.3 多级残差量化原理

#### 3.3.1 第一级：PQ 粗量化

与标准 PQ 完全相同：

```
x → 切成子空间 → 每子空间 argmin 到最近码心 → 得到 pq_code[s]
```

#### 3.3.2 第二级：残差细量化

对每个子空间，计算残差：

```
residualₛ = xₛ − codebookₛ[pq_codeₛ]
```

残差是一个**低维向量**（d/m 维）。RabitQ 将其投影为标量残差 `‖residualₛ‖`，并对该标量做 1~4 bit 编码：

| `rq_residual_bits` | 编码内容 | 用途 |
|---|---|---|
| 1 | 仅符号位（正/负方向） | 极小额外存储 |
| 2 | 符号 + 2 级幅度 | 中等精度 |
| 4 | 符号 + 8 级幅度 | 高精度 |

```
方案示例（1-bit）：
  子空间0 残差: [-0.2, 0.2]    → 方向为「负」→ rq_code = 0
  子空间1 残差: [-0.3, 0.3]    → 方向为「负」→ rq_code = 0
  子空间2 残差: [ 0.1,-0.1]    → 方向为「正」→ rq_code = 1
```

#### 3.3.3 残差步长的设计

残差量化步长由训练阶段统计得到：对每个子空间，先用 K-Means 的码心重建所有训练向量，计算残差的标准差 `σₛ`：

```
stepₛ = σₛ / divisor(rq_residual_bits)

1-bit: divisor = 0.5     （仅符号位，步长大）
2-bit: divisor = 1.5     （粗幅度）
4-bit: divisor = 7.5     （细幅度）
```

这样的设计让量化后的码位大致覆盖「正态分布的一个标准差」，覆盖大多数实际残差。

#### 3.3.4 码字布局

```
┌────────────┬─────────────────────┐
│ PQ 码 (mB) │ 残差码 (m·bits/8 B) │
│ [5,12,...] │ [0,0,1,...]         │
└────────────┴─────────────────────┘

总码字大小 = m + ⌈ m × rq_bits / 8 ⌉

示例 (m=16, rq_bits=1)：
  PQ 码部分：16 字节
  残差部分：2 字节
  总计：18 字节（仅多 2 字节，换来明显精度提升）
```

### 3.4 ADC 距离计算

RQ 的距离计算分两部分：

```
dist(q, x) = Σₛ PQ_table[s][pq_code[s]]      ← 与 PQ 相同的预计算距离
           + Σₛ residual_correction[s][rq_code[s]]  ← 残差引起的偏差校正
```

第二阶段对每个子空间，根据 rq_code 还原残差方向（正/负）与大致幅度，将校正量累加进最终距离。

### 3.5 适用场景

- **DiskANN 压缩向量**：RabitQ 以极小的存储开销（仅多 2 字节）提供接近 LVQ 的精度
- **内存受限但仍要求精度的场景**：向量压缩比 16B → 18B，几乎无成本
- **需要与 PQ 互换的场景**：RQ 的第一级就是 PQ，可平滑替换
- **亚毫秒级召回场景**：相比 LVQ，RQ 的查表依然使用「距离表」，比 LVQ 重新解码向量稍快

### 3.6 与经典 RQ（多级码本）的差异

学术上的 RQ（Chen 等人 2010）通常指**多层级联码本**——第一级码本量化原始向量，第二级码本量化残差，第三级再量化残差的残差，以此类推。本项目中的 RQ 实现是 RabitQ 形式（PQ + 残差符号位），二者思想同源但细节不同：

| 经典 RQ | 本项目 RQ (RabitQ) |
|---------|------------------|
| 多层级联，每级独立码本 | PQ + 残差符号位 |
| 精度与级数成正比 | 精度与 `rq_residual_bits` 成正比 |
| 内存随级数线性增长 | 仅 m 位/多字节 |
| 训练多套 K-Means | 仅训练 PQ + 统计残差步长 |

---

## 4. LVQ（Learning Vector Quantization，局部自适应量化）

### 4.1 命名说明

「Learning Vector Quantization」在经典教材中指 Kohonen 提出的**有监督**学习算法（学习带标签的码心）。但本项目实现的 LVQ 实际是 **Locally-adaptive Vector Quantization**（局部自适应量化），与 DiskANN/SVS 中的方案一致——**对每个向量独立做标量量化，无需训练**。两者中文译名有时都写作「学习向量量化」，需根据上下文区分。

### 4.2 与 K-Means 的本质区别

| 维度 | K-Means（以 PQ 为例） | LVQ |
|------|-----|-----|
| 码本来源 | 训练阶段学习 | **每向量独立** |
| 共享/独立 | 所有向量共享一个全局码本 | 每个向量有自己的 scale + bias |
| 训练开销 | 需要迭代 | **无训练** |
| 量化粒度 | 子空间联合量化 | 每个分量独立量化 |
| 压缩率 | 32× | 1×~4× |
| 增量更新 | 需重训（码本变化） | 随时可用 |

K-Means 的核心假设是「数据有聚类结构」，LVQ 不做此假设——它承认每个向量都有自己的范围（scale），因此**逐向量做仿射变换到 [0, 2^bits-1] 区间**。

### 4.3 学习算法（实为「编码算法」）

LVQ 没有 K-Means 迭代意义上的「学习」，但每向量编码时确实发生「自适应」过程：

```
输入向量 x [d 维]

1. 扫描求范围：
   min_val = min(x)
   max_val = max(x)
   range   = max_val − min_val

2. 计算局部参数：
   scale = range / (2^bits − 1)
   bias  = min_val
   （若 range≈0：scale=1, bias=min_val，所有码字写 0）

3. 量化每个分量：
   code[i] = clamp( round((x[i] − bias) / scale), 0, 2^bits − 1 )

输出码字：[float32 scale][float32 bias][code...]
```

**特点**：
- 完全没有外部训练数据
- 每个向量的 scale 和 bias 反映它自己的分布
- 对孤立点（outlier）友好——分布奇怪不影响其他向量的量化

### 4.4 码字布局

#### 8-bit 模式

```
┌────────────┬────────────┬───────────────────────────────┐
│ scale (4B) │ bias (4B)  │ code[0] code[1] ... code[d-1] │
│  float32   │  float32   │         uint8 × d            │
└────────────┴────────────┴───────────────────────────────┘

总大小 = 8 + d 字节

d=128 时：136 字节 / 向量  （相比原始 4·128 = 512 字节，3.76× 压缩）
```

#### 4-bit 模式（更紧凑）

每字节存两个分量：

```
payload[i/2]：
  低 4 位 = code[2i]      ← 偶数分量
  高 4 位 = code[2i+1]    ← 奇数分量

总大小 = 8 + ⌈ d / 2 ⌉ 字节

d=128 时：8 + 64 = 72 字节 / 向量
```

### 4.5 ADC 距离

LVQ 的 ADC 实现与 PQ/RQ 截然不同：

```
compute_distance_table：
  table ← query  ← 仅复制查询向量到 table（d 个 float）

adc_distance：
  解码向量近似值：x_approx[i] = bias + scale × code[i]
  计算 L2 距离：Σᵢ (query[i] − x_approx[i])²
```

没有「距离表预计算」——所有距离计算都直接遍历解码向量。这种「解码后计算」的成本是 O(d)，比 PQ/RQ 的 O(m) 大，对极致低延迟场景不友好，但对内存极度敏感或解压场景反而更直观。

### 4.6 适用场景

- **无训练数据场景**：冷启动时使用，待积累样本后再迁移到 PQ
- **向量化动态更新**：新增向量无需重新训练
- **DiskANN 中的「压缩向量」**：用于内存受限时仍保留较高精度的兜底方案
- **对参数鲁棒性要求高**：scale 和 bias 自适应，无人工超参
- **简单调试 / 教学**：实现简单、可解释

---

## 5. 对比分析

### 5.1 横向对比表

| 维度 | PQ | RQ (RabitQ) | LVQ |
|------|----|----|-----|
| 提出时间 | 2011（Jegou） | 2023（微软 RabitQ） | 业界长期使用 |
| 训练需要 | 是（K-Means×m） | 是（K-Means×m + 残差统计） | 否 |
| 码字大小 (d=128, m=16) | 16 B | 16~18 B | 72~136 B |
| 压缩率 | 32× | ~28× | 1.9×~3.8× |
| ADC 时间复杂度 | O(m) | O(m) | O(d) |
| 距离表预计算 | 是（m×K floats） | 是（m×K floats） | 否（仅 d floats） |
| 增量插入 | 需重训或更新码本 | 需更新码本 + 残差步长 | **即时** |
| 增量删除 | 同上 | 同上 | 同上 |
| 序列化的稳定性 | 高（码本固定） | 高 | 取决于向量 |
| 调参难度 | 中（m、bits） | 中（再加 rq_bits） | 低（仅 bits） |
| 召回率（典型） | 0.85~0.95 | 0.92~0.98 | 0.95~0.99 |

### 5.2 与 OPQ 的关系

OPQ（Optimized Product Quantization）是 PQ 的**正交改进**，它不影响 PQ 的核心机制——而是在训练前对数据做 PCA 旋转，使子空间方差更均衡，从而降低量化误差。

```
                        ┌──────────┐
                        │  OPQ     │
                        │ 旋转矩阵 │
                        └────┬─────┘
                             │ x → Rᵀ·x
                             ▼
┌──────────┐    ┌──────────────────┐    ┌──────────┐
│   PQ     │ ←→ │       RQ         │    │   LVQ    │
└──────────┘    │ (RabitQ)         │    └──────────┘
                └──────────────────┘
       │             │                   │
   可叠加 OPQ     一级 PQ 可叠加 OPQ    不适用
                                        （无子空间概念）
```

#### 关键事实

- **OPQ 是 PQ/RQ 的「前置模块」**，不是替代关系
- **OPQ 不适用于 LVQ**：LVQ 没有子空间分解，无法受益于旋转
- 本项目的 `quantizer_enable_opq()` 接口可直接作用于 PQ 与 RQ，旋转信息保存在 `rotation_matrix` 字段中

#### 与三种量化器搭配的推荐度

| 搭配 | 推荐度 | 说明 |
|------|--------|------|
| OPQ + PQ | ★★★★ | 业界经典组合，几乎必备 |
| OPQ + RQ | ★★★★★ | RabitQ 论文明确建议先做 OPQ 旋转 |
| OPQ + LVQ | N/A | 不适用 |

### 5.3 选择建议

#### 决策树

```
需要向量压缩？
├── 否 → 不需要量化器
└── 是
    ├── 有训练数据？
    │   ├── 否 → LVQ（无需训练）
    │   └── 是
    │       ├── 内存极度受限（≤ 16B/向量）？
    │       │   ├── 接受有损 + 召回中等 → PQ
    │       │   └── 想要更高召回 → RQ（多 2~4 字节）
    │       └── 内存充足（> 100B/向量）
    │           └── LVQ（精度最高）
    └── 索引持续更新？
        └── LVQ（无训练，最友好）
```

#### 场景化建议

| 场景 | 推荐量化器 | 配置 |
|------|-----------|------|
| 千万级向量、磁盘存储、IVF 索引 | **PQ** | m=16, bits=8 |
| 千万级向量、内存优先 | **RQ** | m=16, rq_bits=1 或 2 |
| 亿级向量、SSD 上的 DiskANN | **RQ (RabitQ)** | m=64~96, rq_bits=1 |
| 千万级向量、动态更新、无训练 | **LVQ** | bits=8（必要时 4） |
| 超高精度重排环节 | **LVQ** | bits=8，几乎无损 |
| 嵌入式 / 边缘部署 | **PQ** 或 **RQ** | m=8, bits=4 |

### 5.4 性能与精度权衡总结

```
压缩率 ↑
   │
   │              ● PQ
   │            ● RQ
   │                  ● LVQ(4-bit)
   │                       ● LVQ(8-bit)
   │                             ● 原向量
   ▼                                    精度/召回率 ↑
```

- 越靠近左上角：压缩越狠，精度越差
- 越靠近右下角：压缩比越小，精度越好
- 三者形成连续谱：PQ（极致压缩）→ RQ（甜点位）→ LVQ（极限精度）

---

## 6. 工程实现要点

### 6.1 量化器配置

```c
typedef struct quantizer_config {
    int32_t dims;
    quantization_type_t type;
    int32_t pq_subquantizers;  // PQ 子空间数
    int32_t pq_bits;           // PQ bits: 4/6/8
    int32_t lvq_bits;          // LVQ bits: 4/8
    int32_t sq_bits;
    int32_t rq_pq_bits;        // RQ 一级 PQ bits
    int32_t rq_residual_bits;  // RQ 残差 bits: 1/2/4
} quantizer_config_t;
```

### 6.2 统一 API 流程

```c
// 1. 创建
quantizer_config_t config;
quantizer_config_init(&config, 128, QUANTIZATION_TYPE_PQ);
config.pq_subquantizers = 16;
config.pq_bits = 8;

quantizer_t *q = quantizer_create(&config);

// 2. 训练（LVQ 可跳过）
quantizer_train(q, n, vectors);

// 3. 批量编码
uint8_t codes[n * 16];
quantizer_encode_batch(q, n, vectors, codes);

// 4. ADC 距离
float table[4096];
quantizer_compute_distance_table(q, DISTANCE_METRIC_L2, query, table);
float dist = quantizer_adc_distance(q, code, table);

// 5. 持久化（PQ/RQ）
quantizer_save(q, "pq.bin");
q = quantizer_load("pq.bin");

// 6. 释放
quantizer_drop(q);
```

### 6.3 常见陷阱

1. **训练样本不足**：PQ/RQ 的 K-Means 在样本数 < 1000 时会欠拟合，建议至少 100×k 个样本
2. **`pq_subquantizers` 不整除 `dims`**：本项目按 `dims / m` 截断，剩余分量被忽略——选择 m 时务必整除
3. **4-bit LVQ 的解码**：注意每字节两个分量的高低半字节分布（偶→低，奇→高）
4. **RQ 的残差位选择**：`rq_residual_bits=1` 仅编码符号，不编码幅度；如需幅度信息用 2 或 4
5. **OPQ 与 PQ 训练顺序**：若启用 OPQ，需先 `enable_opq` 再 `train`，否则旋转无效

---

## 7. 参考资料

- Jégou, H., et al. (2011). *Product Quantization for Nearest Neighbor Search*. IEEE TPAMI.
- Ge, T., et al. (2013). *Optimized Product Quantization for Approximate Nearest Neighbor Search*. CVPR.
- Chen, Y., et al. (2010). *Approximate Nearest Neighbor Search by Residual Vector Quantization*. ISMVL.
- Gao, J., et al. (2023). *RabitQ: Quantizing High-Dimensional Vectors with a Theoretical Guarantee*（微软亚研院）.
- Subramanya, S., et al. *DiskANN / SVS 框架中的向量压缩方案*.
