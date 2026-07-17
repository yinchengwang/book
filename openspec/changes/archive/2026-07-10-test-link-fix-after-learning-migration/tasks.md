# S6 — Tasks (Test Link Fix After Learning Migration)

> **目标**：工程层 `cmake --build engineering/build` 全部通过；学习层继承原工程层 leetcode/self_made 测试，做到双轨独立可测。

## 1.1 验证根因 + 调研

- [x] 1.1.1 已查：17 个 .cpp 编译失败（4 ds 测试 + 15 leetcode 测试）+ algo-prod dict/distributed 学习性残留
- [x] 1.1.2 已查：`engineering/test/CMakeLists.txt` link `ds` `leetcode` `interview` 全部失效
- [x] 1.1.3 已查：engineering/src/self_made/ 不存在 → common_test.cpp 也是悬空测试，已删除

## 1.2 迁移目标

- [x] 1.2.1 创建 `learning/code-solutions/c/test/c/` 目录
- [x] 1.2.2 创建 `learning/code-solutions/c/test/CMakeLists.txt` 调度器
- [x] 1.2.3 创建 `learning/code-solutions/c/test/c/CMakeLists.txt` 测试 target

## 1.3 迁移 15 个 leetcode .cpp

- [x] 1.3.1 `git mv engineering/test/leetcode/CMakeLists.txt learning/code-solutions/c/test/c/CMakeLists.txt`
- [x] 1.3.2 迁移 15 个 .cpp 到 `learning/code-solutions/c/test/c/`
- [x] 1.3.3 修正 include 路径：改 `${CMAKE_CURRENT_SOURCE_DIR}/../../../include`，链接 `leetcode-solutions` 库
- [x] 1.3.4 清理：删 `learning/code-solutions/interview/huawei_od/huawei_od.cpp`（不应被当作生产代码编进 interview-solutions）

## 1.4 工程层清理

- [x] 1.4.1 删除 `engineering/test/self_made/{list,queue,tree,str}_test.cpp`
- [x] 1.4.2 删除 `engineering/test/self_made/common_test.cpp`（引用 `self_made/common.h` 但 src 没了）
- [x] 1.4.3 删除空 `engineering/test/self_made/` 目录
- [x] 1.4.4 删除空 `engineering/test/leetcode/` 目录
- [x] 1.4.5 删除空 `engineering/test/interview/` 目录
- [x] 1.4.6 重写 `engineering/test/CMakeLists.txt`：移除 self_made/、leetcode/、interview/ 引用

## 1.5 工程层 algo-prod 排除学习性残留

- [x] 1.5.1 `engineering/src/algo-prod/CMakeLists.txt` 增加 `list(FILTER ... EXCLUDE REGEX "^dict/")` 与 `"^distributed/impl/"`
- [x] 1.5.2 删除 `target_compile_definitions ... DICT_DEFAULT_FILE_PATH`（dict 已排除）

## 1.6 工程层 vector_index test 隔离

- [x] 1.6.1 注释 `engineering/test/CMakeLists.txt` 中 `add_subdirectory(db/index/vector_index)`（引用旧 algoprod 库名 + 不存在 target，S6 范围外）

## 1.7 学习层启用 GTest

- [x] 1.7.1 `learning/CMakeLists.txt` 添加 `enable_testing()` + 复用 `third_part/googletest` 子模块
- [x] 1.7.2 `learning/CMakeLists.txt` 添加 `add_subdirectory(code-solutions/c/test)`
- [x] 1.7.3 新建 `learning/code-solutions/c/test/c/CMakeLists.txt`：link leetcode-solutions 与 GTest::gtest_main

## 1.8 验证 V1-V7

- [x] 1.8.1 V1: `cmake --build engineering/build` 全部编译成功
- [x] 1.8.2 V2: `cmake -B build-learning -S learning -DBUILD_TESTING=ON` 配置成功
- [x] 1.8.3 V3: `cmake --build build-learning` 学习层编译成功（81 个目标）
- [x] 1.8.4 V4: `ctest --test-dir build-learning` 158 个 leetcode 测试全部通过 (100%)
- [x] 1.8.5 V5: `git grep -E 'algo-prod/|db/|rag/|vector_index/|faiss' learning/code-solutions/` 仅含 distributed/ CMakeLists.txt（已注释，S6 范围外）
- [x] 1.8.6 V6: `git grep -l '"ds/\|"leetcode/' engineering/test/` 零结果
- [x] 1.8.7 V7: engineering/test/ 目录含 self_made_cpp / db / apps / algo，全部独立 build

## 1.9 提交 + 归档

- [ ] 1.9.1 `git add -A openspec/changes/test-link-fix-after-learning-migration/ engineering/ learning/`
- [ ] 1.9.2 `git commit -m "fix(test-link): 修复 S5 迁移后工程层 test 链接错误"`
- [ ] 1.9.3 `git push origin project`
- [ ] 1.9.4 归档到 `openspec/changes/archive/2026-07-10-test-link-fix-after-learning-migration/`
