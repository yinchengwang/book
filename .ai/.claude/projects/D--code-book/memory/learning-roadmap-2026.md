---
name: learning-roadmap-2026
description: 2026 年学习路线图（扎实版 + 项目驱动）
metadata:
  type: project
---

# 学习路线图 2026

## 核心逻辑
DB 存储引擎项目是主轴，底层知识（并发/内存/性能/网络）是燃料。

## 阶段一：收尾 + 启动

- [ ] R11 C++ 最后 4 卡（move_semantics/templates/lambda/跨栈）
- [ ] R12 并发锁（条件变量 + 死锁）

## 阶段二：DB 链路 + 并发深化

- [ ] DB 全链路：Parser → Rewriter → Optimizer → Executor
- [ ] 并发锁（读写锁 + 线程池）
- [ ] 内存管理：ASAN 原理 + 报告解读

## 阶段三：性能 + 网络

- [ ] 火焰图解读 + perf 工具链
- [ ] 网络编程：socket + epoll + 缓冲区

## 阶段四：算法系统化（持续）

- [ ] 按类型集中突破：DP → 图论 → 树
- [ ] 每周 2–3 道中等题复盘

## 每周节奏
- 工作日晚上 1h：看原理 / 写 scaffold
- 周末上午 2h：做实验 / 跑代码 / 刷题

**Why:** 从 grill-me session 诊断出 7 个薄弱领域，按项目需求 + 扎实程度排序。
**How to apply:** 按阶段推进，每周复盘进度，更新 statuses.json。
