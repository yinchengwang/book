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

## 前端说明（已迁移）

knowledge_hub 原为 Taro 3.6 + React 18 + TS 工程，**已于 2026-10-22 完成原生化重写**：
Taro 源码（原 `engineering/apps/web/{knowledge_hub,games-mini-program,reading-radar}`）已删除，
对应的原生微信小程序现位于 `wechat-miniprogram/`：

| 应用 | 位置 |
|---|---|
| knowledge-hub（Reading Radar） | `wechat-miniprogram/knowledge-hub/` |
| games（游戏中心） | `wechat-miniprogram/games/` |
| programmer-workbench（DevBench） | `wechat-miniprogram/programmer-workbench/` |

三者均为零依赖、零构建的原生小程序（纯 JS + WXML + WXSS），无 pnpm / Taro / Vite 构建步骤。
下文关于 `frontend/` monorepo 的结构与命令描述对应的是更早的一版前端布局，现已不适用。

## 开发守则

### 先想清楚再动手

- 明确假设，不确定时询问而非猜测。
- 存在歧义时，列出多种解释，不默默选定单一方案。
- 如果任务有明显更简单的做法，直接指出优化思路。
- 发现代码矛盾、逻辑不一致时及时暂停，请求信息澄清。

### 简洁优先

- 用最少的代码解决问题，拒绝冗余实现。
- 不为一次性需求创建抽象层、复杂架构。
- 不盲目增加扩展性、可配置性，应对"未来可能用到"的场景。
- 若代码可大幅精简，主动重写优化。
- 校验标准：以资深工程师视角判断，代码若过于复杂，立即简化。

### 精准修改

- 仅修改与当前任务直接相关的代码内容。
- 不顺手优化相邻代码、注释、排版格式。
- 不重构原本可以正常运行的代码模块。
- 严格匹配项目现有代码风格，保留原有编码习惯。
- 因本次修改产生的无效导入、废弃变量，可直接删除。
- 发现项目中原有的死代码、冗余内容，仅做文字提醒，不擅自删除。

### 目标驱动执行

- 执行任务前，定义清晰、可落地的成功标准。
- 将"修复 Bug"转化为：编写用例复现问题，再调试至用例正常通过。
- 将"新增校验功能"转化为：针对异常输入编写测试用例，保证全部通过。
- 将"代码重构"转化为：完成重构后，确保原有所有测试用例正常运行。
- 多步骤复杂任务，先输出简短执行计划，同时标注每一步的验证方式。
