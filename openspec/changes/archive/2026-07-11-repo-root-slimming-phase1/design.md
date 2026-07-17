## Context

仓库当前已经由根 `CMakeLists.txt` 调度 `engineering/` 与 `learning/` 双轨：工程轨默认启用，学习轨按需启用；CI 也分别构建工程轨与学习轨。但根目录仍残留历史时期的业务、学习、演示、工具和产物目录，例如顶层 `apps/`、`rag/`、`sdk/`、`Interview/`、`notes/`、`demo*`、`tools/`、`test_data/` 等，与双轨内目录形成重复或归属不清。

本变更是 Phase 1：目标是建立 canonical 路径并完成真实内容迁移，同时保留旧顶层兼容入口。Phase 2 将在引用清零和验证稳定后删除这些兼容入口。

## Goals / Non-Goals

**Goals:**

- 以 `engineering/` 与 `learning/` 为双轨主轴，完成根目录瘦身。
- 定义根目录共享区白名单，避免业务/学习/演示代码继续平铺在根目录。
- 将工程历史资产收敛到 `engineering/`，学习历史资产收敛到 `learning/`。
- 将 `tools/` 按用途拆分到共享脚本区或对应轨道。
- 将编译产物统一到 `build/<项目或轨道>/`，将测试/运行产物统一到 `test-results/<项目或轨道>/`。
- 保留旧顶层 README 指路和必要包装，跨两个变更周期完成兼容迁移。
- 通过分批小提交和每批验证矩阵降低迁移风险。

**Non-Goals:**

- 不重写工程轨或学习轨的业务功能。
- 不改变 RAG、SDK、apps、学习看板、学习脚手架的产品语义。
- 不在 Phase 1 删除旧顶层兼容入口；删除动作属于 Phase 2。
- 不引入新的第三方依赖。
- 不长期保留真实代码双副本，也不使用符号链接/目录联接作为跨平台兼容方案。

## Decisions

### 1. 根目录采用共享区白名单

根目录长期保留：`engineering/`、`learning/`、`docs/`、`openspec/`、`archive/`、`cmake/`、`scripts/`、`third_part/`、`reference/` 以及必要配置文件。业务、学习、演示、RAG、SDK、工具真实内容 MUST 进入对应轨道或共享基础设施目录。

替代方案是极限瘦身，把 `docs/`、`scripts/`、`cmake/`、`reference/` 也下沉；该方案会放大 CMake、CI、文档链接和子模块路径风险，因此不采用。

### 2. 工程资产统一归入 `engineering/`

顶层 `apps/`、`rag/`、`sdk/`、根 Docker 文件、`include/`、`test_data/` 归属工程轨。`include/` 在移动前 MUST 审计依赖，按内容并入 `engineering/include/` 或 `third_part/`；`test_data/` 按工程测试夹具归入 `engineering/test_data/` 或 `engineering/test/fixtures/`。

替代方案是在根目录长期保留 Docker 或测试夹具入口；该方案削弱根目录瘦身目标，因此只允许 Phase 1 保留轻量兼容入口。

### 3. 学习资产统一归入 `learning/`

顶层 `Interview/`、`arkts/`、`blog/`、`exam/`、`notes/`、`demo/`、`demo_code/`、`demo_projects/`、文章草稿和学习 misc 文件归属学习轨。重复项优先保留 `learning/` 下 canonical 内容；存在差异时 MUST 合并差异后再移除旧真实副本。

替代方案是让 `notes/` 或 `Interview/` 保留到 Phase 2；该方案会延长双 canonical 状态，因此不采用。

### 4. `tools/` 按用途拆分

服务全仓库治理、文档、迁移的工具进入 `scripts/` 或文档脚本区；工程专用工具进入 `engineering/tools/`；学习专用工具进入 `learning/tools/`。Phase 1 顶层 `tools/` 只保留 README 指路，Phase 2 删除兼容入口。

替代方案是把全部工具并入 `scripts/`；该方案会让 `scripts/` 混入工程、学习、文档多种职责，因此不采用。

### 5. 产物目录统一为 `build/` + `test-results/`

编译目录统一使用 `build/<项目或轨道>/`，例如 `build/engineering`、`build/learning`、`build/root`。测试日志、覆盖率、测试数据库、运行日志和其他测试运行产物统一使用 `test-results/<项目或轨道>/`。

现有“测试二进制输出到源码目录”的旧约定 MUST 被替换；`.gitignore`、CMake 输出路径、CTest 工作目录和测试临时路径 MUST 同步适配。

### 6. 兼容入口跨两个变更周期

Phase 1 完成真实内容迁移，并在旧顶层路径保留 README 指路和必要包装；旧路径不得保留真实代码副本。Phase 2 在引用扫描清零、CI/本地验证通过后删除兼容入口。

替代方案是 Phase 1 立即硬删除旧路径；该方案风险高，可能破坏隐藏脚本和外部使用习惯，因此不采用。

### 7. 分批小提交与每批验证

Phase 1 MUST 按批次推进：OpenSpec/治理基线、产物目录改造、低风险重复学习资产、工程资产、学习资产、`tools/` 拆分与兼容入口、最终验证收口。每批必须单独验证，失败时只回退当前批次。

## Risks / Trade-offs

- [旧路径隐藏引用遗漏] → 在每批迁移后执行旧路径引用扫描；保留的引用必须是 OpenSpec、迁移说明、兼容 README 或必要包装。
- [CMake/CTest 输出路径变更破坏测试] → 先改产物目录规范并跑工程轨、学习轨、根入口三套构建；失败只回退当前批次。
- [重复目录存在差异导致内容丢失] → 迁移前比对差异；只有完全重复项可直接移除真实副本，差异项必须合并后再替换为指路 README。
- [Windows 下符号链接/目录联接不稳定] → 不使用符号链接作为兼容方案，只使用 README 指路和必要包装。
- [根目录瘦身与兼容窗口冲突] → Phase 1 允许旧入口临时存在，但只允许轻量兼容；Phase 2 专门删除兼容入口。
- [提交过大难以回退] → 按批次小提交，每批验证，失败只回退当前批次并把原因写回任务或风险说明。

## Migration Plan

1. 创建 `repo-root-slimming-phase1` OpenSpec 文档，锁定根目录白名单、路径映射、产物目录、兼容策略和验证矩阵。
2. 改造产物目录：CMake/CTest 输出到 `build/` 与 `test-results/`，更新 `.gitignore` 和构建说明。
3. 迁移低风险重复学习资产，保留旧顶层 README 指路。
4. 迁移工程资产到 `engineering/`，修复 CMake、脚本、Docker、SDK、RAG、apps、测试夹具引用。
5. 迁移学习资产到 `learning/`，修复学习看板、脚手架、笔记和文档引用。
6. 拆分 `tools/` 并保留顶层 README 指路。
7. 执行全量验证矩阵，更新 tasks 状态和 Phase 2 删除兼容入口的后续任务。

## Open Questions

无。关键决策已在 `/grill-me` 会话中确认，Phase 1 按本设计执行。
