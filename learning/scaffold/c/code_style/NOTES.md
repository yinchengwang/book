# code_style 学习笔记

## 概念地图

C/C++ 编码规范是团队长期产出可维护代码的基础保障：

- **clang-format**：LLVM 项目提供的自动代码格式化器，支持数百项风格选项
- **`.clang-format` 文件**：当前仓库根目录已有（见 `.clang-format`），基于 clang 生成
- **`--dry-run --Werror`**：只检查不修改，格式违反时返回非零 exit code
- **`-i`**：in-place 自动格式，覆盖源文件
- **CI gate**：`make check` 成为任何 MR/PR 的必过测试

规范 vs 格式化器：
- 规范（style guide）是人定的规则文档（命名、缩进、行宽、括号位置等）
- 格式化器（clang-format）自动执行这些规则（除语义级部分）
- 现代 C 工程的标准是"格式化器先规范，人工审核后规范"
- 本仓库的 `.clang-format` 配置目前支持 R5/R6 的所有 scaffold 源码

clang-format 的 `BasedOnStyle` 选项：`LLVM` / `Google` / `Chromium` / `Mozilla` / `WebKit`。本仓库用的是用户记忆 `[[生成 clang 编码风格]]` 定制的（project_clang_format.md）。

## 踩坑记录

1. **本机 no clang-format**：MinGW 命令行无 `clang-format`，需 `choco install llvm`（Win）或 `brew install clang-format`（mac）。本卡 Makefile 已做 graceful 降级：clang-format 不可用时 exit 0 而非 exit 1。
2. **`--Werror` 与 `--dry-run`**：`Werror` 让警告级别转为错误；`dry-run` 不写文件——给出干净/不干净的结论，不破坏原代码。
3. **不能一把全覆盖**（特别是 C99 vs C11 零宽度位字段等边际规则）：clang-format 对非常见 pattern（`_Generic`、anonymous union、designated initializer）可能不完美分类——人工审核仍是 gate。
4. **`.clang-format` 文件被发现路径**：clang-format 自动向上树查找到第一个 .clang-format，也可显式 `--style=file` 传。
5. **demo.c 故意写坏**：本 scaffold 的 `demo.c` 是格式化前版本；`make fix` 会把它自动变为规范版。读者可 `diff demo.c` 前后看到变化。

## 工程对照（≥100 字硬约束）

Code style 在工程侧有以下直接迁移价值：

1. **仓库根 `.clang-format` 是已有定制信源**（project_clang_format.md 记忆）。R5/R6 所有 16 张 scaffold 应最终统一到此配置——本卡 `make check` 是"收敛所有学习卡到 project style"的第一步。
2. **`engineering/` (CMake) vs `learning/` (Makefile) 的两种构建**：CMake 侧用 `add_clang_format_target()` 得自定义 CMake macro；但 Makefile 侧 `make check` 更简单——两者都是"格式化器先规范"的标准。
3. **`engineering/.github/workflows/` 中的 CI**：现有 lcov CI 流程已存在（lcov HTML 报告）；扩展为 `lint-check` job 只需加 `make -C learning/scaffold/code_style check` 一步。
4. **R5/R6 16 张卡的全量格式化**：第一步用 `make fix` 自动转化；第二步人工审核"格式后是否改变了语义"；commit。本卡 Makefile 就是第一步的实用脚本。
5. **`engineering/rag/` vs `engineering/` 的 RAG 索引风格**：RAG 侧 C++ 源码 `engineering/rag/src/rag/index/` 需适配 `.clang-format` 确保与 `engineering/` 风格一致；本卡 Makefile 可给 `engineering/rag/` 提供同样的 checks。

**与 memory `[[生成 clang 编码风格]] (project_clang_format.md)` 联动**：该记忆要求基于 clang 为项目生成 C/C++ 编码风格；本卡落实为 Makefile 驱动的标准 CI gate。学完本卡能把所有 scaffold 源码收敛到统一风格，再提交工程侧 CI 的 `lint-check` job。

学完本卡能动手的事：

- 给 `learning/scaffold/code_style/` 的 Makefile 加 `check-all` phony target：循环 16 张卡所有 .c 文件做 clang-format check
- 给 `.github/workflows/` 加 `lint-check` job：跑 `make -C learning/scaffold/code_style check` 并 fail-on-error
