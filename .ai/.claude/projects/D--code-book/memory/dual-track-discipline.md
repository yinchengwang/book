---
name: dual-track-discipline
description: 工程轨 + learning 轨双轨运行时的可执行纪律（2026-07-11 /grill-me 共识）
metadata: 
  node_type: memory
  type: feedback
  originSessionId: a33304ce-6587-423b-a488-1e6ebd17c5c6
---

项目的演进主轴是**工程 + 学习双轨混合**（2026-07-11 /grill-me 锁定 Q1=C）。

双轨纪律条款（Q2 共识，必须作为元规则生效）：

1. **学习轨周期性交付**：每 5 个工作日内，必须至少有 1 个 learning 轨道上的可验证交付（拖卡 / 落新卡 / 测试模板提交 / notes 沉淀），避免工程轨挤掉学习轨。
2. **在制品冻结**：当在制品（active）OPSX 变更 ≥ 3 时，冻结开启新 OPSX 变更，先做归档清理。
3. **OPSX 变更必须有 §7 验证章节**：不能只以"编译通过"为收尾，必须含端到端可观测的验证证据。

具体落地链：
- 工程轨当前活动：[[rag-remote-index-backend]]（执行席推进中，本会话不干预）
- 该变更归档后立即启动的下一个 OPSX：**R5 回到 learning 闭环**（Q4=R5）
- R5 验收形态：**F1 严格勾卡**（Q5=F1）：每张学习卡必须附带 git commit 引用 + 产物；下限 8 张；优先系统编程类（pthread / IPC / epoll / mmap / 信号）——这些与后续 RAG server 化（`pg_ctl` 风格控制面、独立进程、HTTP 长连接、零拷贝）有**直接迁移价值**
- "免证据"豁免不开放：读完即勾算虚勾，禁止

**Why:** 选择 C 双轨的理由是承认纯工程堆码会让 44/44 学习卡永远停在 0 完成；选择 F1 严格的理由是用 commit 引用反向迫使学习内容真正跑起来，避免"读完了就勾"的伪闭环。如果双轨纪律被违反，C 方案会退化成纯工程扩张（A 方案），学习闭环不可达。

**How to apply:** 任何新 OPSX 提案发起前，检查 active OPSX 数量；每 5 个工作日审计一次 learning 交付；用户提到"学习卡"或"kanban"时强制要求 commit 引用。可参考 [[dual-track-architecture]] 的双轨隔离铁律。
