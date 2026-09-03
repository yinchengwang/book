# 多模态数据库架构总览

## 顶层架构

```
┌─────────────────────────────────────────────────────────┐
│              Application / SQL / Cypher / GQL             │
└───────────────────────┬─────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────┐
│         Parser → Planner → Optimizer → Executor          │
│   (Cypher/SQL/JSONPath, 4-stage optimizer, MVCC/SIMD)   │
└───────────────────────┬─────────────────────────────────┘
                        │
┌──────────┬──────────┬──────┬───────┬─────────┬──────────┐
│ Vector   │ KV       │ TS   │ Doc   │ Spatial │ Graph    │ 8 模态
│ faiss    │ Hash+TXN │ Gor  │ FTS   │ R-Tree  │ CSR+COW  │
│ HNSW     │ CAS      │ illu │ Highl │ ST_*    │ 17 algs  │
│ +vtable  │ +16MB    │ +乱序│ +CJK  │ +Geo    │ +RRF     │
└──────────┴─────┬────┴──────┴───────┴────┬────┴──────────┘
                │                       │
        ┌───────▼────────┐    ┌─────────▼──────────┐
        │ Buffer Pool    │    │ BLOB + TOAST       │
        │ + FSM bitmap   │    │ + 4MB chunk        │
        └────────────────┘    └────────────────────┘
                │
┌───────────────▼────────────────────────────────────────┐
│   WAL（共享刷盘策略 fsync/no-fsync/batch）           │
│   + MVCC（xmin/xmax 当前骨架）                       │
│   + Memory Context（Executor per-query 隔离）         │
└───────────────────────────────────────────────────────┘
                │
┌───────────────▼────────────────────────────────────────┐
│         Disk File（local FS + 自研 blob chunks）       │
└───────────────────────────────────────────────────────┘
```

## 19 OpenSpec 变更总览

| Phase | 变更 | 内容 |
|-------|------|------|
| 0（地基） | C0-1 | 统一并发原语 mmdb_rwlock（5 模态） |
| 0 | C0-2 | 共享 WAL 统一覆盖 + 刷盘策略 + 恢复入口 |
| 0 | C0-3 | 统一错误码 DBERR_* + MemoryContext + 序列化契约 |
| 1（炸弹） | C1-1 | 关系 TID 管道修复 |
| 1 | C1-2 | faiss_hnsw 并发安全 + IP 度量修正 |
| 1 | C1-3 | KV 锁启用 + 存储健壮性 |
| 2（功能） | C2-1 | MVCC 集成骨架（parser 触发留待） |
| 2 | C2-2 | 优化器核心（摘假 + 选择率 + DP 骨架） |
| 2 | C2-3 | Graph/Spatial 并发恢复 |
| 2 | C2-4 | TS 增量压缩骨架 |
| 2 | C2-5 | Tree XML + datastore |
| 2 | C2-6 | Document 中文分词 + 词干化 |
| 3（外围） | C3-1 | Blob 存储引擎（分块 + 内容寻址 + Range） |
| 3 | C3-2 | 全文搜索增强（骨架） |
| 3 | C3-3 | 可观测日志引擎核心 |
| 3 | C3-4 | 多模态 AI 原生（NamedVector + 跨模态） |
| 3 | C3-5 | 各模态功能面补齐（聚合） |
| 4（收尾） | C4-1 | 性能专项（SIMD 补全落地） |
| 4 | C4-2 | 可维护性专项（ASAN CI + HNSW 收敛 + E2E） |
| 新 | C5 | vector 多模态核心生产化（COW segment + RRF） |
| 新 | C6 | observability 完整路径（标签索引 + LogQL） |
| 新 | C7 | 内核与索引核心（Hash + TOAST + FSM + kv_txn + ltree） |
| 新 | C8 | 质量与文档（DESIGN + UAF 骨架 + benchmark） |

## 当前状态

- **~120+ commits** 落 main
- **23 个 OpenSpec 变更**全部归档
- **核心生产化**：Vector COW + 多模态检索 + 标签索引 + WAL/事务/Hash/TOAST 全部骨架级落地
