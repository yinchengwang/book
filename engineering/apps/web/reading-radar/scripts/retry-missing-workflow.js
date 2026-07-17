// Retry workflow for the 4 failed batches
// Batch 7 (DS), Batch 13 (DB), Batch 14 (VDB), Batch 18 (Linux)

export const meta = {
  name: 'learn-deep-retry',
  description: '重新生成 4 批失败的 60 个知识点深入内容',
  phases: [
    { title: 'DS', detail: 'Batch 7: 13 个 DS 知识点' },
    { title: 'DB', detail: 'Batch 13: 18 个 DB 知识点' },
    { title: 'VDB', detail: 'Batch 14: 15 个 VDB 知识点' },
    { title: 'Linux', detail: 'Batch 18: 14 个 Linux 知识点' },
  ],
};

const RETRY_BATCHES = [
  {
    batchNum: 7,
    stack: 'ds',
    phase: 'DS',
    systemPrompt: `你的身份：你是一名资深数据结构和算法工程师、算法竞赛教练。
你的任务：为下面的数据结构知识点各写一篇深入的技术讲解文章。站在该数据结构本身的角度，深入讲解其原理、实现、复杂度分析和典型应用场景

## 输出格式要求
每条输出用以下格式包裹：

<!-- item: <itemId> -->
# 文章标题

文章正文...

---

即每条以 \`<!-- item: <itemId> -->\` 开头，内容之间用 \`---\` 分隔。

## 写作原则
1. 深入讲解知识点本身的核心概念和实现原理
2. 内容务必具体、技术准确，给出真实的代码示例和实现细节
3. 不编造不存在的研究或框架
4. 代码示例需简洁可读，用 C/C++/Python 等真实语言编写
5. 延伸阅读使用格式 \`- [文章标题](知识点的itemId)\`

## 写作风格
用你对这个领域的深入了解，写出一篇有深度的技术文章，而不是泛泛的介绍。要具体到源码片段、算法步骤、工程实践。让读者看完后真正理解这个知识点的精髓。`,
    userPrompt: `## 技术栈: ds (数据结构)

以下是为您分配的 13 个知识点。请为每个知识点生成一篇深入的技术讲解文章。

### array
- **标题**: 数组与动态数组
- **描述**: 数组的内存连续存储、随机访问O(1)、插入/删除O(n)。动态数组的扩容策略(翻倍vs线性)与内存管理。
- **深入度**: strong
- **模板**:
- 要求写一篇完整、深入的讲解文章，涵盖以下方面：
  - 核心概念与基本原理
  - 内部实现机制
  - 在主流框架/系统/数据库中的具体实现差异（如适用）
  - 性能特征与优化技巧
  - 代码示例（真实代码，非伪代码）
  - 常见陷阱与最佳实践
  - 延伸阅读链接（2-3个相关learn-deep知识点ID）
- 篇幅：约 800-1500 tokens

### ds-linkedlist
- **标题**: 链表精讲
- **描述**: 单链表/双链表/循环链表增删查改O(n)分析。哨兵节点简化边界。LRU缓存、跳跃表的前驱结构。双向链表在Redis/内核中的广泛应用。
- **深入度**: strong
- **模板**: 同上 strong 模板

### ds-stack
- **标题**: 栈与单调栈
- **描述**: 后进先出的线性结构。数组栈/链式栈实现。函数调用栈、表达式求值。单调栈解决Next Greater Element。
- **深入度**: strong
- **模板**: 同上 strong 模板

### ds-queue
- **标题**: 队列与双端队列
- **描述**: 先进先出的线性结构。环形缓冲区的队列实现。双端队列deque。优先队列与堆。BFS、任务队列、消息队列中的应用。
- **深入度**: strong
- **模板**: 同上 strong 模板

### ds-hashtable
- **标题**: 哈希表原理与实现
- **描述**: 哈希函数设计(djb2/MurmurHash/siphash)、冲突解决(链地址法/开放定址法)、负载因子与rehash、渐进式rehash(Rdis)、布谷鸟哈希(Cuckoo Hashing)。
- **深入度**: strong
- **模板**: 同上 strong 模板

### ds-skiplist
- **标题**: 跳表(Skip List)
- **描述**: 多层索引链表实现O(log n)查找。随机化层高生成。在Redis Sorted Set和LevelDB MemTable中的实现。与B+树的对比。
- **深入度**: medium
- **模板**:
- 要求写一篇简洁但深入的讲解文章，涵盖：
  - 核心概念与基本原理
  - 关键实现细节
  - 实际应用场景
  - 延伸阅读链接（1-2个相关learn-deep知识点ID）
- 篇幅：约 400-600 tokens

### ds-string-ds
- **标题**: 字符串数据结构
- **描述**: 字符串的存储，Trie树(前缀树/压缩Trie/基数树)、后缀树、后缀数组、AC自动机多模式匹配。
- **深入度**: strong
- **模板**: 同上 strong 模板

### ds-bloomfilter
- **标题**: 布隆过滤器与HyperLogLog
- **描述**: 多哈希位数组的概率数据结构，存在性判断。假阳性率分析。布谷鸟过滤器。HyperLogLog基数估计。在Redis/Cassandra中的应用。
- **深入度**: medium
- **模板**: 同上 medium 模板

### ds-lru
- **标题**: LRU/LFU缓存淘汰算法
- **描述**: LRU: 哈希表+双向链表实现O(1)访问。LFU: 多级链表/最小堆实现。在Redis/CPU缓存/操作系统的应用。ARC自适应替换。
- **深入度**: medium
- **模板**: 同上 medium 模板

### lockfree_ds
- **标题**: 无锁数据结构初探
- **描述**: CAS操作、ABA问题、无锁栈(Treiber Stack)、无锁队列(Michael-Scott)、Hazard Pointer与RCU、内存序与内存屏障。
- **深入度**: medium
- **模板**: 同上 medium 模板

### ds-binarytree
- **标题**: 二叉树与遍历
- **描述**: 前序/中序/后序/层序的递归与迭代实现。Morris遍历(线索化)。二叉树序列化。完全二叉树在数组中的紧凑存储。
- **深入度**: strong
- **模板**: 同上 strong 模板

### ds-bst
- **标题**: 二叉搜索树(BST)
- **描述**: 二叉搜索树的查找/插入/删除操作。前驱后继。平衡退化到链表。与二分查找数组的对比。BST在编译器符号表等场景中的应用。
- **深入度**: strong
- **模板**: 同上 strong 模板

### ds-avl
- **标题**: AVL树
- **描述**: 平衡因子与高度平衡。四种旋转操作(LL/RR/LR/RL)。插入/删除后的回溯调整。与红黑树的对比：更严格平衡但旋转更多。在数据库索引中较少使用的原因。
- **深入度**: strong
- **模板**: 同上 strong 模板

---
请严格按照 \`<!-- item: <itemId> -->\` 格式输出，每条之间用 \`---\` 分隔。`
  },
  {
    batchNum: 13,
    stack: 'db',
    phase: 'DB',
    systemPrompt: `你的身份：你是一名资深数据库内核开发工程师和数据库技术布道者。
你的任务：为下面的数据库知识点各写一篇深入的技术讲解文章。站在数据库内核的角度，深入讲解该机制的实现原理、工程设计和在主流数据库中的具体落地

## 输出格式要求
每条输出用以下格式包裹：

<!-- item: <itemId> -->
# 文章标题

文章正文...

---

即每条以 \`<!-- item: <itemId> -->\` 开头，内容之间用 \`---\` 分隔。

## 写作原则
1. 深入讲解知识点本身的核心概念和实现原理
2. 内容务必具体、技术准确，给出真实的代码示例和实现细节
3. 不编造不存在的研究或框架
4. 代码示例需简洁可读，用 C/C++/Python 等真实语言编写
5. 延伸阅读使用格式 \`- [文章标题](知识点的itemId)\`

## 写作风格
用你对这个领域的深入了解，写出一篇有深度的技术文章，而不是泛泛的介绍。要具体到源码片段、算法步骤、工程实践。让读者看完后真正理解这个知识点的精髓。`,
    userPrompt: `## 技术栈: db (数据库)

请为每个知识点生成一篇深入的技术讲解文章。

### deadlock
- **标题**: 死锁检测与处理
- **描述**: 死锁四个必要条件。等待图(Wait-for Graph)构建与环检测。超时vs死锁检测的trade-off。InnoDB死锁检测实现。PostgreSQL的deadlock_timeout和锁顺序。MySQL的innodb_deadlock_detect配置与性能。
- **深入度**: strong. strong 完整文章模板

### db-optimistic
- **标题**: 乐观并发控制(OCC)
- **描述**: 读阶段→验证阶段→写阶段。三种验证策略：时间戳排序、冲突检测、串行化验证。OCC与PCC的对比。乐观锁在内存数据库(Hekaton)中的应用。MongoDB中的OCC：文档版本号。
- **深入度**: medium. medium 简洁深入模板

### db-redo-log-detail
- **标题**: Redo Log详解与组提交
- **描述**: Redo Log的物理/逻辑格式。Write-Ahead Logging(WAL)强制刷盘保证持久性。LSN(Log Sequence Number)的递增和跟踪。组提交(Group Commit)合并多个事务的刷盘。MySQL redo log的双缓冲(ib_logfile)。PostgreSQL的WAL段管理与归档。
- **深入度**: strong. strong 完整文章模板

### db-aries
- **标题**: ARIES恢复算法
- **描述**: ARIES三大原则：WAL、重复历史(Repeating History)、记录变更(Logging Changes)。分析阶段→重做阶段→撤销阶段。Physiological Logging。Page LSN与补偿日志(CLR)。模糊检查点与Scribbling。支持部分回滚(SAVEPOINT)。SQL Server/IBM DB2使用ARIES。
- **深入度**: strong. strong 完整文章模板

### db-snapshot-isol
- **标题**: 快照隔离与Write Skew
- **描述**: 快照隔离(SI)原理：每个事务看到启动时的一致性快照。多版本并发控制(MVCC)实现快照隔离。Write Skew异常：SI不能避免的并发异常。SSI(可序列化快照隔离)：PostgreSQL检测读写冲突预防Write Skew。
- **深入度**: medium. medium 简洁深入模板

### db-2pc
- **标题**: 两阶段提交(2PC)
- **描述**: 协调者与参与者。Prepare阶段→Commit阶段。协议状态机与日志。阻塞问题与协调者单点故障。3PC的改进。XA规范与分布式事务。XA在MySQL与PostgreSQL中的应用。与Saga模式的对比。
- **深入度**: medium. medium 简洁深入模板

### db-redis-event
- **标题**: Redis事件驱动模型
- **描述**: 单线程事件循环。aeEventLoop: aeFileEvent(文件事件:网络/定时器)/aeTimeEvent(时间事件)。epoll作为IO复用后端。网络IO处理流程：读→命令解析→命令执行→写。定时器处理。ae.c源码简要分析。单线程为什么快：纯内存+非阻塞IO+无锁。
- **深入度**: strong. strong 完整文章模板

### db-redis-resp
- **标题**: RESP协议与网络层
- **描述**: RESP(Redis Serialization Protocol): 简单字符串(+OK)、错误(-ERR)、整数(:1)、批量字符串($5hello)、数组(*3...)。管道(Pipeline)批量请求。RESP3的推送能力(推/订阅/Hello)。客户端与服务端的TCP交互。Redis网络层的socket读缓冲与命令解析器。
- **深入度**: strong. strong 完整文章模板

### db-redis-object
- **标题**: Redis对象系统(redisObject)
- **描述**: redisObject结构体(type/encoding/lru/refcount/ptr)。八种编码方式：OBJ_ENCODING_RAW/EMBSTR/INT/HT/ZIPLIST/LINKEDLIST/SKIPLIST/QUICKLIST。类型检查与命令多态。对象共享与引用计数。LRU时钟与过期策略。内存优化：小数据结构的紧凑编码。
- **深入度**: strong. strong 完整文章模板

### db-redis-persist
- **标题**: Redis持久化(RDB/AOF)
- **描述**: RDB快照：fork子进程全量持久化，写时复制优化。SAVE/BGSAVE/自动触发。RDB文件格式魔数和checksum。AOF追加写：每条写命令追加到文件。AOF重写：fork子进程合并命令。混合持久化(Redis 4.0)。RDB与AOF的对比。数据恢复顺序。
- **深入度**: strong. strong 完整文章模板

### db-redis-replica
- **标题**: Redis主从复制
- **描述**: 全量同步(PSYNC)：RUNID+offset。SYNC/RDB传输→缓冲区增量。部分重同步(PSYNC2)：复制积压缓冲区(repl_backlog)。主从心跳(REPLCONF ACK)。从节点只读/无阻塞。无盘复制(repl-diskless-sync)。读写分离架构与一致性延迟。
- **深入度**: strong. strong 完整文章模板

### db-redis-cluster
- **标题**: Redis集群与哨兵
- **描述**: Redis Cluster无中心架构：16384个槽哈希分配。Gossip协议(PING/PONG)。MOVED/ASK重定向。哨兵(Sentinel)的高可用：故障检测与自动故障转移。哨兵领导者选举(Raft)。Cluster vs Sentinel vs 主从的选型对比。
- **深入度**: strong. strong 完整文章模板

### db-consensus
- **标题**: 分布式共识(Raft/Paxos)
- **描述**: 一致性问题的FLP不可能定理。Paxos: Proposer/Acceptor/Learner。Prepare→Promise→Accept→Accepted。Multi-Paxos的Leader优化。Raft: Leader选举/日志复制/安全性。Term递增与心跳超时。Raft在etcd/TiDB/Linux内核的实践。
- **深入度**: strong. strong 完整文章模板

### db-sharding
- **标题**: 数据分片与分区策略
- **描述**: 水平分片(Horizontal Sharding)与垂直分片。Range/哈希/列表/复合分区。一致性哈希：虚拟节点与最小数据迁移。分片键选取原则与Join问题。二级索引的全局vs本地。Vitess/Spanner/Citus的分片方案。
- **深入度**: strong. strong 完整文章模板

### db-ha
- **标题**: 高可用与故障转移
- **描述**: 主从架构：同步复制vs异步复制。半同步复制(rpl_semi_sync_*)避免数据丢失。自动故障转移步骤：检测→选主→切换(MHA/Orchestrator)。脑裂问题(STONITH/Lease)。多数据中心部署。MariaDB MaxScale/ProxySQL的读写分离与故障切换。
- **深入度**: strong. strong 完整文章模板

### db-perf
- **标题**: 数据库性能调优
- **描述**: EXPLAIN执行计划解读(type/rows/Extra)。索引优化：B+树深度、覆盖索引、索引下推。慢查询日志与pt-query-digest。缓存配置(buffer_pool/query_cache)。MySQL的innodb_flush_log_at_trx_commit调优。连接池/HikariCP配置。NUMA感知与O_DIRECT。
- **深入度**: medium. medium 简洁深入模板

### db-sqlite-arch
- **标题**: SQLite 架构与嵌入存储
- **描述**: SQLite的八组件架构：Tokenizer→Parser→Code Generator→VM(B-Tree→Pager→OS Interface)。SQLite VDBE(Virtual Database Engine)的字节码执行。单文件数据库的锁与事务。WAL模式与读不阻塞写。
- **深入度**: strong. strong 完整文章模板

### db-sqlite-btree
- **标题**: SQLite B-Tree 与页面管理
- **描述**: SQLite B-Tree种类：TABLE BTREE与INDEX BTREE。页面组织：sqlite_master表与schema存储。页面分裂与平衡。空闲页面链表与自动VACUUM。增量VACUUM。与InnoDB B+树的对比：SQLite的B-Tree不支持列存储。
- **深入度**: medium. medium 简洁深入模板

---
请严格按照 \`<!-- item: <itemId> -->\` 格式输出，每条之间用 \`---\` 分隔。`
  },
  {
    batchNum: 14,
    stack: 'vdb',
    phase: 'VDB',
    systemPrompt: `你的身份：你是一名向量数据库技术专家和数据库索引工程师。
你的任务：为下面的向量数据库知识点各写一篇深入的技术讲解文章。站在向量数据库的角度，深入讲解该索引/机制的算法原理、工程实现和在主流系统中的应用

## 输出格式要求
每条输出用以下格式包裹：

<!-- item: <itemId> -->
# 文章标题

文章正文...

---

即每条以 \`<!-- item: <itemId> -->\` 开头，内容之间用 \`---\` 分隔。

## 写作原则
1. 深入讲解知识点本身的核心概念和实现原理
2. 内容务必具体、技术准确，给出真实的代码示例和实现细节
3. 不编造不存在的研究或框架
4. 代码示例需简洁可读，用 C/C++/Python 等真实语言编写
5. 延伸阅读使用格式 \`- [文章标题](知识点的itemId)\`

## 写作风格
用你对这个领域的深入了解，写出一篇有深度的技术文章，而不是泛泛的介绍。要具体到源码片段、算法步骤、工程实践。让读者看完后真正理解这个知识点的精髓。`,
    userPrompt: `## 技术栈: vdb (向量数据库)

请为每个知识点生成一篇深入的技术讲解文章。

### db-vector-basic
- **标题**: 向量索引基础
- **描述**: 向量距离度量(L2/IP/Cosine)。精确kNN扫描(暴力计算)。向量索引的分类：基于空间划分(KD-Tree/R-Tree)、基于量化的(PQ/SQ)、基于图的(HNSW/NSG)、基于哈希的(LSH)。召回率vs延迟的trade-off。向量数据库与传统数据库的索引差异。
- **深入度**: strong. strong 完整文章模板

### db-ivf-variants
- **标题**: IVF家族索引(IVFFlat/IVFPQ/IVFSQ)
- **描述**: IVF(倒排文件)的聚类索引结构：K-Means聚类→Nprobe搜索→仅扫描最近簇。IVFFlat: 返回精确向量。IVFPQ: 结合乘积量化压缩向量，大幅减少内存。IVFSQ: 标量量化降低精度。Faiss中IVF实现详解。在Milvus/Pgvector中的配置和调优。
- **深入度**: strong. strong 完整文章模板

### db-hnsw
- **标题**: HNSW索引原理与实现
- **描述**: NSW(可导航小世界图)+分层=HNSW。每层图的构建规则：ep选择、M_max连接数、ef构建参数。搜索算法：自上而下→逐层贪婪。插入算法：逐层确定level→最近邻连接。各层图结构对搜索效率的影响。HNSW在Faiss/Milvus/Qdrant中的实现差异。HNSW的多边形性与CRUD挑战。
- **深入度**: strong. strong 完整文章模板

### db-graph-index
- **标题**: 图索引详解(NSW/HNSW/NSG/Vamana)
- **描述**: 图索引的发展演进。NSW的可导航小世界图原理(贪心搜索+长程边+短程边)。NSG(可导航稀疏图)的Monotonic Relative Neighborhood Graph(MRNG)构建。Vamana的度数约束与锐化策略(DiskANN内置)。图搜索的收敛性分析与降维可视化。
- **深入度**: strong. strong 完整文章模板

### db-pq-quant
- **标题**: 乘积量化(PQ)与向量压缩
- **描述**: PQ的核心思想：向量空间分解→子空间量化→码本训练(K-Means)。SDC(对称距离计算)vs ADC(非对称距离计算)。OPQ(优化乘积量化)的旋转优化。AQ(加法量化)与RVQ(残差向量量化)的改进。PQ在Faiss的实现：编码表与距离查找表。
- **深入度**: strong. strong 完整文章模板

### db-quantization
- **标题**: 量化技术全景(PQ/OPQ/SQ/AQ/RVQ)
- **描述**: 向量量化全景图。标量量化(SQ)的最简单形式：二分查找阈值。PQ的逐段码本。OPQ的预旋转+PQ。AQ的逐级加法逼近。RVQ的残差级联。RAQ-VQ的端到端优化。各类量化在压缩比与精度损失上的对比。是否支持实时索引更新。
- **深入度**: strong. strong 完整文章模板

### db-scalar-quant
- **标题**: 标量量化与SIMD距离计算
- **描述**: 标量量化将32位float映射到8位/4位整数。对称量化与不对称量化。FP32→INT8/INT4的误差分析。SIMD(SSE/AVX)加速距离计算：_mm256_fmadd_ps乘加融合指令。距离计算的汇编级优化。量化后的反量化与搜索精度回退策略。
- **深入度**: strong. strong 完整文章模板

### db-diskann
- **标题**: 磁盘向量索引(DiskANN)
- **描述**: DiskANN的混合设计：内存中的Vamana图导航+压缩向量(SSD)+磁盘上的完整向量。Vamana图的构建：Stitched Vamana Graph。搜索流程：内存图探索→拉取完整向量(SSD随机读)→距离重算。批量删除(墓碑标记)与懒惰合并。DiskANN的VSSCached读取优化。
- **深入度**: strong. strong 完整文章模板

### db-milvus-arch
- **标题**: Milvus系统架构
- **描述**: Milvus 2.0的存储-计算分离架构。Proxy→Coordinator/DataNode/QueryNode/IndexNode。消息队列(NATS/Pulsar/Kafka)支撑日志和CDC。元数据存储(etcd)。数据流写入与查询分离。Grpc通信的微服务架构。混洗(Shuffle)与负载均衡。
- **深入度**: strong. strong 完整文章模板

### db-milvus-segment
- **标题**: Milvus段(Segment)管理
- **描述**: 段是Milvus中数据管理的核心单元。Growing Segment(可写)与Sealed Segment(只读)。段内部组织：插入记录→内存Flush→落盘索引构建。段合并：小段合并为大段提升搜索效率。L0/L1/L2/L3/S0/S1/S2/S3等段状态与生命周期。段压缩(Compaction)机制。
- **深入度**: strong. strong 完整文章模板

### db-milvus-index
- **标题**: Milvus索引构建与调度
- **描述**: IndexNode的索引构建调度。Milvus支持IVF_FLAT/IVF_PQ/IVF_SQ8/HNSW/DISKANN/GPU_IVF/GPU_BRUTEFORCE。索引构建任务队列：调度策略与资源隔离。GPU加速的IVF构建。多副本的索引分发。索引量与内存的关系配置。
- **深入度**: strong. strong 完整文章模板

### db-milvus-search
- **标题**: Milvus混合查询(标量+向量)
- **描述**: Milvus的混合搜索流程。迭代式搜索：先用标量条件过滤(bitmask)→再在结果集上做向量搜索。表达式过滤(Expr Filter)的谓词下推。混合查询的执行管道：QueryNode中的segment→索引搜索→合并→排序。Range Search与Grouping Search。
- **深入度**: strong. strong 完整文章模板

### db-hybrid-search
- **标题**: 混合查询(标量+向量/SPANN)
- **描述**: 混合查询的定义：同时包含标量约束(WHERE)和向量相似度约束(ORDER BY)。Three-Body问题：搜准率与搜全率权衡。Post-filter: 标量过滤后做向量搜索。Pre-filter: 向量搜索后做标量过滤。微软SPANN: 剪枝+残差量化。百度SPHINX: 预过滤索引。
- **深入度**: strong. strong 完整文章模板

### vdb-pinecone-serverless
- **标题**: Pinecone Serverless 架构
- **描述**: Pinecone Serverless的存算分离架构。Pod-based vs Serverless的架构演进。分层存储：热数据(内存HNSW)→温数据(SSD)→冷数据(S3)。Pod的Metadata过滤索引。副本与分区(Pods/Shards)对吞吐量的影响。Serverless下的自动扩展伸缩。
- **深入度**: strong. strong 完整文章模板

### vdb-pinecone-storage
- **标题**: Pinecone 存储与索引
- **描述**: Pinecone使用的底层索引技术。S3对象存储作为主数据层。索引分片到Pods：每个Pod管理一个完整索引分区。副本提高吞吐和容错。HNSW作为默认索引算法的选择。元数据索引(B+树/倒排)支撑标量过滤。集群的拓扑变化与数据再均衡。
- **深入度**: strong. strong 完整文章模板

---
请严格按照 \`<!-- item: <itemId> -->\` 格式输出，每条之间用 \`---\` 分隔。`
  },
  {
    batchNum: 18,
    stack: 'linux',
    phase: 'Linux',
    systemPrompt: `你的身份：你是一名 Linux 内核/系统工程师和系统性能优化专家。
你的任务：为下面的 Linux 知识点各写一篇深入的技术讲解文章。站在 Linux 内核/系统的角度，深入讲解该机制的实现原理、系统调用路径和性能调优实践

## 输出格式要求
每条输出用以下格式包裹：

<!-- item: <itemId> -->
# 文章标题

文章正文...

---

即每条以 \`<!-- item: <itemId> -->\` 开头，内容之间用 \`---\` 分隔。

## 写作原则
1. 深入讲解知识点本身的核心概念和实现原理
2. 内容务必具体、技术准确，给出真实的代码示例和实现细节
3. 不编造不存在的研究或框架
4. 代码示例需简洁可读，用 C/C++/Python 等真实语言编写
5. 延伸阅读使用格式 \`- [文章标题](知识点的itemId)\`

## 写作风格
用你对这个领域的深入了解，写出一篇有深度的技术文章，而不是泛泛的介绍。要具体到源码片段、算法步骤、工程实践。让读者看完后真正理解这个知识点的精髓。`,
    userPrompt: `## 技术栈: linux (Linux 系统)

请为每个知识点生成一篇深入的技术讲解文章。

### linux-proc-fs
- **标题**: procfs 文件系统接口
- **描述**: /proc是虚拟文件系统，通过proc_create注册/proc条目。关键文件：/proc/[pid]/maps/status/smaps/mem/fd。/proc/meminfo/cpuinfo/stat/uptime/net/dev。/proc/sys内核参数。seq_file接口简化大文件输出。
- **深入度**: medium. medium 简洁深入模板

### linux-cpu-diag
- **标题**: CPU 性能诊断
- **描述**: /proc/stat查看CPU时间(user/system/idle/iowait/irq/steal)。mpstat报告每个CPU的使用率。top/htop按CPU排序。perf stat统计上下文切换/迁移/周期。CPU绑定与缓存命中分析。vmstat的r/b列。
- **深入度**: medium. medium 简洁深入模板

### linux-mem-diag
- **标题**: 内存性能诊断
- **描述**: free -m的total/used/free/buffers/cached解读。/proc/meminfo关键字段：MemTotal/MemFree/Cached/SReclaimable/Slab。vmstat的si/so查看交换。top的RES/VIRT/SHR。/proc/[pid]/smaps的RSS/PSS。OOM Killer的日志分析。
- **深入度**: medium. medium 简洁深入模板

### linux-disk-iostat
- **标题**: 磁盘 I/O 性能观测
- **描述**: iostat -x的r/s、w/s、rMB/s、wMB/s、await、svctm、%util。await ≥ svctm表明有排队。%util接近100%意味着磁盘饱和。iotop按进程查看IO。blktrace/btt的IO延迟分解。fio基准测试命令。
- **深入度**: medium. medium 简洁深入模板

### linux-htop
- **标题**: 系统监控与进程管理
- **描述**: htop的交互式进程查看：树状视图、筛选、列选择。颜色代码的含义(CPU/内存/IO等待)。按CPU/内存/时间排序。内核线程(kthreadd/kswapd0/kworker)的识别。systemd-cgtop查看cgroup层级的资源使用。atop的详细指标。
- **深入度**: medium. medium 简洁深入模板

### linux-sar
- **标题**: 系统活动报告(sar)
- **描述**: sar系统活动报告器，收集/var/log/sa/sa??历史数据。sar -u CPU、sar -r内存、sar -b IO、sar -n DEV网络。sadc数据收集器。sar -S交换空间。sysstat包的配置(/etc/cron.d/sysstat)。
- **深入度**: medium. medium 简洁深入模板

### linux-perf-basic
- **标题**: perf 性能分析基础
- **描述**: perf stat运行统计。perf record+perf report的采样模式。perf top的实时热点。perf annotate查看汇编。perf list列出可用事件(hardware/software/tracepoint)。硬件PMU计数器：cycles/instructions/cache-misses/branch-misses。
- **深入度**: medium. medium 简洁深入模板

### linux-blktrace
- **标题**: 块设备 I/O 追踪
- **描述**: blktrace跟踪块层IO事件。六个事件动作：Q(入队)/G(请求)/I(插入)/D(下发)/C(完成)/M(合并)。blkparse格式化输出。btt(bio blkparse timer)按IO生命周期生成延迟直方图。seekwatcher可视化IO模式。
- **深入度**: medium. medium 简洁深入模板

### pagecache-hit
- **标题**: Page Cache 命中率分析
- **描述**: Page Cache命中率 = cache_hits/(cache_hits+cache_misses)。cachestat(或bpftrace脚本)统计cache命中/未命中。cachetop按进程查看cache使用。mincore系统调用检测页面是否在cache中。fincore查看文件的cache状态。
- **深入度**: strong. strong 完整文章模板

### linux-net-diag
- **标题**: 网络性能诊断
- **描述**: ss -tuln查看socket统计。netstat -s的协议统计。ss -ti查看TCP拥塞窗口/重传。ip -s link查看接口统计。tc -s qdisc查看排队延迟。nstat命令直接打印/proc/net/netstat。tcpdump抓包+tcpflow分析。
- **深入度**: medium. medium 简洁深入模板

### linux-ebpf-intro
- **标题**: eBPF 可观测性
- **描述**: eBPF在内核安全虚拟机中执行用户自定义程序。BPF程序类型：kprobe/tracepoint/perf_event/xdp/socket filter。BPF Map：哈希/数组/环形缓冲/栈跟踪。bpftrace的一行式诊断命令。BCC工具集：biolatency/biosnoop/ext4dist/tcptop/runqlat。eBPF的优势：无侵入+安全+内核级观测。
- **深入度**: strong. strong 完整文章模板

### linux-db-flamegraph
- **标题**: 数据库火焰图分析
- **描述**: perf record采样→perf script→FlameGraph/stackcollapse-perf.pl→flamegraph.pl生成SVG火焰图。x轴是采样分布，y轴是调用栈深度。宽栈顶是热点函数。CPU火焰图vs Off-CPU火焰图vs 差分火焰图。案例分析：MySQL慢查询的火焰图特征、PostgreSQL WAL写瓶颈的火焰图定位。
- **深入度**: strong. strong 完整文章模板

### linux-numa-observ
- **标题**: NUMA 观测与亲和性
- **描述**: numactl --hardware查看NUMA拓扑。numastat查看内存分配统计。/sys/devices/system/node/node*/meminfo查看每节点的内存。跨节点内存访问延迟惩罚：本地vs远程。numactl --membind/--cpunodebind绑定。数据库的NUMA感知配置：innodb_numa_interleave。
- **深入度**: strong. strong 完整文章模板

### linux-latency-diag
- **标题**: 延迟诊断与分析
- **描述**: 延迟的USE方法：利用率/饱和度/错误。CPU调度延迟(runqlat/bcc)。IO延迟分布：iostat的await/svctm vs 99分位延迟。网络延迟：ping延迟/tcpdump的RTT分析。链式延迟分析：从用户态到内核态再到硬件。
- **深入度**: strong. strong 完整文章模板

---
请严格按照 \`<!-- item: <itemId> -->\` 格式输出，每条之间用 \`---\` 分隔。`
  }
];

// Phase 1: 生成内容
phase('DS');
phase('DB');
phase('VDB');
phase('Linux');

const results = await pipeline(
  RETRY_BATCHES,
  // Stage 1: 生成内容
  async (batch) => {
    const prompt = batch.systemPrompt + '\n\n' + batch.userPrompt;
    return await agent(prompt, {
      label: 'retry-batch-' + batch.batchNum + ' [' + batch.stack + ']',
      phase: batch.phase,
    });
  },
  // Stage 2: 解析
  async (output, batch) => {
    const items = [];
    const regex = /<!--\s*item:\s*([\w-]+)\s*-->([\s\S]*?)(?=\n<!--\s*item:|\n---|$)/g;
    let match;
    while ((match = regex.exec(output)) !== null) {
      items.push({ itemId: match[1].trim(), content: match[2].trim() });
    }
    return {
      batchNum: batch.batchNum,
      stack: batch.stack,
      items,
    };
  }
);

// 收集结果
let totalItems = 0;
const allRetryContent = [];

for (const result of results) {
  if (!result) continue;
  totalItems += result.items.length;
  for (const item of (result.items || [])) {
    allRetryContent.push({
      itemId: item.itemId,
      content: item.content,
      batchNum: result.batchNum,
      stack: result.stack,
    });
  }
}

log('Retry total items: ' + totalItems);

return {
  totalBatches: results.length,
  totalItems,
  allContent: allRetryContent,
};
