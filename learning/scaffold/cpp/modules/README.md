# C++20 模块

## 概述

C++20 引入了模块（Module）特性，替代传统的 `#include` 头文件方式，带来更快的编译速度和更好的封装性。

## 核心概念

### 模块声明
- `module;` — 全局模块片段开始
- `export module <name>;` — 声明导出模块
- `import <name>;` — 导入模块
- `export { ... }` — 批量导出

### 优势
- **编译加速**：模块只编译一次，结果可缓存
- **消除重复编译**：头文件每次包含都会重新编译
- **更好的封装**：内部实现可对用户隐藏

## 编译要求

### GCC（需 14+，旧版本用 -fmodules-ts）
```bash
g++ -std=c++20 -c math.ixx -o math.pcm
g++ -std=c++20 -fprebuilt-module-path=. main.cpp math.pcm -o test
```

### Clang（16+）
```bash
clang++ -std=c++20 -c math.ixx -o math.pcm
clang++ -std=c++20 -fprebuilt-module-path=. main.cpp math.pcm -o test
```

### MSVC（19.29+）
```bash
cl /std:c++20 /c math.ixx
cl /std:c++20 main.cpp math.obj
```

## 文件说明

- `math.ixx` — 模块接口单元
- `main.cpp` — 导入并使用模块

## 相关资源

- cppreference: https://en.cpppreference.com/w/cpp/language/modules
