# Phase 7-10 实现计划

> 日期：2026-07-18
> 关联设计：docs/superpowers/specs/2026-07-17-db-architecture-design.md

**目标：** 完成 Phase 7（图算子）、Phase 8（Datalog 推理引擎）、Phase 9（Yang 协议适配）、Phase 10（流处理引擎）的 Volcano 算子实现

**架构原则：**
- 每个算子遵循 `op_init / op_next / op_close` 三阶段 Volcano 迭代器协议
- 算子通过 internal state 管理引擎句柄/结果/索引
- 使用 `state->ps.state` 存放内部状态
- 算子直调底层引擎，不走 Access Method 层包装

---

## Phase 7 — 图扫描算子

**现状：** `graph_engine.h` 完整，`graph_scan.h` 骨架已有，`gqlExec.c` 已有 GQL 执行但未封装为 Volcano 算子。
**任务：** 实现 `graph_scan.c` Volcano 算子，包装 graph_engine + traverse 功能。

## Phase 8 — Datalog 扫描算子

**现状：** 仅有 `datalog_scan.h` 骨架头文件，无实现。
**任务：** 实现完整 Datalog 半朴素求值引擎 + Volcano 算子。

## Phase 9 — Yang 扫描算子

**现状：** `yang_engine.h` 有声明但无实现，`yang_scan.h` 骨架已有。
**任务：** 实现 Yang 引擎 + 扫描算子。

## Phase 10 — 流处理算子

**现状：** 4 个头文件（scan/window/join/agg）骨架，无实现。
**任务：** 实现完整流处理引擎 + 4 个算子。
