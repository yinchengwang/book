# Phase 1 迁移盘点

## 工作树状态（任务 2.4）

- 当前分支：`project`
- 已修改（仅限本变更相关）：`AGENTS.md`、`CLAUDE.md`、`README.md`、`docs/architecture/dual-track.md`
- 新增（本变更 OpenSpec）：`openspec/changes/repo-root-slimming-phase1/`、`openspec/changes/repo-root-slimming-phase2/`
- 新增（治理文档）：`engineering/README.md`、`learning/README.md`
- 预存未跟踪文件与本变更无关，不混入提交

## 顶层历史目录映射表（任务 2.1–2.2）

### 工程资产

| 顶层项 | Canonical 目标 | 重复？ | diff 摘要 | 建议 |
|---|---|---|---|---|
| `apps/` | `engineering/apps/` | 是（差异） | 顶层 apps 有旧 copy；engineering/apps 更完整 | 需合并后迁入，顶层保留 README 指路 |
| `rag/` | `engineering/rag/` | 是（差异） | engineering/rag 更完整，有 remote 索引等新增 | 需合并后迁入，顶层保留 README 指路 |
| `sdk/` | `engineering/sdk/` | 是（差异） | 顶层 sdk 为旧版，engineering/sdk 更完整 | 需合并后迁入，顶层保留 README 指路 |
| `Dockerfile` | `engineering/Dockerfile` | 是 | engineering 下已有 | 保留 engineering 版本，根保留薄包装或说明 |
| `docker-compose.yml` | `engineering/docker-compose.yml` | 是 | engineering 下已有 | 保留 engineering 版本，根保留薄包装或说明 |
| `include/` | 审计后归入 `engineering/include/` 或 `third_part/` | 部分重复 | 包含 cJSON.h、compare.h；与 engineering/include 有差异 | 需审计后归并，移除工程 CMake 对 ../include 依赖 |
| `test_data/` | `engineering/test_data/` 或 `engineering/test/fixtures/` | 待确认 | Git 跟踪的测试数据文件 | 复用夹具迁入 fixture 目录，生成数据改写到 `test-results/engineering/` |

### 学习资产

| 顶层项 | Canonical 目标 | 重复？ | diff 摘要 | 建议 |
|---|---|---|---|---|
| `Interview/` | `learning/interview/` | 是（差异） | 两边都存在，learning 版本有更多内容 | 需合并后迁入，顶层保留 README 指路 |
| `arkts/` | `learning/arkts/` | 是（相同） | 内容完全相同 | 直接迁移，顶层替换为 README 指路 |
| `blog/` | `learning/blog/` | 是（相同） | 内容完全相同 | 直接迁移，顶层替换为 README 指路 |
| `exam/` | `learning/exam/` | 是（相同） | 内容完全相同 | 直接迁移，顶层替换为 README 指路 |
| `notes/` | `learning/notes/` | 是（差异） | learning/notes 更完整，有新结构 | 需合并后迁入，顶层保留 README 指路 |
| `demo/` | `learning/playground/demo/` | 待确认 | 可能存在差异 | 合并到 playground |
| `demo_code/` | `learning/playground/demo_code/` | 待确认 | 可能存在差异 | 合并到 playground |
| `demo_projects/` | `learning/playground/demo_projects/` | 待确认 | 可能存在差异 | 合并到 playground |
| `csdn-article-content.md` | `learning/articles/` | 相同 | 与 `learning/articles/csdn-drafts.md` 内容相同 | 直接迁移或删除 |
| `kanban-snapshot.md` | `learning/misc/kanban-snapshot.md` | 相同 | 与 `learning/misc/` 下文件相同 | 直接迁移 |
| `learn-vdb-deepdive-check.yml` | `learning/misc/learn-vdb-deepdive-check.yml` | 相同 | 与 `learning/misc/` 下文件相同 | 直接迁移 |
| `lines_of_code.txt` | `learning/misc/lines_of_code.txt` | 相同 | 与 `learning/misc/` 下文件相同 | 直接迁移 |

### tools 拆分

| 顶层项 | Canonical 目标 | 建议 |
|---|---|---|
| `tools/` (all) | 按用途拆分 | 全仓库治理/文档/迁移→`scripts/`；工程→`engineering/tools/`；学习→`learning/tools/` |

## 旧路径引用扫描结果（任务 2.3）

非 worktree 非 OpenSpec 变更文档中的旧路径引用：

| 文件 | 引用内容 | 类型 | 处理 |
|---|---|---|---|
| `.remember/now.md` | 旧顶层路径 | 历史记录 | 允许保留，不修改 |
| `.remember/today-2026-07-11.md` | 旧顶层路径 | 历史记录 | 允许保留，不修改 |
| `learning/scaffold/c-code-style/NOTES.md` | 路径引用 | 学习资产 | Phase 1 做修复 |
| `learning/scaffold/c-valgrind/NOTES.md` | 路径引用 | 学习资产 | Phase 1 做修复 |
| `learning/scaffold/c-undefined-behavior/NOTES.md` | 路径引用 | 学习资产 | Phase 1 做修复 |
| `learning/scaffold/c-dynamic-memory/NOTES.md` | 路径引用 | 学习资产 | Phase 1 做修复 |
| `learning/scaffold/c-pointer/NOTES.md` | 路径引用 | 学习资产 | Phase 1 做修复 |
| `learning/scaffold/R5-HANDOFF.md` | 路径引用 | 学习资产 | Phase 1 做修复 |
| `openspec/specs/r6-language-tranche/spec.md` | 路径引用 | OpenSpec | 评估是否需要修正 |
| `engineering/sdk/python/README.md` | ../sdk python | 工程资产 | Phase 1 修复 |
| `engineering/rag/docs/*.md` | 参考根 rag | 工程资产 | Phase 1 修复 |
| `learning/notes/README.md` | 笔记路径 | 学习资产 | Phase 1 修复 |

**OpenSpec 变更文档（本变更）中的引用**：属于本变更范围内的文档，不需要额外修复。

## 回退纪律（任务 2.5）

- 每批提交前确认工作树干净（仅含该批次相关文件）
- 若某批次验证失败：`git reset --hard HEAD~1` 回退当前批次，保留前序批次
- 失败原因写回 `tasks.md` 该批次任务中的阻塞说明
- 必要时将大任务拆成子任务继续
- 不允许为"快速干净"回退前序已验证通过的任务
