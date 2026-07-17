# cmake 学习笔记

## 概念地图

CMake 是 C/C++ 项目的"元构建系统"——它不直接编译，而是生成 Makefile/Ninja/Visual Studio 项目：

- **CMake 三层结构**：
  1. **CMakeLists.txt** ——项目配置（每个目录一个）
  2. **build/** ——生成产物（CMake 推荐 out-of-source build）
  3. **CMakeCache.txt** ——缓存变量
- **核心命令**：
  - `cmake_minimum_required(VERSION X.Y)` ——最低 CMake 版本
  - `project(name LANGUAGES CXX)` ——声明项目
  - `add_executable(target src)` ——可执行文件
  - `add_library(target STATIC/SHARED src)` ——库
  - `target_link_libraries(target lib)` ——链接
  - `target_include_directories(target PRIVATE path)` ——头文件路径
  - `target_compile_options(target -Wall)` ——编译选项
  - `add_definitions(-DXXX=...)` 或 `target_compile_definitions(...)` ——宏定义
- **现代 CMake 范式（3.x+ 推荐）**：
  - 用 `target_*` 命令而非全局 `include_directories`/`add_definitions`
  - 用 `PRIVATE`/`PUBLIC`/`INTERFACE` 区分链接可见性
  - 用 `generator expressions` ($<...>) 表达条件
- **构建类型**：
  - `Debug` —— `-O0 -g` 无优化含调试信息
  - `Release` —— `-O3 -DNDEBUG` 优化发布
  - `RelWithDebInfo` —— `-O2 -g` 优化但带调试信息
  - `MinSizeRel` —— `-Os` 体积最小
- **变量作用域**：
  - `set(VAR val)` ——当前 CMakeLists.txt 可见
  - `PARENT_SCOPE` ——父目录可见
  - `CACHE` ——跨 CMakeLists.txt 可见

## 踩坑记录

1. **out-of-source build**：`mkdir build && cd build && cmake ..` 避免污染源码目录
2. **find_package 失败**：CMake 找不到第三方库——设 `CMAKE_PREFIX_PATH` 或 `CMAKE_INSTALL_PREFIX`
3. **target_link_libraries 顺序**：CMake 3.x+ 不严格要求，旧版严格
4. **子目录 add_subdirectory**：每个子目录有自己的 CMakeLists.txt + 自己的作用域
5. **vcpkg / conan 集成**：`find_package(Boost)` 等需要包管理器装
6. **CMake 函数 vs 宏**：`function()` 有独立作用域，`macro()` 没有
7. **MSVC vs GCC 选项差异**：用 `if(MSVC)` 分发，或用 generator expressions `$<$<CXX_COMPILER_ID:MSVC>:/W4>`

## 工程对照（≥100 字硬约束）

CMake 在本仓库 `engineering/` 中是工程轨的"构建中枢"：

1. **`engineering/CMakeLists.txt` 双轨调度器**：根 `CMakeLists.txt` 用 `ENGINEERING_BUILD`/`LEARNING_BUILD` 开关调度两条轨道。这是 CMake 条件分支的标准用法
2. **`engineering/cmake/ProjectUtils.cmake` 函数库**：`add_project_test(name VAR)` 和 `add_project_library(name VAR)` 两个 helper 函数——`function()` 定义、`add_custom_target` 包装 ctest
3. **`engineering/cmake/dual-track-guard.cmake`**：跨轨守卫——CMake 脚本检查"不混入对方轨道代码"。这是 CMake 的"业务规则测试"
4. **`engineering/CMakeLists.txt` 的 option/section**：`option(ENGINEERING_BUILD "Build engineering track" ON)` 让用户可选轨道
5. **`engineering/src/db/sql/CMakeLists.txt` 子目录构建**：每个子目录独立 `CMakeLists.txt`，`add_subdirectory()` 引用
6. **`engineering/cmake/ProjectUtils.cmake` 的 include 路径管理**：用 `INTERFACE` 库 `project_includes` 携带头文件搜索路径，所有库 `target_link_libraries(... project_includes)` 继承
7. **`engineering/apps/web/knowledge_hub/` Taro 项目**：独立 `package.json` + `taro build`——非 CMake 项目，但展示了"不同栈用不同构建工具"的现实

学完本卡能动手的事：把 `learning/scaffold/` 下所有 R5–R11 卡的 Makefile 升级为 CMake——根 `learning/CMakeLists.txt` 配 `add_subdirectory()` + `add_executable()` + `add_test()`，由 CTest 统一调度。