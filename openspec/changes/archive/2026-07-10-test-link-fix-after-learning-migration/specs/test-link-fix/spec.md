# S6 Spec —— 双轨测试架构

## 1. 核心规则（"测试跟着被测库走"）

测试 `.cpp` 文件必须与被测库的 `.c/.h` 文件位于**同一轨道**。

- **学习层库**（ds、leetcode-solutions、interview-solutions、algo-c）→ 测试在 `learning/code-solutions/.../test/`
- **工程层库**（algo-prod、db/*、apps/*、cpp/trie）→ 测试在 `engineering/test/...`

## 2. 测试目录布局

```
engineering/test/
├── self_made_cpp/      # trie_test.cpp → C++ 工程代码
├── db/                 # 数据库内核测试
├── apps/               # 独立应用测试
└── CMakeLists.txt      # 仅声明上述三个 add_subdirectory

learning/code-solutions/c/test/
├── c/
│   ├── CMakeLists.txt  # link leetcode-solutions
│   └── leetcode_*.cpp  # 15 个测试
└── CMakeLists.txt      # 调度器
```

## 3. 工程层 CMakeLists 契约

`engineering/test/CMakeLists.txt` 必须：

- **不得** link 任何 `ds`、`leetcode`、`interview` 库（已迁到学习层）
- **不得** `add_subdirectory(learning)`
- 应仅启用 `self_made_cpp`、`db`、`apps` 子目录测试

## 4. 学习层 CTest 启用

`learning/CMakeLists.txt` 应在 `LEARNING_BUILD=ON` 时：

- 添加 `add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../third_part/googletest ${CMAKE_BINARY_DIR}/_googletest_build)`（如果 LEARNING_BUILD=ON）
- 添加 `add_subdirectory(code-solutions/c/test)`
- 设置 `BUILD_TESTING=ON` 让 `ctest` 可发现 leetcode 测试

## 5. 不做（明确范围外）

- ❌ 引入 GMock / 高级 GTest 特性
- ❌ ds/ 在学习层加 gtest 测试（学习层已有 `main.c` 演示，保证编译即可）
- ❌ 重写工程层测试架构（all_in_one_test target 改为各独立 target 即可）
