# C++ 代码质量工具 clang-tidy

> modernize-use-* + performance-* 检查

## 编译

```bash
g++ -std=c++17 -o main main.cpp
./main
```

## 运行 clang-tidy

```bash
# 检查单个文件
clang-tidy main.cpp -- -std=c++17

# 检查所有 modernize-* 和 performance-*
clang-tidy main.cpp -checks='modernize-*,performance-*' -- -std=c++17

# 自动修复
clang-tidy main.cpp -fix -- -std=c++17
```

## 常用检查项

| 检查项 | 说明 |
|--------|------|
| modernize-use-nullptr | 使用 nullptr |
| modernize-use-emplace | 使用 emplace |
| modernize-use-auto | 使用 auto |
| modernize-use-override | 使用 override |
| performance-* | 性能检查 |
| readability-* | 可读性检查 |
| bugprone-* | Bug 检测 |

## 运行结果

```
=== clang-tidy 演示 ===

1. modernize-use-nullptr:
   建议：使用 nullptr 替代 NULL 或 0

2. modernize-use-emplace:
Construct: hello
Construct: world
Construct: emplace
Construct: xxxxx
   建议：使用 emplace_back 替代 push_back
...
```
