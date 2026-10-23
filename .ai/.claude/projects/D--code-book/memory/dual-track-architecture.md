---
name: dual-track-architecture
description: book 项目的 learning/engineering 双轨架构——双轨独立可构建、双轨隔离铁律、S6 测试责任划分
metadata: 
  node_type: memory
  type: project
  originSessionId: 4acf0542-81ed-46d6-848a-402b18d866a0
---

book 项目从 2026-07-08 起正式采用双轨架构：

**双轨结构**：
- `learning/` —— 学习轨道（ds/、leetcode/、interview/、algo-c/ 等教学代码）。独立可构建：`cmake -B build-learning -S learning`。
- `engineering/` —— 工程轨道（PG 风格存储、MiniVecDB、apps、cpp/trie 等生产代码）。默认构建。
- `reference/` —— 第三方参考源码（open-source/ 子模块）。

**双轨铁律（CLAUDE.md 强制）**：
1. engineering 的 CMakeLists 严禁 `add_subdirectory(learning)` 或 `target_link_libraries(... learning/...)`
2. learning 的 CMakeLists 严禁引用 engineering 任何路径
3. **`dual-track-guard.cmake`** 在两个轨道根 CMakeLists.txt 中 include，跨轨引用会编译失败

**S6 测试责任划分**（2026-07-10 引入）：
- 测试 .cpp 必须和被测库 .c/.h 在**同一轨道**
- 工程层 test 仅剩 self_made_cpp / db / apps / algo
- 学习层 test 在 `learning/code-solutions/c/test/c/`，启用 gtest
- algo-prod 排除学习性 `dict/` 与 `distributed/impl/`（S4 漏迁的子目录）

**Git 验证命令**（最后一道防线）：
```
git grep -E "algo-prod/|db/|rag/|vector_index/|faiss" learning/  # 学习层无工程层路径
git grep -l '"ds/\|"leetcode/' engineering/test/  # 工程层无学习层头引用
```

**Why**: S1 设计目标就是"learning/engineering 双轨独立可编译"，但实施阶段留有 TODO（S5 续做 + S6 修复链接）。这是双轨纪律的可量化指标，违反者编译失败。
**How to apply**: 任何双轨修改都需跑 `cmake --build engineering/build` 与 `cmake --build build-learning` 双验证；任何 cmake 链接错位先查 dual-track-guard.cmake。
