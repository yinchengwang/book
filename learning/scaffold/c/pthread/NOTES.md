# pthread 学习笔记

## 概念地图

POSIX 线程（pthread）是 POSIX 标准定义的多线程编程接口，在 Linux/macOS/部分 Unix 与 MinGW-w64 实现。核心模型：

- `pthread_create / pthread_join` 是线程的生命周期锚点
- `pthread_mutex` 提供共享状态原子访问，常用 `PTHREAD_MUTEX_INITIALIZER` 静态初始化
- `pthread_cond` 实现"等待某条件成立"的同步，常和 mutex 配对（生产者-消费者）
- 线程局部存储：`__thread` 关键字（GCC 扩展）或 pthread_key_t 系列 API

线程与进程的本质区别：进程独享地址空间，线程共享堆但独享栈与寄存器。共享是性能优势也是 bug 温床。

## 踩坑记录

1. **MinGW-w64 链接 pthread**：本机 `gcc main.c -lpthread` 即可；某些发行版需要 `-pthread` 同时作为编译和链接参数。本机 mingw 14.2 下 `-lpthread` 通过。
2. **mutex 静态初始化的妙处**：`PTHREAD_MUTEX_INITIALIZER` 是 file-scope 静态对象，无需 destroy。但若用 `pthread_mutex_init` 动态初始化，结束必须 `pthread_mutex_destroy`。
3. **counter 200000 而不是 200000 次锁切换**：本 demo 是 100k×2 worker 加 mutex，每次循环一次加锁/解锁。mutex 开销在 ~10-30ns 量级，对纯计数器是不可忽略的——大批量累加可用原子操作或 thread-local 累加最后合并。
4. **`pthread_create` 返回非零错误码**：成功返回 0，错误返回 errno 值（不是 errno 本身）。需要 `strerror(rc)` 看人话。
5. **不调用 `pthread_join` 主线程退出** → 子线程被强杀 → 可能写出半截文件、损坏共享状态。本 demo `join` 在主线程内同步等待。

## 工程对照（≥100 字硬约束）

pthread 在工程侧未来的 RAG 服务化（minivecdb 控制面、独立进程、长连接调度）有以下直接迁移价值：

1. **rag-orchestrator 线程池**：未来 rag server 的 LLM 调用是 IO-bound，适合 4-8 worker 线程池并发。本 demo 的"mutex + worker 函数"模型可直接套用作 `worker_t`。
2. **engineering/src/db/storage/wal_writer.c**：WAL 写入是单写多读的关键路径，源码 `bufmgr.c` 已用 pthread mutex 保护 LRU 链表。本卡刷过后可直接读懂那段代码的 lock 顺序。
3. **raft 实现**：你的仓库 `engineering/include/db/consensus/raft.h` 中，每个 raft node 都是一个 pthread 循环跑 election/append 循环；未来扩展 leader 选举时需要懂得 pthread_cancel 与 pthread_setcancelstate 的安全语义。
4. **engineering/src/algo-prod/dist_txn.c** 与 `coordinator.c`：分布式事务的两阶段提交里，每个参与者的状态变更需要锁保护，pthread mutex 比 atomic 更易审计。

迁移至此为止是"读得懂、改得动"的层次。**更高阶用法**（读写锁、自旋锁、lock-free）属于 R6+ 学习闭环。
