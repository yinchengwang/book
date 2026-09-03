# 索引实现代码审查报告

> **审查日期**：2026-07-15
> **审查范围**：HNSW、DiskANN、IVF、LSH 四个索引实现
> **审查维度**：正确性、内存安全，性能热点、代码风格

---

## 一、审查结论概览

| 索引 | 整体评级 | 致命问题 | 严重问题 | 中等问题 |
|------|----------|----------|----------|----------|
| **HNSW** | B | 2 | 4 | 6 |
| **DiskANN** | C+ | 8 | 11 | 17 |
| **IVF** | C | 6 | 7 | 8 |
| **LSH** | C- | 4 | 7 | 10 |

---

## 二、HNSW 索引审查结果

### 2.1 致命问题

#### F1. `faiss_hnsw_prepare_level_tab` 每次插入泄漏旧数组
- **位置**：`faiss_hnsw_insert.c:552-557`
- **问题**：每次 `faiss_hnsw_index_add` 重新分配 `levels/offsets/nbs` 但旧数组未释放
- **影响**：随插入次数线性增长的堆泄漏
- **修复**：在覆盖前 `free(index->levels/offsets/nbs)`

#### F2. `M > 127` 时栈缓冲区溢出
- **位置**：`faiss_hnsw_insert.c:19-20`
- **问题**：宏 `FAISS_HNSW_MAX_CANDIDATES_ON_STACK=256`，但 `capacity=2*M`，`M≥128` 时越界
- **修复**：在 `faiss_hnsw_index_create` 中限制 `M ≤ 64`

### 2.2 严重问题

| 问题 | 位置 | 描述 |
|------|------|------|
| S1 | `faiss_hnsw_search.c` 全程 | 搜索不过滤已删除向量，墓碑返回 |
| S2 | `faiss_hnsw_search.c:69-130` | `ef_search <= 0` 越界写 |
| S3 | `faiss_hnsw_insert.c:514,758` | 固定 `srand` 破坏分层随机性 |
| S4 | `faiss_hnsw_insert.c:791-817` | 插入失败留下不一致状态 |

### 2.3 中等问题

| 问题 | 位置 | 描述 |
|------|------|------|
| M1 | `faiss_hnsw_create.c:63` | `M==1` 时 `log(1)=0` 除零 |
| M2 | `faiss_hnsw_insert.c` | 非 `static` 辅助函数污染命名空间 |
| M3 | `faiss_hnsw_insert.c:116-126` | Heap 模式未训练量化器 |
| M4 | `faiss_hnsw_insert.c:793` | 未拒绝 `n < 0` |
| M5 | 多处 | 有符号/无符号比较隐患 |
| M6 | `faiss_visited_table.c:37-45` | 访问表缺少边界防御 |

---

## 三、DiskANN 索引审查结果

### 3.1 致命问题

| 问题 | 位置 | 描述 |
|------|------|------|
| F1 | `diskann_merge.c:262-317,319-357` | `merge_collect_edges` 空循环，`build_cross_partition_edges` 未实现 |
| F2 | `diskann_spann.c:286-312` | `spann_search` 忽略分区参数，退化为全局搜索 |
| F3 | `diskann_spann.c:433-459` | `spann_load` 只读 partition_count，分区数据全空 |
| F4 | `diskann_fresh.c:208` | `fresh_build` 变量 `dist` 未声明，k-NN 逻辑错误 |
| F5 | `diskann_fresh.c:421-461` | `fresh_update_entry_point` 空壳，`*new_entry_point` 始终为 0 |
| F6 | `diskann_persist.c` 整体 | 缺 CRC、字节序声明、事务保证 |
| F7 | `diskann_insert.c:348-357` | `index_build` O(n^2) 复杂度 |
| F8 | `diskann_persist.c:1000-1007` | `load_extended` offset 计算错误 |

### 3.2 扩展特性状态

| 模块 | 状态 | 备注 |
|------|------|------|
| 基础 Vamana | 基本可用 | 存在 O(n^2) 等需修复问题 |
| SPANN | 完全未实现 | 搜索忽略分区，加载只读 count |
| Merge-Vamana | 完全未实现 | 空循环，空函数 |
| FreshDiskANN | 严重 bug | 未声明变量、逻辑错误 |
| K-Means 分区 | 部分实现 | 随机数未 seed，重叠向量功能失效 |

---

## 四、IVF 索引审查结果

### 4.1 致命问题

| 问题 | 位置 | 描述 |
|------|------|------|
| F1 | `faiss_ivf_core.c:260-293` | 删除中间 ID 后搜索遗漏向量，添加复用已有 ID |
| F2 | `kmeans.c:88-136` | K-Means 零距离时质心数组越界写 |
| F3 | `faiss_ivf_core.c:176-203` | 桶数量/索引计算整数溢出 |
| F4 | `faiss_ivf_add.c:78-167` | 批量添加失败后部分状态污染 |
| F5 | `faiss_ivf_search.c:167-172` | 搜索读取未初始化 `result_dists[0]` |
| F6 | `faiss_ivf_core.c:301-317` | Heap 模式重构空指针解引用 |

### 4.2 严重问题

| 问题 | 位置 | 描述 |
|------|------|------|
| S1 | `faiss_ivf_utils.c:339-357` | K-Means 收敛时标签与最终质心不匹配 |
| S2 | `faiss_ivf_utils.c:185-221` | 量化模式下非 L2 距离语义错误 |
| S3 | `faiss_ivf_core.c:231-246` | `drop` 未释放 Heap/存储资源 |
| S4 | `faiss_direct_map.c:88-108` | DirectMap 扩容失败静默返回 |

---

## 五、LSH 索引审查结果

### 5.1 致命问题

| 问题 | 位置 | 描述 |
|------|------|------|
| F1 | `lsh.c:201-272` | `create` 分配失败未检查 NULL |
| F2 | `lsh.c:717-738` | `_lsh_binary_entry_create` 强转释放导致崩溃 |
| F3 | `lsh.c:418-455` | `_lsh_search` 用 FLT_MAX 填充失败候选 |
| F4 | `lsh.c:382-473` | `search_batch` 部分失败语义不一致 |

### 5.2 严重问题

| 问题 | 位置 | 描述 |
|------|------|------|
| S1 | `lsh.c:435` | 搜索只用 L2，未根据 type 选择度量 |
| S2 | `lsh.c:213` | `srand` 全局状态污染 |
| S3 | `lsh.c:293-308` | `realloc` 失败不回滚 |
| S4 | `lsh.c:354-356` | `binary_vectors` 与 `max_vectors` 不同步扩容 |
| S5 | `lsh.c:570-577` | p-stable 桶索引取最低位缺乏随机化 |
| S6 | `lsh.c:663-669` | 用均匀分布近似正态分布 |
| S7 | `lsh.c:226-236` | LSH_BITWISE 签名语义与采样维度重复 |

---

## 六、存储子系统审查结果（Task 2.2）

详见 `docs/index/review/storage-subsystem-review.md`

### 关键发现

#### P0 数据安全风险
- `storage_mmap.c:349-359`：truncate 失败后 `map_addr` 可能悬空
- `storage_mmap.c:585-590`：文件大小向下对齐静默丢弃尾部字节

#### P1 正确性/性能问题
- `storage_page_file.c:32-40`：ID 与文件大小不一致
- `heap_vector_store.c:227-244`：I/O 放大 16 倍

---

## 七、按优先级修复清单

### P0：必须立即修复

| 索引 | 问题 | 位置 |
|------|------|------|
| HNSW | 内存泄漏 | `faiss_hnsw_insert.c:552-557` |
| HNSW | 栈溢出 | `faiss_hnsw_insert.c:19-20` |
| IVF | 删除 ID 生命周期 | `faiss_ivf_core.c:260-293` |
| IVF | K-Means 越界写 | `kmeans.c:88-136` |
| IVF | 整数溢出 | `faiss_ivf_core.c:176-203` |
| LSH | binary_entry 释放 | `lsh.c:717-738` |
| LSH | create NULL 检查 | `lsh.c:201-272` |
| DiskANN | O(n^2) 构图 | `diskann_insert.c:348-357` |
| Storage | mmap 悬空指针 | `storage_mmap.c:349-359` |

### P1：尽快修复

| 索引 | 问题 | 位置 |
|------|------|------|
| HNSW | 固定随机种子 | `faiss_hnsw_insert.c:514,758` |
| HNSW | ef_search 校验 | `faiss_hnsw_search.c:69-130` |
| IVF | 搜索未初始化读 | `faiss_ivf_search.c:167-172` |
| IVF | Heap 重构空指针 | `faiss_ivf_core.c:301-317` |
| LSH | 距离度量 | `lsh.c:435` |
| LSH | srand 污染 | `lsh.c:213` |
| DiskANN | 缺校验和 | `diskann_persist.c` 整体 |
| DiskANN | 扩展特性未实现 | `diskann_spann/merge/fresh.c` |

---

## 八、审查文件路径

| 模块 | 路径 |
|------|------|
| HNSW | `engineering/{include,src}/db/index/vector_index/hnsw/` |
| DiskANN | `engineering/{include,src}/db/index/vector_index/diskann/` |
| IVF | `engineering/{include,src}/db/index/vector_index/ivf/` |
| LSH | `engineering/{include,src}/db/index/vector_index/lsh/` |
| 存储 | `engineering/{include,src}/db/index/{heap,storage_backend}/` |

---

*报告生成时间：2026-07-15*
