// Final retry: one agent per missing item
// 33 items remaining (15 VDB + 18 DB)

export const meta = {
  name: 'learn-deep-final-retry',
  description: '逐项填补 33 个缺失知识点（VDB/DB）',
  phases: [
    { title: 'VDB', detail: '15 个向量数据库知识点' },
    { title: 'DB', detail: '18 个数据库知识点' },
  ],
};

// Individual item prompts
const ITEMS = [
  // ========== VDB (15) ==========
  { id: 'db-vector-basic', title: '向量索引基础', stack: 'vdb', desc: '向量距离度量(L2/IP/Cosine)。精确kNN扫描。向量索引分类：基于空间划分/量化/图/哈希。召回率vs延迟trade-off。', tier: 'strong' },
  { id: 'db-ivf-variants', title: 'IVF家族索引', stack: 'vdb', desc: 'IVF(倒排文件)聚类索引：K-Means→Nprobe→近簇搜索。IVFFlat精确/IVFPQ量化压缩/IVFSQ标量量化。Faiss IVF实现。Milvus/Pgvector调优。', tier: 'strong' },
  { id: 'db-hnsw', title: 'HNSW索引原理与实现', stack: 'vdb', desc: 'NSW+分层=HNSW。层图构建规则。自顶向下逐层贪婪搜索。插入算法。Faiss/Milvus/Qdrant实现差异。', tier: 'strong' },
  { id: 'db-graph-index', title: '图索引详解', stack: 'vdb', desc: 'NSW可导航小世界图。NSG的MRNG构建。Vamana度数约束与锐化策略(DiskANN)。图搜索收敛性分析。', tier: 'strong' },
  { id: 'db-pq-quant', title: '乘积量化(PQ)与向量压缩', stack: 'vdb', desc: '向量空间分解→子空间量化→码本训练(K-Means)。SDC vs ADC。OPQ旋转优化。Faiss PQ编码表与距离查找表。', tier: 'strong' },
  { id: 'db-quantization', title: '量化技术全景', stack: 'vdb', desc: 'SQ标量量化、PQ逐段码本、OPQ预旋转+PQ、AQ逐级加法逼近、RVQ残差级联、RAQ-VQ端到端优化。压缩比vs精度。', tier: 'strong' },
  { id: 'db-scalar-quant', title: '标量量化与SIMD距离计算', stack: 'vdb', desc: 'FP32→INT8/INT4映射。对称vs非对称量化。SIMD(SSE/AVX)加速距离计算：_mm256_fmadd_ps。', tier: 'strong' },
  { id: 'db-diskann', title: '磁盘向量索引(DiskANN)', stack: 'vdb', desc: '内存Vamana图导航+SSD压缩向量+磁盘完整向量。Stitched Vamana Graph构建。批量删除+懒惰合并。VSSCached优化。', tier: 'strong' },
  { id: 'db-milvus-arch', title: 'Milvus系统架构', stack: 'vdb', desc: '存算分离。Proxy→Coordinator/DataNode/QueryNode/IndexNode。消息队列NATS/Pulsar。etcd元数据。Grpc通信。', tier: 'strong' },
  { id: 'db-milvus-segment', title: 'Milvus段管理', stack: 'vdb', desc: 'Growing/Sealed Segment。内存Flush→落盘索引构建。段合并与压缩(Compaction)。L0-L3段状态与生命周期。', tier: 'strong' },
  { id: 'db-milvus-index', title: 'Milvus索引构建与调度', stack: 'vdb', desc: 'IndexNode调度。IVF_FLAT/PQ/HNSW/DISKANN/GPU_IVF。构建任务队列。资源隔离。GPU加速。', tier: 'strong' },
  { id: 'db-milvus-search', title: 'Milvus混合查询', stack: 'vdb', desc: '标量过滤(bitmask)+向量搜索。Expr Filter谓词下推。QueryNode管道：segment→索引搜索→合并→排序。Range/Grouping Search。', tier: 'strong' },
  { id: 'db-hybrid-search', title: '混合查询(标量+向量/SPANN)', stack: 'vdb', desc: 'Post-filter/Pre-filter。SPANN: 剪枝+残差量化。SPHINX: 预过滤索引。搜准率vs搜全率权衡。', tier: 'strong' },
  { id: 'vdb-pinecone-serverless', title: 'Pinecone Serverless 架构', stack: 'vdb', desc: 'Pod-based vs Serverless。分层存储：内存HNSW→SSD→S3。Metadata过滤。副本与分区。自动扩展。', tier: 'strong' },
  { id: 'vdb-pinecone-storage', title: 'Pinecone 存储与索引', stack: 'vdb', desc: 'S3对象存储主数据层。索引分片到Pods。HNSW默认索引。Metadata B+树/倒排索引。拓扑变化与再均衡。', tier: 'strong' },

  // ========== DB (18) ==========
  { id: 'deadlock', title: '死锁检测与处理', stack: 'db', desc: '等待图环检测。超时vs检测trade-off。InnoDB死锁检测实现。PostgreSQL deadlock_timeout。MySQL innodb_deadlock_detect。', tier: 'strong' },
  { id: 'db-optimistic', title: '乐观并发控制(OCC)', stack: 'db', desc: '读→验证→写三阶段。时间戳排序/冲突检测/串行化验证。OCC vs PCC。Hekaton/OCC。MongoDB文档版本号。', tier: 'medium' },
  { id: 'db-redo-log-detail', title: 'Redo Log详解与组提交', stack: 'db', desc: '物理/逻辑格式。WAL强制刷盘保证持久性。LSN跟踪。组提交合并刷盘。MySQL redo log双缓冲。PostgreSQL WAL段管理。', tier: 'strong' },
  { id: 'db-aries', title: 'ARIES恢复算法', stack: 'db', desc: 'WAL+重复历史+记录变更。分析→重做→撤销。Physiological Logging。Page LSN与CLR。模糊检查点。SQL Server/DB2。', tier: 'strong' },
  { id: 'db-snapshot-isol', title: '快照隔离与Write Skew', stack: 'db', desc: 'MVCC实现SI。Write Skew异常。SSI(可序列化快照隔离)。PostgreSQL检测读写冲突预防Write Skew。', tier: 'medium' },
  { id: 'db-2pc', title: '两阶段提交(2PC)', stack: 'db', desc: 'Prepare→Commit。协议状态机。协调者单点问题。3PC改进。XA规范。Saga对比。', tier: 'medium' },
  { id: 'db-redis-event', title: 'Redis事件驱动模型', stack: 'db', desc: '单线程事件循环。aeEventLoop: aeFileEvent+aeTimeEvent。epoll后端。IO处理流程。ae.c源码分析。', tier: 'strong' },
  { id: 'db-redis-resp', title: 'RESP协议与网络层', stack: 'db', desc: 'RESP：+OK/-ERR/:1/$5hello/*3...。管道批量。RESP3推送。TCP交互。网络层缓冲区与解析器。', tier: 'strong' },
  { id: 'db-redis-object', title: 'Redis对象系统(redisObject)', stack: 'db', desc: 'redisObject: type/encoding/lru/refcount/ptr。8种编码。类型检查与命令多态。引用计数。LRU时钟。', tier: 'strong' },
  { id: 'db-redis-persist', title: 'Redis持久化(RDB/AOF)', stack: 'db', desc: 'RDB: fork子进程全量快照+写时复制。AOF: 追加写→AOF重写。混合持久化(4.0)。RDB/AOF对比。恢复顺序。', tier: 'strong' },
  { id: 'db-redis-replica', title: 'Redis主从复制', stack: 'db', desc: 'PSYNC全量同步。PSYNC2部分重同步+repl_backlog。主从心跳。无盘复制。读写分离架构。', tier: 'strong' },
  { id: 'db-redis-cluster', title: 'Redis集群与哨兵', stack: 'db', desc: '16384个槽。Gossip协议。MOVED/ASK重定向。Sentinel故障转移+Raft选举。Cluster vs Sentinel vs 主从选型。', tier: 'strong' },
  { id: 'db-consensus', title: '分布式共识(Raft/Paxos)', stack: 'db', desc: 'FLP不可能定理。Paxos: Prepare→Promise→Accept→Accepted。Multi-Paxos。Raft: Leader选举/日志复制/安全性。etcd/TiDB实践。', tier: 'strong' },
  { id: 'db-sharding', title: '数据分片与分区策略', stack: 'db', desc: '水平vs垂直分片。Range/哈希/列表/复合分区。一致性哈希+虚拟节点。分片键选择。Vitess/Spanner/Citus方案。', tier: 'strong' },
  { id: 'db-ha', title: '高可用与故障转移', stack: 'db', desc: '同步vs异步复制。半同步复制。MHA/Orchestrator自动切换。脑裂(STONITH/Lease)。MaxScale/ProxySQL。', tier: 'strong' },
  { id: 'db-perf', title: '数据库性能调优', stack: 'db', desc: 'EXPLAIN执行计划。索引优化+B+树深度/覆盖索引/索引下推。慢查询+pt-query-digest。buffer_pool调优。NUMA感知。', tier: 'medium' },
  { id: 'db-sqlite-arch', title: 'SQLite架构与嵌入存储', stack: 'db', desc: '八组件架构。VDBE字节码执行。单文件锁与事务。WAL模式读不阻塞写。', tier: 'strong' },
  { id: 'db-sqlite-btree', title: 'SQLite B-Tree与页面管理', stack: 'db', desc: 'TABLE BTREE vs INDEX BTREE。sqlite_master与schema。页面分裂与平衡。空闲页面链表。增量VACUUM。InnoDB B+树对比。', tier: 'medium' },
];

const TASK = {
  vdb: `你的身份：你是一名向量数据库技术专家和数据库索引工程师。
你的任务：为下面的向量数据库知识点写一篇深入的技术讲解文章。深入讲解该索引/机制的算法原理、工程实现和在主流系统中的应用。

输出格式：
<!-- item: <itemId> -->
# 文章标题

文章正文...

写作原则：
1. 深入讲解知识点本身的核心概念和实现原理
2. 内容务必具体、技术准确
3. 给出真实的代码示例或伪代码
4. 延伸阅读使用格式 \`- [文章标题](知识点的itemId)\`
5. 篇幅：strong=800-1500 tokens, medium=400-600 tokens`,
  db: `你的身份：你是一名资深数据库内核开发工程师和数据库技术布道者。
你的任务：为下面的数据库知识点写一篇深入的技术讲解文章。站在数据库内核的角度，深入讲解该机制的实现原理、工程设计和在主流数据库中的具体落地。

输出格式：
<!-- item: <itemId> -->
# 文章标题

文章正文...

写作原则：
1. 深入讲解知识点本身的核心概念和实现原理
2. 内容务必具体、技术准确
3. 给出真实的代码示例或伪代码
4. 延伸阅读使用格式 \`- [文章标题](知识点的itemId)\`
5. 篇幅：strong=800-1500 tokens, medium=400-600 tokens`,
};

// Phase 1: 并行生成
phase('VDB');
phase('DB');

// One agent per item — each gets a small focused prompt
const results = await pipeline(
  ITEMS,
  // Stage 1: Generate
  async (item) => {
    const task = TASK[item.stack] || TASK.db;
    const tierGuide = item.tier === 'strong'
      ? '要求写一篇完整、深入的文章（800-1500 tokens）'
      : '要求写一篇简洁但深入的文章（400-600 tokens）';

    const prompt = task + '\n\n## 知识点\n\n### ' + item.id + '\n- **标题**: ' + item.title + '\n- **描述**: ' + item.desc + '\n- **深入度**: ' + item.tier + '\n- **模板**:\n' + tierGuide;

    return await agent(prompt, {
      label: item.id,
      phase: item.stack === 'vdb' ? 'VDB' : 'DB',
    });
  },
  // Stage 2: Parse
  async (output, item) => {
    const match = output.match(/<!--\s*item:\s*([\w-]+)\s*-->([\s\S]*)/);
    const content = match ? match[2].trim() : output.trim();
    return { itemId: item.id, content, stack: item.stack };
  }
);

const allContent = [];
for (const r of results) {
  if (r) allContent.push(r);
}

log('Generated: ' + allContent.length + ' items');

return { totalItems: allContent.length, allContent };
