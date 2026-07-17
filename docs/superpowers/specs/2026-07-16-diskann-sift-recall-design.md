# DiskANN SIFT 召回率测试设计规格

创建日期：2026-07-16

## 概述

创建 `diskann_recall.c`，复用 SIFT Small 数据集测试 DiskANN 索引召回率。

## 目标

- Recall@10 ≥ 0.90
- 与现有 HNSW 测试 (`sift_recall.c`) 使用相同数据集，便于对比
- 支持多参数搜索测试

## 文件布局

```
engineering/test/db/index/sift_test/
├── CMakeLists.txt          # 添加 diskann_recall 可执行文件
├── sift_recall.c           # 现有 HNSW 测试
└── diskann_recall.c        # 新增 DiskANN 测试
```

## 核心组件

### diskann_recall.c 函数结构

| 函数 | 职责 | 来源 |
|------|------|------|
| `read_header()` | 从 `_header.txt` 读取 count/dims | 复用 sift_recall.c |
| `read_binary_fvecs()` | 加载 float32 向量数据 | 复用 sift_recall.c |
| `read_binary_groundtruth()` | 加载 int32 ground truth | 复用 sift_recall.c |
| `compute_recall_at_k()` | 计算召回率 | 复用 sift_recall.c |
| `main()` | 构建 DiskANN → 多参数搜索 → 输出召回率 | 新增 |

## 测试参数

### 基本测试（第一轮）

| 参数 | 值 | 说明 |
|------|-----|------|
| R (index_size) | 32 | 目标邻居数 |
| L (build_search_list_size) | 100 | 构图候选数 |
| α (alpha) | 1.2 | Vamana 剪枝参数（默认值） |
| search_list_size | 50, 100, 200 | 搜索候选大小 |
| K | 1, 10, 50 | 返回结果数 |

### 后续扩展（达标后）

- R: 64, 128
- L: 200, 400
- 量化配置：PQ/SQ

## 输出格式

```
=== DiskANN SIFT Recall Test ===

Dataset: c:/code/book/dataset/siftsmall
  Base: 10000 vectors, dimension 128
  Query: 100 vectors
  Ground Truth: K=100

Building DiskANN index (R=32, L=100, α=1.2)...
Index built, total vectors: 10000

Testing recall with different search_list_size values...
--- search_list_size = 50 ---
  Recall@1:  0.9800
  Recall@10: 0.9245
  Recall@50: 0.8762

--- search_list_size = 100 ---
  Recall@1:  0.9900
  Recall@10: 0.9512
  Recall@50: 0.9124

--- search_list_size = 200 ---
  Recall@1:  0.9900
  Recall@10: 0.9634
  Recall@50: 0.9318

Test completed!
```

## CMake 集成

```cmake
# engineering/test/db/index/sift_test/CMakeLists.txt

# 现有 HNSW 测试
add_executable(sift_recall sift_recall.c)
target_link_libraries(sift_recall PRIVATE hnsw algo_prod)
add_project_test(sift_recall)

# 新增 DiskANN 测试
add_executable(diskann_recall diskann_recall.c)
target_link_libraries(diskann_recall PRIVATE diskann algo_prod)
add_project_test(diskann_recall)
```

## 依赖关系

- `diskann` 库（`engineering/src/db/index/vector_index/diskann/`）
- `algo_prod` 库（距离计算）
- SIFT Small 数据集（`dataset/siftsmall/`）

## 文件变更清单

| 操作 | 文件 |
|------|------|
| 新增 | `engineering/test/db/index/sift_test/diskann_recall.c` |
| 修改 | `engineering/test/db/index/sift_test/CMakeLists.txt` |

## 成功标准

1. 编译通过，生成 `diskann_recall` 可执行文件
2. 运行成功，输出各 search_list_size 下的召回率
3. Recall@10 ≥ 0.90（在合理 search_list_size 下）
