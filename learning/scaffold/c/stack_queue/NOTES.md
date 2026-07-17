# stack_queue 学习笔记

## 概念地图

栈和队列是限制访问顺序的线性数据结构——限制即是能力：

- **栈（LIFO）**：push/pop 都在同一端。函数调用栈、表达式求值、括号匹配、DFS 回溯全是栈
- **队列（FIFO）**：enqueue 在一端、dequeue 在另一端。BFS、消息队列、任务调度、IO 缓冲全是队列
- **循环队列**：用固定数组 + 模运算实现 O(1) 入队出队。比链式队列减少 malloc/free 开销，适合高频入队出队场景
- **双端队列（deque）**：两端均可 push/pop——是栈和队列的超集。C++ `std::deque` 是其工程实现，支持随机访问
- **优先级队列**：不是严格 FIFO——出队按优先级而非时间。通常用二叉堆实现。`tree` 卡会讲堆

与本仓库其它卡的关系：`pointer`（指针基础）是链式栈/队列的前置卡。`tree`（树/堆）是优先级队列的后继卡。

## 踩坑记录

1. **循环队列的"满"判断**：如果只用 `head/tail` 两个变量，`head==tail` 时无法区分"空"和"满"——本 demo 用 `size` 变量明确区分，空间换可读性
2. **双端队列的索引方向**：不同文献对 front/rear 的定义不同——本 demo 用"front 从数组头增长、rear 从数组尾收缩"的策略，push_front 和 push_back 的模运算方向相反
3. **链式栈 free 陷阱**：pop 时如果先 free 再读 next——UB！本 demo 用临时变量 `next = top->next` 保存
4. **realloc 不适用于循环队列**：循环队列的逻辑结构是环形的，`realloc` 后所有 `head/tail` 索引需要重新计算——实际工程中循环队列用固定容量或切换到链式

## 工程对照（≥100 字硬约束）

栈和队列在工程侧有以下直接复用点：

1. **SQL Parser 表达式栈**：`engineering/src/db/sql/sql_parser.c` 中解析算术表达式、函数调用、嵌套子查询时使用递归下降——递归的本质是系统栈。如果手写非递归 parser，需要显式维护运算符栈和操作数栈——本卡的数组栈是最小原型
2. **BFS 遍历队列**：`engineering/src/db/index/btree/btreeam.c` 中 BTree 层序遍历用队列。`rag/src/rag/index/hnsw_index.cpp` 中 HNSW 搜索的 beam search 用优先级队列。本卡的循环队列是这些算法的基础构件
3. **WAL Write Buffer**：`engineering/src/db/storage/wal/wal_buf.c` 中 WAL Buffer 是生产者-消费者环形缓冲区——本质是循环队列。write 端追加 log entry（enqueue）、flush 端刷盘后前移 head（dequeue）
4. **`engineering/src/db/storage/buffer/bufmgr.c`**：Clock-Sweep 的时钟指针在 Buffer Pool 链表上循环移动——本质是一个环上的队列轮转。本卡的循环队列模运算逻辑直接迁移

学完本卡后能动手的事：在 `engineering/src/db/storage/buffer/` 下加一个 `ring_buffer.c`，用循环队列实现 WAL 日志的 lock-free 写入缓冲——生产者（SQL 执行器）写 tail、消费者（WAL Writer）读 head
