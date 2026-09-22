# gap04 OLTP 并行查询引擎 —— 设计文档

- 日期：2026-09-22
- 状态：已获用户逐节批准（架构/数据面/错误处理与验收）
- 前置：gap03 统一执行器（`docs/superpowers/specs/2026-09-02-gap03-unified-executor-design.md` §11 预留扩展点）、gap06 分片与负载均衡
- 范围定义来源：gap04 此前仅有一句预留定义（gap03 设计 §11："ExecNode 增加并行属性，Executor 支持多线程调度"），本文档首次定义其范围、交付与验收

## 1. 目标与范围

三大支柱：

1. **并行调度** —— 统一执行器（ExecNode/VectorBlock 世界）内的多线程执行
2. **shard 裁剪** —— 谓词驱动的分片裁剪 + 多分片扇出（修复 gap06 遗留的 `shard_scan_next()` TODO）
3. **分布式 Join** —— 真网络数据面：VectorBlock 序列化 + RPC 流式传输 + BROADCAST/REPARTITION 两策略

明确的目标执行树：gap03 的 `plan_node_t`/`ExecNode`（Volcano pull + VectorBlock 批量）。sql/ 下 PG 风格并行栈（WorkerPool/TupleQueue/Gather，绑定 TupleTableSlot）仅作设计借鉴，不移植。

非目标（本期不做）：

- Morsel 驱动 / work-stealing 流水线（架构级决裂，留待未来评估）
- `PLAN_JOIN_NESTED`/`PLAN_JOIN_MERGE`/`PLAN_SORT`/`PLAN_LIMIT` 的 ExecNode 补齐（独立任务）
- 写路径（INSERT/UPDATE/DELETE）并行；仅只读查询
- sql/ PG 风格执行器内的并行增强

## 2. 总体架构（Exchange 括号模型）

在计划树中插入 Exchange 算子作为并行"括号"：

```
        上层 ExecNode（filter/project/agg…）
                    │ next()  ← 普通 pull，无感知
            ┌───────┴────────┐
            │  Exchange 节点  │  ← 对上层是普通算子
            └───────┬────────┘
         ┌──────────┼──────────┐
      worker 0   worker 1   worker N     ← px_scheduler 分发
         │          │          │
      子计划树   子计划树   子计划树      ← 每 worker 独立实例化，无共享状态
         │          │          │
         └──── px_queue（VectorBlock MPSC，有界背压）────┘
```

核心原则：**worker 各自独立实例化子计划树**（对同一 `plan_node_t` 子树调用 N 次 `exec_create`），每 worker 拥有自己的算子状态。竞态面收敛到 px_queue 与 px_scheduler 两处。

上层接口不变：Exchange 对上层永远是一个普通 ExecNode，pull 模型语义不被打破。

## 3. 计划层改动

文件：`include/db/optimizer/optimizer.h`、`src/db/optimizer/`、`src/db/executor/framework/plan_to_exec.c`

1. `plan_node_type_t` 新增 `PLAN_EXCHANGE`；Exchange 计划节点带 `exchange_mode`：
   - `LOCAL` —— 进程内并行
   - `BROADCAST` —— 小表广播
   - `REPARTITION` —— 按 join key 哈希重分布
2. 新增重写 pass `plan_parallelize(plan_node_t *)`：
   - 利用 `plan_node_t` 已有的 `total_cost`/`plan_rows`，在超过阈值的 scan/filter/project 区段顶部插入 Exchange
   - 阈值与最大并行度走配置：`parallel_min_rows`、`parallel_max_workers`
   - 小于阈值保持单线程 —— OLTP 小查询零开销
   - shard 表扫描处把谓词分片键条件下推到 shard scan 节点（见 §5）
3. `plan_to_exec.c` 增加 `PLAN_EXCHANGE` → `exec_create_exchange()` 映射。
4. `explain_plan_text` 支持显示 Exchange 节点及其 mode/并行度。

## 4. 核心组件（`src/db/executor/parallel/`）

### 4.1 px_queue（`px_queue.c`）

- VectorBlock 多生产者单消费者（MPSC）有界队列，容量 64 块
- 满则生产者阻塞 —— 天然背压
- 所有权规则：块入队即移交所有权，出队方负责 `vecx_block_destroy`
- 关闭语义区分正常 EOF（所有生产者完结）与 abort（错误/取消），消费者两条路径都能可靠醒来
- abort 路径销毁队列时，队列中残余未消费块由队列销毁方负责 `vecx_block_destroy` —— 无泄漏

### 4.2 px_scheduler（`px_scheduler.c`）

- 每服务器实例一个懒初始化线程池，worker 数 = min(CPU 核数, `parallel_max_workers`)
- 任务 = 执行一棵 ExecNode 子计划树直到迭代结束
- 协作式取消：取消标志位，worker 每处理一块检查一次
- `destroy` 必须先置取消、join 线程、再销毁队列 —— 悬挂 worker 可回收，无泄漏
- API 借鉴 sql/ WorkerPool，但数据模型为 VectorBlock，新建不移植

### 4.3 exchange_exec.c（Exchange 算子）

- open：向 scheduler 提交 N 个任务（本地模式）或建立 RPC 流（网络模式）
- next()：从 px_queue 拉块；EOF = 所有 worker 完结且队列空
- 错误帧（见 §7）经队列/网络传播后在 next() 中向上层返回错误

### 4.4 单机并行 hash join（无竞态方案）

- build 侧：Exchange 并行**扫描**，汇成单个全局 build 表 —— 扫描并行、建表串行
- build 完成后表变只读；probe 侧 Exchange 多 worker 并发 probe —— 只读共享，无锁
- 复用现有 `vecx_hashjoin`，不改其内核

## 5. shard 裁剪与扇出

文件：`src/db/executor/operators/shard_scan_exec.c`（重写）、`include/db/sharding/sharding.h`（复用）

现状：`shard_scan_next()` 只选单个最小负载分片且 `return NULL`（不产行，gap06 遗留 TODO）。

1. **裁剪**：计划重写阶段把 filter 谓词中的分片键条件（等值/范围）下推到 shard scan 节点；open 时调用：
   - `shard_route(key)` —— 点查
   - `shard_route_range(min, max, ids[], n)` —— 范围（现成但此前未接线）
   算出候选分片子集，无缘分片整个跳过。无分片键谓词时退化为全分片扇出。
2. **扇出**：每个入选分片生成一个子扫描任务（本地分片走本地表扫描；远端分片走 Exchange sender），全部喂给上层 Exchange —— shard 扇出与并行调度复用同一机制。
3. `shard_coordinator_select_least_load()` 保留用于写入路由；读路径改用裁剪+扇出。

## 6. 分布式数据面

### 6.1 pxwire 块序列化格式（`src/db/executor/parallel/px_wire.c`）

```
┌ header ────────────────────────────────────┐
│ magic "PXB1" │ version │ block_seq │ flags │  flags: LAST=流结束, ERR=携带错误
│ row_count    │ col_count                   │
├ per column ────────────────────────────────┤
│ type_tag │ null_bitmap │ fixed data         │  定长列
│ type_tag │ null_bitmap │ offsets[] │ bytes  │  变长列
└ CRC32 ─────────────────────────────────────┘
```

### 6.2 传输层

- 首选：扩展现有 `rpc.h`（`include/db/distributed/rpc.h`，连接池/24 字节头/CRC32 已就绪）新增流式 API：`rpc_stream_open/send/recv/close`，长连接分帧收发
- 降级方案（仅当 rpc 层扩展评估后侵入过大时启用）：独立 `px_channel`，同风格直接 TCP
- Exchange 的两种传输实现，同一算子语义：
  - `LOCAL`：worker → px_queue
  - 网络：sender 端 worker 序列化后 `rpc_stream_send`；receiver 端 Exchange 的后台收流线程解包投入本地 px_queue，next() 照常拉取

### 6.3 分布式 Join 两策略

- **BROADCAST**：build 侧 `plan_rows` < `broadcast_join_max_rows`（配置阈值）时，build 侧各节点本地扫描 → Exchange BROADCAST 复制到全部 probe 节点 → 各 probe 节点本地建表、本地 hash join、结果上行。网络开销 = 小表 × 节点数
- **REPARTITION**：双侧都大时，两侧各按 `hash(join_key) % N` 把块路由到 N 个目标节点，落在同一目标的左右侧块做本地 hash join —— shuffle 后无跨节点依赖
- 策略由 `plan_parallelize()` 依据代价估计选择

### 6.4 fragment 落地

复用 `src/db/distributed/query/distributed_query.c` 的 `distributed_plan_t{fragments[], stage}` 骨架，把 `execute_fragment_simulation()`（当前 mock）替换为真实执行体：fragment = 序列化 plan 子树 + Exchange sender 指向下游节点。

## 7. 错误处理与资源回收

1. **worker 失败传播**：任一 worker 的 open/next 失败 → 错误码+消息写入 px 共享错误槽（每 Exchange 一个，原子写一次）→ 置取消标志 → 其余 worker 下一块边界退出 → Exchange.next() 拉到错误标记后向上返回错误。ERR 帧沿 pxwire 跨节点传播，receiver 侧同样生效
2. **协作式取消**：worker 每处理一块检查一次取消标志；exec_close/exec_destroy 先置取消、join 线程、再销毁队列
3. **队列关闭**：正常 EOF 与 abort 区分（见 §4.1）
4. **RPC 失败**：只读 scan fragment 允许重试一次（幂等限定）；重试仍败或不可重试 → 整查询失败，已分配的 stream/线程/块全部回收；网络断连视为 receiver 侧 ERR 帧处理

## 8. 测试计划（GTest，`test/db/executor/`）

| 层 | 用例 | 对应验收 |
|---|---|---|
| 单元 | `test_px_queue`：MPSC 并发压测、满阻塞背压、双路径关闭 | 无竞态 |
| 单元 | `test_px_scheduler`：任务执行、取消、销毁时悬挂回收 | 无竞态/资源 |
| 单元 | `test_px_wire`：定长/变长/NULL 位图序列化 roundtrip | 结果一致 |
| 单元 | `test_shard_prune`：等值/范围谓词 → 分片子集正确 | shard 裁剪 |
| 集成 | `test_parallel_exec`：并行 scan/filter/project/agg 与单线程结果集一致（无序比较） | 结果一致 |
| 集成 | `test_parallel_exec`：并行 hash join（build 串行 + probe 并行）与单线程一致 | 结果一致 |
| 基准 | 大数据集 seqscan+agg，4 worker 加速比 ≥3x（沿用 task5.4 基准方法） | 加速比 |
| 质量 | 全部并行用例在 ThreadSanitizer 构建下零报告 | 无竞态 |
| 分布式 | `test_distributed_join`：localhost 双实例（双端口）BROADCAST 与 REPARTITION join 各一，结果与单机一致；kill receiver 故障注入 → 查询报错且资源回收 | 双实例 Join |

## 9. 构建组织

- 新源码并入 `db_executor` 库：`src/db/executor/parallel/{px_queue.c, px_scheduler.c, exchange_exec.c, px_wire.c}`
- RPC 流式扩展：`src/db/distributed/rpc/`；fragment 真实执行体：`src/db/distributed/query/distributed_query.c`
- 测试目标纳入现有测试套件聚合 runner
- 新增 `tsan` 构建配置（或文档化的 `-fsanitize=thread` 手动流程）

## 10. 验收标准（完整基线）

| 标准 | 落点 |
|---|---|
| 并行/单机结果一致（含 Join） | 集成 + 分布式用例 |
| 4 worker 扫描加速比 ≥3x | 基准用例 |
| 无数据竞争 | TSan 全量零报告 |
| 双实例 localhost 分布式 Join 跑通 | 分布式集成用例 |

## 11. 关键设计决策记录

| 决策 | 选择 | 备选 | 理由 |
|---|---|---|---|
| 目标执行器 | 统一执行器 ExecNode/VectorBlock | 移植 sql/ 并行栈；sql/ 内增强 | gap 系列钦定方向，gap03 预留扩展点 |
| 并行模型 | Exchange 括号模型 | Morsel 流水线；仅扫描层扇出 | pull 语义不破；本地/网络传输同构；Join 并行有无竞态经典解 |
| 分布式深度 | 真网络数据面 | 进程内 Exchange 先行；剔除 | 用户选定；rpc.h 连接池/协议头可直接利用 |
| 并发正确性 | worker 独立子树 + build 后只读 | 逐算子加锁 | 竞态面最小化，"最好的锁是没有锁" |
| 错误传播 | 错误编码进队列/流的标记帧 | 旁路错误通道 | 本地与跨节点传播路径同构 |
