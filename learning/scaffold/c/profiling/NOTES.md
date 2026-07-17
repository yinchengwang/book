# profiling 学习笔记

## 概念地图

性能剖析不是"猜哪里慢"——它是**用工具测量**，然后**按数据决策**。一个 profiling 的正确流程：measure → hotspot identification → hypothesis → optimization → measure again：

- **Wall time vs CPU time vs Self time**：
  - Wall time：墙上时钟流逝（= 用户体感延迟）。包含 I/O 等待、sleep、其他进程抢占
  - CPU time：真正占用的 CPU 周期。gprof 按函数分解 CPU time
  - Self time：函数**自己**的耗时（不包含它调用的子函数）。高 self time = 这个函数本身需要优化；低 self time 高 total time = 它的子函数慢
- **Cache miss 的三种类型**：
  - Compulsory miss（首次访问必然 miss——不可优化）
  - Capacity miss（working set 超过 cache 大小——需要 tiling/blocking 缩小工作集）
  - Conflict miss（不同地址映射到同一 cache set——需要 padding 或改变访问步长）
  - 本卡的矩阵乘法优化解决的是 capacity miss——loop interchange 让内层循环的访问范围从 N 个 cache line 降到 ~1 个
- **profiling 工具链层次**：clock()（最粗）→ gprof（函数级）→ perf（硬件计数器）→ cachegrind（cache 模拟）→ VTune（微架构分析）。每提升一层，定位精度上一个数量级，但使用门槛也上一个数量级
- **Amdahl 定律**：`speedup = 1 / ((1-P) + P/S)`。如果优化了 90% 耗时的代码使速度提升 100x，整体加速比 = `1/(0.1 + 0.9/100) = 9.17x`。**优化的上限由不可优化部分决定**——在优化 5% 耗时的代码上花一天不如在 80% 耗时的代码上花一小时

## 踩坑记录

1. **不要优化未经 profiling 的代码**：程序员对"哪里慢"的直觉准确率 ~20%。本卡如果没跑计时就猜"malloc 的 hash 计算慢"，实际瓶颈在 malloc/free 的锁开销不在 hash
2. **-O2 和 -O0 的 profiling 结果完全不同**：`-O0` 下函数调用开销占主导（没有内联），`-O2` 下内联后真实热点才暴露。永远用 `-O2` 做 profiling
3. **clock() 的分辨率问题**：CLOCKS_PER_SEC 通常 = 1000（1ms 精度）。如果函数的耗时 <1ms，clock() 读到的可能是 0。解决办法：循环调用 N 次，除以 N
4. **编译器可能优化掉你想测量的代码**：如果用 `int x = 0; for(...) x += ...;` 但从不使用 x，编译器可能整个删除这个循环。本卡用 `printf` 在最后一次迭代打印 hash 来"锚定"计算——运行时开销可忽略，但防止了死代码消除
5. **本机 MinGW 无 gprof/perf/valgrind**：用 `clock()` 做手动计时兜底。`make profile` / `make perf` 会 graceful 降级（exit 0 + skip 消息）

## 工程对照（≥100 字硬约束）

性能剖析在本仓库的热路径代码中有直接应用价值：

1. **Buffer Pool 的 Clock-Sweep eviction**：`engineering/src/db/storage/bufmgr.c` 中的 `clock_sweep_evict()` 是数据库最热路径——每次需要新页面时调用。用 `clock()` 计时一次 eviction scan 的耗时（取决于 buffer pool 大小），建立基准数据。如果发现 eviction 占总查询时间的 20%+，可以考虑优化策略（LRU 预扫描、后台 cleaner 线程）
2. **HNSW 搜索的 beam search**：`engineering/src/db/index/vector_index/hnsw/faiss_hnsw_search.c` 中的 beam search 每步遍历候选邻居——时间复杂度 O(k × degree)，其中 degree 是图的出度数。如果 k=100, degree=32，就是每层 3200 次距离计算。cache locality 在这里是决定 QPS 的关键——候选列表的访问模式（何时载入 vector data、何时计算距离）决定 L2 cache hit rate
3. **排序算法是性能微观优化的经典范例**：`engineering/src/algo-prod/sort/sort.c` 中的三种 O(n²) 排序在 n<16 时比 qsort 快（因为 O(n log n) 的常数因子更大）——这是"小数组排序"在 BTree 节点内排序中的实用优化。本卡的 `clock()` 计时模式可直接用于对比不同排序算法在不同 n 值下的性能
4. **磁盘 I/O 是 Wall time vs CPU time 差异的主要来源**：`engineering/src/db/storage/page/page.c` 中的页面读写涉及磁盘 I/O——此时 Wall time 远超 CPU time，优化方向应从"减少 CPU 周期"转向"减少磁盘访问次数"（更大的 buffer pool、预读、延迟写）

学完本卡后能动手的事：用 `clock()` 为 `engineering/src/db/storage/bufmgr.c` 的 `clock_sweep_evict()` 函数加上性能埋点——插入 `get_time_ms()` 计时，输出每次 eviction scan 的耗时，建立 buffer pool 性能基线。
