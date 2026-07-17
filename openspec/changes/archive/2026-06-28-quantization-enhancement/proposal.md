# 量化算法扩展提案

## Why

当前 DiskANN 索引的量化系统仅支持 PQ 和 LVQ 两种量化器，且配置不够灵活（PQ 的 bits 固定为 8）。随着向量数据库在超大规模场景（亿级向量）的应用，SQ（标量量化）和 RabitQ（残差量化）等更高效的量化方案成为工业界的主流选择。本变更将为量化系统补充完整的量化算法支持，并开放 bit 配置的灵活性。

## What Changes

### 新增量化类型

1. **SQ (Scalar Quantization)** - 标量量化
   - 全局 min/max 统计，每维度独立量化
   - 支持 4-bit 和 8-bit 配置
   - 码字布局：`[quantized_dim_0, quantized_dim_1, ...]`

2. **RabitQ (Residual Binary Quantization)** - 残差二进制量化
   - 两级量化：第一级 PQ + 第二级残差量化
   - PQ 支持 8 bits，残差支持 1/2/4 bits
   - 显著提升精度（比纯 PQ 减少 40-50% 误差）
   - 详细注释解释原理和实现

### 扩展现有量化器

3. **PQ bit 配置扩展**
   - 支持 4-bit、6-bit、8-bit 可配置
   - 通过 `quantizer_config.pq_bits` 暴露

4. **LVQ 保持现状**
   - 已支持 4-bit 和 8-bit，无需修改

### 接口变更

5. **`quantization_type_t` 枚举扩展**
   ```c
   typedef enum quantization_type {
       QUANTIZATION_TYPE_NONE = 0,
       QUANTIZATION_TYPE_PQ   = 1,
       QUANTIZATION_TYPE_LVQ  = 2,
       QUANTIZATION_TYPE_SQ   = 3,  // 新增
       QUANTIZATION_TYPE_RQ   = 4,  // 新增: RabitQ
   } quantization_type_t;
   ```

6. **`quantizer_config_t` 结构扩展**
   ```c
   typedef struct quantizer_config {
       int32_t dims;
       quantization_type_t type;
       int32_t pq_subquantizers;
       int32_t pq_bits;           // 新增: 可配置 4/6/8
       int32_t lvq_bits;          // 已有
       int32_t sq_bits;           // 新增: 4/8
       int32_t rq_pq_bits;        // 新增: RabitQ PQ bits
       int32_t rq_residual_bits;  // 新增: RabitQ 残差 bits
   } quantizer_config_t;
   ```

## Capabilities

### New Capabilities

- `scalar-quantization`: SQ 标量量化器实现，支持全局 min/max 统计和 per-dimension 量化
- `residual-quantization`: RabitQ 残差量化器实现，两级 PQ + 残差量化，含详细原理注释
- `quantization-bits-config`: PQ/SQ/RabitQ 的 bit 数可配置支持

### Modified Capabilities

- `vdb-quantization`: 现有量化技术文章规格，需要补充 RabitQ 相关内容描述

## Impact

### 受影响代码

| 文件 | 变更类型 | 说明 |
|------|---------|------|
| `include/algo/quantization/quantization.h` | 修改 | 扩展枚举和配置结构 |
| `src/algo/quantization/quantization.c` | 修改 | 添加 SQ/RabitQ 分支 |
| `src/algo/quantization/quantization_private.h` | 修改 | 添加新量化器字段 |
| `src/algo/quantization/sq.h` | 新增 | SQ 头文件 |
| `src/algo/quantization/sq.c` | 新增 | SQ 实现 |
| `src/algo/quantization/rq.h` | 新增 | RabitQ 头文件 |
| `src/algo/quantization/rq.c` | 新增 | RabitQ 实现（详细注释） |

### 新增依赖

- 无外部依赖，复用现有 K-Means 训练基础设施

## 7. DiskANN 集成

- 修改 `diskann_quantization_params_t` 支持新量化类型
- 更新 DiskANN 的元信息页以存储量化类型
- 测试 DiskANN 与 SQ/RabitQ 的集成

## 8. 文档更新

- 更新 `src/algo/quantization/` 目录下的 README
- 更新 `include/algo/quantization/quantization.h` 的注释
- 添加新量化器的使用示例

### 存储格式兼容性

- 新量化类型使用独立的文件标识，无需与旧格式兼容
- DiskANN 加载时会根据元信息页的量化类型创建对应量化器
