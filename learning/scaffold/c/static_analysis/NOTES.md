# static_analysis 学习笔记

## 概念地图

静态分析是在**不运行代码**的前提下，通过语法树/数据流/控制流分析发现缺陷的技术。它位于"编译期"和"测试运行期"之间：

- **编译器警告（GCC -Wall -Wextra）**：最廉价的静态分析——开 flag 即用，零配置。`-Wunused-function`、`-Wmaybe-uninitialized`、`-Wsign-compare` 是本卡演示的三条。warning ≠ 代码真的有 bug（可能你确实想用有符号不匹配），但它是某种"代码异味"的信号
- **AST 级检查（cppcheck）**：解析 C/C++ 源码为抽象语法树，运行预定义规则（如"malloc 后所有路径都必须有 free"）。cppcheck 的哲学是**宁可漏报，不误报**——每条规则都必须有极高的置信度。这使它在 Linux 内核等超大型项目中成为标准工具
- **数据流分析（clang-tidy）**：不仅看语法树，还追踪变量的使用链——比如"这个变量被赋值了但从未被读取"。clang-tidy 规则库可定制，现代 LLVM 项目用它做 CI 的 static check gate
- **安全规则扫描（flawfinder）**：按 CWE（Common Weakness Enumeration）分类映射代码模式——`strcpy` → CWE-120（缓冲区溢出）、`printf(user_input)` → CWE-134（格式字符串漏洞）
- **运行时检测 vs 静态分析**：ASAN/UBSAN/Valgrind 是在程序运行时插桩检测——能发现"真实执行路径"上的 bug，但受限于测试覆盖率。静态分析能覆盖所有路径（包括未测试到的），但会有误报。两者互补

## 踩坑记录

1. **MinGW 的 warning 行为与 Linux gcc 不同**：`-fstack-protector` 在 MinGW 下不生效（Windows 没有 SSP），`-D_FORTIFY_SOURCE` 需要 glibc。本卡只用编译器 warning，不做运行时检查
2. **warning 不等于 bug**：`-Wsign-compare` 在有符号/无符号比较时警告——但有时你确实需要 `u < (unsigned)s` 这种写法。此时有两个选择：a) 显式类型转换消除 warning（推荐）；b) 抑制这条 warning（慎用）
3. **cppcheck 的 `--enable=all` 可能产生噪音**：特别是 `--inconclusive` 模式——在生产项目中建议只启用 `--enable=warning,performance,portability`，关闭 `style`
4. **抑制 vs 修复**：`/* fallthrough */` 注释可以告诉 GCC "我知道这里没有 break，故意的"。`_Pragma("GCC diagnostic ignored \"-Wunused\"")` 可以局部关闭 warning——用于"第三方头文件无法修改"的场景
5. **`-Werror` 的版本锁定问题**：如果 CI 中用了 `-Werror`，编译器升级后可能引入新的 warning 类型——导致之前通过 CI 的代码突然失败。解决办法：锁定 `-std=c11`，并使用 `-Wno-unknown-warning-option` 忽略未来可能新增的 warning flag

## 工程对照（≥100 字硬约束）

静态分析已渗透在本仓库的多个层面：

1. **`.clang-format` — 格式化的静态规则**：`.clang-format` 中的 `BasedOnStyle: LLVM` + `ColumnLimit: 120` + `IndentWidth: 4` 是"静态分析的第一关"——连格式都不对，更复杂的逻辑分析无从谈起。用 `git blame .clang-format` 追溯每条规则的引入原因
2. **CMake 构建中的静态分析集成**：`engineering/cmake/ProjectUtils.cmake` 的 `add_project_library()` 和 `add_project_test()` 可扩展为包含 `add_static_analysis_target()`——在 CMake 中嵌入 cppcheck/clang-tidy 的 custom target。本仓库的 `all_tests` custom target 就是这个模式
3. **errors.c 的死代码检测**：`engineering/src/db/core/errors.c` 中定义了 `SYS_ERROR_*` 和 `DB_ERROR_*` 系列错误码——静态分析能自动检测"定义了但从未使用的错误码"（dead error），这在手工维护 50+ 个错误码时是常态
4. **BUILD 编译日志的 warning 趋势**：`build/engineering/` 的 CMake 输出中已配置了 `-Wall -Wextra`——每次构建时数一下 warning 数量，0 warning 是基线。引入新代码后 warning 数量增加说明可能引入了问题
5. **cppcheck 的 project-level 检查**：对整个 engineering/ 目录运行 `cppcheck --enable=all --project=build/engineering/compile_commands.json`（如果有 compile_commands.json），能从"调用图"层面发现跨文件的问题——比如 A 函数 malloc 了指针传给 B 函数，B 函数的所有路径是否都 free 了

学完本卡后能动手的事：给 `engineering/cmake/ProjectUtils.cmake` 添加 `add_static_analysis_target()` 宏——用 CMake 的 `add_custom_target` 包一层 cppcheck/clang-tidy 命令，让 `cmake --build build/engineering --target static_check` 一键跑完所有静态分析。
