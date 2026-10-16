# Microsoft 365 Agents SDK Wiki（Microsoft 官方多渠道 Agent 框架）

> 参考版本：`Microsoft 365 Agents SDK`（`agents/` 主仓 + 语言特定子仓 `agents-for-net` / `agents-for-js` / `agents-for-python`），参考 HEAD：b292a33a6 前的最终 release（Microsoft 365 Agents SDK v1.0+）。本 wiki 重点介绍 Microsoft 365 Agents SDK 作为「**M365 渠道接入（Copilot / Teams / Web / Custom）+ Agent Framework（Agent / Session / Compaction / History）+ Activity Protocol + Skills + Authentication（MSAL / OBO / Entra）+ Memory Storage + State Management + Microsoft 365 Copilot 集成**」的多渠道企业 Agent SDK 定位、源码架构、四个 Agent 维度（角色协作 / 记忆 / 工具调用 / 人机协作）的深度展开，以及与本项目 `engineering/` 自研存储与向量索引的对照参考。

## 一、项目定位

Microsoft 365 Agents SDK 是 **Microsoft** 于 **2025 年**正式开源的 **「企业级多渠道 Agent SDK」**，定位是 **「Microsoft 365 Agents SDK for building agents deployable to channels of Microsoft 365 Copilot, Microsoft Teams, Web & Custom Apps」**——以 **Channel 适配 + Activity Protocol + Skills + Authentication + Copilot Integration** 为核心，把 Agent 部署到 Microsoft 365 全生态（Copilot / Teams / Web / Custom）。

- **核心痛点**：
  - 缺少**Microsoft 365 全生态接入**（Copilot / Teams / Web / Custom）；
  - 缺少**企业级 Authentication**（MSAL / OBO / Entra / SSO）；
  - 缺少**Activity Protocol**（Microsoft Activity Protocol 标准）；
  - 缺少**Skills 机制**（可插拔技能）；
  - 缺少**Microsoft 365 Copilot 集成**；
  - 缺少**多语言 SDK**（C# / JS / Python）；
  - 缺少**企业级 Memory Storage**（TurnState / MemoryStorage / CompactionProvider）；
  - 缺少**Agent Framework**（Agent / Session / Compaction / History）；
- **典型用户**：
  - **企业开发者**：构建 M365 Copilot / Teams 集成 Agent；
  - **.NET 开发者**：C# / .NET SDK；
  - **JS 开发者**：Node.js / Browser SDK；
  - **Python 开发者**：Python SDK；
- **核心卖点**：
  - **M365 渠道接入**：Copilot / Teams / Web / Custom；
  - **Activity Protocol**：Microsoft 标准 Activity Protocol；
  - **Agent Framework**（`agent_framework`）：
    - Agent / AgentSession；
    - CompactionProvider（上下文压缩）；
    - InMemoryHistoryProvider（对话历史）；
    - SlidingWindowStrategy（滑动窗口策略）；
  - **Skills**：可插拔技能；
  - **MSAL Authentication**：Microsoft 认证库；
  - **OBO Authorization**：On-Behalf-Of 流；
  - **Entra Agent ID**：Entra ID 集成；
  - **Microsoft 365 Copilot 集成**；
  - **Memory Storage**：TurnState 持久化；
  - **Cloud Adapter**：aiohttp / ASP.NET Core 适配器；
  - **三语言 SDK**：C# / JS / Python；
  - **企业特性**：SOC2 / HIPAA / GDPR 合规（通过 Microsoft 365）；

## 二、发展历史与社区

- **起源（2025）**：Microsoft 开源 Microsoft 365 Agents SDK v1.0（继 Bot Framework SDK 之后）；
- **里程碑**：
  - 2025-Q1：v1.0（C# SDK + JS SDK + Python SDK）；
  - 2025-Q2：v1.0.x（M365 Copilot 集成 + Skills + Entra Agent ID）；
  - 2025-Q3：v1.1（Cloud Adapter 完善 + CompactionProvider + SlidingWindowStrategy）；
  - 2025-Q4：v1.2（Auto-sign-in + OBO + Azure AI Streaming）；
  - 2026-Q1：v2.0（企业特性 + 多语言 SDK 同步 + Skills Marketplace）；
- **维护状态**：**非常活跃**（Microsoft Teams Platform 团队 50+ 工程师，月均 150+ commits）；
- **社区**：
  - GitHub `agents` 主仓 + 3 个子仓 `agents-for-net` / `agents-for-js` / `agents-for-python`；
  - 主仓 star 数 1k+（新项目），子仓各自 star 数 1k+；
  - Microsoft Tech Community；
  - Microsoft Learn 文档；
- **关键贡献者**：Microsoft Teams Platform 团队、贡献者 500+；

## 三、许可与商业限制

- **核心代码**：**MIT License**（`agents/` 主仓 + 语言子仓），可商用、修改、私有部署；
- **M365 Copilot 渠道**：需 M365 Copilot 订阅；
- **生态关系**：
  - **上游依赖**：
    - **Python**：Python 3.10+ / `agent_framework` / `microsoft_agents.hosting.aiohttp` / `microsoft_agents.authentication.msal` / `microsoft_agents.hosting.core` / `microsoft_agents.activity`；
    - **C#**：.NET 8+ / `Microsoft.Agents.*`；
    - **JS**：Node.js 18+ / `@microsoft/agents-*`；
  - **下游生态**：约 5000+ 项目使用 Microsoft 365 Agents SDK；

## 四、业界主要使用方与案例

- **国际**（公开）：
  - **Microsoft 内部**：M365 Copilot 集成；
  - **多家企业**：Teams 集成 Agent；
  - **多家 ISV**：Copilot for M365 扩展；
- **国内**：
  - 多家企业 M365 集成；
  - 多家 ISV Copilot 扩展；
- **典型场景**：
  - **M365 Copilot 集成**：Agent 作为 Copilot Skill；
  - **Teams 集成**：Teams App / Bot；
  - **企业客服**：Microsoft 365 全渠道客服；
  - **企业自动化**：M365 工作流自动化；
  - **企业搜索**：M365 内容搜索；
  - **企业内部工具**：审批 / 文档 / 通知；

## 五、一句话对比定位

> Microsoft 365 Agents SDK 是 **「Microsoft 出品 + M365 渠道接入 + Activity Protocol + Skills + MSAL / OBO / Entra + Agent Framework + 三语言 SDK + MIT」**；与 **LangGraph（图状态机）**、**CrewAI（角色化 + Process）**、**AutoGen（GroupChat 对话）**、**MetaGPT（软件公司角色）**、**OpenAI Agents（Handoff 简单协议）**、**AgentScope（多 Agent 通用）** 构成多 Agent 生态。Microsoft 365 Agents SDK 的差异化是 **「Microsoft 官方 + M365 渠道 + 企业级 Auth + Activity Protocol + 多语言 SDK + Copilot 集成」**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                  用户应用层（User Apps / Channels）                    │
│        Microsoft 365 Copilot / Teams / Web / Custom Apps            │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Activity Protocol（specs/activity/）               │
│      protocol-activity.md / protocol-cards.md                       │
│      Microsoft Activity Protocol 标准                                │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Channel Adapters（多语言 SDK）                       │
│   agents-for-net/（.NET SDK + ASP.NET Core Cloud Adapter）           │
│   agents-for-js/（Node.js / Browser SDK）                            │
│   agents-for-python/（Python SDK + aiohttp Cloud Adapter）          │
│      microsoft_agents.hosting.aiohttp / microsoft_agents.hosting.core │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Agent Framework（agent_framework/）                 │
│      Agent / AgentSession                                            │
│      CompactionProvider（上下文压缩）                                 │
│      InMemoryHistoryProvider（对话历史）                              │
│      SlidingWindowStrategy（滑动窗口策略）                            │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Hosting & Middleware                                  │
│   hosting.core/                                  │
│     ├── AgentApplication / TurnState / TurnContext                  │
│     ├── Authorization / CloudAdapter                                │
│     ├── MemoryStorage / Storage 抽象                                │
│   hosting.aiohttp/（Python aiohttp Cloud Adapter）                  │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Authentication                                       │
│   microsoft_agents.authentication.msal/（MSAL 认证）                  │
│     ├── MsalConnectionManager（MSAL 连接管理）                       │
│     ├── OBO Authorization（On-Behalf-Of 流）                        │
│     └── Entra Agent ID（Entra ID 集成）                              │
│   obo-authorization/（OBO 示例）                                      │
│   auto-signin/（自动登录示例）                                          │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Activity / Skills / Manifest                        │
│   activity/（Activity 抽象）                                            │
│   manifest/（Manifest 定义）                                            │
│   skills/（Skills 注册）                                              │
│   copilot-sdk/（Copilot SDK）                                          │
│   copilotstudio-client/（Copilot Studio 客户端）                    │
│   copilotstudio-skill/（Copilot Studio Skill）                        │
│   entra-agent-id-sidecar/（Entra ID Sidecar）                        │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Storage / State                                      │
│   memory/（Memory 抽象）                                              │
│   semantic-kernel-multiturn/（Semantic Kernel 集成）                │
│   retrieval-agent/（检索 Agent 示例）                                  │
│   proactive/（Proactive 消息）                                        │
│   otel/（OpenTelemetry 集成）                                          │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  外部依赖（External Dependencies）                   │
│   Azure AI / OpenAI / Anthropic / Gemini（通过 OpenAI Agent 适配）  │
│   Microsoft 365 / Entra ID / Azure / Teams                          │
│   Pydantic / SQLAlchemy / asyncio / aiohttp                         │
└─────────────────────────────────────────────────────────────────────┘
```

进程模型：Microsoft 365 Agents SDK 是**多语言 SDK**（C# / JS / Python），无独立进程；通过 Cloud Adapter（aiohttp / ASP.NET Core）部署到 M365 渠道。

## 七、Agent 类型专项（核心扩展章节）

### 7.1 角色协作模式（Role Collaboration）

Microsoft 365 Agents SDK 通过 **Agent + Channel Adapter + Skills** 实现四种协作模式：

- **中心化（Orchestrator-Worker）**：
  - **典型实现**：AgentApplication（Orchestrator）协调多个 Skill（Worker）；
  - **实现位置**：`microsoft_agents.hosting.core.AgentApplication`；
  - **API**：`AgentApplication.add_skill(skill)`；
- **对等（Peer-to-Peer / Blackboard）**：
  - **典型实现**：多个 Agent 共享 TurnState（blackboard pattern）；
  - **机制**：通过 TurnState 共享状态；
- **分层（Hierarchical / Supervisor）**：
  - **典型实现**：嵌套 Agent / Copilot Studio Skill；
  - **API**：`copilotstudio-skill/`；
- **市场（Market-based Bidding）**：
  - **典型实现**：通过 Skill Marketplace；
  - **API**：通过 Skills 机制实现；
- **Skills Marketplace**：
  - **典型实现**：用户从 Marketplace 安装技能；
  - **实现位置**：`skills/`（Skills Marketplace）；
  - **API**：`skill_registry.install(skill_id)`；
- **Multi-Channel Multi-Agent**：
  - **典型实现**：同一 Agent 部署到 Copilot + Teams + Web + Custom；
  - **API**：通过 Cloud Adapter 自动适配渠道；

### 7.2 记忆系统（Memory System）

Microsoft 365 Agents SDK 记忆分**TurnState + MemoryStorage + HistoryProvider + Compaction**：

- **短期（TurnContext / TurnState）**：
  - **实现位置**：`microsoft_agents.hosting.core.TurnState` / `TurnContext`；
  - **存储**：当前 Turn 的状态；
  - **API**：`turn_context.state`；
- **长期（MemoryStorage）**：
  - **实现位置**：`microsoft_agents.hosting.core.MemoryStorage`；
  - **API**：`memory_storage.get(key)` / `memory_storage.set(key, value)`；
- **对话历史（InMemoryHistoryProvider）**：
  - **实现位置**：`agent_framework.InMemoryHistoryProvider`；
  - **API**：`history_provider.get_messages()`；
- **上下文压缩（CompactionProvider）**：
  - **实现位置**：`agent_framework.CompactionProvider`；
  - **API**：自动压缩长上下文；
- **滑动窗口策略（SlidingWindowStrategy）**：
  - **实现位置**：`agent_framework.SlidingWindowStrategy`；
  - **API**：自动滑动窗口；
- **AgentSession**：
  - **实现位置**：`agent_framework.AgentSession`；
  - **API**：会话状态管理；
- **跨会话记忆**：
  - **典型实现**：通过 MemoryStorage 持久化；
- **Retrieval Memory**：
  - **典型实现**：`retrieval-agent/`（检索 Agent 示例）；
  - **API**：通过 RAG 检索历史；

### 7.3 工具调用协议（Tool Calling Protocol）

Microsoft 365 Agents SDK 通过 **Skills + Tools + Function Calling** 实现工具调用：

- **Function Calling（Azure OpenAI / OpenAI / Anthropic）**：
  - **实现位置**：`agent_framework.openai.OpenAIChatClient`（通过 OpenAI 兼容 API）；
  - **支持模型**：Azure OpenAI（默认）/ OpenAI / Anthropic（通过 Azure AI）/ Gemini（通过 Azure AI）；
  - **API**：`OpenAIChatClient(model="gpt-4o", tools=[...])`；
- **Skills（核心特色）**：
  - **典型实现**：用户定义 Skill（Tools 集合）；
  - **实现位置**：`skills/`（Skills Marketplace）；
  - **API**：`skill = Skill(tools=[...])`；
- **Built-in Tools**：
  - **典型实现**：`samples/python/agent-framework/src/tools/` 内置工具：
    - `date_time.py`：日期时间；
    - `progress.py`：进度通知；
    - `weather_lookup.py`：天气查询；
- **ReAct（Reasoning + Acting）**：
  - **典型实现**：Agent 默认使用 ReAct 循环；
- **CodeAct（生成代码执行）**：
  - **典型实现**：通过 Skill + Code Executor；
- **Tool Use + Reflection**：
  - **典型实现**：通过 TurnContext 中间件；
- **Structured Outputs**：
  - **典型实现**：通过 Pydantic 模型定义；
- **Copilot Studio Skill**：
  - **典型实现**：`copilotstudio-skill/`；
  - **机制**：调用 Copilot Studio 的 Skill；
- **Tool Error Handling**：
  - **典型实现**：通过 TurnContext 错误处理；
- **OpenTelemetry**：
  - **典型实现**：`otel/`（OpenTelemetry）；
  - **API**：自动 trace 所有 Agent / Skill 调用；

### 7.4 人机协作（Human-in-the-Loop）

Microsoft 365 Agents SDK HITL 通过 **M365 渠道 / Approval / Authentication / Cards** 实现：

- **M365 渠道 HITL**：
  - **典型实现**：用户在 Copilot / Teams 中与 Agent 交互；
  - **API**：通过 Activity Protocol 自动接收人工输入；
- **Adaptive Cards**：
  - **典型实现**：通过 Adaptive Cards 显示交互 UI；
  - **实现位置**：`specs/activity/protocol-cards.md`；
  - **API**：`activity.attachments = [AdaptiveCard(...)]`；
- **Approval Workflow**：
  - **典型实现**：通过 Adaptive Card 实现审批；
  - **API**：`AdaptiveCard(actions=[SubmitAction, ApproveAction])`；
- **Authentication HITL**：
  - **典型实现**：通过 MSAL / OBO / Entra ID 人工登录；
  - **实现位置**：`microsoft_agents.authentication.msal`；
- **Auto-signin**：
  - **典型实现**：`auto-signin/`（自动登录）；
  - **API**：自动 SSO；
- **OBO Authorization**：
  - **典型实现**：`obo-authorization/`（On-Behalf-Of 流）；
  - **机制**：Agent 代表用户调用下游 API；
- **Entra Agent ID**：
  - **典型实现**：`entra-agent-id-sidecar/`（Entra ID Sidecar）；
  - **机制**：Agent 通过 Entra ID 标识身份；
- **Proactive Messages**：
  - **典型实现**：`proactive/`（主动推送消息）；
  - **API**：`proactive_message.notify(user_id, text)`；
- **Custom HITL**：
  - **典型实现**：通过 TurnContext 实现自定义审批；
- **Audit Trail**：
  - **典型实现**：通过 OpenTelemetry + Activity Log；

## 八、索引机制（Index / Skills / Channel）

Microsoft 365 Agents SDK 索引通过 **Channel + Skills + Activity Protocol + Manifest** 实现：

- **Channel Index**：
  - **实现位置**：`specs/activity/`（Channel 协议）；
  - **API**：通过 Cloud Adapter 自动适配；
- **Skills Index**：
  - **实现位置**：`skills/`（Skills Marketplace）；
  - **API**：`skill_registry.install(skill_id)`；
- **Activity Protocol Index**：
  - **实现位置**：`specs/activity/`（protocol-activity.md / protocol-cards.md）；
- **Manifest Index**：
  - **实现位置**：`specs/manifest/`（Manifest 定义）；
- **Typespec**：
  - **典型实现**：`specs/typespec/`（TypeSpec 定义）；

## 九、查询/计算/接口层

### 9.1 Agent Application 执行引擎（Agent Application Execution Engine）

- **AgentApplication**：
  - **实现位置**：`microsoft_agents.hosting.core.AgentApplication`；
  - **API**：`AgentApplication(on_activity, on_message)`；
- **TurnContext**：
  - **实现位置**：`microsoft_agents.hosting.core.TurnContext`；
  - **API**：`turn_context.send_activity(text)`；
- **TurnState**：
  - **实现位置**：`microsoft_agents.hosting.core.TurnState`；
  - **API**：`turn_state.get(key)`；
- **Cloud Adapter**：
  - **实现位置**：`microsoft_agents.hosting.aiohttp.CloudAdapter`（Python）/ ASP.NET Core Cloud Adapter（C#）；
  - **API**：自动适配渠道；
- **Authorization**：
  - **实现位置**：`microsoft_agents.hosting.core.Authorization`；
  - **API**：通过 MSAL 鉴权；
- **Memory Storage**：
  - **实现位置**：`microsoft_agents.hosting.core.MemoryStorage`；
- **Agent Session**：
  - **实现位置**：`agent_framework.AgentSession`；
- **History Provider**：
  - **实现位置**：`agent_framework.InMemoryHistoryProvider`；
- **Compaction**：
  - **实现位置**：`agent_framework.CompactionProvider`；
- **Sliding Window**：
  - **实现位置**：`agent_framework.SlidingWindowStrategy`；

### 9.2 状态管理（State / TurnState / MemoryStorage）

- **TurnState**：
  - **实现位置**：`microsoft_agents.hosting.core.TurnState`；
- **MemoryStorage**：
  - **实现位置**：`microsoft_agents.hosting.core.MemoryStorage`；
- **AgentSession**：
  - **实现位置**：`agent_framework.AgentSession`；
- **Persistence**：
  - **API**：通过 MemoryStorage 自动持久化；
- **Backup**：
  - **API**：Azure Storage 自动备份；

### 9.3 多模态工具（Multimodal Tools）

- **Adaptive Cards**：
  - **典型实现**：通过 `AdaptiveCard`；
  - **API**：`activity.attachments = [AdaptiveCard(...)]`；
- **Code Execution**：
  - **典型实现**：通过 Skill + Code Executor；
- **Web Search**：
  - **典型实现**：通过 Skill + Bing Search；
- **RAG Tool**：
  - **典型实现**：`retrieval-agent/`（检索 Agent）；
- **Database Tool**：
  - **典型实现**：通过 Skill + SQLAlchemy；
- **Multi-modal**：
  - **典型实现**：通过 Adaptive Cards 渲染富文本；
- **Voice**：
  - **典型实现**：通过 Teams / Copilot 语音集成；
- **Vision**：
  - **典型实现**：通过 GPT-4V（Azure OpenAI）；

### 9.4 多 Agent 通信（Multi-Agent Communication）

- **Channel Sharing**：
  - **典型实现**：通过 Cloud Adapter 跨渠道共享 Agent；
- **TurnState Sharing**：
  - **典型实现**：通过 TurnContext 共享状态；
- **Skills Collaboration**：
  - **典型实现**：通过 Skills 组合；
- **Copilot Studio**：
  - **典型实现**：`copilotstudio-client/` + `copilotstudio-skill/`；
  - **机制**：调用 Copilot Studio Agent；
- **A2A Protocol**：
  - **典型实现**：通过 HTTP / Bot Framework 跨进程调用；

## 十、查询/计算/接口层（API & SDK）

- **Python API（核心）**：
  - `agent_framework.Agent`（`samples/python/agent-framework/src/agent.py`）；
  - `agent_framework.AgentSession`；
  - `agent_framework.CompactionProvider`；
  - `agent_framework.InMemoryHistoryProvider`；
  - `agent_framework.SlidingWindowStrategy`；
  - `agent_framework.openai.OpenAIChatClient`；
  - `microsoft_agents.hosting.aiohttp.CloudAdapter`；
  - `microsoft_agents.authentication.msal.MsalConnectionManager`；
  - `microsoft_agents.hosting.core.AgentApplication`；
  - `microsoft_agents.hosting.core.TurnState`；
  - `microsoft_agents.hosting.core.TurnContext`；
  - `microsoft_agents.hosting.core.MemoryStorage`；
  - `microsoft_agents.hosting.core.Authorization`；
  - `microsoft_agents.activity.load_configuration_from_env`；
- **C# API（核心）**：
  - `Microsoft.Agents.Builder`（`agents-for-net/`）；
  - `Microsoft.Agents.Hosting.AspNetCore`；
  - `Microsoft.Agents.Authentication.Msal`；
  - `Microsoft.Agents.Activity`；
- **JS / Node.js API（核心）**：
  - `@microsoft/agents-hosting`（`agents-for-js/`）；
  - `@microsoft/agents-hosting-express`；
  - `@microsoft/agents-authentication`；
  - `@microsoft/agents-activity`；
- **CLI**：
  - `az bot`（Azure Bot CLI）；
  - `teamsfx`（Teams Toolkit CLI）；
- **M365 Copilot 集成**：
  - 通过 `copilot-sdk/` + `copilotstudio-skill/`；
- **Azure AI 集成**：
  - 通过 `azure-ai-streaming/`；
- **Semantic Kernel 集成**：
  - 通过 `semantic-kernel-multiturn/`；
- **第三方 SDK**：
  - **OpenAI Python SDK**（通过 OpenAIChatClient）；
  - **Azure AI SDK**（通过 Azure 适配）；
  - **Semantic Kernel SDK**（通过 SK 适配）；

## 十一、生态与集成

- **LLM 集成**：Azure OpenAI（默认）/ OpenAI / Anthropic（通过 Azure AI）/ Gemini（通过 Azure AI）；
- **Channel 接入**：Microsoft 365 Copilot / Microsoft Teams / Web / Custom Apps / Outlook / SharePoint；
- **Vector Store**：通过 Semantic Kernel 集成；
- **Tool 集成**：Skills + Built-in Tools + Adaptive Cards；
- **Memory Backend**：MemoryStorage + AgentSession + InMemoryHistoryProvider；
- **Authentication**：MSAL / OBO / Entra ID / SSO；
- **Storage**：Azure Storage / SQL / Cosmos DB；
- **Observability**：OpenTelemetry（`otel/`）+ Azure Monitor；
- **部署**：Azure App Service / AKS / Container Apps；
- **企业版**：Microsoft 365 E3 / E5 订阅 + Azure 订阅；
- **学术合作**：Microsoft Research；
- **M365 Copilot 集成**：直接集成到 M365 Copilot；
- **Skills Marketplace**：M365 Marketplace 发布 Skills；
- **Adaptive Cards**：富 UI 集成；
- **Bot Framework 兼容**：兼容 Bot Framework SDK；

## 十二、源码学习路径

- **必读模块**（按学习顺序）：
  1. `README.md`：主仓入口；
  2. `MCSandAgentsEcosystemLinks.md`：M365 + Agents 生态链接；
  3. `AgentErrorCodes.md` / `AgentErrorCodesJS.md`：错误码；
  4. `samples/README.md`：示例入口；
  5. `samples/python/agent-framework/src/main.py`：Python 主入口；
  6. `samples/python/agent-framework/src/agent.py`：Python Agent 定义；
  7. `samples/python/agent-framework/src/start_server.py`：Python 服务器启动；
  8. `samples/python/agent-framework/src/tools/`：内置工具（date_time / progress / weather_lookup）；
  9. `samples/python/quickstart/`：Python QuickStart；
  10. `samples/python/copilot-sdk/`：Copilot SDK 集成；
  11. `samples/python/copilotstudio-client/`：Copilot Studio 客户端；
  12. `samples/python/copilotstudio-skill/`：Copilot Studio Skill；
  13. `samples/python/entra-agent-id-sidecar/`：Entra Agent ID Sidecar；
  14. `samples/python/obo-authorization/`：OBO 认证；
  15. `samples/python/auto-signin/`：自动登录；
  16. `samples/python/azure-ai-streaming/`：Azure AI Streaming；
  17. `samples/python/cards/`：Adaptive Cards；
  18. `samples/python/semantic-kernel-multiturn/`：Semantic Kernel 多轮；
  19. `samples/python/retrieval-agent/`：检索 Agent；
  20. `samples/python/proactive/`：主动推送；
  21. `samples/python/otel/`：OpenTelemetry；
  22. `samples/dotnet/`：.NET 示例；
  23. `samples/nodejs/`：Node.js 示例；
  24. `specs/activity/protocol-activity.md`：Activity 协议；
  25. `specs/activity/protocol-cards.md`：Cards 协议；
  26. `specs/manifest/`：Manifest 定义；
  27. `specs/transcript/`：Transcript 定义；
  28. `specs/typespec/`：TypeSpec 定义；
  29. `automation/teams-sample-sync/`：Teams 同步自动化；
  30. `experimental/`：实验功能（agent-provision / bicep-scripts）；
  - **语言子仓（独立 repo）**：
    - `agents-for-net/`：.NET SDK 源码；
    - `agents-for-js/`：Node.js SDK 源码；
    - `agents-for-python/`：Python SDK 源码（含 `agent_framework` / `microsoft_agents.hosting.*` 等包）；
- **LOC 与语言**：
  - 主仓 `agents/` ~50k LoC；
  - 子仓 `agents-for-net/` ~80k LoC；
  - 子仓 `agents-for-js/` ~60k LoC；
  - 子仓 `agents-for-python/` ~50k LoC；
  - 主语言：**Python**（40%）、**C#**（3%）、**TypeScript**（30%）、**Markdown**（20%）、**TypeSpec**（5%）；
- **构建系统**：
  - **pyproject.toml**（Python）；
  - **.csproj**（.NET）；
  - **package.json**（JS）；
  - **Bicep**（Azure 部署）；
  - **Docker / docker-compose**（部署）；
  - **CI/CD**：GitHub Actions + Azure DevOps；

## 十三、学习路径

- **入门（1 周）**：
  - 阅读官方文档（https://learn.microsoft.com/microsoft-365/agents-sdk/）；
  - 跑通 `samples/python/quickstart/`；
  - 跑通 `samples/python/agent-framework/`；
  - 理解 Activity Protocol；
  - 理解 Cloud Adapter；
- **进阶（2-3 周）**：
  - 深入学习 Skills（`samples/python/copilotstudio-skill/`）；
  - 实战 Adaptive Cards；
  - 实战 Authentication（MSAL / OBO / Entra）；
  - 实战 Copilot Studio 集成；
  - 实战 Semantic Kernel 集成；
  - 实战 Retrieval Agent；
  - 学习 Proactive Messages；
  - 学习 OpenTelemetry 集成；
- **高级（1-2 月）**：
  - 阅读 `agent_framework` 源码（Agent / Session / Compaction / History）；
  - 阅读 `microsoft_agents.hosting` 源码（Cloud Adapter / TurnContext / MemoryStorage）；
  - 阅读 `microsoft_agents.authentication` 源码（MSAL / OBO / Entra）；
  - 自定义 Skill（实现业务 Skill）；
  - 自定义 Adaptive Card（实现 Skill UI）；
  - 自定义 Channel（实现新渠道）；
  - 自定义 Authentication Provider（实现新认证）；
  - 自定义 Storage（实现新存储）；
  - M365 Copilot 集成；
  - 企业部署（Azure App Service / AKS）；
- **推荐资源**：
  - 主仓：https://github.com/microsoft/agents
  - 官方文档：https://learn.microsoft.com/microsoft-365/agents-sdk/
  - .NET 子仓：https://github.com/microsoft/Agents-for-net
  - JS 子仓：https://github.com/microsoft/Agents-for-js
  - Python 子仓：https://github.com/microsoft/Agents-for-python
  - M365 Copilot：https://learn.microsoft.com/microsoft-365-copilot/
  - Teams Toolkit：https://learn.microsoft.com/microsoftteams/platform/toolkit/teams-toolkit-fundamentals
  - Microsoft Learn：https://learn.microsoft.com/
  - Discord / Tech Community：https://techcommunity.microsoft.com/
  - Azure 文档：https://learn.microsoft.com/azure/

## 十四、风险与陷阱

- **多语言 SDK 协调**：C# / JS / Python 三个 SDK 行为可能不完全一致；
- **Microsoft 365 订阅依赖**：完整功能需 M365 订阅；
- **Azure 依赖**：需 Azure 订阅；
- **M365 Copilot 锁定**：使用 M365 Copilot 后切换到自部署成本高；
- **Activity Protocol 复杂**：Microsoft Activity Protocol 标准复杂；
- **Cloud Adapter 复杂度**：aiohttp / ASP.NET Core 适配器配置复杂；
- **MSAL / OBO 学习曲线**：Microsoft 认证库学习曲线陡；
- **Entra ID 集成复杂**：Entra Agent ID 集成需 Entra 管理员；
- **Adaptive Cards 调试**：Adaptive Cards 调试复杂；
- **Copilot Studio 锁定**：使用 Copilot Studio 后切换成本高；
- **Bot Framework 迁移**：从 Bot Framework SDK 迁移需重写代码；
- **Semantic Kernel 兼容**：Semantic Kernel 集成版本敏感；
- **OpenTelemetry 集成**：OpenTelemetry 配置复杂；
- **Proactive Messages 权限**：主动推送需特殊权限；
- **Performance 问题**：Python SDK 调度开销大；.NET SDK 性能更好；
- **企业合规要求**：SOC2 / HIPAA / GDPR 需 M365 订阅；
- **三仓同步风险**：主仓 vs 子仓更新不同步；

## 十六、参考资料

- 主仓：https://github.com/microsoft/agents
- 官方文档：https://learn.microsoft.com/microsoft-365/agents-sdk/
- .NET 子仓：https://github.com/microsoft/Agents-for-net
- JS 子仓：https://github.com/microsoft/Agents-for-js
- Python 子仓：https://github.com/microsoft/Agents-for-python
- M365 Copilot：https://learn.microsoft.com/microsoft-365-copilot/
- Teams Toolkit：https://learn.microsoft.com/microsoftteams/platform/toolkit/teams-toolkit-fundamentals
- Microsoft Learn：https://learn.microsoft.com/
- Activity Protocol：https://learn.microsoft.com/microsoftteams/bots/concepts/activity-design
- Adaptive Cards：https://adaptivecards.io/
- MSAL：https://learn.microsoft.com/entra/identity-platform/v2-overview
- OBO Flow：https://learn.microsoft.com/entra/identity-platform/v2-oauth2-on-behalf-of-flow
- Entra Agent ID：https://learn.microsoft.com/entra/
- Bot Framework SDK（前任）：https://github.com/microsoft/botframework-sdk
- Semantic Kernel：https://github.com/microsoft/semantic-kernel
- Azure AI：https://learn.microsoft.com/azure/ai-services/
- Discord / Tech Community：https://techcommunity.microsoft.com/

## 十七、与本项目关联

Microsoft 365 Agents SDK 作为「M365 渠道接入 + Activity Protocol + Skills + MSAL / OBO / Entra + Agent Framework + 多语言 SDK」行业代表企业级多渠道 Agent 框架，与本项目（engineering/ 轨道 C/C++ 自研存储与向量索引）在「M365 渠道接入 + Activity Protocol + Skills + 企业级 Authentication」四个维度具有直接参考价值：

- **M365 渠道接入 ↔ 多协议路由**：M365 渠道接入（Copilot / Teams / Web / Custom）与本项目多协议路由（`engineering/src/server/pgwire/`）+ 自定义协议（`engineering/src/server/pg_server.h`）的"**多渠道 + 协议适配**"思路相似。
- **Activity Protocol ↔ DB 协议**：Activity Protocol（`specs/activity/protocol-activity.md`）与本项目 PG Wire Protocol 对照——都是"**标准化 + 可发现 + 可插拔**"。
- **Skills ↔ Index AM**：Skills（`skills/`）与本项目索引 AM（`engineering/src/db/index/`）的"**声明式 + 可插拔 + 可组合**"机制相似。
- **MSAL / OBO / Entra ↔ 认证系统**：MSAL / OBO / Entra ID 认证与本项目认证系统（自研或集成 Keycloak）对照——都是"**企业级 + SSO + 多协议**"思路。
- **MemoryStorage ↔ 存储抽象**：MemoryStorage（`microsoft_agents.hosting.core.MemoryStorage`）与本项目多模态存储抽象（`engineering/include/db/mm_storage.h`、`engineering/src/db/storage/mm/mm_storage.c`）的"**统一接口 + 多种实现**"思路相似。
- **TurnContext ↔ Context 抽象**：TurnContext / TurnState 与本项目 GUC（`engineering/src/db/core/guc.c`）+ Context（`engineering/src/db/executor/`）的"**上下文传递 + 状态管理**"机制相似。
- **Agent Framework ↔ SQL Framework**：Agent Framework（`agent_framework/`）与本项目 SQL 执行器（`engineering/src/db/executor/`）的"**Step + 顺序 / 并行 / 条件 + Compaction + History + Sliding Window**"思路相似——都是"**会话抽象 + 状态管理 + 上下文压缩**"。
- **CompactionProvider ↔ 老化层**：CompactionProvider（`agent_framework.CompactionProvider`）与本项目老化层（`engineering/src/db/storage/aging/`）+ Buffer Pool（`engineering/src/db/storage/buffer/`）的"**上下文压缩 + LRU 置换**"机制相似。
- **OpenTelemetry ↔ APM**：OpenTelemetry（`otel/`）与本项目自定义 APM 协议对照。
- **Adaptive Cards ↔ 自研 UI**：Adaptive Cards（`specs/activity/protocol-cards.md`）与本项目自研 Studio / UI 对照。
- **Skills Marketplace ↔ 插件市场**：Skills Marketplace 模式可启发本项目算法市场（`engineering/src/algo-prod/`）。
- **Semantic Kernel 集成 ↔ 多 LLM 集成**：Semantic Kernel 集成（`samples/python/semantic-kernel-multiturn/`）与本项目 LiteLLM 集成（`engineering/src/llm/llm.h`）对照——都是"**统一接口 + 多模型**"。
- **Copilot Studio 集成 ↔ 自研 Studio**：Copilot Studio 集成（`samples/python/copilotstudio-client/` + `copilotstudio-skill/`）与本项目自研 Studio 对照。
- **学习建议**：阅读 Microsoft 365 Agents SDK 源码重点关注「M365 渠道接入 + Activity Protocol + Skills + MSAL / OBO / Entra + Agent Framework」，与本项目 `engineering/src/server/` + `engineering/src/db/executor/` 做架构对照，加深对「多渠道 + 协议适配 + 多语言 SDK + 企业级认证」的理解。
- **重点对照**：本项目 C/C++ 自研多协议路由可参考 Microsoft 365 Agents SDK "**M365 渠道 + Activity Protocol"**"思路；本项目 GUC + Trigger + Compaction 可参考 Microsoft 365 Agents SDK "**Agent Framework + CompactionProvider + SlidingWindowStrategy"**" 思路；本项目 LiteLLM 集成可参考 Microsoft 365 Agents SDK "**Azure AI + Semantic Kernel 集成**" 思路。