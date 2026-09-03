# LSM-Tree 与 kv_engine 的协作对照

## 概述

kv_engine.c（第 145-163 行）实现的是**单层 KV 存储**，每次 `kv_put` 直接追加到文件末尾，
依赖 kv.h 中的 Page Hash 管理实现原地更新。而 LSM-Tree 则是**多层合并结构**，
写入路径为 WAL → MemTable → SSTable，读取路径为 MemTable → 逐层 SSTable。

## MemTable 协作

kv_engine 的 `kv_put` 在 kv_t 中维护一个 `page_map`（Hash 表），新写入直接覆盖 Hash 槽位，
是无界增长的数据结构。相比之下，LSM-Tree 的 MemTable（main.c 第 88-115 行）采用 SkipList 组织，
天然按 key 排序，为后续 SSTable 顺序写入提供便利。当 MemTable 达到容量上限（CAPACITY = 8192）
时，触发 flush 操作，将跳表顺序遍历的结果写入 SSTable。

## WAL 保护机制

kv_engine 依赖 `kv_flush`（第 133 行）在 `kv_close` 前将内存页刷到磁盘，是粗粒度的批持久化。
LSM-Tree（main.c 第 120-140 行）在每次 `lsm_put` 时立即调用 `wal_write`（第 128 行），
通过 `fflush` 确保写操作同步落盘。WAL 记录包含 op 类型和完整的 key-value 对，
宕机重启后可重放日志恢复未刷盘的 MemTable 数据。

## SSTable 分层写入

kv_engine 的数据按页组织，通过 Page ID 定位，单层线性扫描范围查询。
LSM-Tree 的 SSTable 按 Level 分层，每层容量指数增长（LevelDB 典型值为 ×10），
同一 Level 内的 SSTable key 范围互不重叠。main.c 第 142-168 行演示了简化的 flush 逻辑：
按跳表遍历顺序写入 SSTable，同时将 key 追加到 Bloom Filter。
Level 旋转策略（每 3 次 flush 升一层）展示了层间溢出的基本思想。

## 读取路径对比

kv_engine 的读取（kv_engine.c 第 348-354 行）通过 Hash 定位 O(1) 完成，
但写放大严重（每次更新都要写完整的 value）。LSM-Tree 的读取（main.c 第 178-192 行）
先通过 Bloom Filter 快速排除不存在的 key（O(1)，可能有假阳性），若 Bloom 命中再查询 MemTable，
未命中再逐层查询 SSTable。写放大低（追加写入），读放大（需要合并多层）为代价换取写入性能。

## 总结

kv_engine 适合写少读多的单一数据集；LSM-Tree 通过分层写入实现高写入吞吐，
通过 Bloom Filter + MemTable 缓存实现可接受的读取延迟。二者的根本差异在于
**写入路径的选择**（原地更新 vs. 追加分层）。
