# S9 — Tasks (RebuildStrategy.decide 修复)

> **目标**：修复 RebuildStrategy.decide 业务逻辑，让 vector_index 42 个测试 100% 通过。

## 1.1 调研

- [x] 1.1.1 已查根因：原逻辑要求 ratio AND count 双触发，与测试期望不一致
- [x] 1.1.2 已确认：测试期望反映真实业务语义（任一阈值触发即 Rebuild）

## 1.2 修复实现

- [x] 1.2.1 修改 `engineering/src/db/index/vector_index/delete/rebuild_strategy.c`：新决策树
- [x] 1.2.2 验证：8 个 RebuildStrategyTest 全部通过
- [x] 1.2.3 验证：vector_index 42 测试 100% 通过
- [x] 1.2.4 验证：未破坏其他模块（guc + trie + vector_query 测试通过）

## 1.3 验证 V1-V3

- [x] 1.3.1 V1: `vector_delete_test.exe --gtest_filter=RebuildStrategyTest.*` 8/8 pass
- [x] 1.3.2 V2: `ctest -L vector_index` 42/42 pass
- [x] 1.3.3 V3: `vector_delete_test.exe` 全部 32 vector_delete 测试通过

## 1.4 提交 + 归档

- [ ] 1.4.1 `git add -A openspec/changes/rebuild-strategy-decide-fix/ engineering/src/db/index/vector_index/delete/`
- [ ] 1.4.2 `git commit -m "fix(vector-delete): 修复 RebuildStrategy.decide 决策树逻辑"`
- [ ] 1.4.3 `git push origin project`
- [ ] 1.4.4 归档到 `openspec/changes/archive/2026-07-10-rebuild-strategy-decide-fix/`
