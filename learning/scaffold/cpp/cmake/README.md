# cmake scaffold

CMake 构建系统演示——CMakeLists.txt + add_executable + target_link_libraries + add_definitions。

## 文件结构

```
cmake/
├── main.cpp        # 含 greet/math/config 演示
├── CMakeLists.txt  # CMake 配置
├── Makefile        # 同时支持 g++ 直接编译 + cmake 调用
├── README.md
└── NOTES.md
```

## 复现命令

```bash
cd learning/scaffold/cmake

# 方式 1：直接 g++
g++ -Wall -Wextra -O2 -std=c++17 -o test main.cpp && ./test

# 方式 2：CMake 构建（推荐）
mkdir -p build && cd build
cmake ..
make
./cpp_cmake_demo

# 方式 3：Makefile wrapper
make build
```

## 关键点

- **`cmake_minimum_required(VERSION 3.10)`**：最低 CMake 版本
- **`project(name LANGUAGES CXX)`**：声明项目 + 语言
- **`add_executable(target src)`**：定义可执行文件
- **`target_link_libraries(target PRIVATE stdc++)`**：链接库
- **`add_definitions(-DXXX=...)`**：注入全局宏
- **`target_compile_options(target PRIVATE -Wall -Wextra)`**：编译警告
- **构建类型**：`CMAKE_BUILD_TYPE` 可选 `Debug`/`Release`/`RelWithDebInfo`/`MinSizeRel`

详见 NOTES.md 工程对照段。