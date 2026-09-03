# clang-tidy 练习脚手架

本目录提供 clang-tidy 静态分析的练习环境，包含故意设置的问题代码，用于学习和测试 clang-tidy 的检测能力。

## 目录结构

```
clang_tidy/
├── main.cpp      # 练习代码（包含多种可检测问题）
├── .clang-tidy   # clang-tidy 配置文件
├── README.md     # 本文档
└── NOTES.md      # 工程对照笔记
```

## 常用规则速查

| 规则 | 说明 |
|------|------|
| `readability-*` | 命名规范、格式规范、代码可读性 |
| `performance-*` | 性能警告：不必要的拷贝、移动语义、循环优化 |
| `modernize-*` | 现代 C++ 写法：auto、范围 for、emplace、智能指针 |
| `cppcoreguidelines-*` | C++ 核心规范：内存管理、类型安全、边界检查 |
| `bugprone-*` | 常见 bug：除零、空指针解引用、整数溢出 |
| `clang-analyzer-*` | Clang 静态分析器：空指针、未定义行为 |
| `cert-*` | CERT C++ 安全编码规范 |
| `misc-*` | 杂项：代码冗余、注释质量 |

### 常见问题与对应规则

| 问题 | 规则 | 说明 |
|------|------|------|
| 未使用变量 | `readability-identifier-naming` | 变量命名后缀 `_unused` 可触发 |
| 缺少 const | `performance-*` | 按值传递大对象建议 const 引用 |
| std::move 局部变量 | `performance-move-const-arg` | 移动局部变量是无效的 |
| 原始 new/delete | `cppcoreguidelines-owning-memory` | 推荐使用智能指针 |
| goto 语句 | `readability-braces-around-statements` | 遗留代码风格问题 |
| memcpy 非 POD | `cppcoreguidelines-pro-type-reinterpret-cast` | 非平凡类型不应 memcpy |

## 运行方式

### 1. 直接分析单文件

```bash
# 分析 main.cpp
clang-tidy main.cpp -- -std=c++17

# 指定配置文件
clang-tidy main.cpp --config-file=.clang-tidy -- -std=c++17

# 只运行特定检查
clang-tidy main.cpp -checks='performance-*,readability-*' -- -std=c++17
```

### 2. CMake 集成

```bash
# 方式一：启用 clang-tidy CMake 支持
cmake -B build -DCMAKE_CXX_CLANG_TIDY=clang-tidy
cmake --build build

# 方式二：创建自定义 target
# 在 CMakeLists.txt 中添加：
find_package(LLVM REQUIRED)
include(AddClangTidy)

add_executable(clang_tidy_demo main.cpp)
add_clang_tidy_target(clang-tidy-demo clang_tidy_demo)
```

### 3. 输出格式

```bash
# 更详细的输出
clang-tidy main.cpp -checks='*' -fix -- -std=c++17

# 生成 JSON 报告
clang-tidy main.cpp -checks='*' -format=json > report.json

# 只列出发现的问题（不修复）
clang-tidy main.cpp -checks='*' --list-checks
```

## 忽略行的方法

### NOLINT 注释

```cpp
// NOLINT                    - 忽略当前行
// NOLINTNEXTLINE            - 忽略下一行
// NOLINTNEXTLINE(rule-name) - 忽略下一行，指定特定规则
// NOLINT(rule-name)         - 忽略当前行，指定特定规则
```

### 示例

```cpp
int unused = 100;  // NOLINT(readability-identifier-naming)

// NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
Resource* raw = new Resource();
```

### 忽略整个文件

在文件开头添加：
```cpp
// NOLINTBEGIN
// ... 代码 ...
// NOLINTEND
```

## CMake 集成详解

### 方式一：启用 CMake 内置支持

```cmake
cmake -B build -DCMAKE_CXX_CLANG_TIDY="clang-tidy;--config-file=.clang-tidy"
cmake --build build
```

### 方式二：使用 LLVM 的 AddClangTidy 模块

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(ClangTidyDemo)

find_package(LLVM REQUIRED)
include(AddClangTidy)

add_executable(clang_tidy_demo main.cpp)

# 自动为所有目标添加 clang-tidy 检查
add_clang_tidy_target()
```

### 方式三：自定义 target

```cmake
find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy)

add_custom_target(clang-tidy
    COMMAND ${CLANG_TIDY_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    -- -std=c++17
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
```

## 常见问题排查

### Q: clang-tidy 找不到？
```bash
# 检查是否安装
which clang-tidy
clang-tidy --version

# Ubuntu/Debian
sudo apt install clang-tools

# macOS
brew install llvm
```

### Q: 如何只检查头文件？
```bash
clang-tidy "include/**/*.h" -- -std=c++17
```

### Q: 如何排除特定目录？
在 `.clang-tidy` 中配置：
```yaml
AnalyzeTemporaryDirs: false
```

### Q: 如何与 CI/CD 集成？
```bash
# GitHub Actions 示例
- name: Run clang-tidy
  run: |
    find . -name "*.cpp" -o -name "*.h" | \
    xargs clang-tidy --config-file=.clang-tidy -- -std=c++17
```
