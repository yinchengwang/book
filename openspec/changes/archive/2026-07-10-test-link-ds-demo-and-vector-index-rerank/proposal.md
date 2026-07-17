# S7 — Dual-Track Completeness: DS Demos + Vector-Index Test Revival

## What Changes

S5 实施时为遵守双轨纪律，删除了工程层 ds/ 实现（因为它们依赖 algo-prod 的 list_t/stack_t/map_t）。删除时遗留了 3 个学习层 ds/ 头文件**只声明 demo 函数但无 .c 实现**——S5 注释了它们是"后续 S5+ 阶段可重写为独立实现"。

S6 让工程层编译通过但**注释了** `db/index/vector_index` 子目录测试（它们链接旧名 `algo` 库导致失败）。

S7 同时解决两笔债：

**债务 1：3 个 ds_demo 空头**
- `learning/include/ds/linked_list.h` 声明 `void ds_linked_list_demo(void);` 但无 .c
- `learning/include/ds/stack.h` 声明 `void ds_stack_demo(void);` 但无 .c
- `learning/include/ds/hash_table.h` 声明 `void ds_hash_table_demo(void);` 但无 .c
- 学习层的工程化价值受损（ds-c/orig/ 库对外只暴露 trivial main 函数）

**债务 2：vector_index test 失活**
- `engineering/test/db/index/vector_index/CMakeLists.txt` 第 22 行 link `algo`（旧名）
- `engineering/test/db/index/vector_index/delete/CMakeLists.txt` 第 23 行 link `algo`（旧名）
- S4 已重命名 `algo` → `algo-prod`，但这 2 处未同步
- S6 因为不想扩散到历史债，注释掉整个 `add_subdirectory(db/index/vector_index)`
- 失活后，vector_index 模块的 2 套测试（reranker + delete）有 0 覆盖

**变更内容**：

1. **重写 3 个 ds_demo 实现**（学习层独立实现，不依赖 algo-prod）：
   - `learning/ds-c/orig/linked_list/impl/ds_linked_list.c` —— 双向链表 demo
   - `learning/ds-c/orig/stack/impl/ds_stack.c` —— 数组栈 + 链式栈 demo
   - `learning/ds-c/orig/hash/impl/ds_hash_table.c` —— 拉链法哈希表 demo
2. **3 个新 .c 加入 ds 库构建**（无需改 CMakeLists.txt，GLOB `*/impl/*.c` 自动收集）
3. **修复 vector_index test 库名引用**：从 `algo` 改为 `algo-prod`
4. **重新启用 `engineering/test/CMakeLists.txt` 中的 `add_subdirectory(db/index/vector_index)`**

## Why

**符合 CLAUDE.md 双轨铁律**：学习层独立可编译、可验证；工程层 100% 测试覆盖。

**α 价值（工程作品集）**：
- vector_index 是 MiniVecDB 的核心，**0 测试覆盖** 等于工程层无保障
- 修复后 vector_index 的 2 个测试 binary 能编译能跑，提升覆盖率指标

**β 价值（学习日志）**：
- ds_demo 三件套重写后，学习层从"能编译的代码"升级为"能展示的数据结构课"
- 三个 .c 实现既是教学资料也是库公共 API（S5 时被销毁的部分重建）

**前置依赖**：
- S6 已让学习层编译通过
- ds-c/orig/CMakeLists.txt 已用 GLOB `*/impl/*.c` 自动收集
- learning/include/ds/*.{h} 三个头文件已声明 demo 函数，等待实现

## Scope

**包含**：
- 创建 3 个 .c 文件（双向链表 demo、栈 demo、哈希表 demo）
- 修复 2 个 vector_index test 的库名引用
- 解除 `engineering/test/CMakeLists.txt` 中 `db/index/vector_index` 的注释
- V1-V4 验证

**不包含**：
- ❌ 重建 list_t/stack_t/map_t 等通用类型（在 algo-prod 之外的独立类型）
- ❌ 添加 GTest 单测给 3 个 demo（保持 main.c 风格）
- ❌ 重写 vector_index 测试逻辑（仅修链接错位）
- ❌ 修 vector_index reranker 类型 compatibility 问题（若有）

## Risk

| 风险 | 概率 | 缓解 |
|---|---|---|
| ds_demo .c 实现含跨轨引用 | 中 | V4 grep 验证零跨轨 |
| hash_table demo 需要 uthash.h | 中 | `learning/include/uthash/` 头文件已迁入；CMakeLists 加 include path |
| vector_index reranker 测试 reranker.h 依赖问题 | 中 | 跑测试 log 找错 |
| existing impl/single_list.c 等已被 ds 库链入，需要与新 ds_linked_list.c 共存 | 低 | 头文件 guard 不同；C 编译期隔离 |
| single_list.c 既含 .data 字段又含 .next，ds_linked_list.c 用不同 struct | 低 | 设计独立 ds_demo_node struct，不冲突 |

## 不做（明确范围外）

- ❌ 重建 complete_algo (替代 algo-prod) 库
- ❌ 解除 engineering/src/redis 注释（与 S7 无关）
- ❌ 重新启用 learning/code-solutions/distributed/（phase9 RaftRouter 类型待续做）
- ❌ 102 个旧 OpenSpec specs 清理（治理项）
