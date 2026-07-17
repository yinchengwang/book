# parallel_stl - NOTES

## 交叉参考：SIMD 并行化

### C++ Parallel STL vs 手写 SIMD

本模块对比 C++17 Parallel STL（通过 TBB 实现）与手写 SIMD 的并行化能力：

#### engineering/src/algo-prod/sort/sort.c 分析

`sort.c` 实现了通用排序算法：
- 堆排序 (`sort_heap_sift_down`)
- 归并排序 (`sort_merge_recursive`)
- 快速排序（基于范围划分）

**注意**：`sort.c` 目前未使用显式 SIMD 指令（`_mm_*` intrinsics）。
SIMD 并行化通常应用于计算密集型热路径，如：
- `engineering/src/algo-prod/distance/distance.c` - 向量距离计算
- `engineering/src/algo-prod/quantization/adaptive_quantization.c` - 量化压缩

#### SIMD 并行化层次

```
Level 1: 编译器自动向量化
         for (i=0; i<N; i++) a[i] = b[i] + c[i];  // 编译器自动 SIMD

Level 2: Parallel STL (par_unseq)
         std::transform(par_unseq, ...)            // 多线程 + 自动向量化

Level 3: 手写 SIMD Intrinsics
         __m256d v = _mm256_load_pd(&a[i]);        // 显式 AVX2 指令

Level 4: GPU/SIMD 汇编
         极致性能优化场景
```

#### TBB 与 SIMD 的协同

TBB (`-ltbb`) 提供：
- 任务调度（多核并行）
- 内存分配器优化
- 配合 SIMD 向量化（通过 `par_unseq` 策略）

#### 实践建议

1. **优先使用 Parallel STL**：代码可移植，实现简单
2. **性能瓶颈时分析 hotpath**：使用 perf/vtune 定位
3. **手写 SIMD 场景**：
   - 极致性能要求
   - 特殊数据结构（非连续内存）
   - 非标准操作（位操作、打包解包）
4. **参考距离计算模块**：`distance.c` 中的 SIMD 距离计算模式

### 相关文件

- `C:/code/book/engineering/src/algo-prod/sort/sort.c` - 通用排序（无 SIMD）
- `C:/code/book/engineering/src/algo-prod/distance/distance.c` - SIMD 距离计算
- `C:/code/book/engineering/src/algo-prod/quantization/adaptive_quantization.c` - 量化 SIMD

### 延伸学习

- Intel SVML (Short Vector Math Library)
- GCC/Clang auto-vectorization 报告 (`-fopt-info-vec`)
- TBB flow graph 用于复杂并行模式
