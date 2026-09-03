# parallel_stl - C++17 Parallel STL 执行策略

## 概述

本模块演示 C++17 `std::execution` 策略的用法和性能对比，包括：
- `std::execution::seq` - 顺序执行
- `std::execution::par` - 并行执行
- `std::execution::par_unseq` - 并行+向量化

## 编译运行

```bash
# 安装 TBB (如未安装)
choco install tbb    # Windows
# apt install libtbb-dev   # Linux
# brew install tbb         # macOS

# 编译运行
make run
```

## 执行策略说明

| 策略 | 说明 | 适用场景 |
|------|------|----------|
| `seq` | 顺序执行，无并行开销 | 调试、小数据集、线程不安全操作 |
| `par` | 多线程并行，线程安全保证 | 通用并行化 |
| `par_unseq` | 并行+SIMD向量化，最高性能 | 计算密集型大数据集 |

## 性能对比

典型输出（5M 元素）：
```
Transform Performance:
  seq:       320.50 ms
  par:       85.20 ms  (3.8x speedup)
  par_unseq: 42.10 ms  (7.6x speedup)

Sort Performance:
  seq:       890.30 ms
  par:       340.50 ms  (2.6x speedup)

Reduce Performance:
  seq sum:   18.20 ms
  par sum:   5.40 ms    (3.4x speedup)
```

## 关键 API

- `std::transform(exec, first, last, d_first, op)`
- `std::sort(exec, first, last)`
- `std::reduce(exec, first, last, init)`
- `std::for_each(exec, first, last, op)`
- `std::fill(exec, first, last, value)`

## 参考

- [cppreference: Execution Policies](https://en.cppreference.com/w/cpp/algorithm/execution_policy_tag_t)
- TBB: https://github.com/oneapi-src/oneTBB
