# S7 设计文档（Dual-Track Completeness）

## 1. 目标

让学习层的 3 个 ds_demo 头文件再次有 .c 实现（不再是空头），并修复工程层 vector_index 测试的链接错位。

## 2. 总体策略：独立实现 + 修复链接

```
学习层 3 个 ds_demo 文件结构：
  learning/include/ds/linked_list.h    ── 声明 ds_linked_list_demo()
  learning/ds-c/orig/linked_list/impl/ds_linked_list.c  ── 实现

  learning/include/ds/stack.h          ── 声明 ds_stack_demo()
  learning/ds-c/orig/stack/impl/ds_stack.c  ── 实现

  learning/include/ds/hash_table.h     ── 声明 ds_hash_table_demo()
  learning/ds-c/orig/hash/impl/ds_hash_table.c  ── 实现
```

工程层 vector_index test 库名修复：

```
旧：target_link_libraries(... algo gtest ...)
新：target_link_libraries(... algo-prod GTest::gtest gtest_main ...)
```

## 3. 具体改动

### 3.1 ds_linked_list.c（双向链表 demo）

设计要点：
- 使用独立的 `ds_list_node` 类型（不与 algo-prod `list_t`/`list_node_t` 冲突）
- 双链表：head + tail 双向指针，O(1) 头尾增删
- 提供函数：`ds_list_create`, `ds_list_push_back`, `ds_list_push_front`, `ds_list_pop_back`, `ds_list_pop_front`, `ds_list_size`, `ds_list_destroy`
- 主函数 `ds_linked_list_demo` 演示：创建 + 头尾增删 + 打印
- 可独立编译，演示完成后释放内存

### 3.2 ds_stack.c（栈 demo）

设计要点：
- 数组栈：`ds_stack_t` 含 buffer + top + capacity，自动扩容
- 函数：`ds_stack_create`, `ds_stack_push`, `ds_stack_pop`, `ds_stack_top`, `ds_stack_empty`, `ds_stack_size`, `ds_stack_destroy`
- 主函数 `ds_stack_demo` 演示：括号匹配（经典面试题）
- **不**包含链接栈 demo（避开单调栈等复杂概念，保持 demo 简洁）

### 3.3 ds_hash_table.c（哈希表 demo）

设计要点：
- 拉链法哈希表
- 使用 `uthash/uthash.h` 的 `UT_hash_handle` 集成（学习层已迁入 `learning/include/uthash/`）
- 函数：`ds_ht_create`, `ds_ht_insert`, `ds_ht_search`, `ds_ht_delete`, `ds_ht_destroy`
- 主函数 `ds_hash_table_demo` 演示：字符串词频统计（英文经典）
- 需要 `learning/include/` 在 include path

**include path 修复**：当前 `learning/ds-c/orig/CMakeLists.txt` 已设 `${CMAKE_CURRENT_SOURCE_DIR}/..` 到 include。检查是否能找到 uthash：

```
learning/ds-c/orig/.. = learning/ds-c/orig/.. 
learning/include/uthash/uthash.h ✓
learning/include/ds/... ✓
```

正确。

### 3.4 vector_index test 库名修复

`engineering/test/db/index/vector_index/CMakeLists.txt` line 22：

```diff
-    algo
+    algo-prod
```

`engineering/test/db/index/vector_index/delete/CMakeLists.txt` line 23：

```diff
-    algo
+    algo-prod
```

### 3.5 重新启用 vector_index test

`engineering/test/CMakeLists.txt` line 15：

```diff
-# vector_index 测试有 link -lalgo (旧 algoprod 库) 和 vector_reranker (不存在 target) 编译问题
-# add_subdirectory(db/index/vector_index)
+add_subdirectory(db/index/vector_index)
```

## 4. 验证矩阵

| V | 命令 | 期望 |
|---|---|---|
| V1 | `cmake --build engineering/build` | exit 0，包含 vector_reranker_test.exe + vector_delete_test.exe |
| V2 | `ctest --test-dir engineering/build --output-on-failure` | vector_index 测试至少 1 个通过 |
| V3 | `cmake --build build-learning` | exit 0 |
| V4 | `ctest --test-dir build-learning` | 158 + 0 (新 ds_demo 不一定参与 ctest) |
| V5 | `nm build-learning/ds-c/orig/libds.a \| grep ds_linked_list_demo` | 符号存在 |
| V6 | `git grep -E "algo-prod/|db/\|rag/\|vector_index/\|faiss" learning/ds-c/orig/linked_list/impl/ds_linked_list.c` | 零结果 |
| V7 | `engineering/build/test/db/index/vector_index/vector_reranker_test.exe --gtest_list_tests` | 至少 1 个测试 |
| V8 | `engineering/build/test/db/index/vector_index/delete/vector_delete_test.exe --gtest_list_tests` | 至少 1 个测试 |

## 5. 回退方案

如果 ds_demo 实现触发跨轨引用或 hash 触发 inclusion 失败：
- 改为精简到只演示一个操作（如 ds_stack_demo 仅 push+pop，不做括号匹配）
- 或在 `learning/include/uthash/` 缺路径时换成自定义 hash（不依赖 uthash）

如果 vector_index 测试 reranker 库头文件问题：
- 不重新启用整个目录，仅在父级 CMakeLists 注释
- 让 vector_reranker_test 和 vector_delete_test 都注释，仅解决 ds_demo 债

## 6. 不做

- ❌ 重写 vector_index reranker 内部实现（不动 prod code）
- ❌ 为 ds_demo 添加 GTest 单测（保持 main.c 演示风格）
- ❌ 重建 algo-prod 完全独立版本
