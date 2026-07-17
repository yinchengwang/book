# S8 — Tasks (Vector Delete Test gtest Discover) - COMPLETED

> **目标**：修复 vector_delete_test 的 `gtest_main` 链路，让 ctest 能发现并运行 delete 模块的 4 个核心测试。

## 1.1 调研

- [x] 1.1.1 已查根因：algo-prod 含 2 个独立 main() 的 .c (leetcode_2070/3280)，与 gtest_main 冲突
- [x] 1.1.2 验证 vector_delete_test.exe main 是 leetcode_2070 的 main()（disassemble 确认）

## 1.2 排除 + 迁移 problem/

- [x] 1.2.1 修改 `engineering/src/algo-prod/CMakeLists.txt`：排除 `^binary_search/problems/`
- [x] 1.2.2 `git mv engineering/src/algo-prod/binary_search/problems/2070_magic_list/leetcode_2070.c learning/algo-c/problems/leetcode_2070/leetcode_2070.c`
- [x] 1.2.3 `git mv engineering/src/algo-prod/binary_search/problems/3280_count_days/leetcode_3280.c learning/algo-c/problems/leetcode_3280/leetcode_3280.c`
- [x] 1.2.4 `rmdir` 3 个空目录（2070_magic_list、3280_count_days、problems）

## 1.3 gtest label 标注

- [x] 1.3.1 engineering/test/db/index/vector_index/delete/CMakeLists.txt 加 `PROPERTIES LABELS "vector_index"`

## 1.4 验证 V1-V4

- [x] 1.4.1 V1: algo-prod 不含 main()（`nm libalgo-prod.a | grep " T main"` 零结果）
- [x] 1.4.2 V2: vector_delete_test.exe --gtest_list_tests 输出 32 个 TEST_F (RebuildStrategy/GCVacuum/VectorSearchFilter/VectorDeleteBitmap)
- [x] 1.4.3 V3: ctest -L vector_index 找到 42 测试（10 reranker + 32 delete）+ 删除 30 个 + 失败 2 个（RebuildStrategyTest.HighDeletedRatio / CustomConfig —— 业务逻辑 bug，S8 范围外）
- [x] 1.4.4 V4: 学习层 cmake build-learning 成功（含 leetcode_2070/3280 在 algo-c 库中），ctest 158 个 leetcode 测试通过

## 1.5 提交 + 归档

- [ ] 1.5.1 `git add -A openspec/changes/vector-delete-test-gtest-discover/ learning/algo-c/ engineering/src/algo-prod/ engineering/test/db/index/vector_index/delete/`
- [ ] 1.5.2 `git commit -m "fix(vector-delete-test): 迁移 leetcode_2070/3280 到学习层，gtest 链路恢复"`
- [ ] 1.5.3 `git push origin project`
- [ ] 1.5.4 归档到 `openspec/changes/archive/2026-07-10-vector-delete-test-gtest-discover/`
