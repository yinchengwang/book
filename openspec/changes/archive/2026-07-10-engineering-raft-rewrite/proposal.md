# S26 — Engineering Layer Raft Module Rewrite

## What Changes

`engineering/src/db/core/raft.c` 实现 Raft 共识模块。当前已存在（S18 已有 memory phase9 完成 summary 描述为"完整"）。S26 验证：

1. **测试**：跑工程层 ctest 看是否有 raft 相关测试通过
2. **文档**：在 `engineering/src/db/core/raft.c` 增加注释说明 raft 模块对外 API
3. **集成**：把 raft 模块接入工程层 ctest（如果之前未接入）

## Why

**α 价值**：
- 工程层 db/core/raft.c 是真正的 Raft 实现（不是学习层副本）
- 如果它真的可工作，应该在测试套中

## Scope

**包含**：
- 1 个 raft_smoke_test.cpp（新建 ctest）
- `engineering/test/db/core/CMakeLists.txt` 注册
- 验证 V1-V3

**不包含**：
- ❌ 重写 raft.c 业务逻辑（除非发现严重 bug）
- ❌ 改工程层任何其他模块
- ❌ 学习层副本（phase10+ 重做）
