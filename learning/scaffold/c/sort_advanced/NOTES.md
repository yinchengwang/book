# sort_advanced 学习笔记

## 概念地图

从 O(n²) 到 O(n log n) 的跃迁——四种排序代表了不同的工程权衡：

- **快速排序**：分治 + partition。选 pivot 是关键——三数取中缓解有序数组退化。小数组回退插入排序（Introsort 的两个核心优化都在这里）。Linux `qsort`、C++ `std::sort` 的实现基础
- **归并排序**：严格 O(n log n)，稳定，但需 O(n) 辅助空间。是外部排序的基础（多路归并 → 磁盘 I/O 优化）。TimSort（Python/Java 默认）是归并 + 插入的混合——利用现实数据"部分有序"的特点
- **堆排序**：基于最大堆，原地排序（O(1) 辅助空间），O(n log n) 严格。不稳定。适合内存极度受限的场景。Linux 内核 `sort()` 用堆排——因为不能容忍快排 O(n²) 最坏
- **计数排序**：非比较排序，O(n + k)。仅适用于整数且范围 k 不大。基数排序的基础——把"按范围计数"泛化为"按每位计数"

与 `sort_basic` 的关系：快排在小数组回退插入排序——就是前卡讲的插入排序。Introsort 在递归深度超限时回退堆排序

## 踩坑记录

1. **快排 pivot 选择**：固定选最左/最右在已有序数组上退化为 O(n²)——三数取中缓解但无法完全消除（构造特定输入仍可退化）。工业级用"九数取中"或随机 pivot
2. **归并的辅助数组复用**：如果每次 merge 都 malloc/free 临时数组，性能极差——工程实现一次分配、反复使用。本 demo 在 `mergesort()` 入口分配一次，递归传递指针
3. **堆排建堆 O(n) 证明**：从后往前 sift-down 看似 O(n log n)，但实际是 O(n)——因为大部分节点高度很小。这是经典面试题
4. **计数排序的稳定性**：反向遍历原数组填充结果数组可保证稳定性——本 demo 简化实现（直接按频次填充），不稳定。基数排序需要稳定的计数排序

## 工程对照（≥100 字硬约束）

高级排序在工程侧的直接复用点：

1. **BTree 内部排序**：`engineering/src/db/index/btree/btreeam.c` 中 BTree 页面分裂时需要把 m+1 个 key 分为两半——中间 key 上提到父节点。这个过程需要"找中位数"，本质是 partition 的特例。本卡的快排 partition 是 BTree 分裂的前置理解
2. **外部归并排序**：`engineering/src/db/storage/wal/wal_buf.c` 中检查点恢复时需要按 LSN 排序日志条目——日志量可能远超内存，只能用外部归并排序（多路归并 + 磁盘临时文件）。本卡归并的 merge 函数是外部排序的基础单元
3. **Top-K 用堆**：`engineering/src/algo-prod/` + `rag/src/rag/index/hnsw_index.cpp` 中 KNN 搜索的 beam search——维护大小为 K 的最小堆作为候选集。本卡堆排的堆操作（sift-down）是最小实现
4. **基数排序在 VDB 中的应用**：`engineering/src/db/index/vector_index/` 中 IVF 的倒排列表按 vector ID 排序——vector ID 是有限范围的整数（0~N-1），用计数排序或基数排序 O(n) 完成，比快排 O(n log n) 更快

学完本卡后能动手的事：在 `engineering/src/db/storage/wal/` 的 WAL 回放逻辑中识别出排序调用点——如果 LSN 数列基本有序（通常就是），评估是否该用 TimSort 而非 qsort
