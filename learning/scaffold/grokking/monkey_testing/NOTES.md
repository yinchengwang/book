# 混沌测试 学习笔记

## 核心概念

- **混沌工程 (Chaos Engineering)**: 在生产环境中注入故障的实践方法，验证系统韧性
- **故障注入 (Fault Injection)**: 主动引入错误以测试系统容错能力
- **熔断器 (Circuit Breaker)**: 保护系统不被级联故障拖垮的防护模式
- **重试退避 (Retry with Backoff)**: 指数退避策略避免重试风暴

## 关键要点

| 概念 | 说明 | 应用 |
|------|------|------|
| 故障注入 | 模拟网络延迟/服务崩溃/资源耗尽 | Netflix Chaos Monkey |
| 熔断器 | 关闭→打开→半开三态 | Hystrix, Sentinel |
| 重试策略 | 固定/指数/随机退避 | 分布式调用 |
| 恢复验证 | 自动恢复时间 < SLA 容忍时间 | 自愈系统 |

## 工程对照

混沌工程在分布式系统中具有重要地位。`engineering/src/db/txn/` 中的分布式事务处理同样需要面对网络分区和节点故障的挑战。PostgreSQL 风格的 WAL (Write-Ahead Log) 机制（对应 `engineering/src/db/core/wal.c`）本身就是一种"故障恢复"设计——在系统崩溃后通过 Redo Log 恢复到一致状态。Buffer Pool 的 Clock-Sweep 置换算法（`engineering/src/db/buf/bufmgr.c`）也需要在"淘汰脏页→刷盘→一致性问题"之间做权衡，这和混沌工程中的"注入故障→观察行为→验证韧性"的思维范式高度一致。数据库系统的 crash recovery 机制本质上是对系统韧性的深度工程实践。

## 面试要点

1. 区分"混沌工程"和"传统测试"：传统测试验证已知行为，混沌工程发现未知弱点
2. 熔断器 vs 限流器 vs 重试器三个模式的组合使用
3. 恢复时间目标 (RTO) 和恢复点目标 (RPO) 的概念
