## 1. Python 题库扩充（A 系列）

- [x] 1.1 A5：py-c-ext / py-memory / py-numpy / py-pandas 各加 5 题（验证每个 10 题）
- [x] 1.2 A6：py-viz / py-sklearn / py-feature-eng / py-pytorch 各加 5 题
- [x] 1.3 A7：py-nlp / py-distributed-ml / py-model-deploy / py-profiling 各加 5 题
- [x] 1.4 A8：py-pytest / py-git / py-debug / py-web 各加 5 题
- [x] 1.5 A9：py-code-quality / py-docker / py-celery / py-grpc / py-ci 各加 5 题
- [x] 1.6 A10：py-gil / py-async 各加 5 题
- [x] 1.7 验证 Python 全部 43 个知识点均达到 10 题，node --check 通过

## 2. DB 分布式题库扩充（B 系列）

- [x] 2.1 B1：向 quiz-tech.js DB_TECH_DATA 追加分布式知识点元数据（db-raft/db-paxos/db-dist-tx/db-saga/db-tcc）
- [x] 2.2 B2：quiz-questions-db.js 新增 db-raft/db-paxos 各 5 题（Leader 选举、日志复制、脑裂）
- [x] 2.3 B3：quiz-questions-db.js 新增 db-dist-tx/db-saga/db-tcc 各 5 题
- [x] 2.4 B4：追加 db-sharding/db-newsql/db-tidb 元数据 + 各 5 题
- [x] 2.5 B5：追加 db-mvcc/db-lsm/db-wal 元数据 + 各 5 题
- [x] 2.6 B6：追加 db-buffer-pool/db-columnstore/db-bloom 元数据 + 各 5 题
- [x] 2.7 B7：追加 db-redis-object/db-redis-event/db-redis-aof 元数据 + 各 5 题（Redis 底层）
- [x] 2.8 B8：追加 db-redis-cluster/db-redis-sentinel 元数据 + 各 5 题

## 3. 向量数据库题库（B 向量系列）

- [x] 3.1 B-V1：quiz-tech.js 追加向量 DB 知识点元数据（db-vector-intro/db-milvus/db-chroma/db-hnsw/db-ivf/db-pq/db-diskann/db-scann/db-ann-eval/db-index-tuning/db-hybrid-search）
- [x] 3.2 B-V2：quiz-questions-db.js 新增 db-hnsw/db-ivf 各 5 题（图结构、倒排索引、nlist/nprobe 参数）
- [x] 3.3 B-V3：新增 db-pq/db-diskann 各 5 题（PQ 量化子空间、DiskANN 磁盘图索引）
- [x] 3.4 B-V4：新增 db-milvus/db-vector-intro 各 5 题（Milvus 架构、向量数据库选型）
- [x] 3.5 B-V5：新增 db-ann-eval/db-index-tuning/db-hybrid-search 各 5 题（召回率评估、混合检索）
- [x] 3.6 验证 DB 全部知识点均有题库，node --check 通过

## 4. C/C++/DS 题库扩充

- [x] 4.1 C 语言：c-syntax/c-control-flow/c-array-string/c-function-scope/c-struct-union 各加 5 题
- [x] 4.2 C 语言：c-pointer/c-dynamic-memory/c-func-pointer/c-preprocessor/c-c11-features 各加 5 题
- [x] 4.3 C 语言：剩余知识点各加 5 题，全部达到 10 题
- [x] 4.4 C++：cpp 技术栈所有知识点各加 5 题，全部达到 10 题
- [x] 4.5 DS：数据结构所有知识点各加 5 题，全部达到 10 题

## 5. 学习内容页（learn.html）

- [x] 5.1 创建 `project/reading-radar/learn.html`，支持 hash 路由（`#<catId>/<itemId>`），含导航栏（返回看板 / 去测验）
- [x] 5.2 创建 `data/learn-content-py.js`，覆盖 Python 43 个知识点，每个含入门/原理/实战/调优面试 4 个 section
- [x] 5.3 创建 `data/learn-content-c.js`，覆盖 C 语言所有知识点
- [x] 5.4 创建 `data/learn-content-cpp.js`，覆盖 C++ 所有知识点
- [x] 5.5 创建 `data/learn-content-db.js`，覆盖 DB 所有知识点（含新增分布式+向量 DB）
- [x] 5.6 创建 `data/learn-content-ds.js`，覆盖数据结构所有知识点
- [x] 5.7 修改 learning-kanban.html，为每个知识点卡片新增"学习内容"链接（`learn.html#<catId>/<itemId>`）

## 6. 五年计划融合

- [x] 6.1 修改 dashboard.html，底部新增"五年成长路线"摘要区块（至少 3 个阶段卡片，含"查看完整计划"链接）
- [x] 6.2 修改 five-year-plan.html，顶部新增"← 返回仪表盘"导航链接

## 7. README 更新

- [x] 7.1 重写 `project/reading-radar/README.md`，新增四层联动架构表格（Layer 1-4）
- [x] 7.2 补充完整文件结构说明（所有 .html 和 data/*.js 文件用途）
- [x] 7.3 补充数据规范章节（QUESTION_BANK 结构、题目对象字段、题型枚举）
- [x] 7.4 补充各技术栈知识点汇总表（总数、题库状态）
- [x] 7.5 补充 learn.html 使用说明和 URL 格式

## 8. 联调驱动补题闭环（后续）

- [x] 8.1 把“页面联调 + DB 补题”合并方案同步到 tasks.md，作为后续执行基线
- [x] 8.2 第一批 DB 补题：db-dist-tx / db-saga / db-tcc / db-2pc / db-consensus 各补 5 题，全部达到 10 题
- [x] 8.3 第一轮页面联调：看板 → learn → quiz → dashboard → five-year-plan 路由与状态检查
- [x] 8.4 第二批 DB 补题：db-graph-index / db-quantization / db-scann / db-ann-eval / db-index-tuning 各补 5 题
- [x] 8.5 第三批 DB 补题：db-milvus-index / db-milvus-search / db-ha / db-perf / db-newsql 各补 5 题
- [x] 8.6 第四批 DB 补题：db-raft / db-paxos / db-redis-cluster / db-redis-sentinel / db-hybrid-search 各补 5 题
- [x] 8.7 复跑 DB 题量统计并收口缺口，确认当前覆盖 78/78，但仍有 52 个知识点未达到 10 题
- [x] 8.8 第五批 DB 补题：db-storage-overview / db-page-block / db-row-format / db-datafile / db-change-buffer
- [x] 8.9 第六批 DB 补题：db-checkpoint / db-compaction / db-sstable / db-bloom / db-btree-idx
- [x] 8.10 第七批 DB 补题：db-btree-impl / db-hash-idx / db-fulltext / db-spatial / db-sql-ddl
- [x] 8.11 第八批 DB 补题：db-sql-dml / db-vector-intro / db-vector-basic / db-ivf / db-ivf-variants
- [x] 8.12 第九批 DB 补题：db-pq / db-pq-quant / db-scalar-quant / db-sql-parser / db-optimizer
- [x] 8.13 第十批 DB 补题：db-optimizer-detail / db-executor / db-executor-detail / db-acid / db-sql-dcl
- [x] 8.14 第十一批 DB 补题：db-mvcc / db-mvcc-principle / db-readview / db-undo-log / db-locking
- [x] 8.15 第十二批 DB 补题：db-deadlock / db-optimistic / db-redo-log-detail / db-aries / db-snapshot-isol
- [x] 8.16 第十三批 DB 补题：db-redis-event / db-redis-resp / db-redis-object / db-redis-persist / db-redis-aof
- [x] 8.17 第十四批 DB 补题：db-redis-replica / db-milvus / db-milvus-arch / db-milvus-segment / db-chroma
- [x] 8.18 第十五批 DB 补题：db-sharding / db-tidb
- [x] 8.19 复跑 DB 题量统计并结合页面联调结果最终收口，验证全部知识点达到至少 10 题
