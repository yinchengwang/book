# repo-root-build-slimming — Design

## Context

Phase 1 已统一产物目录约定：`build/<项目或轨道>/`、`test-results/<项目或轨道>/`。但 root `CMakeLists.txt` 双轨调度器在早期（Phase 1 之前）被以错误方式构建——`cmake -B build -S .` 把整个根项目构建到 `build/` 而不是 `build/root/`，产生了一组违规的顶层配置文件和子目录（`lib/`、`bin/`、`third_part/`、`CMakeFiles/`、`Testing/`、`Makefile` 等）。

本次清理把这些违规产物删除，恢复 `build/` 目录的"轨道隔离"语义：只有 `build/engineering/`、`build/learning/`、`build/root/` 三个规范轨道目录，无顶层散落。

## Goals / Non-Goals

**Goals:**

- 删除所有 `build/` 顶层违规产物，恢复 build 目录的清晰结构
- 清理 `build/engineering/test/test_db_init_*/` gtest 夹具残留
- 不影响任何正在工作的轨道（engineering/learning/root）

**Non-Goals:**

- 不修改 CMake 配置（违规的根入口约定已在 Phase 1 用 `cmake -B build/root -S .` 取代）
- 不重建 `build/` 顶层的任何配置
- 不删除 `build/engineering/`、`build/learning/`、`build/root/` 中任何内容（本变更只清理外部违规残留）

## Decisions

### 1. 全量删除 build/ 顶层违规产物

依据：`build/CMakeCache.txt` 中 `CMAKE_HOME_DIRECTORY=C:/code/book`，明确指向根入口项目。这些条目是历史违规构建残留，不应长期保留。

### 2. 保守处理 compile_commands.json

`build/compile_commands.json` 是 clangd 用于 IDE 智能提示的索引文件。如果删除，需要 IDE 重新生成。但它是根入口构建的产物，应该跟随其源头 `CMakeCache.txt` 一起被清理。

**风险**：用户 IDE 中的 clangd 索引可能短暂失效，重新 `cmake -B build/engineering -S engineering` 或 `cmake -B build/learning -S learning` 即可重新生成。

### 3. 清理 test_db_init_* 时间戳目录

这是 gtest 使用 timestamp 命名创建临时 db 文件的残留。每次测试运行若未被清理，就会留下一个 4KB 空目录。30+ 个累积约 120KB。直接删除，未来由 .gitignore + CMake cleanup 机制防御。

### 4. 不修改 build/root/

虽然 `build/root/` 也是根入口产物，但它放在 Phase 1 规范的 `build/root/` 路径下，不在 `build/` 顶层，保留。

## Risks / Trade-offs

- [IDE clangd 索引失效] → 用户重新 cmake 配置即可恢复
- [删除后某个脚本依赖 build/ 顶层文件] → 已经过 grep 确认无脚本引用 build/ 顶层（仅根 CMakeCache.txt 记录了产物路径）
- [未来误用 `cmake -B build` 再次产生违规产物] → `.gitignore` 已 ignore 整个 `build/`，不影响 git 跟踪；本变更也不需要在文档中新增约束

## Migration Plan

1. 删除 `build/` 顶层违规产物（约 11 个条目，28MB）
2. 删除 `build/engineering/test/test_db_init_*/` 时间戳目录（30+ 个 4KB 空目录）
3. 运行 `git status --short` 确认 build/ 下无意外变动
4. 一个 commit 提交（按 OPSX 纪律，变更内只含本次清理）