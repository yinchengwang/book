## Why

`repo-root-slimming-phase1` 会迁移真实内容并保留旧顶层兼容入口；如果不提前记录删除阶段，兼容入口容易长期存在并重新形成双 canonical。Phase 2 的目标是在 Phase 1 稳定后删除这些临时入口，完成根目录最终瘦身。

## What Changes

- **BREAKING** 删除 Phase 1 保留的旧顶层兼容 README、薄包装、alias 或清晰报错入口。
- 清零 active 文档、脚本、CI、CMake、测试与 smoke 中对旧顶层路径的 canonical 引用。
- 执行最终根目录白名单检查，确保根目录不再保留历史业务、学习、演示、RAG、SDK 或工具入口。
- 将 Phase 1 的迁移说明收口到正式文档，保留必要历史记录但不作为可用入口。
- 执行双轨构建/CTest、根入口构建、关键 smoke、旧路径引用扫描和产物落点检查，作为删除兼容入口的闸门。

## Capabilities

### New Capabilities

- `repo-root-compatibility-removal`: 定义 Phase 2 删除旧顶层兼容入口、执行最终根目录白名单检查和禁止恢复真实/兼容双 canonical 的要求。
- `legacy-path-reference-cleanup`: 定义旧路径引用清零、active 文档/脚本/CI/CMake/测试切换到 canonical 路径、保留历史引用边界和扫描报告要求。

### Modified Capabilities

- 无。

## Impact

- 影响 Phase 1 创建的旧顶层兼容入口，例如 `apps/`、`rag/`、`sdk/`、`tools/`、`Interview/`、`notes/`、`demo*` 等路径下的 README/包装入口。
- 影响 README、CLAUDE、AGENTS、架构文档、CI、CMake、脚本、测试、smoke 命令中残留的旧路径引用。
- 对仍依赖旧顶层路径的外部或本地脚本是破坏性变更；实施前必须通过引用扫描和 Phase 1 稳定验证。
