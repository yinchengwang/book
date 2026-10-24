# 文档导航

本目录按**内容分区**组织，各分区与代码的生命周期一致：

| 分区 | 内容 | 归属 |
|------|------|------|
| [`arch/`](arch/) | 本仓架构总览与各组件设计文档（索引、RAG、SQL 执行器、部署、API 规范等） | 与 `engine/` 同生命周期 |
| [`storage/`](storage/) | 存储引擎设计文档（向量 / 文档 / 图 / 空间 / 时序 / CCEH 索引） | 与 `engine/` 同生命周期 |
| [`analysis/`](analysis/) | 差距分析与研究报告（跨轨，不随代码变动） | 跨轨研究 |
| [`process/`](process/) | 工程流程与实施计划（历史计划、设计稿、规格） | 过程记录 |
| [`diagrams/`](diagrams/) | 跨轨 L0–L6 分层图（不随单一轨道下沉） | 跨轨视图 |

顶层散落的 `*.md` 为**跨轨报告与设计**，不属于任何单一分区：

- `ARCHITECTURE.md` —— 仓库总体架构
- `system_design.md` —— 系统设计（分区方案的来源）
- `path-mapping.md` —— 阶段二重排的路径引用核对表（迁移档案）
- `build-debt-fixes.md` —— 构建债修复记录（根因、判据、验证方法）
- `multimodal-*.md` —— 多模态数据库 / RAG 设计
- `memory-context-*.md` —— memctx 迁移记录
- `p6-production-ready-report.md`、`performance-scale-report.md` —— 上线与性能报告
- `review-task-1.1.md` —— 任务评审记录
- `project-metrics.json`、`postgresql.conf.example` —— 配置与度量数据

## 已切出为独立仓的内容

以下目录已迁出本仓，本目录不再包含：

- `learning/` → [yinchengwang/learning](https://github.com/yinchengwang/learning)
- `reference/`（含 `db-wiki` / `rag-wiki` / `agent-wiki`）→ [yinchengwang/reference](https://github.com/yinchengwang/reference)
