# 数据库整体架构设计

> 日期：2026-07-17
> 状态：设计稿
> 参考：PostgreSQL 15 源码布局 + 多模态数据库架构

## 1. 概述

本项目参考 PostgreSQL 架构，构建一个支持多模态数据的生产级数据库。架构采用**火山模型执行器 + 专用算子**的组合方式，各数据模型通过独立的查询引擎或协议接入，共享底层的缓冲区管理、存储管理和 WAL 基础设施。

## 2. 总体架构分层

```
┌────────────────────────────────────────────────────────────────────────────┐
│                          第 0 层：连接层                                     │
│  PG Wire Protocol │ 连接池 │ 认证 │ SSL/TLS                               │
└─────────────────────────────────┬──────────────────────────────────────────┘
                                  ↓
┌─────────────────────────────────┬──────────────────────────────────────────┐
│                          第 1 层：会话与事务层                               │
│  会话管理 │ 事务管理器 │ 锁管理器 │ 快照管理 │ 权限检查                      │
└─────────────────────────────────┬──────────────────────────────────────────┘
                                  ↓
┌─────────────────────────────────┬──────────────────────────────────────────┐
│                     第 2 层：查询引擎层（多入口并行）                         │
│                                                                              │
│  ┌────────────────────┐  ┌────────────────────┐  ┌──────────────────────┐  │
│  │  SQL 查询引擎      │  │  RESP 协议 (P2:KV) │  │  GQL 图查询 (P6)    │  │
│  │  PG Wire → Parser  │  │  Redis 序列化协议  │  │  GQL Parser →       │  │
│  │  → Analyzer →      │  │  → SET/GET/DEL/   │  │  模式匹配编译 →     │  │
│  │  Rewriter →        │  │  EXPIRE/... →     │  │  图遍历计划生成     │  │
│  │  Planner/Optimizer  │  │  → KV 算子        │  │                     │  │
│  └─────────┬──────────┘  └────────┬───────────┘  └──────────┬───────────┘  │
│            │                      │                         │              │
│  ┌─────────┴──────────┐  ┌───────┴──────────┐  ┌───────────┴───────────┐  │
│  │  Datalog 推理(P7)  │  │  NETCONF/        │  │  Stream SQL (P9)     │  │
│  │  Datalog Parser →  │  │  RESTCONF (P8)   │  │  CREATE STREAM →     │  │
│  │  规则编译 →        │  │  Yang Path 解析  │  │  物化视图 →          │  │
│  │  半朴素求值器      │  │  → 树遍历/修改   │  │  Watermark/Trigger   │  │
│  └─────────┬──────────┘  └──────────────────┘  └──────────┬───────────┘  │
│            │                                               │              │
│            └──────────────────┬────────────────────────────┘              │
└───────────────────────────────┼───────────────────────────────────────────┘
                                ↓
┌────────────────────────────────────────────────────────────────────────────┐
│                     第 3 层：执行器层（Volcano 模型）                        │
│                                                                              │
│  ┌────────────────────────────────────────────────────────────────────┐    │
│  │  标准算子（SQL 查询引擎使用）                                        │    │
│  │  SeqScan / IndexScan / IndexOnlyScan / BitmapScan / TidScan       │    │
│  │  NestLoop / HashJoin / MergeJoin                                   │    │
│  │  HashAgg / SortAgg / GroupAgg / WindowAgg                          │    │
│  │  Sort / Limit / Project / Filter / Distinct / Unique               │    │
│  │  Insert / Update / Delete / ModifyTable / Vacuum / Analyze         │    │
│  │  Result / SubqueryScan / CteScan / Materialize / RecursiveUnion    │    │
│  └────────────────────────────────────────────────────────────────────┘    │
│                                                                              │
│  ┌────────────────────────────────────────────────────────────────────┐    │
│  │  多模态算子（各模型专用算子，优先级排列）                               │    │
│  │                                                                      │    │
│  │  P1 向量:  VectorScan / HnswScan / IvfScan / VectorDistJoin        │    │
│  │  P2 KV:    KvScan / KvPointLookup / KvRangeScan                    │    │
│  │  P3 时序:  TsScan / TsWindowAgg / TsDownsample                     │    │
│  │  P4 文档:  DocumentScan / Bm25Scan / JsonPathFilter                │    │
│  │  P5 空间:  SpatialScan / SpatialKnnScan / SpatialJoin              │    │
│  │  P6 图:    GraphScan / GqlTraverse / GraphShortestPath             │    │
│  │  P7 逻辑:  DatalogScan / RecursiveCteScan                          │    │
│  │  P8 树:    YangScan / YangPathTraverse                             │    │
│  │  P9 流:    StreamScan / StreamWindow / StreamJoin / StreamAgg      │    │
│  └────────────────────────────────────────────────────────────────────┘    │
│                                                                              │
│  算子直调多模态引擎（不经过 Access Method 层包装）                            │
└─────────────────────────────────┬──────────────────────────────────────────┘
                                  ↓
┌─────────────────────────────────┬──────────────────────────────────────────┐
│                     第 4 层：访问方法层（Access Method）                      │
│                                                                              │
│  ┌────────────────────────────────────────────────────────────────────┐    │
│  │  Table AM 接口                                                     │    │
│  │  ├── Heap AM — 堆表存储（已有）                                     │    │
│  │  └── FDW 接口 — 外部表包装器                                       │    │
│  └────────────────────────────────────────────────────────────────────┘    │
│                                                                              │
│  ┌────────────────────────────────────────────────────────────────────┐    │
│  │  Index AM 接口                                                     │    │
│  │  ├── BTree AM — B+树索引（已有）                                    │    │
│  │  ├── Hash Index — 哈希索引（待实现）                                │    │
│  │  └── GiST / SP-GiST / GIN — 通用索引框架（待实现）                  │    │
│  └────────────────────────────────────────────────────────────────────┘    │
│                                                                              │
│  ┌────────────────────────────────────────────────────────────────────┐    │
│  │  多模态引擎（算子直调，不走 AM 包装）                                   │    │
│  │  vector_engine / kv_engine / ts_engine / doc_engine /              │    │
│  │  spatial_engine / graph_engine / yang_engine（各引擎已有）          │    │
│  └────────────────────────────────────────────────────────────────────┘    │
├────────────────────────────────────────────────────────────────────────────┤
│                     第 5 层：缓冲区管理（Buffer Manager）                     │
│  页面哈希表 │ Clock-Sweep 置换 │ 脏页管理 │ Pin/Unpin │ 大对象缓冲        │
├────────────────────────────────────────────────────────────────────────────┤
│                     第 6 层：存储管理（Storage Manager）                      │
│  页面布局 │ 元组布局 │ FSM │ VM │ TOAST │ 分区管理                        │
├────────────────────────────────────────────────────────────────────────────┤
│                     第 7 层：WAL 与恢复（Write-Ahead Log）                   │
│  WAL 写入/回放 │ WAL 缓冲 │ 检查点 │ 归档                                  │
├────────────────────────────────────────────────────────────────────────────┤
│                     第 8 层：系统元数据（Catalog）                             │
│  pg_class / pg_attribute / pg_index / pg_type / OID 管理                  │
├────────────────────────────────────────────────────────────────────────────┤
│                     第 9 层：工具与基础设施                                     │
│  GUC 配置 │ 统计收集 │ 内存上下文(palloc) │ 错误处理(elog) │ 日志 │ 后台进程 │
└────────────────────────────────────────────────────────────────────────────┘
```

## 3. 各模型优先级与输入方式

| 优先级 | 模型 | 输入方式 | 说明 |
|--------|------|----------|------|
| P0 | 关系 | SQL（PG Wire Protocol） | 核心模型，已有完整实现 |
| P1 | 向量 | SQL 扩展（`ORDER BY vec <-> query LIMIT K`） | SQL 扩展，已有索引层 |
| P2 | KV | **RESP 协议**（Redis 兼容）+ SQL 可选 | 独立协议，Redis 生态兼容 |
| P3 | 时序 | SQL 扩展 + **InfluxDB Line Protocol**（写入） | SQL 扩展 + 独立写入协议 |
| P4 | 文档 | SQL + **JSONPath（RFC 9535）** | SQL 扩展，PostgreSQL 路线 |
| P5 | 空间 | SQL + **PostGIS 风格**（ST_* 函数） | SQL 扩展，OGIS 标准 |
| P6 | 图 | **GQL 图查询语言** | 独立语言，ISO 标准 |
| P7 | Datalog | **Datalog 逻辑编程语言** | 独立语言，声明式推理 |
| P8 | Yang | **NETCONF/RESTCONF + Yang Path** | 独立协议，网络设备管理 |
| P9 | 流 | SQL 扩展（`CREATE STREAM` + 物化视图）+ **Kafka 协议** | SQL 扩展 + 输入协议 |

## 4. 目录结构

顶层按模块组织，层内参考 PostgreSQL 的布局：

```
engineering/
├── include/
│   └── db/
│       ├── access/              # 第 4 层：访问方法
│       │   ├── heapam.h         # Heap AM 接口
│       │   ├── btreeam.h        # BTree AM 接口
│       │   ├── table.h          # Table AM 统一接口
│       │   └── index.h          # Index AM 统一接口
│       │
│       ├── catalog/             # 第 8 层：系统元数据
│       │   ├── catalog.h
│       │   ├── guc.h            # GUC 配置系统
│       │   └── initdb.h
│       │
│       ├── executor/            # 第 3 层：Volcano 执行器
│       │   ├── executor.h       # 执行器入口（execMain 等价）
│       │   ├── node*.h          # 标准算子（SeqScan / HashJoin / Agg...）
│       │   ├── vector/          # P1: 向量算子
│       │   │   ├── vector_scan.h
│       │   │   ├── hnsw_scan.h
│       │   │   └── ivf_scan.h
│       │   ├── kv/              # P2: KV 算子
│       │   │   ├── kv_scan.h
│       │   │   └── kv_lookup.h
│       │   ├── ts/              # P3: 时序算子
│       │   │   ├── ts_scan.h
│       │   │   └── ts_window.h
│       │   ├── doc/             # P4: 文档算子
│       │   │   ├── doc_scan.h
│       │   │   └── bm25_scan.h
│       │   ├── spatial/         # P5: 空间算子
│       │   │   ├── spatial_scan.h
│       │   │   └── spatial_knn.h
│       │   ├── graph/           # P6: 图算子
│       │   │   ├── graph_scan.h
│       │   │   └── gql_traverse.h
│       │   ├── datalog/         # P7: Datalog 算子
│       │   │   └── datalog_scan.h
│       │   ├── yang/            # P8: Yang 算子
│       │   │   └── yang_scan.h
│       │   └── stream/          # P9: 流算子
│       │       ├── stream_scan.h
│       │       ├── stream_window.h
│       │       ├── stream_join.h
│       │       └── stream_agg.h
│       │
│       ├── optimizer/           # 第 2 层：优化器
│       │   ├── optimizer.h
│       │   ├── path/
│       │   ├── plan/
│       │   ├── prep/
│       │   └── util/
│       │
│       ├── parser/              # 第 2 层：SQL 解析器
│       │   ├── gram.y / scan.l
│       │   └── sql/
│       │
│       ├── rewrite/             # 第 2 层：查询重写
│       │   └── rewrite.h
│       │
│       ├── nodes/               # 节点系统（与 PG 一致）
│       │   ├── list.h
│       │   ├── bitmapset.h
│       │   └── ...
│       │
│       ├── storage/             # 第 5~7 层：存储
│       │   ├── buf.h            # 第 5 层：缓冲区管理
│       │   ├── page.h           # 第 6 层：页面布局
│       │   ├── disk.h           # 第 6 层：文件 I/O
│       │   ├── smgr.h           # 存储管理器抽象
│       │   ├── freespace.h      # 空闲空间映射
│       │   ├── wal.h            # 第 7 层：WAL 日志
│       │   └── wal_buf.h
│       │
│       ├── tcop/                # 第 0~1 层：交通协管
│       │   ├── postgres.h       # 后端主循环
│       │   ├── utility.h        # 工具命令
│       │   └── dest.h           # 结果接收器
│       │
│       ├── libpq/               # 第 0 层：通信协议
│       │   └── pqcomm.h
│       │
│       ├── postmaster/          # 进程管理
│       │   ├── postmaster.h
│       │   └── bgworker.h
│       │
│       ├── replication/         # 复制
│       │   └── replication.h
│       │
│       ├── utils/               # 第 9 层：工具
│       │   ├── palloc.h         # 内存上下文
│       │   ├── elog.h           # 错误处理
│       │   ├── hash.h
│       │   ├── sort.h
│       │   ├── resowner.h
│       │   └── ...
│       │
│       ├── commands/            # DDL 命令
│       │   ├── vacuum.h
│       │   ├── explain.h
│       │   └── copy.h
│       │
│       ├── engine/              # 多模态引擎（直调）
│       │   ├── vector_engine.h
│       │   ├── kv_engine.h
│       │   ├── ts_engine.h
│       │   ├── doc_engine.h
│       │   ├── spatial_engine.h
│       │   ├── graph_engine.h
│       │   ├── yang_engine.h
│       │   └── stream_engine.h
│       │
│       └── protocol/            # 非 SQL 协议层
│           ├── resp.h           # P2: RESP（Redis）
│           ├── influx_line.h    # P3: InfluxDB Line Protocol
│           ├── gql.h            # P6: GQL 图查询语言
│           ├── datalog.h        # P7: Datalog 语言
│           ├── yang_path.h      # P8: Yang Path
│           └── kafka.h          # P9: Kafka 协议
│
├── src/
│   └── db/
│       ├── access/              # 访问方法实现
│       ├── catalog/             # 系统表实现
│       ├── executor/            # 执行器实现
│       │   ├── execMain.c
│       │   ├── execProcnode.c
│       │   ├── nodeSeqscan.c
│       │   ├── nodeIndexscan.c
│       │   ├── nodeHashjoin.c
│       │   ├── ...              # 标准算子
│       │   ├── vector/          # P1 向量算子实现
│       │   ├── kv/              # P2 KV 算子实现
│       │   ├── ts/              # P3 时序算子实现
│       │   ├── doc/             # P4 文档算子实现
│       │   ├── spatial/         # P5 空间算子实现
│       │   ├── graph/           # P6 图算子实现
│       │   ├── datalog/         # P7 Datalog 算子实现
│       │   ├── yang/            # P8 Yang 算子实现
│       │   └── stream/          # P9 流算子实现
│       ├── optimizer/
│       ├── parser/
│       ├── rewrite/
│       ├── nodes/
│       ├── storage/
│       ├── tcop/
│       ├── libpq/
│       ├── postmaster/
│       ├── replication/
│       ├── utils/
│       ├── commands/
│       ├── engine/              # 多模态引擎实现
│       └── protocol/            # 协议实现
│
└── test/
    └── db/
        ├── executor/            # 执行器测试
        ├── storage/             # 存储测试
        └── ...
```

## 5. 架构决策说明

### 5.1 为什么多模态引擎不走 Table AM 包装

多模态引擎（向量/KV/时序等）的查询语义与关系模型的表扫描差异过大：

- 向量 KNN 搜索不是全表扫描 + 排序，而是近似最近邻搜索（HNSW/IVF）
- 图遍历不是关系表的投影，而是递归的邻接探索
- 时序降采样不是标准聚合，而是带时间窗口的滑动计算

如果强行包装成 Table AM，优化器无法理解语义，会生成错误的执行计划。因此采用**专用算子直调**方案——执行器层新增对应算子，算子直接调用底层引擎接口。

### 5.2 为什么保留 Heap AM 和 BTree AM

关系模型的核心操作（全表扫描、索引查找、B+树范围扫描）仍然是数据库的基础设施，也是 WAL、Buffer Pool、事务系统的主要消费者。多模态引擎在需要持久化元数据时也依赖 Catalog 系统表（存储在 Heap 中）。因此 Access Method 层作为关系模型的存储骨架保留。

### 5.3 删除整合式执行器

原 `engineering/include/db/executor/sql/sql_executor.h` 是一个将 Catalog、Buffer Pool、WAL 直接耦合在一起的包装器，职责不清晰。该文件无其他模块引用，已删除。

保留 `engineering/include/db/sql/sql_executor.h`（Volcano 模型执行器）作为 SQL 查询的唯一执行入口。

## 6. 多模态算子接口规范

每个多模态算子遵循统一的 Volcano 迭代器协议：

```c
// 算子生命周期
void *op_init(PlanState *parent, /* 算子特有参数 */);
TupleTableSlot *op_next(void *state);  // 返回下一行，NULL 表示结束
void op_close(void *state);
```

各算子的输入参数：

| 算子 | 核心输入参数 |
|------|-------------|
| VectorScan | query_vector, top_k, distance_metric, index_oid |
| HnswScan | ef, m, query_vector, top_k |
| IvfScan | nprobe, query_vector, top_k |
| KvScan | key (点查), key_range (范围查) |
| TsScan | time_range, tag_filters, down_sample_window |
| DocumentScan | json_path_expr, fulltext_query |
| SpatialScan | geometry, spatial_op (contains/within/intersects), bbox |
| GraphScan | start_vertex, traversal_pattern, max_depth |
| DatalogScan | rule_set, edb_idb (扩展/内涵数据库) |
| YangScan | yang_path, xpath_filter |
| StreamScan | stream_oid, watermark, batch_size |

## 7. 后续实现计划

按优先级分阶段实施：

- **Phase 1**: 架构落地 + 关系模型完善（目录结构调整、Volcano 算子整合）
- **Phase 2**: 向量算子（VectorScan / HnswScan / IvfScan）
- **Phase 3**: KV 算子 + RESP 协议
- **Phase 4**: 时序算子 + InfluxDB Line Protocol
- **Phase 5**: 文档算子 + JSONPath
- **Phase 6**: 空间算子 + PostGIS 风格函数
- **Phase 7**: 图算子 + GQL
- **Phase 8**: Datalog 推理引擎
- **Phase 9**: Yang 协议适配
- **Phase 10**: 流处理引擎 + Kafka 协议

每个 Phase 包含：算子接口定义、算子实现、引擎直调集成、单元测试。