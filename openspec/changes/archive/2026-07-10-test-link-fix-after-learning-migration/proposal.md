# S6 — Test Link Fix After Learning Migration

## What Changes

工程层 `cmake --build build --target all_in_one_test` **编译失败** 17 个 .cpp 文件：

| 来源 | 数量 | 错误 |
|---|---|---|
| `engineering/test/self_made/{list_test,queue_test,tree_test,str_test}.cpp` | 4 | `ds/list.h` 等不存在 |
| `engineering/test/leetcode/*.cpp` | 15 | `leetcode/leetcode.h`、`leetcode/leetcode_cpp.h` 不存在 |

**根因**：S1 + S1' + S5 实施过程中，`ds/`、`leetcode/`、`interview.h`、`common.h` 等头文件已迁移到 `learning/include/`，但**对应测试文件还留在 `engineering/test/`**，引用旧 include 路径。`engineering/test/CMakeLists.txt` 同时引用了 `ds` 和 `leetcode` 库（也已迁移）。

S5 虽修复了 `learning/code-solutions/c/{c,interview}/CMakeLists.txt` 让学习层可独立构建，但**遗留了 engineering 层测试的 include 路径断链**。

**变更内容**：

1. **删除** `engineering/test/self_made/{list_test,queue_test,tree_test,str_test}.cpp` —— 它们测的是 ds 库，ds 库已迁到学习层，且没有工程层 ds 实现可测
2. **迁移** `engineering/test/leetcode/*.cpp` → `learning/code-solutions/c/test/c/` 作为 c++ 测试配套 —— 它们测的是 leetcode-solutions 库，库在 learning 层
3. **重写** `engineering/test/CMakeLists.txt`：
   - 去掉 `add_subdirectory(self_made)` 整条引用（list/queue/tree/str 头已迁走）
   - 去掉 `add_subdirectory(leetcode)`（库已迁走）
   - 去掉 `add_subdirectory(interview)`（已迁走）
   - 保留 `add_subdirectory(self_made_cpp)`（trie 仍在 engineering 层）
   - 修正 `all_in_one_test` target 链接：去掉 `ds`、`leetcode`，只链 `self_made_cpp` + Threads
4. **新增** 学习层 gtest 支持：`learning/code-solutions/c/test/CMakeLists.txt` 用 `find_package(GTest REQUIRED)` 链接学习层库并 discover tests
5. **验证**：
   - V1: `cmake --build engineering/build` 工程层全部编译成功
   - V2: `cmake -B build-learning -S learning` 学习层配置 + 编译成功
   - V3: `cmake --build build-learning && ctest --test-dir build-learning` 测试可跑
   - V4: 双轨隔离（learning ↔ engineering）grep 验证零跨轨引用

## Why

**符合 CLAUDE.md 双轨铁律**：CLAUDE.md 明确规定"engineering 的 CMakeLists 严禁 add_subdirectory(learning) 或 target_link_libraries(... learning/...)"。S5 已把 ds/、leetcode/、interview/ 库迁到学习层，工程层测试应在学习层。

**α 价值（工程作品集）**：
- 工程层 all_in_one_test 编译失败 = α 价值打折，无法向外部展示 "100% 编译"
- 修复后工程层只剩独立的 self_made_cpp（C++ 工程代码：trie）+ 工程 db/apps 测试，更纯粹

**β 价值（学习日志）**：
- 测试 ds/、leetcode/ 题解 = 学习闭环的重要组成
- 迁移到学习层后，学习层自带 ctest 是 β 可量化的里程碑

**前置依赖**：
- S5 学习层可独立构建已完成（4 个库生成）
- learning/include/ 已迁入 ds/、leetcode/、common.h、uthash/、interview.h

**关键陷阱**：
- ❌ 不能简单改 include 路径（`ds/list.h` → `<learning/include/ds/list.h>`）：那让工程层依赖学习层，违反双轨纪律
- ❌ 不能保留工程层 ds 测试 + 移除工程层 ds 实现：测试目标库不存在，永远 link 失败
- ❌ 不能直接删除所有 test：破坏已有测试覆盖
- ✅ 正确做法：测试跟着它**实际测试的库**走，ds/ 库在 learning 层，测试就去 learning 层

## Scope

**包含**：
- 删除 4 个 ds 测试 .cpp：`list_test.cpp`、`queue_test.cpp`、`tree_test.cpp`、`str_test.cpp`
- 迁移 15 个 leetcode 测试 .cpp 到 `learning/code-solutions/c/test/c/`
- 迁移 `engineering/test/leetcode/CMakeLists.txt` → `learning/code-solutions/c/test/c/CMakeLists.txt`
- 重写 `engineering/test/CMakeLists.txt`：移除 4 个空目录，正确链接 self_made_cpp_test
- 学习层启用 googletest：`learning/CMakeLists.txt` 加 `find_package(GTest REQUIRED)`（如果 LEARNING_BUILD=ON）
- V1-V4 验证

**不包含**：
- 增加新的 ds/ 测试覆盖（学习层原有 main.c 演示已足够）
- 修改 leetcode 测试内容（仅迁移位置）
- 引入 gmock 等高级 googletest 特性

## Risk

| 风险 | 概率 | 缓解 |
|---|---|---|
| 学习层启用 GTest 后配置变慢 | 低 | GTest 已内置在 third_part/googletest/，工程层已配置 |
| 迁移 .cpp 后学习层 ctest 失败 | 中 | V3 验证；按失败逐个修复 |
| common_test.cpp 引用 `self_made/common.h` | 低 | 该头在 engineering/include/self_made/ 仍在；common.c 也仍在；测试可编译——但它测的是 S1 前的 self_made 残留代码，需要单独处理（提交前删除以保持清洁）|
| 旧测试删除后覆盖度下降 | 低 | 学习层 ctest 接手覆盖度 |
| trie_test.cpp 编译失败 | 低 | 工程层仍有 trie.cpp + trie.h（self_made_cpp 库），不需动 |
| gtest_discover_tests 在 learning 层不可用 | 中 | 沿用工程层同样的模式，`include(GoogleTest)` 必须能找到 Module |

## 不做（明确范围外）

- ❌ 重写 self_made 为可用工程库（交付给学习层重做）
- ❌ 删除 engineering/include/self_made/（里面 common.h/data_type.h 工程代码可能仍有用）
- ❌ common_test.cpp 的修复（提交前删除，避免悬空测试引用 `self_made/common.h`）
- ❌ 引入 testing 框架到整个 learning/（只针对 code-solutions/）
