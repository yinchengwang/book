# Letta Wiki（长期记忆 Agent 框架，原 MemGPT）

> 参考版本：`letta-ai/letta` 主仓（仓库已迁移，源码位于独立仓库 `letta-ai/letta-code` 节点仓库；当前主仓保留 README + 文档 + 配置）。参考 HEAD：b292a33a6 前的最终 release（Letta v0.6+ / v0.7+）。本 wiki 重点介绍 Letta 作为「**长期记忆（Memory Hierarchy / Core + Archival + Recall）+ Stateful Agent + Memory 编辑 + Context Window 分层 + 多渠道（TUI / Desktop / Web）+ Voice Agent + MCP + Channels**」的长期记忆 Agent 框架定位、源码架构、四个 Agent 维度（角色协作 / 记忆 / 工具调用 / 人机协作）的深度展开，以及与本项目 `engineering/` 自研存储与向量索引的对照参考。

## 一、项目定位

Letta（前身 MemGPT）是由 **UC Berkeley Sky Computing Lab**（Sarah Wooders 等）于 **2024 年 11 月**（MemGPT 重命名为 Letta）开源的 **「长期记忆 + Stateful Agent」** 框架，定位是 **「Build stateful agents with memory that can learn and improve over time」**——把 OS 虚拟内存分页（paging）思想引入 LLM 上下文窗口，把"Core Memory（系统内存）+ Archival Memory（磁盘）+ Recall Memory（页缓存）"三层记忆抽象作为 Agent 的持久记忆层。

- **核心痛点**：
  - 缺少**长期记忆**（LLM 默认无跨会话记忆）；
  - 缺少**Memory Hierarchy**（OS 虚拟内存分页思想）；
  - 缺少**Core Memory 编辑**（Agent 自动 / 人工编辑）；
  - 缺少**Archival Memory**（向量库 / 数据库持久化）；
  - 缺少**Recall Memory**（历史对话检索）；
  - 缺少**Context Window 管理**（智能分页进出上下文窗口）；
  - 缺少**Stateful Agent**（可恢复 / 可暂停 / 可恢复）；
  - 缺少**多渠道**（TUI / Desktop App / Web App）；
  - 缺少**Voice Agent**（语音 Agent）；
- **典型用户**：
  - **长期对话应用**：客服 / 虚拟助手；
  - **学习型 Agent**：跨会话学习；
  - **个人助理**：日记 / 笔记 / 计划；
  - **多渠道部署**：TUI / Desktop / Web；
  - **Voice Agent**：语音助手；
- **核心卖点**：
  - **Memory Hierarchy**：Core Memory（系统内存）+ Archival Memory（磁盘）+ Recall Memory（页缓存）；
  - **Stateful Agent**：可恢复 / 可暂停 / 可恢复；
  - **Context Window 分层**：智能分页进出上下文窗口；
  - **Memory 编辑**：Agent 自动 / 人工编辑 Core Memory；
  - **Archival Memory**：向量库 + 关系数据库；
  - **Recall Memory**：历史对话检索；
  - **多渠道**：TUI（`letta-code`）+ Web（`chat.letta.com`）+ Desktop App；
  - **Voice Agent**：实时语音对话；
  - **MCP**：Model Context Protocol 集成；
  - **Channels**：多渠道消息；
  - **App Server**：本地 / 自托管 Server；
  - **MemGPT 思想**：OS 虚拟内存分页类比；
  - **Open Source**：Apache-2.0；
  - **Cloud + Self-Hosted**：Letta Cloud / 自托管；

## 二、发展历史与社区

- **起源（2023-10）**：UC Berkeley Sky Lab 发布 MemGPT 论文（arXiv:2310.08560）；
- **里程碑**：
  - 2023-10：MemGPT 论文发表；
  - 2023-12：MemGPT v0.1（开源）；
  - 2024-06：MemGPT v0.5（Core / Archival / Recall Memory 完善）；
  - 2024-11：**重命名为 Letta** v0.6（公司化 + 商业化）；
  - 2025-03：Letta v0.7（Letta Cloud + Desktop App）；
  - 2025-06：Letta v0.8（Voice Agent + Channels + MCP）；
  - 2025-10：**仓库重构**——核心代码迁移到 `letta-ai/letta-code` 独立仓库；`letta-ai/letta` 主仓保留 README + 文档；
  - 2026-03：Letta v1.0+（稳定 + 企业级 + 跨平台）；
- **维护状态**：**活跃**（Letta Inc. 团队 30+ 工程师 + Sky Lab 学术合作，月均 100+ commits）；
- **社区**：
  - GitHub 18k+ star（合并历史）、`letta` PyPI 月下载量 30 万+；
  - Discord 3k+ 成员、Forum、Cookbook；
  - 学术论文：MemGPT: Towards LLMs as Operating Systems（arXiv）；
  - 学术合作：UC Berkeley Sky Lab；
- **关键贡献者**：Sarah Wooders（创始人）、UC Berkeley Sky Lab、Letta Inc. 团队、贡献者 200+；

## 三、许可与商业限制

- **核心代码**：**Apache-2.0 License**（`letta-code/`、`letta/` 主仓），可商用、修改、私有部署；
- **Letta Cloud**：商业 SaaS（`chat.letta.com`）；
- **生态关系**：
  - **上游依赖**：
    - Python 3.10+（后端）；
    - TypeScript（前端或桌面 App）；
    - FastAPI / Flask（App Server）；
    - PostgreSQL（持久化）；
    - OpenAI / Anthropic / Azure / Gemini / Ollama；
    - tiktoken；
    - pgvector（Archival Memory）；
    - MCP（`mcp_client.py`）；
    - Voice Pipeline（TTS + STT）；
  - **下游生态**：约 5000+ 项目使用 Letta / MemGPT；

## 四、业界主要使用方与案例

- **国际**（公开）：
  - **多家初创公司**：长期对话应用；
  - **多家学术机构**：MemGPT 论文复现；
- **国内**：
  - 多家互联网公司长期对话应用；
  - 多家企业内部虚拟助理；
- **典型场景**：
  - **长期对话**：客服 / 虚拟助手；
  - **学习型 Agent**：跨会话学习；
  - **个人助理**：日记 / 笔记 / 计划；
  - **多渠道部署**：TUI / Desktop / Web；
  - **Voice Agent**：语音助手；
  - **学术研究**：MemGPT 论文复现；
  - **生产部署**：Letta Cloud + Self-Hosted；

## 五、一句话对比定位

> Letta 是 **「UC Berkeley Sky Lab 出品 + MemGPT 重命名 + 长期记忆（Memory Hierarchy）+ Stateful Agent + Context Window 分层 + 多渠道（TUI / Desktop / Web）+ Voice Agent + 18k star + Apache-2.0」**；与 **LangGraph（图状态机）**、**CrewAI（角色化 + Process）**、**AutoGen（GroupChat 对话）**、**MetaGPT（软件公司角色）**、**OpenAI Agents（Handoff 简单协议）**、**AgentScope（多 Agent 通用）** 构成多 Agent 生态。Letta 的差异化是 **「长期记忆 + Memory Hierarchy + Stateful + Context Window 分层 + OS 虚拟内存类比 + UC Berkeley 学术底座 + 多渠道部署」**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                  用户应用层（User Apps）                              │
│        chat.letta.com（Web） / Desktop App / TUI（letta-code）       │
│        Letta Cloud / Self-Hosted App Server                         │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Frontend（React SPA / Desktop）                     │
│      Web App / Desktop App / Mobile（规划）                          │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  App Server（FastAPI / Flask）                       │
│      REST API / WebSocket / Auth Middleware / SSO                   │
│      Tools / Agents / Channels                                       │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Agent / Memory / Tools                              │
│   agents/                                                            │
│     ├── voice_agent.py（Voice Agent）                                │
│     └── agents.py（Agent 抽象）                                       │
│   memory/                                                            │
│     ├── core_memory.py（Core Memory / 系统内存）                     │
│     ├── archival_memory.py（Archival Memory / 磁盘）                 │
│     ├── recall_memory.py（Recall Memory / 页缓存）                   │
│     └── memory_hierarchy.py（Memory Hierarchy 抽象）                 │
│   tools/                                                              │
│     ├── tool_base.py                            │
│     ├── memory_edit_tools.py（编辑 Core Memory）                     │
│     ├── archival_tools.py（操作 Archival）       │
│     ├── recall_tools.py（检索 Recall）           │
│     └── function_calling.py                                  │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  LLM / Provider / MCP                                │
│   llm/                                                               │
│     ├── openai.py / anthropic.py / gemini.py / ollama.py           │
│   mcp_client.py（Model Context Protocol 集成）                       │
│   context_window_manager.py（Context Window 分层管理）              │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Channels / Voice / Multimodal                       │
│   channels/（多渠道消息）                                             │
│   voice/（语音处理 / TTS + STT）                                       │
│   multimodal/（多模态）                                                │
│   realtime/（实时）                                                    │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Storage / Database                                   │
│   PostgreSQL（持久化）                                                  │
│   pgvector（向量库 / Archival Memory）                                 │
│   SQLite（本地 / Desktop App）                                         │
│   In-Memory Cache（Recall Memory）                                    │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  外部依赖（External Dependencies）                   │
│   OpenAI / Anthropic / Azure / Gemini / Ollama / tiktoken          │
│   FastAPI / Flask / SQLAlchemy / Pydantic / asyncio                │
│   MCP / Voice Pipeline / pgvector                                  │
└─────────────────────────────────────────────────────────────────────┘
```

进程模型：Letta 是**Python App Server**（`letta-code/` 后端）+ **Node.js TUI**（`letta-code/` 前端）+ **Desktop App**（Electron / Tauri）+ **Web App**（`chat.letta.com`）四层架构；通过 PostgreSQL 持久化所有状态。

## 七、Agent 类型专项（核心扩展章节）

### 7.1 角色协作模式（Role Collaboration）

Letta 通过 **Agent + Memory + Channels** 实现四种协作模式：

- **中心化（Orchestrator-Worker）**：
  - **典型实现**：通过 Multi-Agent Orchestration；
  - **实现位置**：`letta-code/agents/`（Agent 注册）+ Memory Hierarchy；
  - **API**：`orchestrator.call(worker);`；
- **对等（Peer-to-Peer / Blackboard）**：
  - **典型实现**：所有 Agent 共享 PostgreSQL（blackboard pattern）；
  - **机制**：通过 DB 共享状态；
- **分层（Hierarchical / Supervisor）**：
  - **典型实现**：嵌套 Agent（Agent 内调用其他 Agent）；
  - **API**：`agent_a.call(agent_b);`；
- **市场（Market-based Bidding）**：
  - **典型实现**：通过 Channels 实现 Bidding；
  - **不直接支持**，需用户自定义；
- **Channels 协作**：
  - **典型实现**：Channels（`channels/`）实现多渠道协作；
  - **机制**：Agent 通过 Channel 发送 / 接收消息；
  - **典型 Channel**：Email / Slack / Discord / Webhook；

### 7.2 记忆系统（Memory System，核心扩展章节）

Letta 记忆系统是**核心卖点**，分**Core Memory + Archival Memory + Recall Memory**：

- **Core Memory（系统内存）**：
  - **实现位置**：`memory/core_memory.py`；
  - **语义**：类比 OS 虚拟内存，**始终在上下文窗口**；
  - **存储**：结构化字段（Persona / Human 等）；
  - **典型字段**：`persona` / `human` / `system`；
  - **API**：`agent.core_memory.append("persona", "I am a helpful Assistant")`；
  - **机制**：Agent 通过 Tool 自动编辑 Core Memory；
- **Archival Memory（磁盘）**：
  - **实现位置**：`memory/archival_memory.py`；
  - **语义**：类比 OS 磁盘，**长期持久化**；
  - **存储**：pgvector / SQLite / PostgreSQL；
  - **API**：`agent.archival_memory.insert(text); results = agent.archival_memory.search(query)`；
  - **机制**：通过向量检索访问；
- **Recall Memory（页缓存）**：
  - **实现位置**：`memory/recall_memory.py`；
  - **语义**：类比 OS 页缓存，**最近对话历史**；
  - **存储**：PostgreSQL + In-Memory Cache；
  - **API**：`agent.recall_memory.search(query)`；
  - **机制**：通过时间窗口 + 关键词检索；
- **Memory Hierarchy（记忆层次）**：
  - **实现位置**：`memory/memory_hierarchy.py`；
  - **机制**：三层抽象（Core + Archival + Recall）；
  - **典型场景**：Core 频繁访问 / Archival 偶尔访问 / Recall 按需访问；
- **Context Window 分层**：
  - **实现位置**：`context_window_manager.py`；
  - **机制**：智能分页进出上下文窗口；
  - **策略**：Core 永远在窗口 + Recall 按相关性分页 + Archival 按需分页；
- **Memory 编辑**：
  - **典型实现**：Agent 通过 Tool 编辑 Core Memory；
  - **实现位置**：`tools/memory_edit_tools.py`（编辑 Core Memory）；
  - **API**：`memory_replace` / `memory_insert` / `memory_delete`；
- **Memory 压缩**：
  - **典型实现**：在 Context Window 接近上限时压缩 Core Memory；
  - **API**：自动压缩；
- **Memory 检索**：
  - **向量检索**：Archival Memory 通过 pgvector 实现；
  - **关键词检索**：Recall Memory 通过 PostgreSQL FTS；
  - **时间检索**：Recall Memory 按时间窗口；
- **跨会话记忆**：
  - **实现位置**：PostgreSQL 持久化所有 Memory；
  - **API**：自动跨会话保留；
- **Voice Memory**：
  - **实现位置**：`agents/voice_agent.py`（Voice Agent）；
  - **机制**：Voice Agent 也使用三层 Memory Hierarchy；

### 7.3 工具调用协议（Tool Calling Protocol）

Letta 通过 **Tool + Memory Edit Tool + Function Calling** 实现工具调用：

- **Function Calling（OpenAI / Anthropic / Gemini / Ollama）**：
  - **实现位置**：`llm/openai.py` / `llm/anthropic.py` / `llm/gemini.py` / `llm/ollama.py`；
  - **支持模型**：OpenAI GPT / Anthropic Claude / Google Gemini / Ollama / 自部署；
  - **API**：`agent = create_agent(model="gpt-4o", tools=[...])`；
- **Memory Edit Tool（核心特色）**：
  - **典型实现**：通过 `memory_replace` / `memory_insert` / `memory_delete` Tool 编辑 Core Memory；
  - **实现位置**：`tools/memory_edit_tools.py`；
  - **API**：`memory_replace(persona_block, "old", "new")`；
- **Archival Tool**：
  - **典型实现**：通过 `archival_memory_insert` / `archival_memory_search` Tool；
  - **实现位置**：`tools/archival_tools.py`；
  - **API**：`archival_memory_insert(text)` / `archival_memory_search(query, k=5)`；
- **Recall Tool**：
  - **典型实现**：通过 `conversation_search` Tool；
  - **实现位置**：`tools/recall_tools.py`；
  - **API**：`conversation_search(query, start_date=..., end_date=...)`；
- **ReAct（Reasoning + Acting）**：
  - **典型实现**：Agent 默认使用 ReAct 循环；
  - **机制**：每轮 Agent 生成 Thought + Action（Tool 调用）→ 观察结果 → 下一轮；
- **CodeAct（生成代码执行）**：
  - **典型实现**：通过 Custom Tool + Python REPL；
  - **沙箱**：本地 / Docker（生产推荐）；
- **Tool Use + Reflection**：
  - **典型实现**：通过 Memory Edit Tool 反思 Tool 输出；
- **Structured Outputs**：
  - **实现位置**：`tools/function_calling.py`（Function Schema）；
  - **API**：通过 Pydantic 模型定义；
- **MCP（Model Context Protocol）**：
  - **实现位置**：`mcp_client.py`；
  - **API**：`MCPClient(server="...")`；
- **Custom Tool**：
  - **API**：`tools.register("my_tool", MyTool)`；
- **Tool Error Handling**：
  - **API**：Tool 内部 try/except；

### 7.4 人机协作（Human-in-the-Loop）

Letta HITL 通过 **Memory Edit / Channels / Approval / Web UI** 实现：

- **Memory Edit Tool（核心特色）**：
  - **典型实现**：用户可通过 Web UI 编辑 Core Memory；
  - **实现位置**：`chat.letta.com` Web App + Desktop App；
  - **API**：在 UI 中编辑 `persona` / `human` 字段；
- **Channels HITL**：
  - **典型实现**：通过 Email / Slack 接收人工输入；
  - **API**：Channel 监听外部事件；
- **Approval Workflow**：
  - **典型实现**：自定义 Tool 调用 `input()`；
  - **API**：`tool.require_approval = True`；
- **TUI HITL**：
  - **典型实现**：`letta-code` TUI 中随时介入；
  - **API**：TUI 提示人工决策；
- **Web / Desktop HITL**：
  - **典型实现**：通过 Web / Desktop App 中断 / 批准 / 修改；
  - **API**：`chat.letta.com` 内置 HITL UI；
- **Voice HITL**：
  - **典型实现**：通过 Voice Agent 实时人工介入；
- **Stateful Interrupt**：
  - **典型实现**：Agent 可暂停 / 恢复；
  - **API**：`agent.pause()` / `agent.resume()`；
- **Audit Trail**：
  - **实现位置**：PostgreSQL 持久化所有操作；
  - **API**：自动 trace 所有 Agent / Tool / Memory 操作；
- **Permission**：
  - **典型实现**：通过 Channel 权限控制；

## 八、索引机制（Index / Memory Hierarchy）

Letta 索引通过 **Memory Hierarchy + Channel + Archival + Recall** 实现：

- **Memory Hierarchy Index**：
  - **实现位置**：`memory/memory_hierarchy.py`；
  - **机制**：三层抽象（Core + Archival + Recall）；
- **Core Memory Index**：
  - **实现位置**：`memory/core_memory.py`；
  - **机制**：结构化字段索引（Persona / Human）；
- **Archival Memory Index**：
  - **实现位置**：`memory/archival_memory.py`；
  - **机制**：pgvector / SQLite / PostgreSQL 向量索引；
- **Recall Memory Index**：
  - **实现位置**：`memory/recall_memory.py`；
  - **机制**：PostgreSQL FTS 全文搜索；
- **Context Window Index**：
  - **实现位置**：`context_window_manager.py`；
  - **机制**：智能分页进出上下文窗口；
- **Channel Index**：
  - **实现位置**：`channels/`；
  - **API**：`channel_registry.register("email", EmailChannel)`；

## 九、查询/计算/接口层

### 9.1 Agent 执行引擎（Agent Execution Engine）

- **Agent Loop**：
  - **API**：`agent.send_message("Hello")`；
  - **机制**：Agent 通过 ReAct 循环 + Tool 调用 + Memory 编辑；
- **Stateful**：
  - **API**：`agent.state()` 返回当前状态；
- **Pause / Resume**：
  - **API**：`agent.pause()` / `agent.resume()`；
- **Voice Agent**：
  - **实现位置**：`agents/voice_agent.py`；
  - **API**：`voice_agent.speak(text)`；
- **Channels**：
  - **实现位置**：`channels/`；
  - **API**：`channel.send(message)`；

### 9.2 状态管理（State / Memory）

- **Agent State**：
  - **实现位置**：PostgreSQL 持久化；
  - **API**：`agent.state()`；
- **Memory State**：
  - **实现位置**：`memory/`；
  - **API**：`agent.core_memory.state()`；
- **Persistence**：
  - **实现位置**：PostgreSQL（生产）/ SQLite（本地）；
  - **API**：自动持久化；
- **State Migration**：
  - **API**：通过 Alembic 迁移 Schema；
- **Backup**：
  - **API**：PostgreSQL 自动备份；

### 9.3 多模态工具（Multimodal Tools）

- **Voice**：
  - **实现位置**：`voice/`（TTS + STT）；
  - **API**：`voice.speak(text)` / `voice.listen()`；
- **Vision**：
  - **典型实现**：通过 Model Client 集成 GPT-4V / Claude 3 / Gemini Vision；
- **Video**：
  - **典型实现**：通过 Model Client 集成视频分析；
- **RAG Tool**：
  - **典型实现**：通过 Archival Memory；
- **Browser Tool**：
  - **典型实现**：通过 Custom Tool + Playwright；
- **Code Executor**：
  - **典型实现**：通过 Custom Tool + Python REPL；
- **Database Tool**：
  - **典型实现**：通过 Custom Tool + SQLAlchemy；
- **Search Tool**：
  - **典型实现**：通过 Custom Tool + Tavily / Serper；

### 9.4 多 Agent 通信（Multi-Agent Communication）

- **Shared PostgreSQL**：
  - **机制**：所有 Agent 共享 PostgreSQL；
- **Channels 协作**：
  - **实现位置**：`channels/`；
  - **机制**：通过 Channel 消息协作；
- **Multi-Agent Orchestration**：
  - **API**：自定义 Multi-Agent 编排；
- **A2A Protocol**：
  - **Letta 通过 MCP + HTTP** 实现 A2A；
  - **典型实现**：通过 App Server 跨进程调用 Agent；

## 十、查询/计算/接口层（API & SDK）

- **Python API（核心）**：
  - `letta.Agent`（`letta-code/agents/agents.py`）；
  - `letta.VoiceAgent`（`agents/voice_agent.py`）；
  - `letta.MemoryHierarchy`（`memory/memory_hierarchy.py`）；
  - `letta.CoreMemory`（`memory/core_memory.py`）；
  - `letta.ArchivalMemory`（`memory/archival_memory.py`）；
  - `letta.RecallMemory`（`memory/recall_memory.py`）；
  - `letta.Tool`（`tools/tool_base.py`）；
  - `letta.MemoryEditTool`（`tools/memory_edit_tools.py`）；
  - `letta.ArchivalTool`（`tools/archival_tools.py`）；
  - `letta.RecallTool`（`tools/recall_tools.py`）；
  - `letta.FunctionCalling`（`tools/function_calling.py`）；
  - `letta.LLM`（`llm/`）；
  - `letta.ContextWindowManager`（`context_window_manager.py`）；
  - `letta.Channel`（`channels/`）；
  - `letta.MCPClient`（`mcp_client.py`）；
- **Node.js TUI**：
  - `npm install -g @letta-ai/letta-code`；
  - `letta`（启动 TUI）；
  - `letta server`（启动 App Server）；
- **Web App**：
  - `chat.letta.com`（托管 Web App）；
- **Desktop App**：
  - macOS / Windows / Linux（Electron / Tauri）；
- **App Server REST API**：
  - `POST /api/v1/agents`（创建 Agent）；
  - `GET /api/v1/agents/{agent_id}`（获取 Agent）；
  - `POST /api/v1/agents/{agent_id}/messages`（发送消息）；
  - `POST /api/v1/agents/{agent_id}/core-memory`（编辑 Core Memory）；
  - `POST /api/v1/agents/{agent_id}/archival-memory`（操作 Archival）；
  - `GET /api/v1/agents/{agent_id}/recall-memory/search`（检索 Recall）；
- **Voice Pipeline**：
  - **API**：`voice.synthesize(text)` / `voice.transcribe(audio)`；
- **第三方 SDK**：
  - **OpenAI Python SDK**；
  - **Anthropic Python SDK**；
  - **Letta JS SDK**（前端集成）；
- **MCP 集成**：
  - 通过 `mcp_client.py` 集成 MCP Server；

## 十一、生态与集成

- **LLM 集成**：OpenAI GPT / Anthropic Claude / Google Gemini / Azure OpenAI / Ollama / 自部署；
- **Vector Store**：pgvector（默认）/ Chroma / Qdrant；
- **Tool 集成**：Memory Edit Tool + Archival Tool + Recall Tool + Custom Tool + MCP Server；
- **Memory Backend**：PostgreSQL（生产）/ SQLite（本地）；
- **Channels**：Email / Slack / Discord / Webhook / Custom；
- **Voice**：OpenAI TTS / Whisper / 阿里 TTS / ElevenLabs；
- **Multimodal**：GPT-4V / Claude 3 / Gemini Vision；
- **MCP**：Model Context Protocol 集成；
- **Observability**：PostgreSQL 日志 + Audit Trail；
- **部署**：Letta Cloud（商业托管）/ Self-Hosted（Docker / Kubernetes）+ Desktop App + TUI；
- **企业版**：Letta Cloud Enterprise + SSO + RBAC + Audit + Compliance；
- **学术合作**：UC Berkeley Sky Lab；
- **中文社区**：Letta Inc. + 中文用户；
- **Skills Marketplace**：预制 Skills（搜索 / 摘要 / 翻译 / 校对）；
- **Agent Marketplace**：社区 Agent 模板；

## 十二、源码学习路径

- **必读模块**（按学习顺序）：
  1. `README.md`：项目入口与文档；
  2. `AGENTS.md`：Agent 文档；
  3. `CONTRIBUTING.md`：贡献文档；
  4. `AI_POLICY.md`：AI 政策；
  5. `LICENSE`：许可；
  - **主仓库当前主要保留 README + 文档，源码在独立仓库 `letta-ai/letta-code`（节点仓库）；** 本 wiki 后续路径以 `letta-code/` 为参考：
  1. `letta-code/agents/agents.py`：Agent 抽象；
  2. `letta-code/agents/voice_agent.py`：Voice Agent；
  3. `letta-code/memory/core_memory.py`：Core Memory；
  4. `letta-code/memory/archival_memory.py`：Archival Memory；
  5. `letta-code/memory/recall_memory.py`：Recall Memory；
  6. `letta-code/memory/memory_hierarchy.py`：Memory Hierarchy；
  7. `letta-code/tools/tool_base.py`：Tool 基类；
  8. `letta-code/tools/memory_edit_tools.py`：Memory Edit Tool；
  9. `letta-code/tools/archival_tools.py`：Archival Tool；
  10. `letta-code/tools/recall_tools.py`：Recall Tool；
  11. `letta-code/tools/function_calling.py`：Function Calling；
  12. `letta-code/llm/openai.py`：OpenAI LLM；
  13. `letta-code/llm/anthropic.py`：Anthropic LLM；
  14. `letta-code/llm/gemini.py`：Gemini LLM；
  15. `letta-code/llm/ollama.py`：Ollama LLM；
  16. `letta-code/context_window_manager.py`：Context Window 管理；
  17. `letta-code/channels/`：Channels；
  18. `letta-code/voice/`：Voice Pipeline；
  19. `letta-code/multimodal/`：Multimodal；
  20. `letta-code/realtime/`：Realtime；
  21. `letta-code/mcp_client.py`：MCP Client；
  22. `letta-code/storage/`：Storage；
  23. `letta-code/server/`：App Server；
  24. `letta-code/tui/`：TUI；
  25. `letta-code/desktop/`：Desktop App；
  26. `letta-code/web/`：Web App；
- **LOC 与语言**：
  - `letta-code/` ~80k LoC；
  - 主语言：**Python**（后端 80%）、**TypeScript**（前端 / TUI 15%）、**Rust**（Desktop 部分）；
- **构建系统**：
  - **pyproject.toml**（Python）；
  - **package.json**（TypeScript / TUI）；
  - **Cargo.toml**（Rust Desktop）；
  - **monorepo**；
  - **CI/CD**：GitHub Actions；
  - **Docker / docker-compose**：部署；
  - **npm publish**：`@letta-ai/letta-code` TUI；

## 十三、学习路径

- **入门（1 周）**：
  - 阅读官方文档（https://docs.letta.com/）；
  - 阅读 MemGPT 论文（arXiv:2310.08560）；
  - 跑通 `npm install -g @letta-ai/letta-code`；
  - 启动 `letta server` + `letta` TUI；
  - 访问 `chat.letta.com`（托管 Web）；
  - 体验 Core Memory / Archival Memory / Recall Memory；
  - 编辑 Core Memory（Persona / Human）；
- **进阶（2-3 周）**：
  - 深入学习 Memory Hierarchy（Core + Archival + Recall）；
  - 实战 Memory Edit Tool；
  - 实战 Archival Operations（insert / search）；
  - 实战 Recall Operations（search / 时间窗口）；
  - 学习 Context Window 分层管理；
  - 学习 Channels（Email / Slack / Webhook）；
  - 学习 Voice Agent；
  - 学习 App Server（自托管）；
  - 学习 PostgreSQL 配置；
- **高级（1-2 月）**：
  - 阅读源码（Agent + Memory + Tool + Context Window Manager）；
  - 自定义 Memory Tool（实现新 Memory 操作）；
  - 自定义 Channel（实现新渠道）；
  - 自定义 Voice Pipeline（实现 TTS + STT）；
  - 自定义 App Server（自托管部署）；
  - Desktop App 二次开发；
  - Web App 二次开发；
  - TUI 二次开发；
  - 企业部署（SSO / RBAC / Audit）；
- **推荐资源**：
  - 官方仓库：https://github.com/letta-ai/letta
  - 官方仓库（源码）：https://github.com/letta-ai/letta-code
  - 官方文档：https://docs.letta.com/
  - MemGPT 论文：https://arxiv.org/abs/2310.08560
  - Web App：https://chat.letta.com/
  - TUI：https://www.npmjs.com/package/@letta-ai/letta-code
  - 博客：https://blog.letta.com/
  - Discord：https://discord.gg/letta
  - Forum：https://github.com/letta-ai/letta/discussions
  - 学术合作：UC Berkeley Sky Lab

## 十四、风险与陷阱

- **仓库迁移**：源码从主仓库迁移到独立 `letta-code` 节点仓库，文档可能过时；
- **学习曲线陡**：Memory Hierarchy 概念多，新人需 1-2 周入门；
- **Memory 压缩策略难调**：Context Window 分层策略需根据场景调优；
- **Memory 编辑冲突**：Agent 自动编辑 + 人工编辑可能冲突；
- **Archival Memory 性能**：大规模 Archival Memory 检索性能下降；
- **Recall Memory 一致性**：跨会话 Recall Memory 一致性难保证；
- **Core Memory 容量限制**：Core Memory 有容量上限（context window 限制）；
- **Voice Pipeline 延迟**：实时语音对话延迟较高；
- **MCP 不稳定**：MCP 协议还在演进；
- **App Server 复杂度**：自托管 App Server 配置复杂；
- **Letta Cloud 锁定**：使用 Cloud 后切换到自部署成本高；
- **PostgreSQL 依赖**：本地需要 PostgreSQL；
- **pgvector 索引**：大规模 Archival Memory 需 pgvector 调优；
- **Tool 错误处理**：Tool 内部需 try/except；
- **Channel 适配**：每渠道需自定义 Channel；
- **Stateful 持久化成本**：长期对话 PostgreSQL 存储成本；
- **Performance 问题**：Python 调度开销大；
- **商业化风险**：Letta Cloud Enterprise 价格高；

## 十六、参考资料

- 官方仓库：https://github.com/letta-ai/letta
- 官方仓库（源码）：https://github.com/letta-ai/letta-code
- 官方文档：https://docs.letta.com/
- MemGPT 论文：https://arxiv.org/abs/2310.08560
- Web App：https://chat.letta.com/
- TUI：https://www.npmjs.com/package/@letta-ai/letta-code
- Desktop App：https://docs.letta.com/letta-code/desktop-app
- 论文：MemGPT: Towards LLMs as Operating Systems
- 论文：Letta: A Framework for Stateful Agents
- 博客：https://blog.letta.com/
- Tutorial：https://docs.letta.com/quickstart/
- Discord：https://discord.gg/letta
- Forum：https://github.com/letta-ai/letta/discussions
- YouTube：https://www.youtube.com/@LettaAI
- 学术合作：UC Berkeley Sky Lab

## 十七、与本项目关联

Letta 作为「长期记忆 + Memory Hierarchy + Stateful + Context Window 分层 + 多渠道部署」行业代表长期记忆 Agent 框架，与本项目（engineering/ 轨道 C/C++ 自研存储与向量索引）在「Memory Hierarchy 三层抽象 + Core/Archival/Recall + Context Window 管理 + 长期持久化」四个维度具有直接参考价值：

- **Memory Hierarchy ↔ 多层存储**：Memory Hierarchy（Core + Archival + Recall）与本项目多层存储（`engineering/src/db/storage/` Hot 热 / Cold 冷 / Blob 二元大对象 / mmview 多模态视图 / `mm_storage.c`）的"**多层抽象 + 按需分页**"思路高度相似——都是"**Hot 频访问 + Cold 偶访问 + Cache 按需访问**"。
- **Core Memory ↔ Buffer Pool**：Core Memory（始终在上下文窗口）类比 OS 系统内存，与本项目 Buffer Pool（`engineering/src/db/storage/buffer/`）的"**热访问 + LRU + Clock-Sweep**"机制相似——都是"**频访问 + 容量受限 + 自动置换**"思路。
- **Archival Memory ↔ Blob 存储**：Archival Memory（磁盘持久化）类比 OS 磁盘，与本项目 Blob 存储（`engineering/src/db/storage/blob/`）的"**冷访问 + 大容量 + 持久化**"机制相似。
- **Recall Memory ↔ WAL + Buffer**：Recall Memory（页缓存）类比 OS 页缓存，与本项目 WAL + Buffer Pool（`engineering/src/db/storage/wal_buf/`）的"**中频访问 + 时间窗口 + 检索**"机制相似。
- **Context Window 分层管理 ↔ 内存分页**：Context Window Manager（`ctx_window_manager.py`）的智能分页进出与本项目 OS 虚拟内存分页的"**按需分页 + LRU 置换**"思路完全一致。
- **Stateful Agent ↔ 事务**：Stateful Agent（可恢复 / 可暂停 / 可恢复）与本项目 PG 风格事务（`engineering/src/db/executor/transaction.c`）+ Checkpoint（`engineering/src/db/storage/`）的"**ACID + 可恢复**"机制相似。
- **Memory Edit Tool ↔ Index AM**：Memory Edit Tool（`tools/memory_edit_tools.py`）与本项目索引 AM（`engineering/src/db/index/`）的"**声明式 + 可编辑 + 可索引**"机制相似。
- **Archival Tool ↔ 向量索引**：Archival Tool（`tools/archival_tools.py`）与本项目 HNSW / IVF / PQ 向量索引对照，本项目 wiki `docs/db-wiki/vector/` 已系统梳理。
- **Recall Tool ↔ WAL + 历史查询**：Recall Tool（`tools/recall_tools.py`）与本项目 WAL 历史查询（`engineering/src/db/storage/log/`）对照。
- **PostgreSQL + pgvector ↔ 多模态存储**：PostgreSQL + pgvector 与本项目 PG 风格 Catalog（`engineering/src/db/storage/catalog/`）+ 向量引擎（`engineering/src/db/storage/vector/vector_engine.c`）的"**PG 兼容 + 向量检索**"思路相似。
- **Channels ↔ DB 协议**：Channels（`channels/`）与本项目 PG Wire Protocol（`engineering/src/server/pgwire/`）对照——都是"**多渠道 + 协议适配**"思路。
- **Voice Pipeline ↔ 多模态 Pipeline**：Voice Pipeline（`voice/`）与本项目多模态 RAG 应用（`engineering/apps/multimodal_rag/`）的"**多模态 + 流水线 + 实时**"思路相似。
- **MCP ↔ DB 协议**：MCP（`mcp_client.py`）与本项目 PG Wire Protocol 对照。
- **TUI / Web / Desktop ↔ 自研 Studio**：TUI / Web / Desktop 与本项目自研 Studio 对照。
- **Letta Cloud ↔ 自研云**：Letta Cloud 与本项目自研云（`engineering/src/db/core/db_server.c`）对照。
- **学习建议**：阅读 Letta 源码重点关注「Memory Hierarchy 三层抽象 + Context Window 智能分页 + Stateful Agent + Memory Edit Tool」，与本项目 `engineering/src/db/storage/` + `engineering/src/db/storage/buffer/` 做架构对照，加深对「多层存储 + 智能分页 + 持久化」的理解。
- **重点对照**：本项目 C/C++ 自研（`engineering/src/algo-prod/` + `engineering/src/db/`）与 Letta 的 Python 实现对照，是「C/C++ vs Python + PG 风格 vs pgvector + Buffer Pool vs Memory Hierarchy」的典型案例。本项目可在"**多层存储 + Context Window 分层 + Memory Edit 工具**"方向获得启发，建议本项目 `engineering/src/db/storage/mm/mm_storage.c` 参考 Letta "**Core/Archival/Recall 三层**" 设计模式来强化多模态存储抽象。