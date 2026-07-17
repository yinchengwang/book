## 1. Phase 1: InvertedLists 抽象层

- [x] 1.1 新建 `include/index/vector_index/ivf/inverted_lists.h` 公开头文件，声明数据结构与 API
- [x] 1.2 新建 `src/index/vector_index/ivf/faiss_inverted_lists.c`，实现 create/drop/add_entry/resize_list/reset 及内联查询函数
- [x] 1.3 修改 `include/index/vector_index/ivf/IndexIVF.h`，添加 InvertedLists 头文件引用
- [x] 1.4 修改 `src/index/vector_index/ivf/faiss_ivf_private.h`，同步结构体变更（移除 inverted_lists/list_sizes/list_capacities，添加 invlists）
- [x] 1.5 修改 `src/index/vector_index/ivf/faiss_ivf_core.c`，create/drop/reset 改用 InvertedLists API
- [x] 1.6 修改 `src/index/vector_index/ivf/faiss_ivf_add.c`，用 add_entry 替代手动扩容和插入
- [x] 1.7 修改 `src/index/vector_index/ivf/faiss_ivf_search.c`，用 `list_size`/`get_ids` 读取桶数据
- [x] 1.8 训练后 code_size 由 quantizer 设置，invlists->code_size 保持 0（无每桶编码存储）
- [x] 1.9 修改 `src/index/vector_index/ivf/faiss_ivf_utils.c`，移除旧的桶容量管理函数
- [x] 1.10 GLOB_RECURSE 自动发现新文件，无需修改 CMakeLists
- [x] 1.11 构建并运行所有 IVF 测试，确认无回归

## 2. Phase 2: DirectMap + 删除 + 重构

- [x] 2.1 新建 `include/index/vector_index/ivf/direct_map.h`，声明 DirectMap 数据结构与 API
- [x] 2.2 新建 `src/index/vector_index/ivf/faiss_direct_map.c`，实现 init/add/get/remove/clear/drop
- [x] 2.3 修改 `include/index/vector_index/ivf/IndexIVF.h`，添加 DirectMap 字段和 public API 声明
- [x] 2.4 修改 `include/index/vector_index/ivf/inverted_lists.h`，添加 `remove_entry` 声明
- [x] 2.5 修改 `src/index/vector_index/ivf/faiss_inverted_lists.c`，实现 `remove_entry` 墓碑标记
- [x] 2.6 修改 `src/index/vector_index/ivf/faiss_ivf_add.c`，添加时写 DirectMap，优先复用墓碑位置
- [x] 2.7 修改 `src/index/vector_index/ivf/faiss_ivf_search.c`，搜索时跳过 id==-1 的墓碑
- [x] 2.8 修改 `src/index/vector_index/ivf/faiss_ivf_core.c`，实现 `remove_ids`、`reconstruct`、`compact`
- [x] 2.9 修改 `src/index/vector_index/ivf/CMakeLists.txt`，添加 `faiss_direct_map.c`
- [x] 2.10 新增 `test/vector_index/ivf/ivf_direct_map_gtest.cpp` DirectMap 与删除单元测试
- [x] 2.11 构建并运行所有测试，确认无回归

## 3. Phase 3: 搜索效率优化

- [x] 3.1 修改 `src/index/vector_index/ivf/faiss_ivf_search.c`，用 MinimaxHeap 维护 top-k 结果替代 qsort 全局排序
- [x] 3.2 实现桶按二级中心距离排序，优先访问最近桶
- [x] 3.3 实现提前终止：桶中心距离 > top-k 最差距离时跳过
- [x] 3.4 消除 n_total 大小的 candidates 临时数组分配
- [x] 3.5 构建并运行测试，验证搜索 recall 不下降且内存占用显著降低
