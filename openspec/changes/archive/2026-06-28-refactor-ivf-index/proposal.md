## Why

当前 IVF 索引实现使用裸 `int32_t**` 管理倒排桶，存储与算法紧耦合，缺少 DirectMap 无法 reconstruct/删除，搜索使用 `qsort` 全局排序 + 桶内全量线性扫描无提前终止。需要系统性升级为具备抽象层、完整功能、高效搜索的生产级实现。

## What Changes

### Phase 1: 架构重构
- 新增 `InvertedLists` 抽象层，统一管理倒排桶的 ids[]/codes[]/sizes[]/caps[]
- 重构 `faiss_ivf` 结构体，以 `invlists` 替换裸数组
- **BREAKING**: 内部数据结构变更，`inverted_lists`、`list_sizes`、`list_capacities` 字段移除，替换为 `faiss_inverted_lists_t *invlists`

### Phase 2: 功能补充
- 新增 `DirectMap` 数据结构，支持向量 ID → (list_no, offset) 映射
- 新增 `reconstruct()` 通过 DirectMap 解码还原向量
- 新增 `remove_ids()` 墓碑标记删除（id=-1），搜索时自动跳过
- 新增 `compact()` 真空清理墓碑

### Phase 3: 效率优化
- 搜索用 MinimaxHeap 维护 top-k 替换 qsort 全局排序
- 桶按二级中心距离排序，优先访问最近桶
- 提前终止：桶距离 > 当前 top-k 最差时跳过
- 消除 n_total 大小的 candidates 数组分配

## Capabilities

### New Capabilities
- `inverted-lists`: 倒排桶抽象层，管理 ids/codes 的动态扩容和访问
- `direct-map`: 向量 ID 到桶位置的映射表，支持 reconstruct 和删除定位
- `ivf-delete`: 墓碑标记删除，搜索时自动跳过已删除向量
- `ivf-search-optimization`: 基于 heap 的搜索优化，桶优先级排序 + 提前终止

### Modified Capabilities
<!-- 无现有 capability 被修改，所有均为新增 -->

## Impact

### 受影响代码
| 文件 | 变更类型 |
|------|---------|
| `include/index/vector_index/ivf/inverted_lists.h` | **新增** |
| `include/index/vector_index/ivf/direct_map.h` | **新增** |
| `src/index/vector_index/ivf/faiss_inverted_lists.c` | **新增** |
| `src/index/vector_index/ivf/faiss_direct_map.c` | **新增** |
| `include/index/vector_index/ivf/IndexIVF.h` | 修改 |
| `src/index/vector_index/ivf/faiss_ivf_private.h` | 修改 |
| `src/index/vector_index/ivf/faiss_ivf_core.c` | 修改 |
| `src/index/vector_index/ivf/faiss_ivf_add.c` | 修改 |
| `src/index/vector_index/ivf/faiss_ivf_search.c` | 修改 |
| `src/index/vector_index/ivf/faiss_ivf_train.c` | 修改 |
| `src/index/vector_index/ivf/faiss_ivf_utils.c` | 修改 |
| `src/index/vector_index/ivf/CMakeLists.txt` | 修改 |

### API 影响
- 公共 API `faiss_ivf_index_*` 系列函数签名保持不变
- 内部 `faiss_ivf` 结构体字段变更，影响所有内部引用
