## Context

现有 `github-issue-todo` 是一个完整的 Issue 管理工具（C 后端 + Winsock HTTP 服务器 + JSON 文件持久化 + Vue 3 SPA 前端）。本次变更将其演变为通用待办事项系统，增加优先级、截止日期、分组、评论、看板视图、统计看板等能力，并与 OPSX 变更系统打通。

当前状态：
- 后端：C 语言，单文件 HTTP 服务器，JSON 文件持久化
- 前端：内嵌 `web/` 目录的单 HTML 文件 + Vue 3 CDN
- 数据文件：`issue_tool.json`
- 项目目录：`engineering/apps/github-issue-todo/`

## Goals / Non-Goals

**Goals:**
- 将项目重命名为 `todo-app`，API 前缀从 `/api/issues` 改为 `/api/todos`
- 扩展数据模型：增加优先级、截止日期、分组、排序、归档状态
- 新增分组管理、评论功能、统计看板
- 支持拖拽排序
- 前端迁移为独立 Vue 3 + Vite 工程化项目
- 旧数据自动迁移到新 schema
- 支持从待办一键创建 OPSX 变更

**Non-Goals:**
- 不更换后端语言或架构（保持 C + Winsock）
- 不引入关系存储引擎（保持 JSON 文件持久化）
- 不实现用户认证/多用户（保持单用户本地工具）
- 不实现实时推送/WebSocket

## Decisions

1. **后端原地进化 vs 重写**：选择原地进化。现有代码结构清晰，直接增量扩展即可，重写成本高且无必要。
2. **JSON 文件持久化保持不变**：当前 JSON 方案简单可靠，对于单用户工具足够。分组/评论等新数据直接追加到同一 JSON 文件。
3. **前端工程化选型**：Vue 3 + Vite + Vue Router。Vue 3 与现有前端技术栈一致，Vite 是 Vue 生态推荐构建工具，Vue Router 支持多页面路由。
4. **OPSX 桥接方式**：通过 `popen()` 子进程调用 `opsx propose` CLI。最简单的方式，无需引入进程间通信框架。
5. **数据迁移策略**：启动时自动检测旧文件并迁移，无手动步骤，降低用户心智负担。迁移后备份旧文件以防回退。

## Risks / Trade-offs

- **[数据兼容性]** 旧客户端（如 curl 脚本）使用 `/api/issues` 路径 → 不兼容，需手动更新。影响小（内部工具）。
- **[迁移失败]** 旧数据损坏或格式异常 → 迁移失败时回滚旧文件并报错退出，不丢失原始数据。
- **[前端构建]** 新增 Vite 构建步骤 → CMake 中增加 `add_custom_target` 调用 npm build，开发时仍可单独启动 Vite dev server。
- **[OPSX CLI 依赖]** 子进程调用依赖系统安装 opsx CLI → 调用失败时返回明确的错误信息，不阻塞其他功能。