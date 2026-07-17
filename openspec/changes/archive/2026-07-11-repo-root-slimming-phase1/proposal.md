## Why

当前仓库已经形成 `engineering/` 与 `learning/` 双轨，但根目录仍保留多组历史业务、学习、演示、RAG、SDK、工具与产物目录，导致 canonical 路径不清、重复副本并存、构建/测试产物污染源码目录。现在需要在双轨基础上做根目录瘦身，把真实内容收敛到对应轨道，并建立统一产物目录和兼容迁移规则。

## What Changes

- 建立根目录共享区白名单：根目录长期只保留双轨入口、共享治理/文档/构建基础设施、第三方/参考源码与必要配置文件。
- 将工程历史资产收敛到 `engineering/`：包括顶层 `apps/`、`rag/`、`sdk/`、根 Docker 入口、`include/` 审计归并、`test_data/` 工程测试夹具归位等。
- 将学习历史资产收敛到 `learning/`：包括顶层 `Interview/`、`arkts/`、`blog/`、`exam/`、`notes/`、`demo*`、文章草稿和 misc 文件等。
- 拆分顶层 `tools/`：全仓库治理工具进入 `scripts/` 或文档脚本区，工程专用工具进入 `engineering/tools/`，学习专用工具进入 `learning/tools/`。
- 统一产物目录：编译产物进入 `build/<项目或轨道>/`，测试日志、覆盖率、测试数据库、运行日志等进入 `test-results/<项目或轨道>/`，并覆盖现有 CMake/测试输出路径旧约定。
- Phase 1 全量迁移真实内容但保留旧顶层兼容入口：旧目录只保留 README 指路和必要包装，不保留真实代码副本；Phase 2 再删除兼容入口。
- 每批迁移必须执行双轨构建/CTest、根入口调度、旧路径引用扫描、关键 smoke 和产物落点检查。
- 同步根治理文档、架构文档、轨道 README、OpenSpec 文档和兼容入口 README。

## Capabilities

### New Capabilities

- `repo-root-organization`: 定义根目录白名单、双轨资产归属、顶层历史目录迁移规则、`tools/` 拆分规则和两阶段兼容策略。
- `build-test-artifacts`: 定义编译产物与测试/运行产物的统一落点，并要求 CMake、CTest、覆盖率、测试数据库和日志不再污染源码目录。

### Modified Capabilities

- 无。

## Impact

- 影响根目录、`engineering/`、`learning/`、`tools/`、`apps/`、`rag/`、`sdk/`、`include/`、`test_data/`、`Interview/`、`notes/`、`demo*` 等目录的路径组织与引用。
- 影响 CMake/CTest 输出路径、`.gitignore`、CI/本地构建命令、测试夹具路径和部分脚本入口。
- 影响文档：`README.md`、`CLAUDE.md`、`AGENTS.md`、`docs/architecture/dual-track.md`、轨道 README、兼容入口 README。
- 不改变工程轨和学习轨的业务语义；主要风险来自路径迁移、旧链接失效和隐藏脚本依赖。
