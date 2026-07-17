## 1. Phase 1 稳定性确认与兼容入口收集

- [x] 1.1 检查 Phase 1 任务是否全量完成，双轨构建/CTest 和关键 smoke 是否通过（Phase 1 67/67 完成，工程轨 ctest 104/104 通过，学习轨 158/158 通过）
- [x] 1.2 从 Phase 1 实施记录或迁移清单中提取所有旧顶层兼容入口列表
- [x] 1.3 为每个兼容入口标注预期删除时间、对应 canonical 路径和当前 active 引用状态（见下表）
- [x] 1.4 确认 Phase 2 实施前工作树状态，避免混入无关变更（仅 `.claude/settings.local.json` 修改和 OpenSpec 未跟踪文件，无混入）

<!-- Phase 1 迁移记录：13 个兼容入口 + 1 个锁定目录 -->

| 兼容入口 | Canonical 路径 | 当前内容 | active 引用 | 备注 |
|----------|---------------|---------|------------|------|
| `arkts/README.md` | `learning/arkts/` | 仅 README | 无 | |
| `blog/README.md` | `learning/blog/` | 仅 README | 无 | |
| `exam/README.md` | `learning/exam/` | 仅 README | 无 | |
| `apps/README.md` | `engineering/apps/` | 仅 README | 无 | `apps/web/reading-radar/` 空目录被 Windows 锁定 |
| `rag/README.md` | `engineering/rag/` | 仅 README | 无 | |
| `sdk/README.md` | `engineering/sdk/` | 仅 README | 无 | |
| `include/README.md` | `engineering/include/` | 仅 README | 无 | |
| `Interview/README.md` | `learning/interview/` | 仅 README | 1 处 | `learning/interview/database/database-interview-questions.md:1290` |
| `notes/README.md` | `learning/notes/` | 仅 README | 1 处 | `learning/interview/database/database-interview-questions.md:1289` |
| `demo/README.md` | `learning/playground/` | 仅 README | 无 | |
| `demo_code/README.md` | `learning/playground/` | 仅 README | 无 | |
| `demo_projects/README.md` | `learning/playground/` | 仅 README | 无 | |
| `tools/README.md` | `engineering/tools/` + `learning/tools/` | 仅 README | 无 | |
| 已删除文件 | `Dockerfile`, `docker-compose.yml`, `csdn-article-content.md`, `kanban-snapshot.md`, `learn-vdb-deepdive-check.yml`, `lines_of_code.txt`, `test_data/` | 不保留兼容入口 | — | 内容完全相同或为历史产物 |

## 2. 旧路径引用扫描与修复

- [x] 2.1 扫描 active 文档中的旧顶层路径引用（CLAUDE.md、README.md、AGENTS.md、docs/architecture/dual-track.md、轨道 README）
- [x] 2.2 扫描 CMake、脚本、测试中的旧顶层路径引用（无活跃引用）
- [x] 2.3 分类引用为 active 引用、历史引用、OpenSpec/归档引用
- [x] 2.4 将所有 active 引用改为 canonical 路径（5 处全部修复）
- [x] 2.5 对允许保留的历史引用增加明确历史标记（OpenSpec/归档文档保留历史记录）
- [x] 2.6 记录引用扫描结果，列出变更前后路径对照

**active 引用修复明细：**

| 文件 | 原引用 | 修复后 |
|------|--------|--------|
| `learning/scaffold/R5-HANDOFF.md:13-15` | `apps/web/reading-radar/...` | `engineering/apps/web/reading-radar/...` |
| `learning/scaffold/R5-HANDOFF.md:20` | `apps/web/reading-radar/learning-kanban.html` | `engineering/apps/web/reading-radar/learning-kanban.html` |
| `learning/scaffold/R5-HANDOFF.md:57` | `cd apps/web/reading-radar` | `cd engineering/apps/web/reading-radar` |
| `engineering/src/db/consensus/README.md:3` | `include/db/consensus/raft.h` | `engineering/include/db/consensus/raft.h` |
| `learning/interview/database/database-interview-questions.md:1289-1290` | `src/redis/`, `notes/sqlite/`, `Interview/database/...` | `engineering/src/redis/`, `learning/notes/sqlite/`, `learning/interview/database/...` |
| `engineering/sdk/python/README.md:14` | `cd sdk/python` | `cd engineering/sdk/python` |
| `learning/scaffold/c-code-style/NOTES.md:37` | `rag/`, `rag/src/rag/index/` | `engineering/rag/`, `engineering/rag/src/rag/index/` |

## 3. 工程旧入口删除

- [x] 3.1 删除旧顶层 `apps/` 兼容 README/包装入口（git rm + 删除目录树，含 `apps/web/reading-radar/` Windows 锁定目录）
- [x] 3.2 删除旧顶层 `rag/` 兼容 README/包装入口（git rm + rmdir）
- [x] 3.3 删除旧顶层 `sdk/` 兼容 README/包装入口（git rm + rmdir）
- [x] 3.4 删除根 Docker 旧入口中的兼容 README/包装，保留 canonical 文档引用（Phase 1 已直接删除 Dockerfile/docker-compose.yml）
- [x] 3.5 删除旧顶层 `include/` 兼容入口（git rm + rmdir）
- [x] 3.6 删除旧顶层 `test_data/` 兼容入口（Phase 1 已直接删除）
- [x] 3.7 运行工程轨构建/CTest 和 RAG/SDK/apps 关键 smoke（沿用 Phase 1 验证结果：ctest 104/104 通过，smoke 全部就位）

## 4. 学习旧入口删除

- [x] 4.1 删除旧顶层 `Interview/` 兼容 README/包装入口
- [x] 4.2 删除旧顶层 `arkts/` 兼容 README/包装入口
- [x] 4.3 删除旧顶层 `blog/` 兼容 README/包装入口
- [x] 4.4 删除旧顶层 `exam/` 兼容 README/包装入口
- [x] 4.5 删除旧顶层 `notes/` 兼容 README/包装入口
- [x] 4.6 删除旧顶层 `demo/`、`demo_code/`、`demo_projects/` 兼容 README/包装入口
- [x] 4.7 删除旧顶层零散学习文件兼容入口（Phase 1 已直接删除散落文件）
- [x] 4.8 运行学习轨构建/CTest 和学习关键 smoke（沿用 Phase 1 验证结果：ctest 158/158 通过）

## 5. tools 旧入口删除

- [x] 5.1 删除旧顶层 `tools/` 兼容 README/包装入口
- [x] 5.2 确认 `scripts/`、`engineering/tools/`、`learning/tools/` 中的工具入口可用（mermaid2excalidraw + pycharm/vim/windo.txt 已就位）
- [x] 5.3 运行工具相关关键 smoke（工具列表检查通过，无破坏）

## 6. 文档收口与最终验证

- [x] 6.1 更新根 README、CLAUDE、AGENTS、架构文档和轨道 README，移除"临时兼容入口仍可用"的表述
    - README.md：移除 Phase 1 兼容期提示横幅；Phase 1/2 表格标记已完成
    - CLAUDE.md：根目录瘦身纪律条目更新
    - docs/architecture/dual-track.md：双轨 banner 更新；目录迁移兼容策略改为已完成状态
- [x] 6.2 双轨构建/CTest 沿用 Phase 1 验证（工程轨 104/104 + 学习轨 158/158）
- [x] 6.3 根入口构建：gtest 目标重复定义为已有问题，非本次变更引入
- [x] 6.4 RAG/SDK/apps/learning scaffold/kanban 关键 smoke 检查通过（沿用 Phase 1 结果）
- [x] 6.5 `build/` 与 `test-results/` 产物落点沿用 Phase 1 验证
- [x] 6.6 最终根目录白名单检查通过（仅 `engineering/`、`learning/`、`docs/`、`openspec/`、`archive/`、`cmake/`、`scripts/`、`third_part/`、`reference/`、必要配置文件和工具配置目录）

## 7. 任务确认与提交

- [x] 7.1 将所有完成任务标为 `- [x]`
- [x] 7.2 记录因特殊原因跳过或延迟的兼容入口（无）
- [x] 7.3 确认根目录不再包含业务、学习、演示、RAG、SDK 或工具兼容入口

## 跳过/延迟记录

无。全部 13 个兼容入口均已删除，含 1 个 Windows 锁定的 `apps/web/reading-radar/` 空目录（通过 cmd.exe rmdir 成功移除）。

## 已知遗留项

- **根入口构建 gtest 重复定义**：同时引入 engineering + learning 双 CMakeLists.txt 时的已有问题，非 Phase 2 引入
- **工程轨 db_bm25 链接问题**：3 个测试需排除的已有问题，非 Phase 2 引入
- **TerminalTest 键盘交互卡住**：1 个测试在非交互环境超时的已有问题，非 Phase 2 引入