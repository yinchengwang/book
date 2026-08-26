# 商业化路线图（Meta-Plan）

> **性质**：路线文档（Roadmap），不是可执行 plan。每个 sub-plan 是独立详细 plan（含 TDD steps、commit 粒度、测试覆盖），通过 subagent-driven-development 流程逐个执行。
>
> **创建日期**：2026-08-24
> **状态**：🟡 Draft，待用户 review

## 上下文

P3（混合检索 + Embedding + 跨模型 + RAG）已 ARCHIVED，11 commits。当前自实现多模态数据库在 SDK 层基本完整，但距商业化使用仍有显著差距。本路线图梳理未完成差距，按优先级拆解为 5 个 sub-plan，依序推进。

### 当前能力快照（2026-08-23）

| 维度 | 现状 |
|------|------|
| 多模态 SDK | vector / text / graph / timeseries / spatial / document 基础 API（P1） |
| 混合检索 | RRF + FTS5 + filter，P3 完成（单通道路由，已文档化） |
| Embedding | hash + OpenAI stub（P2 + P3） |
| 跨模型 join | text → vector（P3，flat 路径，≤100 候选） |
| RAG Pipeline | retrieve + BM25 rerank 占位（P3） |
| SDK 插入速度 | **158K vec/s**（Task #29，远超 P2 目标 30K vec/s） |
| SQL 引擎 | Phase 5（执行器 + 优化器 + EXPLAIN + 并行 + 触发器 + JIT）已基本就位 |
| 可靠性 | 单点 WAL，无副本 / HA / 备份 |

### 已知设计妥协（P3 whole-branch review）

| ID | 限制 | 后续 sub-plan |
|----|------|---------------|
| **CI-1** | hybrid_search 单通道路由 | P5（架构升级） |
| **CI-2** | RAG 硬编码 HASH，无 embedding 入口 | P4（P3 收尾） |
| **CI-3** | xquery id ≥64B 静默跳过（已 warn） | P4（升级为错误） |

---

## 路线图总览

5 个 sub-plan 优先级排序，每个 sub-plan 是独立可执行单元：

```
┌────────────────────────────────────────────────────────────────────┐
│                          商业化路线（5 sub-plans）                    │
└────────────────────────────────────────────────────────────────────┘
                                    │
        ┌───────────────────────────┼───────────────────────────┐
        ▼                           ▼                           ▼
    ┌───────┐                  ┌─────────┐                  ┌────────┐
    │ P4    │ ──────────────▶  │ P5      │ ──────────────▶  │ P6     │
    │SDK 完善│                  │性能规模化│                  │可靠性运维│
    └───────┘                  └─────────┘                  └────────┘
        │                           │                           │
        │                           ▼                           │
        │                       ┌─────────┐                     │
        │                       │ P7      │                     │
        │                       │SQL 兼容 │                     │
        │                       └─────────┘                     │
        │                           │                           │
        │                           ▼                           │
        │                       ┌─────────┐                     │
        │                       │ P8      │ ◀──────────────────┘
        │                       │多模态完整│
        │                       └─────────┘
        ▼
   归档 + 评估商业化成熟度
```

| Sub-plan | 名称 | Task 数估计 | 工期估计 | 依赖 | 优先级 |
|----------|------|------------|----------|------|--------|
| **P4** | SDK 收尾 + P3 遗留 | 4-6 | 2-3 天 | 无 | P1 |
| **P5** | 性能规模化（1M 基准） | 6-8 | 4-5 天 | P4 | P1 |
| **P6** | 可靠性运维（HA + 备份） | 10-14 | 7-10 天 | P4 | P0 |
| **P7** | SQL 兼容深度 | 8-12 | 5-7 天 | P4 | P1 |
| **P8** | 多模态完整 SDK | 6-10 | 4-6 天 | P4, P5 | P2 |

---

## Sub-Plan 详述

### P4：SDK 收尾 + P3 遗留清理（P1，2-3 天）

**目标**：补 P3 whole-branch review 识别的 CI-1/2/3 三个限制，清除延后 Minor findings。

**范围**：
- **Task 4.1**：RAG embedding 配置入口（CI-2）— `mmdb_rag_query_t` v2 + `mmdb_rag_set_embedding()` 接口，接入 OpenAI 真实调用
- **Task 4.2**：hybrid_search 双通道真正融合（CI-1）— VECTOR 集合支持 text_query / TEXT 集合支持 vector，RRF 真正融合
- **Task 4.3**：xquery id 静默截断升级为 `MMDB_ERR_INVALID`（CI-3）
- **Task 4.4**：清理 4 项延后 Minor — `= {}` 改 `= {0}`、末尾换行符（6 文件）、`MMDB_ERR_*` 宏替换字面量、`tokenize_lower max_tokens` 动态化
- **Task 4.5**：xquery 路径加 HNSW 支持（先于 P5 Task 5.1 完成，使 1M 规模 Recall 可验证）

**验收标准**：
- CI-1/2/3 全部关闭（架构 + ABI 安全 + 测试覆盖）
- 16 项 Minor findings 全部清零或文档化保留
- xquery HNSW 路径可走（前提：HNSW + filter 已在 P2 中实现）
- 既有 P1/P2/P3 测试全部回归 PASS

**依赖**：无

**风险**：低；Task 4.2 双通道融合需要重新设计 hybrid routing，影响现有 T1.3 benchmark。

---

### P5：性能规模化（1M 基准 + HNSW + SIMD）（P1，4-5 天）

**目标**：在 1M × 128 规模上达成搜索 ≥2000 qps + Recall@10 ≥0.95。

**范围**：
- **Task 5.1**：HNSW 懒创建（N ≥10K 时自动构建内存 HNSW）
- **Task 5.2**：HNSW 同步 add/delete + SQLite 同步
- **Task 5.3**：search 路由（HNSW 候选 → SQLite 回查 metadata/text）
- **Task 5.4**：selector 自动决策（vector_index_selector）
- **Task 5.5**：SIMD AVX2 距离（运行时 CPU 检测 + fallback 标量）
- **Task 5.6**：并发读优化（pthread_rwlock 读写锁分离）
- **Task 5.7**：1M × 128 基准测试 + Recall@10 验证
- **Task 5.8**：100K / 1M / 10M 阶梯基准 + 性能报告

**验收标准**：
- 1M×128 搜索 ≥2000 qps
- 1M×128 Recall@10 ≥0.95
- AVX2 fallback 标量路径可用
- 100K → 1M → 10M 性能曲线平滑
- 报告产出：`docs/performance-scale-report.md`

**依赖**：P4 Task 4.5（HNSW + filter）

**风险**：中。HNSW 内存膨胀（10M×128 约 5GB），需可配置阈值。AVX2 兼容性需 Windows/Linux/macOS 跨平台测试。

---

### P6：可靠性运维（HA + 备份 + 监控）（P0，7-10 天）

**目标**：从单点 WAL 升级到生产级可靠性（复制 / 高可用 / 备份恢复 / 监控）。

**范围**：
- **Task 6.1**：流复制框架（基于 WAL 的 primary → standby 异步复制）
- **Task 6.2**：同步复制选项（`synchronous_commit=on/off` 等价）
- **Task 6.3**：自动 failover 监控 + 切换
- **Task 6.4**：逻辑备份（`pg_dump` 等价：导出全库 DDL + 数据）
- **Task 6.5**：物理备份（`pg_basebackup` 等价：文件系统级快照 + WAL 归档）
- **Task 6.6**：PITR（Point-in-Time Recovery，基于 WAL 时间线）
- **Task 6.7**：监控指标（`pg_stat_*` 等价：连接、查询、缓存命中率、WAL 量）
- **Task 6.8**：健康检查端点 + HTTP 管理 API
- **Task 6.9**：备份恢复端到端测试（kill primary → failover → 验证 standby 接管）
- **Task 6.10**：混沌测试（随机 kill -9 + 网络分区）

**验收标准**：
- primary 故障 30 秒内 standby 自动接管
- 物理备份可恢复至任意时间点（PITR）
- 监控指标可 HTTP 查询
- 混沌测试 100 次随机故障 95% 自愈
- 报告产出：`docs/reliability-ops-report.md`

**依赖**：P4

**风险**：高。HA 切换的正确性需要严格测试（脑裂、网络分区）；PITR 实现复杂度高。

---

### P7：SQL 兼容深度（P1，5-7 天）

**目标**：补 SQL 引擎 DDL/DML/事务完整性，PostgreSQL 兼容度从基础提升到 TPC-C 可跑。

**范围**：
- **Task 7.1**：DDL 完整性（CREATE INDEX / ALTER TABLE / DROP CONSTRAINT）
- **Task 7.2**：DML 完整性（UPDATE / DELETE / RETURNING / UPSERT）
- **Task 7.3**：事务隔离级别（READ COMMITTED / REPEATABLE READ / SERIALIZABLE）
- **Task 7.4**：MVCC 改造（基于 xid 的多版本）
- **Task 7.5**：连接池 + 会话管理
- **Task 7.6**：SQL 函数扩展（内置函数 + 用户自定义函数）
- **Task 7.7**：prepared statement + 参数化查询
- **Task 7.8**：客户端驱动：psql 兼容 wire protocol 100% 覆盖
- **Task 7.9**：ODBC 驱动骨架（可选）
- **Task 7.10**：TPC-C 子集基准

**验收标准**：
- TPC-C 子集可运行（10 warehouse 规模 tpmC ≥100）
- psql 标准客户端零修改可连接 + 基础 SQL 全部可用
- 事务隔离级别符合 SQL 标准

**依赖**：P4

**风险**：中。MVCC 是大改造（涉及 Heap AM 重构），需要严格测试覆盖。

---

### P8：多模态完整 SDK（P2，4-6 天）

**目标**：graph / timeseries / spatial / document 四个模态从基础 API 升级到商用级 SDK 接口。

**范围**：
- **Task 8.1**：graph SDK 完善（Cypher 子集 + 路径查询 + 子图匹配）
- **Task 8.2**：timeseries SDK 完善（窗口函数 + 降采样 + 异常检测）
- **Task 8.3**：spatial SDK 完善（PostGIS 子集 + 空间索引）
- **Task 8.4**：document SDK 完善（JSONPath 完整 + 全文检索 + 索引）
- **Task 8.5**：跨模型 query 扩展（vector → text / graph → vector / 多模态 join）
- **Task 8.6**：多模态 RAG（混合 graph + text + vector）

**验收标准**：
- 4 个模态 SDK 接口完整
- 跨模型 query 覆盖 N×N 矩阵
- 多模态 RAG 端到端可用

**依赖**：P4, P5

**风险**：低；纯 SDK 层扩展。

---

## 执行顺序建议

```
P4 (2-3 天) → P5 (4-5 天) → P6 (7-10 天)  ← 主线
                 ↘            ↘
                  P7 (5-7 天)  P8 (4-6 天)  ← 可并行
```

- **第一阶段（必做）**：P4 → P5 → P6
- **第二阶段（并行）**：P7 和 P8 可独立 subagent 并行执行（不同模块）
- **总工期估计**：2-3 + 4-5 + 7-10 + 5-7 + 4-6 = **22-31 天**

---

## 商业化成熟度评估（每 sub-plan 完成后）

| Sub-plan | 完成后成熟度增量 | 关键指标 |
|----------|----------------|----------|
| P4 | +5%（SDK 完整度） | CI 全部关闭 + 0 Minor |
| P5 | +15%（性能规模化） | 1M Recall ≥0.95 + ≥2000 qps |
| P6 | +30%（可靠性核心） | HA 自愈率 95% + PITR |
| P7 | +20%（SQL 兼容） | TPC-C 可跑 + psql 兼容 |
| P8 | +10%（多模态） | 4 模态 SDK 完整 |
| **合计** | **80% → 商用可用** | — |

完成 P4-P8 后，距 100% 商用仍需：商业 license / 法务 / SLA / 客户案例，但技术上可支撑中小规模生产。

---

## 资源 & 流程约束

- **执行流程**：每个 sub-plan 走 `superpowers:subagent-driven-development`（fresh subagent per task + review + fix 循环）
- **OpenSpec 流程**：每个 sub-plan 在 `openspec/changes/` 下建独立目录（proposal.md / tasks.md / design.md / specs/），按 CLAUDE.md OpenSpec 铁律
- **Worktree**：建议每个 sub-plan 独立 worktree（避免 main 分支污染）
- **P1 dirty 工作树**：P4 开始前必须先清理 P1 dirty（`git status` 已显示）
- **测试回归**：每个 sub-plan 完成后回归全部既有测试

---

## 待用户决策

- [ ] Meta-plan 整体路线是否认可？
- [ ] 执行顺序（P4 → P5 → P6 → P7/P8）是否同意？
- [ ] 第一个 sub-plan 是 P4（SDK 收尾）还是 P6（可靠性 P0）？
- [ ] P5 与 P6 是否并行派发两个独立 worktree？
- [ ] 工期估计是否合理？

确认后进入第一个 sub-plan 的 writing-plans 阶段（详细 plan + TDD steps + commit 粒度）。