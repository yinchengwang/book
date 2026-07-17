# S6 — 设计文档（Test Link Fix After Learning Migration）

## 1. 目标

让工程层 `cmake --build engineering/build` 全部编译通过，让学习层 `ctest --test-dir build-learning` 能跑 leetcode 测试。两个轨道独立、互不依赖、各自承担自己的测试责任。

## 2. 总体策略：测试跟着被测库走

**核心原则**：测试 .cpp 文件应该和被测库的 .c/.h 文件在同一轨道。S1+S5 把 ds/、leetcode/、interview/ 库迁到了 learning/，所以对应测试也必须迁过去。

```
迁移前：
  engineering/test/self_made/list_test.cpp   → 引用 ds/list.h   (engineering 已不存在)
  engineering/test/leetcode/leetcode_*.cpp   → 引用 leetcode.h  (engineering 已不存在)
  engineering/test/CMakeLists.txt            → link_libraries(ds leetcode) (库已迁走)

迁移后：
  engineering/test/self_made_cpp/trie_test.cpp  (仅剩这一份)
  learning/code-solutions/c/test/c/leetcode_*.cpp
  engineering/test/CMakeLists.txt → 仅 self_made_cpp + Threads
```

## 3. 具体改动

### 3.1 删除 engineering/test 4 个过期测试

```bash
# ds/ 头文件已迁到 learning/include/ds/
# 工程层不再有 ds 库实现，测试目标不存在
git rm engineering/test/self_made/list_test.cpp
git rm engineering/test/self_made/queue_test.cpp
git rm engineering/test/self_made/tree_test.cpp
git rm engineering/test/self_made/str_test.cpp
```

### 3.2 删除 self_made/common_test.cpp

`engineering/test/self_made/common_test.cpp` 引用 `self_made/common.h`（在 engineering/include/self_made/common.h 中仍存在）。看 common.c 是否还在：

- 如果 `engineering/src/` 没有 `self_made` 目录 → common_test.cpp 也是悬空测试，删除
- 如果 common.c 存在 → 测试目标库在，测试可保留

需提交前查证。

### 3.3 迁移 15 个 leetcode 测试到学习层

```bash
mkdir -p learning/code-solutions/c/test/c
git mv engineering/test/leetcode/leetcode_c_100.cpp              learning/code-solutions/c/test/c/
git mv engineering/test/leetcode/leetcode_1_100_test.cpp          learning/code-solutions/c/test/c/
git mv engineering/test/leetcode/leetcode_1_100_cpp_test.cpp      learning/code-solutions/c/test/c/
# ... 共 15 个 .cpp
git mv engineering/test/leetcode/CMakeLists.txt                  learning/code-solutions/c/test/c/
```

调整 include 路径：
- `engineering/test/leetcode/CMakeLists.txt` 引用 `${CMAKE_SOURCE_DIR}/../include` → 改为 `${CMAKE_SOURCE_DIR}/include` (因为新路径下 `CMAKE_SOURCE_DIR` 是 `learning/`)

### 3.4 重写 engineering/test/CMakeLists.txt

```cmake
# 修改前
add_subdirectory(self_made)
add_subdirectory(self_made_cpp)
add_subdirectory(leetcode)
add_subdirectory(interview)
...
target_link_libraries(all_in_one_test
    PRIVATE
        project_includes
        GTest::gtest_main
        ds                  # 已迁到学习层
        leetcode            # 已迁到学习层
        self_made_cpp       # 仍在工程层
        interview           # 已迁到学习层
        Threads::Threads
)

# 修改后
add_subdirectory(self_made_cpp)  # trie 仍在工程层
...
# 简化 all_in_one_test：只剩 self_made_cpp + 工程层 db/apps test
# 或删除 all_in_one_test，由各模块自有 test target 承担
```

更优做法：**删除 all_in_one_test target**，由各模块独立 target 跑测试。

### 3.5 学习层添加 googletest 支持

修改 `learning/CMakeLists.txt`：

```cmake
# 添加在学习层 root
if(BUILD_TESTING AND NOT TARGET GTest::gtest)
    # 复用工程层的 googletest 子模块
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../third_part/googletest ${CMAKE_BINARY_DIR}/_googletest_build)
endif()

# 在 add_subdirectory(code-solutions) 后
add_subdirectory(code-solutions/c/test)
```

新建 `learning/code-solutions/c/test/c/CMakeLists.txt`：

```cmake
find_package(Threads REQUIRED)
include(GoogleTest)

file(GLOB LEETCODE_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(leetcode_tests ${LEETCODE_TESTS})
target_link_libraries(leetcode_tests
    PRIVATE
        leetcode-solutions
        Threads::Threads
        GTest::gtest_main
)

target_include_directories(leetcode_tests PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../../include
)

set_target_properties(leetcode_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

gtest_discover_tests(leetcode_tests)
```

## 4. 验证矩阵

| 验证项 | 命令 | 期望 |
|---|---|---|
| V1 | `cmake --build engineering/build` | exit 0 |
| V2 | `cmake -B build-learning -S learning` | exit 0 |
| V3 | `cmake --build build-learning` | exit 0 |
| V4 | `ctest --test-dir build-learning` | 至少 1 个 leetcode 测试通过 |
| V5 | `git grep -E 'algo-prod/\|db/\|rag/\|vector_index/\|faiss' learning/code-solutions/c/test/` | 零结果 |
| V6 | `git grep -l '"ds/\|"leetcode/' engineering/test/` | 零结果 |
| V7 | `eng ls engineering/test/` | 应只剩 `self_made_cpp/` + `db/` + `apps/` |

## 5. 回退方案

如果 S6 实施过程中遇到无法解决的学习层 ctest 问题：
- 保留学习层 leetcode-solutions 库，但不发现 tests
- 工程层只保留 self_made_cpp test，删除 all_in_one_test target

最小目标：**让工程层编译成功**。

## 6. 不做的（明确范围）

- ❌ 重写 ds/ 库到学习层（已在 learning/ds-c/，且有自己的 main.c 演示）
- ❌ 重写工程层测试架构（最小破坏原则）
- ❌ 加 GMock / GTest 高级特性
- ❌ CI 修改（后续 S7+ 处理）
