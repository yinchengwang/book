## 1. learn.html 渲染适配 + 导学补齐

- [x] 1.1 在 learn.html 的 SECTION_META 追加 `{ key: "deepdive", title: "📖 深度阅读", badge: "05" }`
- [x] 1.2 在 learn.html 新增 `renderDeepDive()` 函数：fetch `data/learn-deep/<item-id>.md` + 简易 markdown 渲染器
- [x] 1.3 实现渲染器：支持标题(`##`)、段落、代码块(````)、表格、链接、无序列表、引用（~55 行）
- [x] 1.4 在 learn.html 的 `render()` 中调用 `renderDeepDive()`
- [x] 1.5 创建 `data/learn-content-vdb.js`：VDB 15 个知识点的 FOCUS + 4 段导学
- [x] 1.6 创建 `data/learn-content-linux.js`：Linux 62 个知识点的 FOCUS + 4 段导学
- [x] 1.7 在 learn.html 的 OPTIONAL_CONTENT_SCRIPTS 中追加 vdb 和 linux 导学
- [x] 1.8 验证：打开浏览器确认 `db-vector-basic` 的深度文章正常渲染

## 2. VDB 核心索引算法深度文章

- [x] 2.1 写 `data/learn-deep/db-ivf-variants.md` — IVF 家族（IVFFlat/IVFPQ/IVFSQ）
- [x] 2.2 写 `data/learn-deep/db-hnsw.md` — HNSW 索引原理与实现
- [x] 2.3 写 `data/learn-deep/db-graph-index.md` — 图索引族谱（NSW/HNSW/NSG/Vamana）

## 3. VDB 量化压缩技术深度文章

- [x] 3.1 写 `data/learn-deep/db-pq-quant.md` — 乘积量化 PQ
- [x] 3.2 写 `data/learn-deep/db-quantization.md` — 量化技术全景（PQ/OPQ/SQ/AQ/RVQ）
- [x] 3.3 写 `data/learn-deep/db-scalar-quant.md` — 标量量化与 SIMD 距离计算

## 4. VDB 系统工程实践深度文章

- [x] 4.1 写 `data/learn-deep/db-diskann.md` — DiskANN 磁盘向量索引
- [x] 4.2 写 `data/learn-deep/db-milvus-arch.md` — Milvus 存算分离架构
- [x] 4.3 写 `data/learn-deep/db-milvus-segment.md` — Milvus 段管理
- [x] 4.4 写 `data/learn-deep/db-milvus-index.md` — Milvus 索引构建与调度
- [x] 4.5 写 `data/learn-deep/db-milvus-search.md` — Milvus 混合查询
- [x] 4.6 写 `data/learn-deep/db-hybrid-search.md` — 混合查询（标量+向量/SPANN）
- [x] 4.7 写 `data/learn-deep/vdb-pinecone-serverless.md` — Pinecone Serverless 架构
- [x] 4.8 写 `data/learn-deep/vdb-pinecone-storage.md` — Pinecone 存储与索引

## 5. DB 存储引擎深度文章

- [x] 5.1 写 `data/learn-deep/db-page-block.md` — 数据页与块结构
- [x] 5.2 写 `data/learn-deep/db-buffer-pool.md` — Buffer Pool 缓存管理
- [x] 5.3 写 `data/learn-deep/db-wal.md` — WAL 预写日志原理
- [x] 5.4 写 `data/learn-deep/db-lsm.md` — LSM-Tree 原理与实现
- [x] 5.5 写 `data/learn-deep/db-compaction.md` — Compaction 与空间回收

## 6. DB 索引与查询引擎深度文章

- [x] 6.1 写 `data/learn-deep/db-btree-idx.md` — B+树索引原理
- [x] 6.2 写 `data/learn-deep/db-btree-impl.md` — B+树实现与优化
- [x] 6.3 写 `data/learn-deep/db-optimizer.md` — 查询优化器原理
- [x] 6.4 写 `data/learn-deep/db-executor.md` — 查询执行引擎

## 7. DB 事务与恢复深度文章

- [x] 7.1 写 `data/learn-deep/db-mvcc-principle.md` — MVCC 多版本并发控制
- [x] 7.2 写 `data/learn-deep/db-locking.md` — InnoDB 锁机制详解
- [x] 7.3 写 `data/learn-deep/db-redo-log-detail.md` — Redo Log 与组提交
- [x] 7.4 写 `data/learn-deep/db-aries.md` — ARIES 恢复算法
- [x] 7.5 写 `data/learn-deep/db-consensus.md` — 分布式共识 Raft/Paxos

## 8. DB Redis 子系统深度文章

- [x] 8.1 写 `data/learn-deep/db-redis-event.md` — Redis 事件驱动模型
- [x] 8.2 写 `data/learn-deep/db-redis-object.md` — Redis 对象系统
- [x] 8.3 写 `data/learn-deep/db-redis-persist.md` — Redis 持久化 RDB/AOF
- [x] 8.4 写 `data/learn-deep/db-redis-cluster.md` — Redis 集群与哨兵

## 9. DB 分布式与嵌入式深度文章

- [x] 9.1 写 `data/learn-deep/db-sharding.md` — 数据分片与分区策略
- [x] 9.2 写 `data/learn-deep/db-ha.md` — 高可用与故障转移
- [x] 9.3 写 `data/learn-deep/db-sqlite-arch.md` — SQLite 架构与嵌入存储

## 10. Linux 可观测性深度文章

- [x] 10.1 写 `data/learn-deep/linux-proc-fs.md` — procfs 文件系统接口
- [x] 10.2 写 `data/learn-deep/linux-cpu-diag.md` — CPU 性能诊断
- [x] 10.3 写 `data/learn-deep/linux-mem-diag.md` — 内存性能诊断
- [x] 10.4 写 `data/learn-deep/linux-disk-iostat.md` — 磁盘 I/O 性能观测
- [x] 10.5 写 `data/learn-deep/linux-htop.md` — 系统监控与进程管理
- [x] 10.6 写 `data/learn-deep/linux-sar.md` — 系统活动报告 sar
- [x] 10.7 写 `data/learn-deep/linux-perf-basic.md` — perf 性能分析基础
- [x] 10.8 写 `data/learn-deep/linux-blktrace.md` — 块设备 I/O 追踪
- [x] 10.9 写 `data/learn-deep/linux-pagecache-hit.md` — Page Cache 命中率分析
- [x] 10.10 写 `data/learn-deep/linux-net-diag.md` — 网络性能诊断
- [x] 10.11 写 `data/learn-deep/linux-ebpf-intro.md` — eBPF 可观测性
- [x] 10.12 写 `data/learn-deep/linux-db-flamegraph.md` — 数据库火焰图分析
- [x] 10.13 写 `data/learn-deep/linux-numa-observ.md` — NUMA 观测与亲和性
- [x] 10.14 写 `data/learn-deep/linux-latency-diag.md` — 延迟诊断与分析

## 11. Linux 内核子系统深度文章

- [x] 11.1 写 `data/learn-deep/linux-syscall.md` — 系统调用机制
- [x] 11.2 写 `data/learn-deep/linux-pagecache.md` — Page Cache 内核实现
- [x] 11.3 写 `data/learn-deep/linux-cfs.md` — CFS 完全公平调度器
- [x] 11.4 写 `data/learn-deep/linux-vm.md` — 虚拟内存管理
- [x] 11.5 写 `data/learn-deep/linux-direct-io.md` — 直接 IO 原理
- [x] 11.6 写 `data/learn-deep/linux-hugepages.md` — 大页机制 HugePages
- [x] 11.7 写 `data/learn-deep/linux-fsync.md` — fsync 与数据持久化
- [x] 11.8 写 `data/learn-deep/linux-io-scheduler.md` — IO 调度器
- [x] 11.9 写 `data/learn-deep/linux-strace.md` — 系统调用追踪
- [x] 11.10 写 `data/learn-deep/linux-seccomp.md` — seccomp 安全计算
- [x] 11.11 写 `data/learn-deep/linux-iouring.md` — io_uring 异步 IO
- [x] 11.12 写 `data/learn-deep/linux-mmap-db.md` — mmap 与数据库应用
- [x] 11.13 写 `data/learn-deep/linux-cgroup2.md` — cgroup v2 资源隔离
- [x] 11.14 写 `data/learn-deep/linux-rcu.md` — RCU 机制

## 12. Linux 存储与文件系统深度文章

- [x] 12.1 写 `data/learn-deep/linux-ext4-xfs.md` — ext4/xfs 文件系统
- [x] 12.2 写 `data/learn-deep/linux-disk-ssd.md` — HDD/SSD 特性与性能模型
- [x] 12.3 写 `data/learn-deep/linux-block-layer.md` — 块设备层
- [x] 12.4 写 `data/learn-deep/linux-lvm.md` — LVM 逻辑卷管理
- [x] 12.5 写 `data/learn-deep/linux-fallocate.md` — 文件预分配 fallocate
- [x] 12.6 写 `data/learn-deep/linux-fdatasync.md` — fdatasync 与元数据优化
- [x] 12.7 写 `data/learn-deep/linux-fs-journal.md` — 文件系统日志
- [x] 12.8 写 `data/learn-deep/linux-fio.md` — fio 存储基准测试
- [x] 12.9 写 `data/learn-deep/linux-btrfs.md` — Btrfs 快照与压缩
- [x] 12.10 写 `data/learn-deep/linux-nfs.md` — NFS 分布式文件系统
- [x] 12.11 写 `data/learn-deep/linux-fuse.md` — FUSE 用户态文件系统
- [x] 12.12 写 `data/learn-deep/linux-raid.md` — RAID 与存储可靠性
- [x] 12.13 写 `data/learn-deep/linux-fs-cache.md` — 文件系统缓存层
- [x] 12.14 写 `data/learn-deep/linux-io-uring-storage.md` — io_uring 存储应用

## 13. Linux 网络栈深度文章

- [x] 13.1 写 `data/learn-deep/linux-tcp-state.md` — TCP 状态机
- [x] 13.2 写 `data/learn-deep/linux-socket-buf.md` — Socket 缓冲区
- [x] 13.3 写 `data/learn-deep/linux-tcpdump.md` — tcpdump 抓包分析
- [x] 13.4 写 `data/learn-deep/linux-conn-pool.md` — 连接池管理
- [x] 13.5 写 `data/learn-deep/linux-epoll.md` — epoll 事件驱动
- [x] 13.6 写 `data/learn-deep/linux-zero-copy.md` — 零拷贝技术
- [x] 13.7 写 `data/learn-deep/linux-cpu-affinity.md` — CPU 绑核与中断亲和性
- [x] 13.8 写 `data/learn-deep/linux-unix-socket.md` — Unix Domain Socket
- [x] 13.9 写 `data/learn-deep/linux-so-reuseport.md` — SO_REUSEPORT 与多线程
- [x] 13.10 写 `data/learn-deep/linux-iptables.md` — iptables/nftables 防火墙
- [x] 13.11 写 `data/learn-deep/linux-namespace.md` — 网络命名空间
- [x] 13.12 写 `data/learn-deep/linux-veth.md` — veth 与虚拟网络设备
- [x] 13.13 写 `data/learn-deep/linux-bridge.md` — Linux 网桥
- [x] 13.14 写 `data/learn-deep/linux-xdp.md` — XDP 高速数据路径

> 进度：99/99
