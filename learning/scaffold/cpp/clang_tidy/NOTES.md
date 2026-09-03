# 代码质量工具工程对照

## 项目中的 clang-tidy 应用

### 1. CMake 集成

```cmake
# engineering/cmake/clang_tidy.cmake
include(GoogleTest)

# 启用 clang-tidy
set(CMAKE_CXX_CLANG_TIDY
    clang-tidy
    -checks=-*,modernize-*,performance-*,readability-*,bugprone-*)

# 构建时自动检查
# make VERBOSE=1 2>&1 | grep "clang-tidy"
```

### 2. CI/CD 集成

```yaml
# .github/workflows/clang-tidy.yml
- name: Run clang-tidy
  run: |
    find engineering/src -name "*.cpp" | xargs clang-tidy \
      -checks=-*,modernize-*,performance-* \
      -header-filter='.*' \
      -- -std=c++17 -I engineering/include
```

### 3. 常用检查规则

| 规则 | 问题 | 建议 |
|------|------|------|
| modernize-use-nullptr | `int* p = NULL` | `int* p = nullptr` |
| modernize-use-emplace | `v.push_back(T(args))` | `v.emplace_back(args)` |
| modernize-use-auto | `std::vector<int>::iterator it = v.begin()` | `auto it = v.begin()` |
| modernize-use-override | 虚函数无 override | 添加 override |
| performance-copy-begin-end | `copy(v1.begin(), v1.end(), v2.begin())` | `v2 = v1` |
| performance-no-int-to-ptr | `reinterpret_cast<T*>(int)` | `reinterpret_cast<T*>(uintptr_t)` |

### 4. .clang-tidy 配置示例

```yaml
# .clang-tidy
Checks: '-*,modernize-*,performance-*,readability-*,bugprone-*'
WarningsAsErrors: modernize-*,performance-*
HeaderFilterRegex: 'engineering/.*\\.h'
FormatStyle: file
CheckOptions:
  modernize-use-nullptr.NullptrLiteralThreshold: 1
  readability-identifier-naming.ClassCase: CamelCase
```

## clang-tidy 检查清单

### modernize-*（现代 C++）
- [ ] modernize-use-nullptr
- [ ] modernize-use-emplace
- [ ] modernize-use-auto
- [ ] modernize-use-override
- [ ] modernize-use-transparent-functors
- [ ] modernize-loop-convert

### performance-*（性能）
- [ ] performance-inefficient-vector-operation
- [ ] performance-move-const-arg
- [ ] performance-noexcept-move-constructor
- [ ] performance-unnecessary-copy-initialization
- [ ] performance-for-range-copy

### readability-*（可读性）
- [ ] readability-identifier-naming
- [ ] readability-redundant-string-init
- [ ] readability-simplify-boolean-expr
- [ ] readability-implicit-bool-cast

### bugprone-*（Bug 检测）
- [ ] bugprone-unused-local-non-trivial-variable
- [ ] bugprone-suspicious-semicolon
- [ ] bugprone-terminating-continue
- [ ] bugprone-incorrect-roundings

## 与其他工具对比

| 工具 | 用途 | 侧重点 |
|------|------|--------|
| clang-tidy | 静态分析 | 现代 C++ 最佳实践 |
| cppcheck | 静态分析 | 内存错误、C++ 标准 |
| clang-format | 代码格式化 | 代码风格 |
| coverity | 商业静态分析 | 安全漏洞 |
