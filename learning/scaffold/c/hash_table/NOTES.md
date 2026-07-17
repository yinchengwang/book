# hash_table 学习笔记

## 概念地图

哈希表是 O(1) 平均查找时间的数据结构——但"平均"这个词承载了大量工程细节：

- **哈希函数**：输入任意长度 key，输出固定范围整数。两个核心指标：均匀性（减少冲突）和速度（每次查表都要算一次）。djb2 用乘 33 累积，极简极快；MurmurHash 用乘常数+移位+异或混合，分布更均匀
- **链地址法（Chaining）**：每个 bucket 是一个链表头。冲突时新节点插入链表。负载因子可超过 1.0。Java HashMap、C++ `std::unordered_map` 标准实现
- **开放定址法（Open Addressing）**：所有数据存数组内。冲突时探测下一个空位。线性探测（index+1）最简单但易产生主聚类；二次探测（index+i²）和双重哈希（两个哈希函数组合）更好
- **Rehash**：负载因子超过阈值时，创建更大数组（通常 2×），把所有 key 重新哈希插入。是 O(n) 操作但摊销到每次 put 只有 O(1)
- **负载因子** = size / capacity。链地址法 λ>1.0 触发 rehash；开放定址法 λ>0.7 触发 rehash

## 踩坑记录

1. **模运算与负数**：C 语言 `-1 % 8 = -1`，如果哈希值可能为负，需要 `(hash % cap + cap) % cap`。本 demo 用 `unsigned long` 避免负数
2. **开放定址法删除不能用 NULL**：如果直接置 NULL，后续探测链被切断——需要用"墓碑"标记（deleted sentinel）。本 demo 简化处理——不删除，仅演示插入和查找
3. **strdup 内存泄漏**：`strdup` 分配堆内存——本 demo 的 `oht_free()` 和 `cht_free()` 分别释放。生产代码中通常 key 由外部管理而非 strdup
4. **哈希碰撞攻击**：如果攻击者知道哈希函数，可以构造全部冲突的 key 使哈希表退化为 O(n) 查找——生产代码用随机种子（per-process random seed）防御

## 工程对照（≥100 字硬约束）

哈希表在工程侧的直接复用点：

1. **Buffer Pool Hash 表**：`engineering/src/db/storage/buffer/bufmgr.c` 中 `BufHashEntry` 用链地址法实现 `page_id → BufferDesc*` 的 O(1) 查找。`BufHashLookup` 函数就是本卡 `cht_get` 的工程版——加了 partition lock 并发控制
2. **Catalog Hash 查找**：`engineering/src/db/storage/catalog/catalog.c` 中 `CatalogSearch` 用链地址法从 `OID → CatalogEntry*`——本卡链地址法的 bucket 链表直接对应这里的 hash bucket
3. **HNSW visited 表**：`rag/src/rag/index/hnsw_index.cpp` 中 HNSW 搜索时维护 `visited_nodes` 哈希集合——用于去重，避免同一节点在 beam search 中重复扩展。用开放定址法而非链地址法——因为不需要存 value，只需标记 visited
4. **Page Buffer 的 Hash 定位**：`engineering/src/db/storage/buffer/bufpage.c` 中 `PageHash` 函数用高 32 位和低 32 位异或——本质是 MurmurHash 的变体。本卡的 `murmur_oat` 是其最简原型

学完本卡后能动手的事：在 `engineering/src/db/storage/catalog/` 中定位 `CatalogSearch` 调用链，用本卡知识画出 OID→表名的 hash 映射路径——理解为什么 Postgres 的 `SearchSysCache` 不用开放定址法（需要支持并发淘汰）
