# 量化算法扩展 - 任务清单

## 1. 基础设施修改

- [x] 1.1 扩展 `quantization_type_t` 枚举，添加 `QUANTIZATION_TYPE_SQ` 和 `QUANTIZATION_TYPE_RQ`
- [x] 1.2 扩展 `quantizer_config_t` 结构体，添加 `sq_bits`、`rq_pq_bits`、`rq_residual_bits` 字段
- [x] 1.3 修改 `quantizer_config_init()` 为新增字段设置默认值
- [x] 1.4 修改 `quantization_type_is_valid()` 支持新类型
- [x] 1.5 修改 `quantizer_config_validate()` 添加 SQ 和 RabitQ 的参数验证
- [x] 1.6 修改 `quantization_private.h` 的 `quantizer` 结构体，为 SQ 和 RabitQ 添加内部字段

## 2. SQ (Scalar Quantization) 实现

- [x] 2.1 创建 `src/algo/quantization/sq.h`，定义 SQ 内部接口
- [x] 2.2 创建 `src/algo/quantization/sq.c`，实现以下函数：
  - [x] 2.2.1 `sq_init()` - 初始化（分配全局 min/max 存储）
  - [x] 2.2.2 `sq_free()` - 释放资源
  - [x] 2.2.3 `sq_reset()` - 重置状态
  - [x] 2.2.4 `sq_train()` - 全局统计：扫描所有向量求 min/max，计算 scale
  - [x] 2.2.5 `sq_encode()` - 编码：每维度量化并打包
  - [x] 2.2.6 `sq_encode_batch()` - 批量编码
  - [x] 2.2.7 `sq_compute_distance_table()` - 距离表：存储查询向量
  - [x] 2.2.8 `sq_adc_distance()` - ADC 距离计算
  - [x] 2.2.9 `sq_code_size()` - 码字大小计算
  - [x] 2.2.10 `sq_distance_table_size()` - 距离表大小
  - [x] 2.2.11 `sq_model_float_count()` - 模型参数数量
  - [x] 2.2.12 `sq_export_model()` - 导出模型（global_min, scale）
  - [x] 2.2.13 `sq_load_model()` - 导入模型

## 3. RabitQ (Residual Quantization) 实现

- [x] 3.1 创建 `src/algo/quantization/rq.h`，定义 RabitQ 内部接口
- [x] 3.2 创建 `src/algo/quantization/rq.c`，实现以下函数（含详细注释）：
  - [x] 3.2.1 `rq_init()` - 初始化：分配 PQ 码本和残差步长存储
  - [x] 3.2.2 `rq_free()` - 释放资源（PQ 码本 + 残差参数）
  - [x] 3.2.3 `rq_reset()` - 重置状态
  - [x] 3.2.4 `rq_train()` - 两级训练：
    - [x] 第一级：调用 PQ 训练生成码本
    - [x] 第二级：计算每个子空间的残差统计量
  - [x] 3.2.5 `rq_encode()` - 两级编码：
    - [x] PQ 编码：每子空间找最近码字
    - [x] 残差编码：计算并编码残差方向/幅度
  - [x] 3.2.6 `rq_encode_batch()` - 批量编码
  - [x] 3.2.7 `rq_compute_distance_table()` - 距离表：PQ 距离表
  - [x] 3.2.8 `rq_adc_distance()` - ADC 距离：PQ 距离 + 残差校正
  - [x] 3.2.9 `rq_code_size()` - 码字大小：m + ceil(m × rq_bits / 8)
  - [x] 3.2.10 `rq_distance_table_size()` - 距离表大小
  - [x] 3.2.11 `rq_model_float_count()` - 模型参数数量（PQ 码本）
  - [x] 3.2.12 `rq_export_model()` - 导出 PQ 码本
  - [x] 3.2.13 `rq_load_model()` - 导入 PQ 码本

## 4. quantization.c 统一调度器修改

- [x] 4.1 修改 `quantizer_create()` 添加 SQ 和 RabitQ 的初始化分支
- [x] 4.2 修改 `quantizer_train()` 添加 SQ 和 RabitQ 的训练分支
- [x] 4.3 修改 `quantizer_encode()` 添加 SQ 和 RabitQ 的编码分支
- [x] 4.4 修改 `quantizer_encode_batch()` 添加 SQ 和 RabitQ 的批量编码分支
- [x] 4.5 修改 `quantizer_compute_distance_table()` 添加 SQ 和 RabitQ 的距离表分支
- [x] 4.6 修改 `quantizer_adc_distance()` 添加 SQ 和 RabitQ 的 ADC 分支
- [x] 4.7 修改 `quantizer_code_size()` 添加 SQ 和 RabitQ 的码字大小分支
- [x] 4.8 修改 `quantizer_distance_table_size()` 添加 SQ 和 RabitQ 的距离表大小分支
- [x] 4.9 修改 `quantizer_model_float_count()` 添加 SQ 和 RabitQ 的模型大小分支
- [x] 4.10 修改 `quantizer_export_model()` 添加 SQ 和 RabitQ 的导出分支
- [x] 4.11 修改 `quantizer_create_from_model()` 添加 SQ 和 RabitQ 的导入分支
- [x] 4.12 修改 `quantizer_type()` 和 `quantizer_is_trained()` 确保新类型正常工作
- [x] 4.13 修改 `quantizer_reset()` 添加 SQ 和 RabitQ 的重置分支
- [x] 4.14 修改 `quantizer_drop()` 添加 SQ 和 RabitQ 的释放分支

## 5. PQ bits 配置扩展

- [x] 5.1 `pq_init()` 已使用 `config.pq_bits`（无需修改）
- [x] 5.2 `pq_compute_distance_table()` 已正确处理不同的 codebook_size
- [x] 5.3 PQ(4-bit), PQ(6-bit), PQ(8-bit) 配置均可用（通过 config 传递）

## 6. 测试用例

- [x] 6.1 创建 `test/algo/quantization/test_sq.c`，测试 SQ
- [x] 6.2 创建 `test/algo/quantization/test_rq.c`，测试 RabitQ
- [x] 6.3 PQ bits 配置已通过 config 传递（无需额外测试）

## 7. DiskANN 集成（可选，如需在索引中使用新量化器）

- [ ] 7.1 修改 `diskann_quantization_params_t` 支持新量化类型
- [ ] 7.2 更新 DiskANN 的元信息页以存储量化类型
- [ ] 7.3 测试 DiskANN 与 SQ/RabitQ 的集成

## 8. 文档更新

- [x] 8.1 更新 `include/algo/quantization/quantization.h` 添加注释和使用示例
- [x] 8.2 SQ/RabitQ 实现文件自带详细原理注释
