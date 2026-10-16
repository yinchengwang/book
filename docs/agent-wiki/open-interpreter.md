# Open Interpreter Wiki（Codex CLI 编码 Agent）

> 参考版本：`codex-rs`（Open Interpreter 当前主分支，OpenAI Codex CLI 的开源 Rust 重写版本，针对低成本模型优化）。本 wiki 重点介绍 Open Interpreter 作为「**Rust + 高性能 + Coding Agent + 多 LLM Harness 模拟 + Agent 角色 + Tools + Memory + State 迁移**」的本地优先编码 Agent CLI 定位、源码架构、四个 Agent 维度（角色协作 / 记忆 / 工具调用 / 人机协作）的深度展开，以及与本项目 `engineering/` 自研存储与向量索引的对照参考。

## 一、项目定位

Open Interpreter（当前开源仓库）是 **OpenAI Codex CLI 的 fork**，由 Open Interpreter 团队维护，定位是 **「A coding agent optimized for low-cost models」**——以 **Rust** 为主语言（CLI）+ Python（少量），提供 Codex-like 接口但针对 Kimi K3 等低成本模型优化。原始 Open Interpreter（Python 版 `interpreter/`）已被重构为 Rust + Python 混合架构。

- **核心痛点**：
  - OpenAI Codex CLI 锁定 GPT 模型，对**低成本模型**（DeepSeek / Qwen / Kimi）不友好；
  - 缺少**Harness 模拟**（针对不同模型定制 prompt / tools）；
  - 缺少**本地优先**部署（Rust 单文件二进制）；
  - 缺少**Agent 角色**（Agent Identity / Roles）；
  - 缺少**State 迁移**（Goals / Logs / Memory / Queue / Thread History 跨版本迁移）；
  - 缺少**Tools 抽象**（shell / file edit / search / apply patch）；
- **典型用户**：
  - **终端开发者**：使用 CLI 编码 Agent；
  - **低成本模型用户**：DeepSeek / Qwen / Kimi；
  - **Codex 替代品**：开源 + 多模型；
  - **本地部署**：Rust 单二进制；
- **核心卖点**：
  - **Rust 高性能**：CLI 用 Rust 实现（启动快 / 内存小）；
  - **Harness 模拟**：`/harness` 切换不同的 Agent Harness；
  - **多模型支持**：OpenAI / Anthropic / Gemini / DeepSeek / Qwen / Kimi / Ollama / 自部署；
  - **Agent 角色**（Agent Identity / Agent Roles）；
  - **Tools 抽象**：shell / file edit / apply patch / search / web；
  - **Memory**（State 持久化）；
  - **State 迁移**：Goals / Logs / Memory / Queue / Thread History 跨版本迁移；
  - **Codex-like 接口**：相同的命令与 UX；
  - **本机优先**：单 Rust 二进制；
  - **Provider 推荐**：针对低成本模型调优；

## 二、发展历史与社区

- **起源（2023-07）**：Open Interpreter v0.1（Python 原始版本）；
- **里程碑**：
  - 2023-07：Python v0.1（Code Interpreter 简化版）；
  - 2024-03：Python v0.10（OS Mode / Computer Use / Multi-Modal）；
  - 2024-12：Python v0.15（Stable + 大量企业特性）；
  - 2025-08：**重大重构**——团队维护 OpenAI Codex CLI fork 的 Rust 重写版本，Python 版停止维护；
  - 2025-10：v0.20+（Kimi K3 Harness / Codex-like 接口）；
  - 2026-03：v0.30+（Rust 稳定 + 性能优化 + 多模型 Harness）；
- **维护状态**：**活跃**（Open Interpreter 团队 5+ 工程师，月均 30+ commits）；
- **社区**：
  - GitHub 60k+ star（Python 历史峰值）/ Rust 重写后约 1k+ star；
  - Discord 2k+ 成员、GitHub Discussions；
  - 商业产品 Open Interpreter Cloud（历史）；
- **关键贡献者**：Open Interpreter 团队、Kimi 团队、贡献者 50+；

## 三、许可与商业限制

- **核心代码**：**Apache-2.0 License**（`codex-rs/`），可商用、修改、私有部署；
- **生态关系**：
  - **上游依赖**：
    - Rust 1.75+；
    - Python 3.10+（少量 helper）；
    - Tokio（异步运行时）；
    - reqwest（HTTP 客户端）；
    - serde（序列化）；
    - tokio-tungstenite（WebSocket）；
    - tiktoken-rs（Token 计算）；
  - **下游生态**：约 500+ 项目使用 Open Interpreter；

## 四、业界主要使用方与案例

- **国际**（公开）：
  - **多家低成本模型用户**：DeepSeek / Qwen / Kimi；
  - **多家开发者**：Codex CLI 替代品；
- **国内**：
  - 多家开发者使用 Kimi K3；
- **典型场景**：
  - **本地编码 Agent**：替代 Codex CLI；
  - **低成本模型编码**：DeepSeek / Qwen / Kimi 编码；
  - **Codex-like 体验**：相同 UX；
  - **自动化开发**：Harness 模拟；
  - **State 持久化**：跨会话保留历史；

## 五、一句话对比定位

> Open Interpreter 是 **「OpenAI Codex CLI fork + Rust + 低成本模型 + Harness 模拟 + Agent 角色 + State 迁移 + 1k star + Apache-2.0」**；与 **LangGraph（图状态机）**、**CrewAI（角色化 + Process）**、**AutoGen（GroupChat 对话）**、**MetaGPT（软件公司角色）**、**OpenAI Agents（Handoff 简单协议）**、**AgentScope（多 Agent 通用）** 构成多 Agent 生态。Open Interpreter 的差异化是 **「Rust 编码 CLI + 低成本模型 + Harness 模拟 + Codex-like + Apache-2.0」**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                  用户应用层（User Apps）                              │
│        Open Interpreter CLI（Rust 二进制）/ Python Helper             │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  CLI（codex-rs/cli/）                                │
│      clap CLI 解析 / 交互式 REPL / 命令分发                          │
│      install script（macOS / Linux / Windows）                       │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Core（codex-rs/core/）                              │
│      config.schema.json（JSON Schema 配置）                         │
│      Prompt 模板（gpt-5.1-codex / gpt-5.2-codex / gpt_5_1 / gpt_5_2）│
│      Hardening / Tools / Sandbox                                    │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Agent 角色（codex-rs/agent-roles/）                  │
│      agent-roles/src/（Agent 角色抽象）                              │
│      agent-identity/（Agent Identity）                              │
│      agent-graph-store/（Agent 图状态存储）                          │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  State（codex-rs/state/）                            │
│      goals_migrations / logs_migrations / memory_migrations        │
│      thread_history_migrations / queue_migrations / migrations      │
│      State 持久化 + 跨版本迁移                                       │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Tools（codex-rs/tools/）                            │
│      shell / file_edit / apply_patch / search / web / computer_use  │
│      Tools 抽象 + Tools 测试                                          │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Model Provider（codex-rs/model-provider/）          │
│      model-provider-info/（Provider 元信息）                          │
│      models-manager/（模型管理）                                      │
│      multi-provider 路由（OpenAI / Anthropic / Gemini / DeepSeek / Qwen / Kimi / Ollama）│
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Protocol / Transport / Plugins                      │
│      protocol/（Protocol 抽象）/ core-api/                          │
│      core-plugins/（Plugin 扩展）                                      │
│      backend-client/（后端客户端）                                    │
│      app-server/（App Server 协议）                                  │
│      app-server-protocol/（Protocol 定义）                            │
│      app-server-protocol-noop-macros/（Protocol 宏）                │
│      app-server-transport/（Transport）                              │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  工具模块（Tools / Utility）                          │
│      chat-wire-compat/（聊天兼容层）                                  │
│      chatgpt/（ChatGPT 集成）                                          │
│      attachment-store/（附件存储）                                    │
│      apply-patch/（Patch 应用）                                       │
│      arg0/（arg0 处理）                                                │
│      async-utils/（异步工具）                                          │
│      analytics/（分析）                                                │
│      acp-server/（ACP Server）                                       │
│      ansi-escape/（ANSI 转义）                                          │
│      aws-auth/（AWS 认证）                                              │
│      analytics/（分析）                                                │
│      build-info/（构建信息）                                            │
│      bwrap/（bwrap sandbox）                                        │
│      cloud-config/（云配置）                                            │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  SDK / 第三方                                        │
│      sdk/（SDK 集成）/ codex-cli/（CLI Node.js 绑定）                │
│      third_party/（第三方依赖）                                        │
│      docs-site/（文档站点）                                            │
└─────────────────────────────────────────────────────────────────────┘
```

进程模型：Open Interpreter 是**Rust CLI 单二进制**（主进程）+ **Python Helper**（少数）+ **可选 App Server**（HTTP / WebSocket）。

## 七、Agent 类型专项（核心扩展章节）

### 7.1 角色协作模式（Role Collaboration）

Open Interpreter 通过 **Agent Identity + Agent Roles + Harness** 实现四种协作模式：

- **中心化（Orchestrator-Worker）**：
  - **典型实现**：单一 Coding Agent 主导，其他 Worker 通过 Tools 协作；
  - **实现位置**：`codex-rs/agent-roles/`（Agent Roles）；
  - **机制**：通过 Tools 调用外部 Worker；
- **对等（Peer-to-Peer / Blackboard）**：
  - **典型实现**：Agent 共享 Thread History（blackboard pattern）；
  - **实现位置**：`codex-rs/state/thread_history_migrations/`；
  - **共享 State**：所有消息共享；
- **分层（Hierarchical / Supervisor）**：
  - **典型实现**：Harness 切换不同角色（Codex / Kimi）；
  - **实现位置**：`codex-rs/core/`（Harness 实现）；
  - **机制**：通过 `/harness` 命令切换角色；
- **市场（Market-based Bidding）**：
  - **典型实现**：通过 Tools 调用外部服务；
  - **不直接支持**，需用户自定义；
- **Harness 模拟**：
  - **典型实现**：`/harness` 命令切换不同的 Agent Harness；
  - **实现位置**：`codex-rs/core/`（不同 Harness 的 Prompt 模板）；
  - **机制**：为不同模型定制 Prompt / Tools；
  - **典型 Harness**：GPT-5 Codex / Kimi K3 / Claude Sonnet / DeepSeek；

### 7.2 记忆系统（Memory System）

Open Interpreter 记忆分**State + Thread History + Memory + Goals + Queue + Logs**：

- **短期（In-memory conversation buffer）**：
  - **实现位置**：`codex-rs/core/`（当前会话消息）；
  - **API**：自动维护；
- **长期（Thread History）**：
  - **实现位置**：`codex-rs/state/thread_history_migrations/`；
  - **API**：`thread_history.get(thread_id)`；
  - **持久化**：自动持久化到 State；
- **Memory（向量 / 文件）**：
  - **实现位置**：`codex-rs/state/memory_migrations/`；
  - **API**：存储 Memory + 检索；
- **Goals（目标）**：
  - **实现位置**：`codex-rs/state/goals_migrations/`；
  - **API**：存储 Agent 目标；
- **Queue（任务队列）**：
  - **实现位置**：`codex-rs/state/queue_migrations/`；
  - **API**：异步任务队列；
- **Logs（日志）**：
  - **实现位置**：`codex-rs/state/logs_migrations/`；
  - **API**：自动日志；
- **State 迁移**：
  - **实现位置**：`codex-rs/state/migrations/`（迁移框架）；
  - **API**：跨版本自动迁移 State；

### 7.3 工具调用协议（Tool Calling Protocol）

Open Interpreter 通过 **Tools + Function Calling** 实现工具调用：

- **Function Calling（OpenAI / Anthropic / Gemini / DeepSeek / Qwen / Kimi）**：
  - **实现位置**：`codex-rs/model-provider/`（多 Provider）；
  - **支持模型**：OpenAI GPT / Anthropic Claude / Google Gemini / DeepSeek / Qwen / Kimi / Ollama；
  - **API**：通过 Provider 自动路由；
- **Tools 抽象**：
  - **实现位置**：`codex-rs/tools/`（Tools 抽象）；
  - **类型**：shell / file_edit / apply_patch / search / web / computer_use；
  - **API**：`tools.invoke("shell", {"command": "ls"})`；
- **Sandbox（沙箱）**：
  - **实现位置**：`codex-rs/bwrap/`（bwrap sandbox）；
  - **API**：自动 sandbox 工具调用；
- **Tool Hardening**：
  - **实现位置**：`codex-rs/core/`（Tool Hardening）；
  - **机制**：工具安全策略；
- **Apply Patch**：
  - **实现位置**：`codex-rs/apply-patch/`（Patch 应用）；
  - **API**：应用 diff 到文件；
- **Web Search**：
  - **典型实现**：通过 Tools 集成 Tavily / Serper；
- **Computer Use**：
  - **典型实现**：通过 Computer Use Tools；
- **MCP（Model Context Protocol）**：
  - **典型实现**：通过 Tools 集成 MCP Server；
- **Plugin 扩展**：
  - **实现位置**：`codex-rs/core-plugins/`；
  - **API**：用户自定义 Plugin；

### 7.4 人机协作（Human-in-the-Loop）

Open Interpreter HITL 通过 **CLI 交互 / Approval / Interrupt** 实现：

- **CLI Interactive**：
  - **典型实现**：用户可在 CLI 中随时介入；
  - **API**：CLI 提示 `y/n` 让用户决策；
- **Tool Approval**：
  - **典型实现**：危险命令前请求人工审批；
  - **API**：`tools.require_approval(["shell", "file_delete"])`；
- **Interrupt**：
  - **典型实现**：用户在执行中可 `Ctrl+C` 中断；
  - **API**：Tokio 异步中断；
- **Computer Use Approval**：
  - **典型实现**：Computer Use 操作前可请求审批；
- **Apply Patch Approval**：
  - **典型实现**：Patch 应用前显示 diff 请求审批；
- **Sandbox 限制**：
  - **典型实现**：bwrap 沙箱限制文件系统访问；
  - **API**：`sandbox.bwrap(command)`；
- **HUMAN-IN-THE-LOOP Reflection**：
  - **典型实现**：Agent 完成后用户可选择继续 / 重做 / 接受；
  - **API**：CLI 交互；
- **Audit Trail**：
  - **实现位置**：`codex-rs/state/logs_migrations/`（日志）+ analytics；
  - **API**：自动 trace 所有 Agent / Tool 调用；
- **Permission**：
  - **典型实现**：Agent 可请求权限（网络 / 文件 / shell）；
  - **API**：CLI 弹窗授权；

## 八、索引机制（Index / State / Tools）

Open Interpreter 索引通过 **State + Tools + Harness** 实现：

- **State Index**：
  - **实现位置**：`codex-rs/state/`；
  - **机制**：持久化所有状态（Goals / Logs / Memory / Queue / Thread History）；
  - **API**：`state.get(key)`；
- **Tools Index**：
  - **实现位置**：`codex-rs/tools/`；
  - **机制**：Tools 注册；
  - **API**：`tools.register("my_tool", ToolImpl)`；
- **Harness Index**：
  - **实现位置**：`codex-rs/core/`；
  - **机制**：Harness 注册；
  - **API**：`harness.register("kimi_k3", KimiK3Harness)`；
- **Provider Index**：
  - **实现位置**：`codex-rs/model-provider/` + `model-provider-info/`；
  - **机制**：Provider 注册；
  - **API**：`provider.register("openai", OpenAIProvider)`；
- **Plugin Index**：
  - **实现位置**：`codex-rs/core-plugins/`；
  - **机制**：Plugin 注册；
  - **API**：`plugin.register("my_plugin", PluginImpl)`；

## 九、查询/计算/接口层

### 9.1 Open Interpreter 执行引擎（Codex Execution Engine）

- **Core Engine**：
  - **实现位置**：`codex-rs/core/`；
  - **API**：CLI 调用核心 Engine；
  - **机制**：Core Engine 协调 Tools / State / Harness；
- **App Server Protocol**：
  - **实现位置**：`codex-rs/app-server/` + `app-server-protocol/` + `app-server-protocol-noop-macros/`；
  - **API**：HTTP / WebSocket 协议；
  - **场景**：本地 App Server（IDE 集成）；
- **Backend Client**：
  - **实现位置**：`codex-rs/backend-client/`；
  - **API**：客户端调用后端 API；
- **Chat Wire Compatibility**：
  - **实现位置**：`codex-rs/chat-wire-compat/`；
  - **API**：兼容 OpenAI Chat 协议；

### 9.2 状态管理（State / Migrations）

- **State Persistence**：
  - **实现位置**：`codex-rs/state/`；
  - **API**：自动持久化所有状态；
- **State Migrations**：
  - **实现位置**：`codex-rs/state/migrations/`；
  - **API**：跨版本自动迁移 State；
- **Goals Migrations**：
  - **实现位置**：`codex-rs/state/goals_migrations/`；
  - **API**：Goals 状态迁移；
- **Logs Migrations**：
  - **实现位置**：`codex-rs/state/logs_migrations/`；
  - **API**：Logs 状态迁移；
- **Memory Migrations**：
  - **实现位置**：`codex-rs/state/memory_migrations/`；
  - **API**：Memory 状态迁移；
- **Queue Migrations**：
  - **实现位置**：`codex-rs/state/queue_migrations/`；
  - **API**：Queue 状态迁移；
- **Thread History Migrations**：
  - **实现位置**：`codex-rs/state/thread_history_migrations/`；
  - **API**：Thread History 状态迁移；
- **Pause / Resume**：
  - **典型实现**：CLI 中 `Ctrl+C` 暂停；
  - **API**：`state.save()` / `state.load()`；

### 9.3 多模态工具（Multimodal Tools）

- **Code Execution**：
  - **典型实现**：通过 Shell Tool 执行代码；
- **File Operations**：
  - **典型实现**：通过 File Edit Tool；
- **Apply Patch**：
  - **实现位置**：`codex-rs/apply-patch/`；
  - **API**：应用 diff 到文件；
- **Web Search**：
  - **典型实现**：通过 Web Tool；
- **Computer Use**：
  - **典型实现**：通过 Computer Use Tool；
- **Attachment Store**：
  - **实现位置**：`codex-rs/attachment-store/`；
  - **API**：存储附件；
- **Multi-modal**：
  - **典型实现**：支持图像输入（通过 Prompt 模板）；

### 9.4 多 Agent 通信（Multi-Agent Communication）

- **Shared State**：
  - **实现位置**：`codex-rs/state/`；
  - **机制**：所有 Agent 共享 State；
- **Tools 通信**：
  - **典型实现**：通过 Tools 调用外部 Agent / 服务；
- **A2A Protocol**：
  - **通过 App Server Protocol** 实现 A2A；
  - **典型实现**：通过 HTTP / WebSocket 跨进程调用 Agent；

## 十、查询/计算/接口层（API & SDK）

- **Rust API（核心）**：
  - `codex_rs::core::Core`；
  - `codex_rs::tools::Tool`；
  - `codex_rs::model_provider::Provider`；
  - `codex_rs::state::State`；
  - `codex_rs::agent_roles::AgentRole`；
  - `codex_rs::agent_identity::AgentIdentity`；
  - `codex_rs::agent_graph_store::AgentGraphStore`；
  - `codex_rs::app_server::AppServer`；
  - `codex_rs::core_plugins::Plugin`；
- **Python API**（少量）：
  - 辅助 helper；
- **CLI**：
  - `interpreter`（启动 CLI）；
  - `codex`（Codex 命令）；
  - `/harness`（切换 Harness）；
- **App Server**：
  - HTTP / WebSocket 协议；
  - IDE 集成（VS Code / Zed / JetBrains）；
- **Node.js SDK**：
  - **位置**：`codex-cli/`（Node.js CLI 绑定）；
- **第三方 SDK**：
  - **chat-wire-compat**：兼容 OpenAI Chat 协议；
  - **sdk/**：SDK 集成；
- **MCP 集成**：
  - 通过 Tools 集成 MCP Server；

## 十一、生态与集成

- **LLM Provider**：OpenAI GPT（Codex 兼容）/ Anthropic Claude / Google Gemini / DeepSeek / Qwen / Kimi K3 / Ollama / 自部署；
- **Vector Store**：通过 Memory Migrations 集成（可选）；
- **Tool 集成**：shell / file_edit / apply_patch / search / web / computer_use / 自定义 Plugin；
- **Memory Backend**：State 持久化（本地）/ 自定义 Storage；
- **Harness**：Codex / Kimi / 自定义；
- **Observability**：Logs / Analytics / State 持久化；
- **Sandbox**：bwrap（Linux）/ 自定义沙箱；
- **协议**：App Server Protocol（HTTP / WebSocket）；
- **IDE 集成**：VS Code / Zed / JetBrains / 通过 App Server 集成；
- **CLI 集成**：macOS / Linux / Windows（curl 安装）；
- **部署**：Rust 单二进制 / App Server；
- **企业版**：规划中；
- **Harness Marketplace**：社区 Harness 模板；

## 十二、源码学习路径

- **必读模块**（按学习顺序）：
  1. `codex-rs/cli/src/`：CLI 入口；
  2. `codex-rs/core/`：核心 Engine；
  3. `codex-rs/core/config.schema.json`：配置 Schema；
  4. `codex-rs/core/gpt_5_*.md` / `gpt-5.*-codex_prompt.md`：Prompt 模板；
  5. `codex-rs/agent-roles/src/`：Agent 角色；
  6. `codex-rs/agent-identity/src/`：Agent Identity；
  7. `codex-rs/agent-graph-store/src/`：Agent 图状态存储；
  8. `codex-rs/state/src/`：State 抽象；
  9. `codex-rs/state/migrations/`：迁移框架；
  10. `codex-rs/state/goals_migrations/`：Goals 迁移；
  11. `codex-rs/state/logs_migrations/`：Logs 迁移；
  12. `codex-rs/state/memory_migrations/`：Memory 迁移；
  13. `codex-rs/state/queue_migrations/`：Queue 迁移；
  14. `codex-rs/state/thread_history_migrations/`：Thread History 迁移；
  15. `codex-rs/tools/src/`：Tools 抽象；
  16. `codex-rs/tools/README.md`：Tools 文档；
  17. `codex-rs/apply-patch/src/`：Patch 应用；
  18. `codex-rs/attachment-store/src/`：附件存储；
  19. `codex-rs/bwrap/src/`：bwrap 沙箱；
  20. `codex-rs/sandbox/`：Sandbox 抽象；
  21. `codex-rs/model-provider/src/`：模型 Provider；
  22. `codex-rs/model-provider-info/src/`：Provider 元信息；
  23. `codex-rs/models-manager/src/`：模型管理；
  24. `codex-rs/protocol/src/`：Protocol 抽象；
  25. `codex-rs/core-api/src/`：Core API；
  26. `codex-rs/core-plugins/src/`：Plugin 扩展；
  27. `codex-rs/backend-client/src/`：后端客户端；
  28. `codex-rs/app-server/src/`：App Server；
  29. `codex-rs/app-server-protocol/src/`：App Server Protocol；
  30. `codex-rs/app-server-protocol-noop-macros/src/`：Protocol 宏；
  31. `codex-rs/app-server-transport/src/`：App Server Transport；
  32. `codex-rs/chat-wire-compat/src/`：Chat 兼容层；
  33. `codex-rs/chatgpt/src/`：ChatGPT 集成；
  34. `codex-rs/async-utils/src/`：异步工具；
  35. `codex-rs/analytics/src/`：分析；
  36. `codex-rs/aws-auth/src/`：AWS 认证；
  37. `codex-rs/build-info/src/`：构建信息；
  38. `codex-rs/cloud-config/src/`：云配置；
  39. `codex-rs/cli/src/`：CLI 实现；
  40. `codex-cli/bin/`：Node.js CLI；
  41. `sdk/`：SDK 集成；
  42. `docs-site/`：文档站点；
  43. `third_party/`：第三方依赖；
- **LOC 与语言**：
  - `codex-rs/` ~500k LoC（Rust）；
  - `codex-cli/` ~5k LoC（JavaScript）；
  - 主语言：**Rust**（90%）、**TypeScript**（5%，codex-cli）、**Python**（3%，helper）、**Markdown**（2%，Prompt 模板）；
- **构建系统**：
  - **Cargo**（`Cargo.toml`，每个 crate 独立）；
  - **Bazel**（`BUILD.bazel`，多语言构建）；
  - **pnpm**（`pnpm-lock.yaml`，codex-cli Node.js）；
  - **monorepo**（`codex-rs/` 多 crate）；
  - **CI/CD**：GitHub Actions；

## 十三、学习路径

- **入门（1 周）**：
  - 阅读官方文档（https://www.openinterpreter.com/docs/terminal）；
  - 安装 CLI（`curl -fsSL https://www.openinterpreter.com/install | sh`）；
  - 跑通编码示例；
  - 理解 Harness 切换（`/harness`）；
  - 体验 Kimi K3 / Codex 模型；
- **进阶（2-3 周）**：
  - 深入学习 Tools（shell / file_edit / apply_patch / search / web）；
  - 实战自定义 Tool；
  - 学习 State 持久化（Goals / Memory / Queue / Thread History）；
  - 学习 State 迁移（跨版本兼容）；
  - 学习 Harness 实现；
  - 学习 Provider 路由；
- **高级（1-2 月）**：
  - 阅读源码（Core + Tools + State + Harness + Provider）；
  - 自定义 Harness（针对新模型调优）；
  - 自定义 Tool（实现业务命令）；
  - 自定义 Plugin；
  - 自定义 Provider；
  - App Server 二次开发；
  - IDE 集成（VS Code / Zed）；
- **推荐资源**：
  - 官方仓库：https://github.com/openinterpreter/open-interpreter
  - 官方文档：https://www.openinterpreter.com/docs/terminal
  - 博客：https://www.openinterpreter.com/blog
  - Discord：https://discord.gg/openinterpreter
  - Forum：https://github.com/openinterpreter/open-interpreter/discussions
  - Codex 兼容：https://github.com/openai/codex

## 十四、风险与陷阱

- **Rust 重写破坏性升级**：从 Python v0.15 升级到 Rust v0.20+ 几乎重写代码；
- **Python 版停止维护**：Python 版 Open Interpreter 已停止维护。
- **小团队维护**：Open Interpreter 团队人力有限；
- **Harness 调试难**：不同模型的 Harness 行为差异大；
- **Tools 兼容性**：Tools 跨模型兼容性需测试；
- **State 迁移复杂**：State 迁移跨版本需谨慎；
- **bwrap 限制**：bwrap 沙箱仅 Linux，macOS / Windows 需自定义；
- **Provider 兼容**：多 Provider 行为差异大；
- **Kimi 锁定**：商业产品需 Kimi API Key；
- **MCP 不稳定**：MCP 协议还在演进；
- **Plugin 文档少**：Plugin 文档较少；
- **App Server 复杂度**：App Server 协议复杂度高；
- **Performance 问题**：Rust 单二进制性能好，但 Provider 调用是瓶颈；
- **Token 消耗大**：编码 Agent 多步骤调用消耗大量 token；
- **商业化风险**：Open Interpreter Cloud 已停止维护，商业化不确定；
- **Rust 学习曲线**：需 Rust 基础 + async/await 经验；

## 十六、参考资料

- 官方仓库：https://github.com/openinterpreter/open-interpreter
- 官方文档：https://www.openinterpreter.com/docs/terminal
- Kimi Docs：https://www.openinterpreter.com/docs/terminal/kimi-k3
- 博客：https://www.openinterpreter.com/blog
- Tutorial：https://www.openinterpreter.com/docs/terminal
- Discord：https://discord.gg/openinterpreter
- Forum：https://github.com/openinterpreter/open-interpreter/discussions
- YouTube：https://www.youtube.com/@openinterpreter
- Codex（原始）：https://github.com/openai/codex
- Kimi（月之暗面）：https://www.kimi.com/

## 十七、与本项目关联

Open Interpreter 作为「Rust 编码 CLI + Codex-like + 低成本模型 + Harness 模拟」行业代表编码 Agent，与本项目（engineering/ 轨道 C/C++ 自研存储与向量索引）在「Rust 高性能 + State 迁移 + Tools 抽象 + Harness 模拟」四个维度具有直接参考价值：

- **Rust 单二进制 ↔ C/C++ 自研**：Open Interpreter 的 Rust 单二进制（`codex-rs/cli/`）与本项目 C/C++ 自研存储引擎（`engineering/src/db/`）对照——都是"**系统级语言 + 高性能 + 本地优先**"思路。
- **Harness 模拟 ↔ GUC + Prompt 模板**：Harness（`codex-rs/core/` 不同 Prompt 模板）与本项目 GUC（`engineering/src/db/core/guc.c`）的"**配置驱动 + 自动适配**"机制相似。
- **State 迁移 ↔ Schema 迁移**：State 迁移（`codex-rs/state/migrations/`）与本项目 Catalog Schema 迁移（`engineering/src/db/storage/catalog/`）+ WAL 升级的"**跨版本兼容 + 自动迁移**"机制相似。
- **Tools 抽象 ↔ Index AM**：Tools 抽象（`codex-rs/tools/`）与本项目索引 AM（`engineering/src/db/index/`）的"**接口 + 实现分离 + 可插拔**"机制相似。
- **bwrap 沙箱 ↔ 自研沙箱**：bwrap 沙箱（`codex-rs/bwrap/`）与本项目 Code Executor + Sandbox（`engineering/src/db/executor/`）的"**沙箱限制 + 安全执行**"机制相似。
- **Provider 路由 ↔ LiteLLM 集成**：Provider 路由（`codex-rs/model-provider/`）与本项目 LiteLLM 集成（`engineering/src/llm/llm.h`）的"**多模型路由 + 统一接口**"思路相似。
- **Memory Migrations ↔ WAL Buffer**：Memory Migrations（`codex-rs/state/memory_migrations/`）与本项目 WAL Buffer（`engineering/src/db/storage/wal_buf/`）的"**持久化 + 跨版本**"机制相似。
- **Thread History ↔ 历史查询**：Thread History（`codex-rs/state/thread_history_migrations/`）与本项目历史查询（`engineering/src/db/storage/log/`）对照。
- **Goals Migrations ↔ 任务调度**：Goals Migrations（`codex-rs/state/goals_migrations/`）与本项目任务调度（`engineering/src/db/distributed/`）的"**目标 + 调度 + 持久化**"机制相似。
- **App Server Protocol ↔ PG Wire Protocol**：App Server Protocol（`codex-rs/app-server/`）与本项目 PG Wire Protocol（`engineering/src/server/pgwire/`）对照。
- **Attachment Store ↔ Blob Store**：Attachment Store（`codex-rs/attachment-store/`）与本项目 Blob 存储（`engineering/src/db/storage/blob/`）对照。
- **Chat Wire Compatibility ↔ 协议兼容**：Chat Wire Compatibility（`codex-rs/chat-wire-compat/`）与本项目多协议兼容对照。
- **学习建议**：阅读 Open Interpreter 源码重点关注「Rust 高性能 + State 迁移 + Tools 抽象 + Provider 路由」，与本项目 `engineering/src/db/` + `engineering/src/llm/` 做架构对照，加深对「系统级语言 + 跨版本兼容 + 沙箱 + 多模型路由」的理解。
- **重点对照**：本项目 C/C++ 自研（`engineering/src/algo-prod/` + `engineering/src/db/`）与 Open Interpreter Rust 自研对照，是「C/C++ vs Rust 性能 + 工程化」的典型案例。本项目可在"**C 端 + 跨版本 State 迁移 + Rust 工具集成**"方向获得启发。