# AlgorithmPractice

C/C++ 算法与数据结构练习项目。CMake 3.20+、C11、C++17，无运行时依赖。

## 构建

### 工程轨道（默认）

```bash
cmake -B build/engineering -S engineering -DBUILD_TESTING=ON
cmake --build build/engineering --parallel 4
ctest --test-dir build/engineering --output-on-failure
ctest --test-dir build/engineering -R <name> --output-on-failure  # 单个测试
```

### 学习轨道

```bash
cmake -B build/learning -S learning -DBUILD_TESTING=ON
cmake --build build/learning --parallel 4
ctest --test-dir build/learning --output-on-failure
```

### 根入口（双轨同时）

```bash
cmake -B build/root -S . -DENGINEERING_BUILD=ON -DLEARNING_BUILD=ON
cmake --build build/root --parallel 4
```

`all_tests` 是 `add_custom_target` 包了一层 `ctest`，不是 CTest 原生概念。

## 编译与测试产物规范

- 编译产物统一进入 `build/<项目或轨道>/`（如 `build/engineering`、`build/learning`、`build/root`）
- 测试日志、覆盖率、临时数据库、运行日志等进入 `test-results/<项目或轨道>/`
- **测试二进制不再输出到源码目录**

## 测试结构

GoogleTest vendored 在 `third_part/googletest/`，根 CMakeLists 自动包含，无需系统安装。

测试可执行文件命名（不规则）：
- `self_made_test`、`self_made_cpp_test`、`leet_code_test`、`interview_test`
- `kmeans_gtest` 等位于 `engineering/test/algo/`、`engineering/test/vector_index/` 下
- `all_in_one_test` —— 合并 self_made + self_made_cpp + leetcode + interview 的单一二进制

## 新增模块

- 新模块用 `engineering/cmake/ProjectUtils.cmake`：`add_project_test()`、`add_project_library()`
- 老模块手写 CMakeLists，两种都接受

## 库

- `self_made` — C 数据结构（list、queue、tree、str、common）
- `self_made_cpp` — C++ 数据结构
- `algo` — 算法
- `leetcode` — LeetCode 题解（C 在 `c/`、C++ 在 `cpp/`）
- `interview` — 面试题
- `index` — 向量索引（hnsw、ivf、diskann、BM25 等）

## 代码风格

clang-format，LLVM，`ColumnLimit: 120`，`IndentWidth: 4`。

clang-format **不递归**，需配合 glob：
```bash
clang-format -i $(git ls-files '*.c' '*.cpp' '*.h' '*.hpp')
```

## 编译器标志

- MSVC：`/W4 /WX`
- GCC/Clang：`-Wall -Wextra -Wpedantic -Werror -Wno-sign-compare`
- Debug：`-O0 -g3`；Release：`-O2 -DNDEBUG`
