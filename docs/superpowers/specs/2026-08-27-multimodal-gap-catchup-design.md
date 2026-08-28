# 多模态数据库差距追平方案（2026）

> 日期：2026-08-27 ｜ 状态：待评审
> 依据：`docs/multimodal-gap-analysis-2026/`（22 领域差距深度分析）
> 载体：OpenSpec 分阶段变更（15 个）
> 路线决策：**外围能力全部自研**（对象存储/全文搜索/可观测日志不集成开源，从零实现）
> 目标深度：**P0+P1+P2 全量 + 功能/设计/性能/可维护性四维度**

---

## 一、目标线与四维度衡量标准

| 维度 | 目标线 | 衡量标准（验收可执行） |
|------|--------|----------------------|
| **功能（正确性）** | 差距报告 §5.1-5.3 全部条目关闭 | ① P0 缺陷清零（每个缺陷先有复现测试再有修复）② P1/P2 功能项有专项测试 ③ 跨模态 E2E 测试基线建立 |
| **设计** | 模块边界与契约完整 | ① 统一并发原语（`mmdb_lock` 推广到 5 模态）② 统一错误码（`DBERR_*`）③ `mm_storage` 契约补全（drop/scan/TID/序列化版本）④ 三套 HNSW 路径收敛为一 ⑤ WAL 统一覆盖全部写路径 |
| **性能** | 各模态达到业界可用线 50%+ | ① 基准测试进 CI（`engineering/test/db/benchmark/` 扩展）② Vector 搜索 ≥2.5K QPS、插入 ≥150K vec/s ③ Timeseries 热路径压缩率 ≥5x ④ 各模态基准报告落 `docs/` |
| **可维护性** | 测试与工具链护航 | ① ASAN/UBSAN 进 CI（Linux 侧 MinGW 无 libasan 用 CI Linux）② Release UAF 8 个存量清零 ③ 每模态 DESIGN.md 与实现同步 ④ 错误路径清理自动化（MemoryContext 覆盖执行器） |

**明确不在本期范围**（多年级工程，本期不做）：分布式分片与 Raft 集群、GQL 标准合规认证、100+ 图算法全量、对象存储 Erasure Coding/多 DC、PostGIS 1000+ ST_* 全量。

---

## 二、通用分层框架

所有模态统一按 8 层拆解，后续每模态的差距对比与追平措施均落到这 8 层：

```
L1 协议/接口层   —— 客户端协议、API 契约、Schema 定义
L2 查询/执行层   —— 解析、优化、执行计划、迭代器
L3 索引层       —— 主索引/辅助索引结构
L4 核心算法层   —— 模态特有算法（量化/图算法/压缩/几何）
L5 存储布局层   —— 页面/段/列存布局、空间复用
L6 缓冲层       —— Buffer Pool、缓存、置换
L7 WAL/持久化层 —— redo log、checkpoint、fsync 策略
L8 并发/事务层  —— 锁、MVCC、事务边界
```

---

## 三、跨模态公共层差距对比与追平（地基，阶段 0）

### 3.1 L8 并发控制层

| 项 | 自研现状 | 业界标杆设计 | 设计差异 | 追平措施 | 变更 |
|----|---------|-------------|---------|---------|------|
| 锁原语 | 5 个模态各自复刻 buggy `simple_rwlock_t`（vector_engine.c:1557 / ts_engine.c:752 / doc_engine.c:757）；Graph/Spatial 零锁 | PostgreSQL：LWLock 体系；Qdrant：不可变 segment + RwLock；SQLite：文件锁 + WAL 并发 | **已有正确组件未复用**——`include/sdk/impl/mmdb_lock.h`（SRWLOCK/pthread_rwlock 跨平台 wrapper，P5 落地）只在 SDK 层使用 | 将 `mmdb_lock.h` 从 SDK 层提升到 `include/db/` 公共层，5 个模态替换各自 simple_rwlock | C0-1 |
| 默认安全 | `use_lock=false` 默认（vector_engine.c:219 等） | 业界默认并发安全，无锁是 opt-in | 安全与性能的默认值颠倒 | 改为默认加锁 + `mm_disable_lock()` 显式关闭（仅 benchmark 场景） | C0-1 |
| 写时并发 | faiss_hnsw add 直接 realloc（faiss_hnsw_stubs.c:211-241），CSR grow 同（graph_csr.c:46-64） | Qdrant：segment 不可变 + 后台 merge；FAISS：批量构建期无并发搜索 | 写路径原地改共享结构 | 插入走「copy-on-write 批量段 + 原子指针切换」：add 攒批到 immutable buffer，search 持旧快照 | C1-2 |
| 事务边界 | DML 无事务包裹（nodeModifyTable.c:56）；图/KV 写操作裸奔 | PostgreSQL：事务命令驱动 begin/commit；FoundationDB：乐观事务 | 无事务状态机 | `txn_begin/commit/rollback`（已实现于 txn/txn.c:296-392）接入 SQL 执行器入口 | C2-1 |

### 3.2 L7 WAL/持久化层

| 项 | 自研现状 | 业界标杆设计 | 设计差异 | 追平措施 | 变更 |
|----|---------|-------------|---------|---------|------|
| 覆盖范围 | 仅 KV 接入共享 WAL（kv.c:443-447）；Vector 有独立 WAL；Relational/Timeseries/Spatial/Tree 零 redo | PostgreSQL：全 DML XLogInsert；RocksDB：所有写过 WAL | 三套并存且 4 模态空白 | 共享 WAL（storage/wal/）扩展：新增 heap/ts/spatial/yang 记录类型，五模态统一入口 `db_wal_log()` | C0-2 |
| 刷盘语义 | SYNC 模式只 fflush 无 fsync（vector_wal.c:282-289）；checkpoint 同（:404） | PostgreSQL：`synchronous_commit` 分级 + fsync；Redis AOF：always/everysec/no | 只防进程崩溃不防断电 | 统一 `wal_flush_policy_t`（NONE/OS/BUFFER/FSYNC 四级），默认 FSYNC，GUC 可调 | C0-2 |
| 失败语义 | WAL 写失败仅 LOG_WARN 继续写主存（vector_engine.c:377-379）；KV 忽略 wal_write_* 返回值（kv.c:445,459） | PostgreSQL：WAL 失败 = 事务 abort | WAL-first 语义被破坏 | WAL 写失败 → 返回错误并中止当次 DML；页面写入移到 WAL 成功之后 | C0-2 |
| 恢复路径 | KV 有 kv_wal_apply（kv.c:671）；Vector 有 replay（vector_wal.c:528）；其余无 | PostgreSQL：启动时 redo 重放 + 两阶段 | 恢复能力不对称 | 统一启动恢复流程 `db_startup_recover()`：按模态注册 apply 回调，启动时统一重放 | C0-2 |

### 3.3 错误码与资源管理层

| 项 | 自研现状 | 业界标杆设计 | 设计差异 | 追平措施 | 变更 |
|----|---------|-------------|---------|---------|------|
| 错误码 | KV 有 7 值枚举（kv.h:44-52）；Vector 返回 -1/0；SQL 层另有体系 | PostgreSQL ERRCODE_* 全局分类体系 | 各模态私有码，跨层无法统一判断 | `include/db/errors.h` 扩展统一 `DBERR_*` 空间（模态前缀 + 通用类），各模态错误码映射进来 | C0-3 |
| 资源清理 | 执行器初始化手工逐资源 free（nodeSeqscan.c:160-176）；Yang goto fail 多处 | PostgreSQL：MemoryContext 统一 Reset | memctx 已有（sql/memctx.c 834 行）但未覆盖执行器初始化 | PlanState/EState 创建挂到 per-query MemoryContext，EndPlan 一次 Reset | C0-3 |
| 静默成功 | drop 空操作返回 0（vector_engine.c:351-354）；scan 恒 NULL（:435-444） | 未实现接口返回 ENOTSUP 类明确错误 | 假成功比报错危险 | `mm_storage` 契约补全：未实现操作返回 `DBERR_NOT_IMPLEMENTED` | C0-3 |

### 3.4 序列化契约（L1 公共）

| 项 | 自研现状 | 业界 | 追平措施 | 变更 |
|----|---------|------|---------|------|
| mm_insert 数据格式 | 手工字节偏移（vector_engine.c:360-372），无版本 | 每格式带 magic+version | 定义 `mm_record_header_t`（magic + version + model + len），各模态解析统一走头部 | C0-3 |

---

## 四、各模态分层差距对比与追平方案

### 4.1 Vector 模态

对标：Milvus / FAISS / Qdrant / pgvector

| 层 | 自研设计 | 标杆设计 | 设计差异 | 追平措施 | 变更 |
|----|---------|---------|---------|---------|------|
| L1 API/Schema | mm_insert 手工字节流；集合无 schema 结构 | Milvus Collection：字段化 schema + DML 协议 | 无类型契约 | `vector_collection_schema_t`（dim/metric/索引配置）+ 序列化版本号 | C0-3 |
| L2 查询 | selector（vector_index_selector.c 380 行）+ 过滤搜索（faiss_hnsw_search_filtered.c） | Milvus 布尔表达式预/后/混合过滤；Qdrant payload 过滤可配 | 过滤仅简单等值 | 过滤表达式 AST 求值器（复用 SQL 层 expr.c） | C3-5 |
| L3 索引 | 21+ 索引并列 + **三套 HNSW 路径**（storage/vector/faiss_hnsw_stub.c、index/vector_index/faiss_hnsw/、index/vector_index/hnsw/ 占位） | FAISS 索引类层次 + 工厂；Milvus index engine 插件化 | 无统一索引抽象，路径冗余 | `vector_index_vtable_t` 统一接口（build/add/search/serialize），三路收敛为一，旧头 24 处引用迁移 | C4-2 |
| L4 算法/量化 | PQ/SQ/BQ/OPQ/ITQ/LVQ/RQ 各自独立实现 | FAISS：OPQ 旋转与 PQ 联合训练；ScaNN：各向异性量化 | 量化器间无联合优化 | OPQ 旋转矩阵训练接入各量化器入口 | C3-5 |
| L4 度量 | IP 度量 fallback L2²（faiss_hnsw_search.c:46-52） | FAISS：IP 用倒序比较器 | **语义错误** | IP 路径独立比较器（距离 = -inner_product）或建索引时显式拒绝 | C1-2 |
| L4 SIMD | 底层 search_layer 已接 AVX2（faiss_hnsw_search_layer.c:31-53，P6-M1.3）；上层贪婪下降 compute_distance_internal 仍标量（faiss_hnsw_search.c:21-54） | FAISS 全路径向量化 | 部分接入 | compute_distance_internal 接入同一 SIMD 距离函数族 | C4-1 |
| L5 存储 | VecPage 页池 + 文件回退（vector_engine.c:383-432） | Milvus immutable segment + sealed merge | 页池原地写 | 段式布局：活跃段 append-only，sealed 段只读 | C3-5 |
| L7 持久化 | 独立 vector_wal（无 fsync）+ WAL 记录 VLA 栈溢出（vector_wal.c:261）+ 异步 memcpy 越界（:292-298） | Milvus WAL + checkpoint | 同步语义与内存安全双缺陷 | ① WAL 记录改堆缓冲 ② 接入 C0-2 统一刷盘策略 ③ 删除独立 WAL 并入共享 WAL（或保留但修缺陷） | C0-2 |
| L8 并发 | buggy rwlock + faiss_hnsw 无锁 realloc | Qdrant segment + RwLock | UAF 风险 | mmdb_lock 推广 + COW 快照（见 3.1） | C0-1/C1-2 |
| 删除/GC | bitmap + graph_repair + vacuum（delete/ 三文件） | Milvus compaction 后台合并 | 无调度器 | 后台 compaction worker（复用 bgworker/ 框架） | C3-5 |

**Vector 关键设计差异详述**：自研版与 Qdrant 的根本差异在「段不可变性」——Qdrant 的写路径永远只 append 到活跃段，封口后只读，搜索拿段快照，因此天然无锁；自研版原地修改共享数组，靠（有缺陷的）锁兜底。C1-2 引入 COW 段是追平此设计的关键。

### 4.2 Relational 模态

对标：PostgreSQL / DuckDB / SQLite

| 层 | 自研设计 | 标杆设计 | 设计差异 | 追平措施 | 变更 |
|----|---------|---------|---------|---------|------|
| L1 协议 | pgwire.c 1292 行 + MySQL Wire + REST | PostgreSQL 原生协议 + 扩展生态 | 广度已够 | 维持；补 Extended Query 协议 | C3-5 |
| L2 解析 | parser/（Flex/Bison，~4200 行规格） | PostgreSQL gram.y | 规模相当 | 维持 | - |
| L2 优化 | optimizer.c 五条规则全是 TODO 桩（:199-304）+ cost.c 选择率仅等值（:205-216） | PostgreSQL：动态规划 join + GEQO + 直方图/MCV/相关性 | **优化器是空壳** | ① ≤10 表 DP join 枚举 ② AttStats 扩展 histogram/MCV ③ 摘假开关直到实现 | C2-2 |
| L2 执行 | Volcano 12 算子 + Gather 并行 | PG17 仍 Volcano（15+ 算子）；DuckDB 向量化 | 算子数差距小，无向量化 | 向量化 TupleBatch 中间表示（先 SeqScan/HashAgg 两算子试点） | C4-1 |
| L2 DML | ModifyTable 伪造 TID（nodeModifyTable.c:70-75,96-99）+ 错误静默（:62-64,117） | PostgreSQL：slot 携带 tid 全链路 | **改错行** | ① TupleTableSlot 增加 tts_tid，扫描算子填充 ② heap 返回真实 (block,offset) ③ DML 失败中止 | C1-1 |
| L3 索引 | index/btree 内存教科书版（CLRS top-down） | PG nbtree 页式 + dedup；SQLite 页式 + cell 层 | 非页式、无叶子链、value 内联 | B+Tree 页式化（页面复用 Buffer Pool，叶子链支持范围扫描） | C3-5 |
| L3 索引种类 | 仅 B-Tree | PG：GiST/GIN/BRIN/Hash/SP-GiST | 5 种缺 4 | 本期只加 Hash（等值）+ BRIN（时序大表），GiST/GIN 延后 | C3-5 |
| L4 统计 | AttStats 只有 ndistinct | PG：analyze 采样 + histogram + MCV + correlation | 选择率估算原始 | ANALYZE 命令 + pg_stats 视图 | C2-2 |
| L5 存储 | heapam 紧凑页 + 无 FSM（heapam.c:305-364），删除空间永不复用 | PG：FSM + TOAST + 多次空闲空间复用 | 空间复用缺失 | ① FSM（每页 bitmap）② TOAST 大元组外存（对接 C3-1 Blob） | C3-5 |
| L7 WAL | heap DML 零 redo（heapam.c:294-382） | PG：heap_insert → XLogInsert 全覆盖 | 崩溃不可恢复 | 接入共享 WAL（heap insert/delete/update 三种记录） | C0-2 |
| L8 事务/MVCC | txn.h 4000+ 行完整 MVCC 但**零调用方**（heap_visibility.c:30 无调用；SeqScan 无可见性过滤 nodeSeqscan.c:223-256；heap_insert 不戳 xmin heapam.c:300） | PostgreSQL：可见性判断是扫描路径内建 | **模块完成 ≠ 系统完成** | ① ExecSeqScan 调 heap_tuple_visible ② heap_insert/delete 戳 xmin/xmax ③ SQL 层 BEGIN/COMMIT 驱动 txn_begin/commit ④ vacuum 接入 | C2-1 |

**Relational 关键设计差异详述**：自研与 PG 的差距不在"有没有 MVCC 代码"而在"可见性判断是否内建到每次堆扫描"——PG 的 heapgettup 每取一行都过 HeapTupleSatisfiesMVCC；自研的等价函数写好了却没人调用。C2-1 是接线工程而非新算法。

### 4.3 Graph 模态

对标：Neo4j / Memgraph / NebulaGraph

| 层 | 自研设计 | 标杆设计 | 设计差异 | 追平措施 | 变更 |
|----|---------|---------|---------|---------|------|
| L1 查询语言 | graph_cypher.c 1024 行 Cypher 子集 | openCypher 全集 + GQL 标准 | 无变长路径/OPTIONAL MATCH/UNWIND | ① 变长路径 `*1..n` ② OPTIONAL MATCH ③ UNWIND | C3-5 |
| L2 执行 | gqlExec.c 359 + traverse.c 709 | Neo4j slot-based pipeline + morsel 并行 | 单线程解释执行 | 维持（规模不足时不值得重写） | - |
| L4 算法 | 7 个（BFS/DFS/BFS最短/Dijkstra/PageRank/连通分量/统计，graph_algorithms.c） | Memgraph MAGE 100+；Neo4j GDS 60+ | 差 90+ | 本期 +10 核心（betweenness/closeness/katz/Louvain/leiden/三角计数/弱连通/jaccard 相似/节点度分布/环路检测） | C3-5 |
| L4 PageRank | pagerank_new（graph_algorithms.c:678）悬挂节点处理未验 | NetworkX：dangling mass 均匀再分配 | 疑似精度偏差 | 单元测试对拍 NetworkX 参考输出 | C2-3 |
| L5 存储 | CSR + COO 增量缓冲（graph_csr.c:111-115）+ compact 重建（:427） | Neo4j：property chain 原地更新；TigerGraph：CSR 压缩 | 取舍合理，但 COO→CSR 转换窗口数据不可查询 | 双视图：查询走旧 CSR，写入走 COO，compact 后原子切换 | C2-3 |
| L3 索引 | 标签索引（graph_csr.c:565）+ graph_index 两套 | Neo4j：range/text/全文/向量索引 | 无属性范围索引 | 属性 B-Tree 二级索引（复用 index/btree） | C3-5 |
| L7 持久化 | graph_csr_save（:206）无 fsync；崩溃后 COO 未 compact 数据恢复路径未核 | Neo4j transaction log + checkpoint | 崩溃一致性未证 | ① save 接 fsync ② 启动时 COO 重放进 CSR | C0-2/C2-3 |
| L8 并发 | 全文件零锁（graph_csr.c grep 零命中） | Neo4j 锁管理器（行级）；Memgraph MVCC | 无并发控制 | mmdb_lock + COW CSR（同 4.1 段式思路） | C0-1/C2-3 |

### 4.4 KV 模态

对标：Redis / RocksDB / FoundationDB

| 层 | 自研设计 | 标杆设计 | 设计差异 | 追平措施 | 变更 |
|----|---------|---------|---------|---------|------|
| L1 API | put/get/delete/scan（kv.h:123-182）；无 CAS/Watch/Multi | Redis MULTI/WATCH；FDB 乐观事务；etcd Txn | 无并发控制原语 | `kv_cas`（compare-and-swap）+ `kv_txn`（begin/put/commit） | C3-5 |
| L1 错误码 | 7 值枚举（kv.h:44-52），page full 误用 KV_ERROR（kv.c:453） | RocksDB Status 分类完整 | 分类粒度粗 | 扩展 KV_FULL/KV_CONFLICT/KV_LOCKED | C1-3 |
| L3 有序 | kv_ordered.c 419 行字节序扫描 | RocksDB：MemTable 跳表有序；支持 comparator 定制 | 仅 memcmp 序 | `kv_comparator_t` 注入点 | C3-5 |
| L5 存储 | 页式紧凑布局，**无页分裂**（page full 即失败 kv.c:451-455）；更新走 delete+insert（:431-435） | RocksDB LSM（memtable+SSTable+compaction）；Redis dict 渐进 rehash | 空间受限、写放大 | ① 页分裂（半满分裂 + 父节点上提，复用 index/btree 逻辑）② Value 1MB → 16MB（溢出页） | C1-3 |
| L6 缓冲 | 共享 Buffer Pool | Redis 全内存 + LRU 淘汰 | 落后 | 维持（Buffer Pool 已是合理设计） | - |
| L7 WAL | **唯一正确接入共享 WAL 的模态**（kv.c:443-447,671），但 wal_write_* 返回值忽略（:445,459） | RocksDB WAL+Manifest；Redis AOF 重写 | 覆盖对、失败处理错 | 返回值检查 + 失败中止（C0-2 统一） | C0-2 |
| L8 并发 | 全文件零锁（kv.c grep 零命中）；结构体 lock_mgr 字段未用（kv.h:66） | RocksDB 多线程 compaction；Redis 单线程主 + IO 线程 | 丢更新 | `kv_put` 内 mmdb_rwlock 写锁包裹读-改-写序列 | C1-3 |
| L8 TTL | kv_ttl.c 587 行独立管理 | Redis：惰性删除 + 定期主动删除双策略 | 单一策略（未核全文） | 惰性 + 后台主动双策略，过期写 tombstone 进 WAL | C3-5 |
| CF | cf_engine/cf_row/cf_column 三层（P3-4） | RocksDB CF + atomic cross-CF write | 已对齐基本形态 | 补跨 CF 原子写（WriteBatch） | C3-5 |

### 4.5 Timeseries 模态

对标：InfluxDB 3.0 / TimescaleDB / TDengine / QuestDB

| 层 | 自研设计 | 标杆设计 | 设计差异 | 追平措施 | 变更 |
|----|---------|---------|---------|---------|------|
| L1 查询 | ts_sql_functions.c 456 行 SQL 函数 | PromQL/Flux/InfluxQL/SQL 全套 | 函数面窄 | derivative/rate/percentile/first/last 核心函数 | C3-5 |
| L2 连续聚合 | ts_continuous_agg.c 514 行（config/state 分离）+ ts_mview.c | TimescaleDB：policy 驱动 + 增量窗口刷新 | 刷新策略简化 | 窗口级增量刷新（只重算迟到数据覆盖的窗口） | C3-5 |
| L4 压缩 | ts_compress.c：add 阶段存原始 16B/点（:148-149），flush 才 Gorilla XOR（:175+） | Influx IOx：写入即列式编码；Gorilla 全程生效 | **热路径无压缩** | 增量编码器：每点即时 delta-of-delta + XOR 追加位流 | C2-4 |
| L4 乱序 | 未核（ts_segment.c 458 行） | Influx/Timescale：时间桶重写或 merge-on-read | 疑似不支持 | 按时间桶分段的 merge-on-read（迟到数据进旁路缓冲，查询时归并） | C2-4 |
| L5 列存 | ts_columnar.c 716 行 | ClickHouse MergeTree + 多级压缩编码 | 单级编码 | 列式块 + 每列独立编码（RLE/dictionary/Gorilla 按列型选） | C2-4 |
| L3 索引 | segment + tag_index（524 行） | Timescale：chunk 裁剪 + 分区索引；Influx：tag 倒排 | 高基数标签效率未证 | tag 倒排 + 基数上限保护（超限标签降级为全扫） | C3-5 |
| L6 保留 | ts_retention.c 418 行 | retention policy 自动 drop | 已有基础 | 维持 + 后台调度接入 bgworker | C3-5 |
| L7 持久化 | 无 WAL（grep 零命中） | Influx WAL；Timescale 走 PG WAL | 空白 | 接入共享 WAL（ts append 记录） | C0-2 |
| L8 并发 | 复刻 buggy rwlock（ts_engine.c:752-770） | QuestDB 单写多读；Influx 单节点 | 同 3.1 | mmdb_lock 替换 | C0-1 |

### 4.6 Document 模态

对标：MongoDB / Elasticsearch / CouchDB

| 层 | 自研设计 | 标杆设计 | 设计差异 | 追平措施 | 变更 |
|----|---------|---------|---------|---------|------|
| L1 查询 | jsonpath.c 670 行 | MongoDB 查询语义（操作符/数组/投影） | 路径可用、操作符面窄 | $eq/$gt/$in/$exists/$regex 操作符族 + 数组量词 | C3-5 |
| L2 聚合 | doc_pipeline.c 1372 行 6 阶段（match/group/sort/limit/skip/project）+ doc_agg.c | MongoDB 30+ 阶段 | 核心流已有 | +$lookup（跨集合）/$unwind（数组展开）/$facet/$bucket | C3-5 |
| L4 全文分析器 | standard/whitespace/keyword 三 tokenizer（doc_fts.c:196/285/346）+ DocSynonyms 同义词（:97-175） | Lucene：IK/Jieba/Snowball/Kuromoji + 自定义链 | **缺中文分词与词干化** | ① 自研中文词典分词（正向最大匹配 + 词典可插拔，1-2 万词条起步）② 自研 Snowball（English Porter2 移植，算法公开）③ tokenizer 链式组合（char filter → tokenizer → token filter） | C2-6 |
| L4 打分 | bm25.c 299 行 | Lucene：BM25 + 字段加权 + function score + LTR | 打分不可扩展 | 字段 boost + function_score 钩子（回调注入自定义打分） | C3-2 |
| L3 索引 | doc_inverted.c 349 行倒排 | MongoDB：B-Tree+TTL+2dsphere+向量；ES：多层倒排 + doc values | 索引类型单一 | ① B-Tree 二级索引（复用 index/btree）② TTL 索引（复用 kv_ttl） | C3-5 |
| L4 高亮 | 无 | ES highlighter（统一/普通/fvh 三种） | 缺失 | postings 记录 offset + 前后文窗口高亮器 | C3-2 |
| L5 嵌套 | doc_nested.c 861 行 | MongoDB 嵌套文档语义 + nested mapping | 已有基础 | 维持 | - |
| L4 混合 | doc_vector.c 584 + sparse/hybrid_retrieval.c | ES RRF；Milvus hybrid | 已有 RRF 基础 | 维持 + 与 C3-4 多模态对象打通 | C3-4 |
| L7 持久化 | 无 WAL | ES translog；Mongo journal | 空白 | 接入共享 WAL（doc upsert 记录） | C0-2 |
| L8 并发 | 复刻 buggy rwlock（doc_engine.c:757） | Mongo WiredTiger 文档级并发 | 同 3.1 | mmdb_lock 替换 | C0-1 |

### 4.7 Spatial 模态

对标：PostGIS / DuckDB Spatial / H3 / S2

| 层 | 自研设计 | 标杆设计 | 设计差异 | 追平措施 | 变更 |
|----|---------|---------|---------|---------|------|
| L4 几何类型 | Point/LineString/Polygon（spatial_geo.c 600 行） | PostGIS 全 OGC（Multi*/GeometryCollection/CircularString/TIN） | 类型不全 | +MultiPoint/MultiLineString/MultiPolygon/GeometryCollection | C3-5 |
| L4 坐标系 | 仅平面欧氏（grep haversine/spherical 零命中） | PostGIS geography（球面）；S2/H3 全球编码 | **地理数据算错距离** | geography 类型 + Haversine/Vincenty 距离 + 度↔米转换层 | C2-3 |
| L3 索引 | R-Tree Guttman quadratic split（rtree.c:227-300，算法忠实）+ Hilbert 辅助（spatial_engine.c:489-740）+ 四叉树 | PostGIS GiST（R-Tree 变体 + picksplit 调优） | 算法对、无 GiST 泛化 | R*Tree 分裂策略（重插入降低重叠）替换 quadratic | C3-5 |
| L4 空间函数 | bbox 查询 + P2-3 补充 | ST_* 1000+ | 函数面差距最大 | +ST_Distance/ST_Within/ST_Intersects/ST_Contains/ST_Buffer/ST_Union/ST_Area/ST_Length/ST_Centroid（10 个核心，覆盖 80% 常用查询） | C3-5 |
| L5 持久化 | rtree_file.c 470 行 | PostGIS 走 PG 存储 | 已有基础 | 接入共享 Buffer Pool 与 WAL | C0-2 |
| L8 并发 | R-Tree 零锁（rtree.c grep 零命中） | PostGIS 走 PG 锁体系 | 同 3.1 | mmdb_lock + 分裂时 COW 节点 | C0-1/C2-3 |
| L4 拓扑/网络 | 无 | PostGIS 拓扑/路由/3D | 不在本期 | - | - |

### 4.8 Tree/Yang 模态

对标：libyang / sysrepo / PostgreSQL ltree

| 层 | 自研设计 | 标杆设计 | 设计差异 | 追平措施 | 变更 |
|----|---------|---------|---------|---------|------|
| L4 模型解析 | yang_model.c 递归下降（parse_leaf/list/container/leaf_list/stmt_block，:193-442），YANG 1.1 主 statement 覆盖 | libyang：全 statement + import/include/deviation/feature | 覆盖核心缺外围 | +import/include 解析 + grouping/uses 展开 | C2-5 |
| L4 XPath | 无（grep 零命中） | libyang：完整 Yang XPath 子集 | 空白 | 自研 XPath 子集求值器（axis 限定 descendant/child，谓词支持） | C2-5 |
| L1 协议 | netconf_server.c 631 行，**字符串扫描 XML，不支持属性/命名空间**（:3-6 明示） | RFC 6241：所有 RPC 必带 message-id 属性与 base 命名空间 | **标准客户端无法接入** | 自研 XML 解析器升级：属性 + 命名空间 + 前缀解析（不引入 libxml2，自研递归下降 XML tokenizer） | C2-5 |
| L2 datastore | 未确认（疑无） | sysrepo：running/candidate/startup 三态 + 事务 | 空白 | 三态 datastore + commit 校验（candidate → running 原子切换） | C2-5 |
| L1 framing | NETCONF 1.0 only | RFC 6242 chunked framing（1.1） | 版本落后 | chunked framing 解析层 | C2-5 |
| L7 持久化 | 未确认 | sysrepo：持久化 + 启动恢复 | 空白 | datastore 落盘 + 共享 WAL | C0-2/C2-5 |
| L8 并发 | 无锁 | sysrepo：订阅锁 + 读写锁 | 同 3.1 | mmdb_lock | C0-1 |
| L4 goto fail 清理 | yang_model.c:352,388 多处 goto fail 依赖人工审计 | libyang：统一错误状态机 | 清理脆弱 | AST 节点 arena 分配器（一次性释放） | C2-5 |

---

## 五、自研外围能力设计（5 个新引擎）

### 5.1 对象/Blob 存储引擎（自研，对标 S3/MinIO）

**定位与边界**：单机完整对象存储——分块布局、内容寻址、Range 读、Multipart 写。不做 Erasure Coding、多副本、多 DC（留接口）。

| 层 | 自研设计 | S3/MinIO 对标 | 差异说明 |
|----|---------|--------------|---------|
| L1 API | `blob_put/get/delete/stat` + `blob_range_get(offset,len)` + `blob_multipart_begin/upload/complete` | S3 REST 全集 | 只做数据面核心 API，不做生命周期/版本/ACL |
| L5 布局 | **分块存储**：对象切成 4MB chunk，chunk 为文件系统单位；`blob-id = SHA-256(内容)` 内容寻址，同内容去重 | S3 分块 + EC；MinIO xl.meta | 无 EC 用单副本，去重是 S3 没有的优势 |
| L3 元数据 | KV catalog：`blob-id → (chunk 列表, 总长, content-type, 时间戳)`；复用 KV 模态 + CF | MinIO xl.meta；S3 内部表 | 复用已有 KV，零新存储 |
| L6 缓存 | LRU chunk 缓存挂 Buffer Pool 之上 | MinIO 页缓存 | 复用已有 |
| L2 引用 | mm_storage 增加 `MODEL_BLOB`；Relational TOAST 大元组外存引用 blob-id | S3 独立服务 | 与数据库深度整合是自研的意义 |
| L7 持久化 | chunk 只追加 + fsync；元数据走 KV WAL | MinIO 纠删码落盘 | 简化但完整 |

**变更**：C3-1 ｜ **工作量**：M

### 5.2 全文搜索引擎增强（自研，对标 Elasticsearch/Lucene）

**定位与边界**：把 Document 模态从"有 BM25"升级为"完整单机搜索引擎"。不做分布式分片/副本（留 gossip 接口）。

| 层 | 自研设计 | Lucene/ES 对标 | 差异说明 |
|----|---------|---------------|---------|
| L4 分析器 | **链式分析器框架**：char filter → tokenizer → token filter 三段管线（DocTokenizer 接口已有，扩为链） | Lucene Analyzer 链 | 框架已有雏形（doc_fts.c），补链式组合 |
| L4 中文分词 | **自研词典分词**：正向最大匹配 + 逆向最大匹配双向校验；词典 1-2 万词条可插拔（外部文件加载）；未登录词 fallback 二元组（bigram） | IK（细/智能切分）、Jieba（HMM） | 词典分词召回够用；HMM 新词发现留扩展 |
| L4 词干化 | **Snowball English (Porter2) 自研移植**：算法公开（~500 行 C），无 license 风险 | Lucene Snowball 全语种 | 先英文，其余语种留接口 |
| L4 打分 | BM25（已有）+ **字段加权**（field boost 乘子）+ **function_score 钩子**（用户回调注入自定义打分） | ES function_score/query DSL | 打分可编程化 |
| L3 索引 | doc_inverted 扩展：**segment 化近实时索引**——内存 segment 追加，达到阈值 seal 为不可变磁盘 segment，搜索时多 segment 归并 | Lucene segment + refresh | 借鉴 Qdrant/Lucene 段式设计（与 C1-2 同构） |
| L4 高亮 | postings 记录 token offset + 前后文窗口高亮器（unified 风格） | ES 三种 highlighter | 一种够用 |
| L2 聚合 | 复用 doc_pipeline 聚合（facet = terms agg） | ES aggregations 全家 | 不重复造 |

**变更**：C3-2 ｜ **工作量**：L

### 5.3 可观测日志引擎（自研，对标 Loki/ClickHouse Observability）

**定位与边界**：标签索引 + 列式日志块 + LogQL 子集查询。不做 Trace/Metric 全家桶（后续扩展）。

| 层 | 自研设计 | Loki/CH 对标 | 差异说明 |
|----|---------|--------------|---------|
| L1 摄取 | `log_push(stream_labels, lines[])` 批量推送 API + 内存写缓冲；OTLP/Loki HTTP 协议留接口 | Loki push API；OTLP collector | 先做内部 API，协议适配后置 |
| L1 查询语言 | **LogQL 子集**：`{service="x", level="error"}` 流选择器 + `|= "keyword"` 行过滤 + `| json` 字段提取 + rate/count 聚合 | LogQL 全集 | 覆盖 80% 日常查询 |
| L3 标签索引 | **流式标签倒排**：label → stream-id 集合（复用倒排索引）；**高基数保护**：单标签基数超阈值自动降级全扫 + 告警 | Loki 流索引；CH 标签列 + 跳数索引 | 高基数是 Loki 设计核心，必须有保护 |
| L5 存储 | **按流分块列式存储**：每 (stream, 时间窗) 一个 chunk，chunk 内行式时间戳 + 原始行；chunk 压缩复用 ts_compress（Gorilla 时间戳 + zstd 式行压缩） | Loki chunk（对象存储）；CH 列式 part | 复用时序压缩组件 |
| L6 保留 | 复用 ts_retention 按时间窗 drop | Loki retention | 复用 |
| L7 持久化 | 接入共享 WAL（log append 记录） | Loki WAL | 统一 |
| L8 并发 | 写单流锁 + 读多流并行（mmdb_lock） | Loki 单写多读 | 统一 |

**变更**：C3-3 ｜ **工作量**：L

### 5.4 多模态 AI 原生存储（契合度最高，优先投入）

**定位**：把"单向量单 ID"升级为"多模态对象"——一个对象携带多 named embedding + Blob 引用 + 元数据，支撑跨模态检索与多模态 RAG（与 commit efcd239f8 的多态 RAG 架构文档闭环）。

| 层 | 自研设计 | LanceDB/Qdrant/Vespa 对标 | 差异说明 |
|----|---------|--------------------------|---------|
| L1 Schema | **NamedVector schema**：`{id, blob_ref?, metadata, embeddings: {clip: vec768, siglip: vec512, text_bge: sparse}}`——一对象多向量多模型 | Qdrant named vectors；Vespa tensor 字段 | schema 级扩展 |
| L3 索引 | 每个命名向量独立索引（复用 21+ 索引族 + selector）；**跨向量统一 top-k 归并（RRF）** | Qdrant 多向量查询 | 复用已有索引与 hybrid_retrieval |
| L4 检索 | **跨模态检索算子**：text_query → image embedding 空间（模型在外部，数据库提供空间 + 距离） | CLIP/BLIP 系（模型层） | 数据库层职责：多空间共存 + 归并 |
| L5 Blob 绑定 | 图像/音频/视频原数据存 C3-1 Blob 引擎，对象表持 blob-id | LanceDB blob 列 | 与 5.1 联动 |
| L2 RAG | 复用 engineering/rag/（rag_pipeline/graphrag_*）+ executor/rag/ 算子，扩展为多模态输入（图文混合 chunk） | LangChain/LlamaIndex 多模态 RAG | 已有 RAG 骨架是最大优势 |

**变更**：C3-4（依赖 C3-1）｜ **工作量**：L

### 5.5 宽表 Wide-Column（阶段化自研）

**定位**：Cassandra 风格 row → (column, timestamp) → value 模型。分两期：本期完成第一期。

| 层 | 第一期（本期） | Cassandra 对标 | 第二期（下期） |
|----|--------------|----------------|---------------|
| L1 模型 | `wide_row_get/put(row_key, column, ts, value)` + 范围列扫描（clustering key 排序） | CQL：partition key + clustering key | CQL 语法子集 |
| L5 存储 | 复用 KV + CF：row_key → CF，column → KV key（带时间戳版本） | Cassandra LSM commit log + memtable + SSTable | **LSM 引擎**：memtable（跳表）+ L0-Ln SSTable + Leveled compaction |
| L4 版本 | 时间戳多版本 cell（最新读 + TTL） | Cassandra cell timestamp | 写冲突 LWW 语义 |
| L3 索引 | 聚簇索引（row 内列有序）+ 二级索引（复用 B-Tree） | SSTable index + view | - |
| L7 持久化 | 走 KV WAL | commit log | LSM 自带 WAL |

**变更**：C3-5（第一期部分）｜ **工作量**：第一期 M，第二期 L（下期）

---

## 六、性能与可维护性专项

### 6.1 性能专项（C4-1）

| 项 | 措施 | 目标 |
|----|------|------|
| Vector SIMD 补全 | compute_distance_internal 接入 AVX2 族（search_layer 已有，faiss_hnsw_search.c:21-54 补齐） | 搜索吞吐 ≥2.5K QPS @1M |
| 向量化执行试点 | TupleBatch 中间表示，SeqScan + HashAgg 两算子先行 | 聚合查询 2x |
| Timeseries 增量压缩 | 每点即时编码（见 C2-4） | 热路径内存 ≥5x 压缩 |
| 基准进 CI | engineering/test/db/benchmark/ 扩展：每模态一个基准可执行文件 + ctest 标签 | 回归即报警 |
| 图算法优化 | CSR 双视图后遍历无锁化 | 遍历吞吐 3x |

### 6.2 可维护性专项（C4-2）

| 项 | 措施 |
|----|------|
| ASAN/UBSAN CI | GitHub Actions Linux 侧跑全测试套（MinGW 无 libasan，Windows 本地维持 Debug 双分配器校验） |
| Release UAF 清零 | 8 个存量 use-after-free 逐一修复（memory: p6 记录） |
| HNSW 三路收敛 | 统一 `vector_index_vtable_t`，删 storage/vector/faiss_hnsw_stub.c 与 hnsw/hnsw_placeholder.c，旧头 24 处引用迁移 |
| 跨模态 E2E 测试 | 三条基线：① Graph+Vector+RAG 混合 ② Relational MVCC+WAL 事务 ③ Vector 并发插入搜索（复现 C1-2 修复） |
| DESIGN.md 同步 | 每模态设计文档随变更更新（faiss_hnsw/DESIGN.md、streaming/DESIGN.md 模式推广） |
| MemoryContext 覆盖 | 执行器初始化路径挂 per-query context（见 C0-3） |

---

## 七、OpenSpec 变更组织与依赖图

### 7.1 变更清单（15 个）

| ID | 名称 | 阶段 | 工作量 | 依赖 |
|----|------|------|--------|------|
| C0-1 | 统一并发原语推广（mmdb_lock → 5 模态） | 0 | M | 无 |
| C0-2 | 共享 WAL 统一覆盖 + fsync 策略 | 0 | M | 无 |
| C0-3 | 统一错误码 + MemoryContext 覆盖 + mm 序列化契约 | 0 | M | 无 |
| C1-1 | 关系模态 TID 管道修复 | 1 | M | C0-3 |
| C1-2 | faiss_hnsw COW 快照 + IP 度量修正 | 1 | M | C0-1 |
| C1-3 | KV 锁启用 + WAL 失败处理 + 页分裂 | 1 | M | C0-1, C0-2 |
| C2-1 | MVCC 集成执行路径 | 2 | L | C0-2, C1-1 |
| C2-2 | 优化器实现（DP join + 统计扩展） | 2 | M | C0-3 |
| C2-3 | Graph/Spatial 并发与恢复（CSR 双视图 + geography） | 2 | M | C0-1, C0-2 |
| C2-4 | Timeseries 增量压缩 + 乱序处理 | 2 | M | C0-2 |
| C2-5 | Tree XML 解析器升级 + datastore | 2 | M | C0-1, C0-2 |
| C2-6 | Document 中文分词 + 词干化（自研） | 2 | M | 无 |
| C3-1 | 对象/Blob 存储引擎（自研） | 3 | M | C0-3 |
| C3-2 | 全文搜索引擎增强（自研） | 3 | L | C2-6 |
| C3-3 | 可观测日志引擎（自研） | 3 | L | C0-2, C2-4 |
| C3-4 | 多模态 AI 原生存储 | 3 | L | C3-1 |
| C3-5 | 各模态功能补齐（索引种类/算法/函数/API 汇总变更） | 3 | L | C0-* |
| C4-1 | 性能专项（SIMD 补全/向量化/基准 CI） | 4 | M | C1-2 |
| C4-2 | 可维护性专项（ASAN/UAF/HNSW 收敛/E2E） | 4 | M | C1-2, C2-1 |

> 注：C3-5 是聚合变更，内部按模态拆 task；实际执行时若过大可再拆为 C3-5a/b/c。

### 7.2 依赖图

```
C0-1 ──┬──→ C1-2 ──→ C4-1
       ├──→ C1-3        C4-2
       ├──→ C2-3
       ├──→ C2-5
       └──→ C0-2 ──→ C2-1 ──→ C4-2
              │    ↗（还需 C1-1）
              ├──→ C2-4 ──→ C3-3
              └──→ C3-3
C0-3 ──┬──→ C1-1 ──→ C2-1
       ├──→ C2-2
       └──→ C3-1 ──→ C3-4
C2-6 ──→ C3-2
```

### 7.3 执行纪律（沿用仓库 OpenSpec 铁律）

- 每变更：proposal → tasks → 实施 → verify → archive，独立 commit 链
- tasks.md 更新触发 proposal/design/specs 联动检查
- 回退只回退当前变更代码；提交只提交变更相关文件
- 每个正确性修复**先写复现测试再修**（差距报告中的缺陷编号作为测试名后缀，如 `tid_pipeline_bug_02rel`）

---

## 八、验收标准（对应四维度）

| 维度 | 验收项 |
|------|--------|
| 功能 | ① 差距报告 8 个深挖卷的「实现质量缺陷清单」（共 44 项确认 + 19 项疑似）全部关闭或显式标注 won't-fix ② 15 个变更全部 archive ③ 跨模态 E2E 三基线通过 |
| 设计 | ① `grep simple_rwlock` 在 src/db 下零命中 ② `grep fflush` 仅存在于统一刷盘策略模块 ③ 三套 HNSW 路径收敛为一 ④ mm_storage 全部接口有真实实现或 DBERR_NOT_IMPLEMENTED |
| 性能 | ① 各模态基准报告落 docs/ ② CI 基准回归绿灯 ③ Vector ≥2.5K QPS、插入 ≥150K vec/s、Timeseries 压缩 ≥5x |
| 可维护 | ① ASAN CI 绿灯 ② Release UAF = 0 ③ 全部变更 archive 归档完整 |

---

## 九、对差距报告的修正记录

撰写本方案时核实的两处修正（差距报告 01 卷相应结论以本节为准）：

1. **SIMD 接入状态**：`faiss_hnsw_search_layer.c:31-53` 与 `faiss_hnsw_stubs.c:20-33` 已接入 AVX2 L2 距离（P6-M1.3）——01 卷"主搜索路径未接入 SIMD"修正为"底层 search_layer 已接入，上层贪婪下降（faiss_hnsw_search.c:21-54）仍标量"，对应 C4-1 范围缩小。
2. **跨平台锁已有组件**：`include/sdk/impl/mmdb_lock.h`（Windows SRWLOCK / POSIX pthread_rwlock wrapper，P5 X2 落地）已存在——C0-1 从"新建"调整为"把既有 wrapper 从 SDK 层提升到 db 公共层并推广到 5 个存储模态"。
