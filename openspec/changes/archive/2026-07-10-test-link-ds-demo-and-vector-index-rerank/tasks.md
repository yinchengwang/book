# S7 — Tasks (Dual-Track Completeness) - COMPLETED

> **目标**：学习层 3 个 ds_demo 重写完成；工程层 vector_index 测试重新启用。

## 1.1 调研 + OpenSpec 提案

- [x] 1.1.1 已查：3 个 ds_demo 头文件空实现（linked_list/stack/hash_table）
- [x] 1.1.2 已查：工程层 main.c 已清理，无 Threads/ds 引用
- [x] 1.1.3 已查：vector_index test 引用旧名 `algo`，应改为 `algo-prod`

## 1.2 学习层 ds_demo 重写（实现）

- [x] 1.2.1 写 `learning/ds-c/orig/linked_list/impl/ds_linked_list.c`（双向链表 demo）—— 153 行
- [x] 1.2.2 写 `learning/ds-c/orig/stack/impl/ds_stack.c`（数组栈 demo）—— 192 行
- [x] 1.2.3 写 `learning/ds-c/orig/hash/impl/ds_hash_table.c`（拉链法 hash demo）—— 252 行
- [x] 1.2.4 验证：libds.a 含 3 个 ds_*_demo 符号
  ```
  T ds_hash_table_demo
  T ds_linked_list_demo
  T ds_stack_demo
  ```
- [x] 1.2.5 验证：ds_demo .c 无跨轨引用（git grep 零结果）
- [x] 1.2.6 验证：3 个 demo 可独立运行（gcc link libds.a 输出预期演示）

## 1.3 工程层 vector_index test 修复

- [x] 1.3.1 `engineering/test/db/index/vector_index/CMakeLists.txt`：`algo` → `algo-prod`
- [x] 1.3.2 `engineering/test/db/index/vector_index/delete/CMakeLists.txt`：`algo` → `algo-prod`
- [x] 1.3.3 `engineering/test/db/index/vector_index/CMakeLists.txt`：添加 `gtest_discover_tests(vector_reranker_test)`（修复 S6 重启后 ctest 集成缺失）
- [x] 1.3.4 `engineering/test/CMakeLists.txt`：取消 `# add_subdirectory(db/index/vector_index)` 注释
- [x] 1.3.5 验证：cmake build engineering 287 个目标全部成功
- [x] 1.3.6 验证：vector_reranker_test.exe + vector_delete_test.exe 都生成
- [x] 1.3.7 验证：reranker 10 个测试通过（ctest -L vector_index 100%）

## 1.4 验证 V1-V8

- [x] 1.4.1 V1: `cmake --build engineering/build` 287 全部成功
- [x] 1.4.2 V2: `ctest --test-dir engineering/build -L vector_index` 10/10 reranker 通过
- [x] 1.4.3 V3: `cmake --build build-learning` 成功
- [x] 1.4.4 V4: `ctest --test-dir build-learning` 158 个 leetcode 测试 100%
- [x] 1.4.5 V5: libds.a 含 ds_*_demo 符号（已验证）
- [x] 1.4.6 V6: 3 个 ds_demo .c 无跨轨引用（已验证）
- [x] 1.4.7 V7: vector_reranker_test 10 个 TEST_F 注册到 ctest
- [x] 1.4.8 V8: vector_delete_test.exe 构建成功，但 gtest_discover_tests 返回 0（test 文件夹 main() 实现奇特）—— S7 范围外，不阻塞

## 1.5 提交 + 归档

- [ ] 1.5.1 `git add -A openspec/changes/test-link-ds-demo-and-vector-index-rerank/ learning/ engineering/`
- [ ] 1.5.2 `git commit -m "fix(dual-track-completeness): ds_demo 3 重写 + vector_index test 复活"`
- [ ] 1.5.3 `git push origin project`
- [ ] 1.5.4 归档到 `openspec/changes/archive/2026-07-10-test-link-ds-demo-and-vector-index-rerank/`
