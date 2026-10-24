# P2-1 GPU 向量加速 提案

## 背景

多模态能力补齐系列中，P2-1 GPU 向量加速是高性能检索的关键模块。当前向量索引（HNSW、IVF-PQ 等）已实现，但在大规模数据集（1M+ 向量）上需要 GPU 加速以实现亚毫秒级查询。

## 变更范围

### 新增文件
| 文件 | 说明 |
|------|------|
| `engineering/include/db/index/vector/gpu_vector_index.h` | GPU 向量索引接口 |
| `engineering/src/db/index/vector/gpu_vector_index.c` | GPU 索引实现（CUDA/OpenCL） |
| `engineering/src/db/index/vector/gpu_kernels.cu` | CUDA 内核实现 |
| `engineering/test/db/index/gpu_vector_test.cpp` | GPU 索引测试 |

### 修改文件
| 文件 | 说明 |
|------|------|
| `engineering/src/db/index/vector/CMakeLists.txt` | 注册 GPU 模块 |

## 核心功能

1. **GPU 加速的向量索引**
   - GPU-HNSW：GPU 加速的 HNSW 图构建和搜索
   - GPU-IVF：GPU 加速的 IVF 倒排文件索引
   - 支持 CUDA 和 OpenCL 双后端

2. **SIMD 优化**
   - AVX-512 向量量化
   - 批量距离计算
   - 内存预取优化

3. **混合搜索**
   - CPU-GPU 协同搜索
   - 流水线并行
   - 内存映射优化

## 技术指标

- 向量维度：128-4096
- 批量插入：100K+ 向量/秒（GPU）
- 查询延迟：< 1ms（1M 数据集）
- Recall：>= 0.95

## 验收标准

- [ ] GPU-HNSW 搜索测试通过
- [ ] GPU-IVF 搜索测试通过
- [ ] 混合搜索模式工作正常
- [ ] 性能基准达标

## 风险与缓解

| 风险 | 缓解 |
|------|------|
| GPU 硬件依赖 | 通过 CUDA/OpenCL 抽象层支持多平台 |
| 大内存占用 | 使用内存映射和流式处理 |
