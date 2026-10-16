# AutoGen Wiki（Microsoft 出品的多 Agent 协作框架）

> 参考版本：`autogen` Python 包（`python/packages/autogen-agentchat/` + `python/packages/autogen-core/` + `python/packages/autogen-ext/` + `python/packages/autogen-magentic-one/`），参考 HEAD：b292a33a6 前的最终 release（autogen-agentchat 0.4+/autogen-core 0.4+）。本 wiki 重点介绍 AutoGen 作为「**事件驱动（Actor Model）+ GroupChat 对话 + Magentic-One 编排 + Auto-Build 自举**」的多 Agent 协作框架定位、源码架构、四个 Agent 维度（角色协作 / 记忆 / 工具调用 / 人机协作）的深度展开，以及与本项目 `engineering/` 自研存储与向量索引的对照参考。

## 一、项目定位

AutoGen 是由 **Microsoft Research**（Chi Wang 团队）于 **2023-08** 开源的 **「事件驱动 + 可对话 + 可自定义」** 多 Agent 协作框架，定位是 **「Framework for building multi-agent applications with customizable, conversable agents」**——以"**Agent + Chat + Event + Runtime**"四要素为核心，支持多种对话模式（GroupChat / Magentic-One / Swarm / AutoBuild）。

- **核心痛点**：
  - LangChain AgentExecutor 缺少**多 Agent 对话**支持；
  - CrewAI 缺少**学术研究底座**（AutoGen 起源是论文 Multi-Agent LLM Collaboration）；
  - LangGraph 缺少**开箱即用的 GroupChat**；
  - 缺少**事件驱动运行时**（Actor 模型，异步消息驱动）；
  - 缺少**AutoBuild 自举能力**（自动生成 Agent 配置）；
  - 缺少**Magentic-One 复杂任务编排**（自适应规划）；
- **典型用户**：
  - **研究型用户**：实验新算法 / 论文复现；
  - **企业应用开发者**：构建可生产的多 Agent；
  - **Auto-Build 用户**：自动生成 Agent；
  - **复杂任务编排**：研究 / 决策 / 软件开发；
- **核心卖点**：
  - **Agent 对话协议**：ConversableAgent + Reply / Initiate / Receive；
  - **GroupChat**：群聊模式（Manager 协调 Speaker Selection）；
  - **Magentic-One**：复杂任务自适应编排；
  - **事件驱动**：Actor 模型（RoutedAgent / ClosureAgent）；
  - **AutoBuild**：自动生成 Agent 配置；
  - **Extension 生态**：OpenAI / Anthropic / Azure / LiteLLM / Cache / Memory / RAG；
  - **Studio**：AutoGen Studio（Web 可视化）；
  - **Magentic-One CLI**：复杂任务命令行；

## 二、发展历史与社区

- **起源（2023-08）**：Microsoft Research 开源 AutoGen v0.1（基于 ConversableAgent）；
- **里程碑**：
  - 2023-08：v0.1（ConversableAgent + GroupChat）；
  - 2023-12：v0.2（GroupChatManager + Speaker Selection）；
  - 2024-03：v0.3（RAG Agent + Custom Reply）；
  - 2024-06：v0.4（**重大重写**——Actor Model / RoutedAgent / ClosureAgent / Magentic-One / Studio）；
  - 2024-10：v0.4.x（AutoBuild / Magentic-One 完善）；
  - 2025-02：v0.4.x（OpenAI o1 / Anthropic Sonnet 集成）；
  - 2025-08：v0.5+（多模态 + Computer Use）；
  - 2026-03：v0.6+（稳定 + 性能 + Studio 4.0）；
- **维护状态**：**活跃**（Microsoft Research + AutoGen 团队 20+ 工程师，月均 80+ commits）；
- **社区**：
  - GitHub 35k+ star、`autogen-agentchat` PyPI 月下载量 200 万+；
  - Discord 4k+ 成员、GitHub Discussions；
  - AutoGen Studio（Web IDE）；
  - 学术论文：Multi-Agent Collaboration: A Survey（Microsoft Research）；
- **关键贡献者**：Chi Wang（创始人）、Microsoft Research 团队、贡献者 300+；

## 三、许可与商业限制

- **核心代码**：**MIT License / Creative Commons Attribution 4.0**（`python/packages/autogen-core/`、`python/packages/autogen-agentchat/`、`python/packages/autogen-ext/`、`python/packages/autogen-magentic-one/`、`python/packages/autogen-studio/`），可商用、修改、私有部署；
- **AutoGen Studio**：商业组件部分闭源；
- **生态关系**：
  - **上游依赖**：
    - Python 3.10+；
    - OpenAI / Anthropic / Azure OpenAI / Ollama / LiteLLM；
    - tiktoken（数据库本地缓存）；
    - opentelemetry（observability）；
  - **下游生态**：约 5000+ 学术研究 + 企业应用；

## 四、业界主要使用方与案例

- **国际**（公开）：
  - **Microsoft Research**：学术研究项目；
  - **多家金融 / 保险**：复杂任务多 Agent；
  - **多家互联网公司**：研究型 Agent；
- **国内**：
  - 多家互联网公司研究 + 生产；
  - 多家企业内部知识库研究型 Agent；
- **典型场景**：
  - **学术研究**：论文复现 / 新算法；
  - **软件开发**：自动 Bug Fix / Code Review；
  - **复杂任务**：研究 / 决策 / 谈判；
  - **多模态**：图像 / 视频 / 音频 + 文本；
  - **Computer Use**：自动操作电脑；

## 五、一句话对比定位

> AutoGen 是 **「Microsoft Research 出品 + Actor Model + GroupChat + Magentic-One + AutoBuild + RAG + Studio + 35k star + 学术研究底座 + MIT/CC-BY-4.0」**；与 **LangGraph（图状态机）**、**CrewAI（角色化 + Process）**、**MetaGPT（软件公司角色）**、**OpenAI Agents（Handoff 简单协议）**、**Swarm（教学 Handoff）**、**AgentScope（多 Agent 通用）** 构成多 Agent 生态。AutoGen 的差异化是 **「事件驱动（Actor 模型）+ 学术研究 + Microsoft 出品 + Magentic-One 自适应 + Studio IDE」**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                  用户应用层（User Apps）                              │
│        AutoGen Studio / CLI / Python API / Magentic-One CLI          │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  High-Level API（python/packages/autogen-agentchat/）│
│   agents/（AssistantAgent / UserProxyAgent / CodeExecutorAgent）       │
│   teams/（RoundRobinGroupChat / SelectorGroupChat / Swarm / Magentic）│
│   conditions/（MaxMessageTermination / TokenUsageTermination）        │
│   messages/（TextMessage / MultiModalMessage / StopMessage）          │
│   tools/（FunctionTool / Tool / AgentTool）                          │
│   ui/（ConsoleUI / WebUI / Studio）                                  │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Core Runtime（python/packages/autogen-core/）       │
│   RoutedAgent（事件驱动 Agent）                                       │
│   ClosureAgent（闭包 Agent）                                          │
│   AgentRuntime（单进程 / 分布式 Runtime）                              │
│   MessageBus（消息路由 / 发布订阅）                                    │
│   RpcAgentClient / RpcAgentServer（跨进程通信）                      │
│   CacheStore（Disk / Redis）                                          │
│   Subscription（TopicType / TypeSubscription）                                  │
│   default.BaseBehavior（自动订阅 / 路由）                                 │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Model Client（autogen-ext + autogen-core）           │
│   OpenAIChatCompletionClient / AnthropicChatCompletionClient        │
│   AzureOpenAIChatCompletionClient / OllamaChatCompletionClient       │
│   LiteLLMChatCompletionClient（多 LLM 路由）                         │
│   CacheClient（Disk / Redis 缓存）                                    │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Extensions（autogen-ext/）                          │
│   autogen-ext-openai / anthropic / azure / ollama / litellm          │
│   autogen-ext-cache-disk / cache-redis                              │
│   autogen-ext-tools-*（browser / code / docker / mcp / rag）      │
│   autogen-ext-magentic-one（Magentic-One 编排）                      │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  外部依赖（External Dependencies）                   │
│   OpenAI / Anthropic / Azure / Ollama / LiteLLM / Redis / Docker    │
│   tiktoken / opentelemetry / Chroma / FAISS / Instructor           │
└─────────────────────────────────────────────────────────────────────┘
```

进程模型：AutoGen 0.4 引入**Actor Model + Runtime**——支持单进程（`SingleThreadedAgentRuntime`）/ 分布式（`DistributedAgentRuntime`）。

## 七、Agent 类型专项（核心扩展章节）

### 7.1 角色协作模式（Role Collaboration）

AutoGen 通过 **GroupChat / Magentic-One / Swarm** 实现四种协作模式：

- **中心化（Orchestrator-Worker）**：
  - **典型实现**：RoundRobinGroupChat（轮询 Speaker Selection）；
  - **实现位置**：`autogen-agentchat/src/autogen_agentchat/teams/round_robin_group_chat.py`；
  - **状态共享**：通过 Team Context 共享 messages；
  - **示例**：`python/packages/autogen-agentchat/samples/agentchat_groupchat/`；
- **对等（Peer-to-Peer / Blackboard）**：
  - **典型实现**：所有 Agent 共享 Team Context；
  - **实现位置**：`autogen-agentchat/src/autogen_agentchat/teams/group_chat.py`；
  - **共享 State**：所有消息共享；
- **分层（Hierarchical / Selector）**：
  - **典型实现**：SelectorGroupChat（LLM-based Speaker Selection）；
  - **实现位置**：`autogen-agentchat/src/autogen_agentchat/teams/selector_group_chat.py`；
  - **Selector Prompt**：用 LLM 选择下一个 Speaker；
  - **典型场景**：复杂任务自动委派；
- **市场（Market-based Bidding）**：
  - **典型实现**：Magentic-One（自适应规划 + Bidding）；
  - **实现位置**：`autogen-magentic-one/`（Magentic-One Orchestrator + Coder / WebSurfer / FileSurfer / Executor / ComputerTerminal）；
  - **机制**：Orchestrator 评估任务，Worker 报价，Orchestrator 选择；
  - **典型场景**：复杂任务（软件开发 / 数据分析 / 网络研究）；
- **Swarm**：
  - **典型实现**：`SwarmGroupChat`（Handoff 风格）；
  - **实现位置**：`autogen-agentchat/src/autogen_agentchat/teams/swarm.py`；
  - **典型场景**：Handoff 简单协作；
- **Magentic-One**：
  - **典型实现**：`MagenticOneGroupChat`（自适应规划 + 自适应重规划）；
  - **实现位置**：`autogen-magentic-one/src/autogen_magentic_one/`；
  - **机制**：Orchestrator 跟踪进度（Ledger）+ 计划（FactSheet）+ Replan；
  - **典型场景**：真实世界复杂任务；

### 7.2 记忆系统（Memory System）

AutoGen 0.4 记忆分**线程级 + 模型级 + Cache**：

- **短期（In-memory conversation buffer）**：
  - **实现位置**：`autogen-agentchat/src/autogen_agentchat/state/`（`TeamState` / `AgentState`）；
  - **Team Context**：Team 内所有 Agent 共享；
  - **典型实现**：`team.run_stream(task="...")` 自动维护；
- **长期（向量库 + 文件系统）**：
  - **实现位置**：`autogen-ext/`（`autogen-ext-tools-rag-*`）；
  - **RAG 工具**：`RagTool`（从 PDF / Web / DB 检索）；
  - **API**：`agent = AssistantAgent(..., tools=[rag_tool])`；
- **模型记忆（Model Context）**：
  - **实现位置**：`autogen-core/src/autogen_core/models/`；
  - **Model Context**：维护 LLM 调用上下文（Messages / Tools）；
  - **API**：`ChatCompletionContext` 类；
- **Cache（Disk / Redis）**：
  - **实现位置**：`autogen-core/src/autogen_core/cache_store.py`；
  - **API**：`DiskCacheStore` / `RedisCacheStore`；
  - **典型场景**：LLM 调用结果缓存（节省成本 + 提高速度）；
- **RAG over Memory**：
  - **典型实现**：`autogen-ext-rag-chromadb`；
  - **API**：`RagAgent` / `RagTool`；
- **Studio Memory**：
  - **实现位置**：`autogen-studio/`（Web UI）；
  - **持久化**：Studio 数据库；
- **Subscription-based 消息路由**：
  - **实现位置**：`autogen-core/src/autogen_core/base.py`（`TopicType` / `TypeSubscription`）；
  - **机制**：基于订阅自动收消息；

### 7.3 工具调用协议（Tool Calling Protocol）

AutoGen 通过 **FunctionTool / CodeExecutor** / **RAG** 实现工具调用：

- **Function Calling（OpenAI / Anthropic / Gemini）**：
  - **实现位置**：`autogen-core/src/autogen_core/models/`（`OpenAIChatCompletionClient` / `AnthropicChatCompletionClient`）；
  - **支持模型**：OpenAI GPT-4 / Anthropic Claude / Google Gemini / Mistral / Ollama / vLLM；
  - **绑定工具**：`agent = AssistantAgent(..., tools=[my_function_tool])`；
- **ReAct（Reasoning + Acting）**：
  - **典型实现**：`AssistantAgent` 默认使用 ReAct 循环；
  - **实现位置**：`autogen-agentchat/src/autogen_agentchat/agents/assistant_agent.py`；
  - **循环**：Reason → Action → Observation → Reason；
- **CodeAct（生成代码执行）**：
  - **典型实现**：`CodeExecutorAgent`（执行 LLM 生成的代码）；
  - **实现位置**：`autogen-ext/src/autogen_ext/agents/`（`code_executor_agent.py`）；
  - **Backend**：`LocalCommandLineCodeExecutor` / `DockerCommandLineCodeExecutor`；
  - **沙箱**：本地 / Docker（生产推荐）；
- **Tool Use + Reflection**：
  - **典型实现**：`reflect_on_tool_use=True`（双模型自反思）；
  - **API**：`AssistantAgent(..., reflect_on_tool_use=True)`；
- **Structured Outputs**：
  - **实现位置**：`autogen-core/src/autogen_core/models/`；
  - **Pydantic 模型**：`agent = AssistantAgent(..., output_content_type=MyModel)`；
- **MCP（Model Context Protocol）**：
  - **实现位置**：`autogen-ext/src/autogen_ext/mcp/`（`McpWorkbench` / `McpToolAdapter`）；
  - **API**：`workbench = McpWorkbench(server_params=...)`；
  - **典型场景**：连接外部 MCP 服务器；
- **Tool Error Handling**：
  - **典型实现**：`tool_call_max_consecutive_failure`（连续失败次数）；
  - **Fallback**：`AssistantAgent(..., tool_call_max_consecutive_failure=3)`；
- **Computer Use**：
  - **典型实现**：`autogen-ext-computer-use`；
  - **API**：`ComputerUseAgent`（OpenAI CUA / Anthropic Computer Use）；

### 7.4 人机协作（Human-in-the-Loop）

AutoGen HITL 通过 **UserProxyAgent / Studio / Magentic-One** 实现：

- **UserProxyAgent**：
  - **典型实现**：`UserProxyAgent`（人类作为 Agent 参与对话）；
  - **实现位置**：`autogen-agentchat/src/autogen_agentchat/agents/user_proxy_agent.py`；
  - **API**：`user_proxy = UserProxyAgent("user", human_input_mode="ALWAYS")`；
  - **模式**：`ALWAYS` / `TERMINATE` / `NEVER`；
- **Input Functions**：
  - **API**：`agent.human_input("Continue? [y/n]: ")`；
  - **典型场景**：每次 Agent 完成 step 询问人类；
- **Studio HITL**：
  - **Studio Web UI**：用户可在 UI 中接管 / 批准 / 修改；
  - **API**：Studio 内置人在回路 UI；
- **Magentic-One Human Approval**：
  - **实现位置**：`autogen-magentic-one`（Orchestrator 支持人工干预）；
  - **API**：`MagenticOneGroupChat(..., user_proxy=user_proxy_agent)`；
- **Approval Workflow**：
  - **典型实现**：自定义 Tool（如 `approval_tool`）调用前等待审批；
  - **API**：Tool 内部调用 `input("Approve? [y/n]: ")`；
- **Studio Approval**：
  - **Studio 内置审批 UI**：用户在 Studio 中审批敏感操作；
- **Audit Trail**：
  - **实现位置**：`autogen-core/src/autogen_core/telemetry/`（OpenTelemetry）；
  - **API**：自动 trace 所有 Agent / Tool 调用；
- **Custom HITL**：
  - **典型实现**：自定义 Agent 加 `before_execute` / `after_execute` 钩子；
  - **API**：在 RoutedAgent 中实现 `on_message` 钩子；

## 八、索引机制（Index / Subscription）

AutoGen 通过 **Subscription + Message Routing** 实现事件索引：

- **TopicType（主题类型）**：
  - **实现位置**：`autogen-core/src/autogen_core/base.py`；
  - **语义**：消息主题（如 `"task_started"` / `"agent_response"`）；
- **TypeSubscription（类型订阅）**：
  - **实现位置**：`autogen-core/src/autogen_core/base.py`；
  - **机制**：Agent 订阅特定 Type 消息；
  - **API**：`agent = MyAgent().add_subscription(TypeSubscription("agent_response"))`；
- **DefaultSubscription（默认订阅）**：
  - **API**：`agent = MyAgent().add_subscription(DefaultSubscription())`；
- **Message Router（消息路由）**：
  - **实现位置**：`autogen-core/src/autogen_core/_agent_runtime.py`；
  - **机制**：根据 Subscription 路由消息到对应 Agent；
- **Message Queue（消息队列）**：
  - **实现位置**：`autogen-core/src/autogen_core/_agent_instantiation.py`；
  - **机制**：每 Agent 一个 in-memory queue；

## 九、查询/计算/接口层

### 9.1 Team 执行引擎（Team Execution Engine）

- **Team 编排**：
  - **实现位置**：`autogen-agentchat/src/autogen_agentchat/teams/`；
  - **类型**：RoundRobinGroupChat / SelectorGroupChat / Swarm / MagenticOneGroupChat / GraphGroupChat；
- **Team.run_stream()**：
  - **API**：`result = await team.run_stream(task="...")`；
  - **异步流式**：异步流式返回消息；
- **Termination Conditions**：
  - **实现位置**：`autogen-agentchat/src/autogen_agentchat/conditions/`；
  - **类型**：`MaxMessageTermination` / `TokenUsageTermination` / `TimeoutTermination` / `HandoffTermination`；
  - **API**：`team = Team(agents, termination_condition=MaxMessageTermination(10))`；
- **Cancellation**：
  - **API**：`await task.cancel()`；
  - **场景**：用户中断；
- **State Snapshot**：
  - **API**：`await team.save_state()` / `await team.load_state(state)`；
  - **场景**：持久化 + 恢复；
- **Reentrant**：
  - **典型实现**：Team 支持 reentrant（同一 Team 多个任务）；
  - **API**：`async with team` 上下文管理；

### 9.2 状态管理（State / Snapshot / Pause / Resume）

- **Team State**：
  - **实现位置**：`autogen-agentchat/src/autogen_agentchat/state/`；
  - **内容**：Team 内所有 Agent 的状态 + Messages；
  - **API**：`state = await team.save_state()`；
- **Agent State**：
  - **API**：`state = await agent.save_state()`；
- **Pause / Resume**：
  - **API**：`await team.pause()` / `await team.resume()`；
  - **场景**：长任务暂停 / 恢复；
- **Replay**：
  - **API**：`team.run_stream(..., replay_state=state)`；
  - **场景**：重放历史任务；
- **Cancellation**：
  - **API**：`await task.cancel()`；
  - **场景**：用户中断；
- **Subscription State**：
  - **实现位置**：`autogen-core/src/autogen_core/_agent_instantiation.py`；
  - **场景**：Agent 订阅状态持久化；

### 9.3 多模态工具（Multimodal Tools）

- **浏览器**：`WebSurferAgent`（Playwright + Azure OpenAI GPT-4V）；
- **代码执行**：`CodeExecutorAgent`（本地 / Docker）；
- **文件操作**：`FileSurferAgent`（本地文件系统）；
- **API 调用**：`APITool`（REST API 通用工具）；
- **RAG 工具**：`RagTool` / `RagAgent`（Chroma / FAISS）；
- **数据库**：`SQLDatabaseTool`（查询 SQL 数据库）；
- **搜索**：`WebSearchAgent`（Tavily / Serper）；
- **多模态**：`MultimodalAgent`（图像 / 视频 / 音频）；
- **Computer Use**：`ComputerUseAgent`（OpenAI CUA / Anthropic Computer Use）；
- **自定义**：`@tool("my_tool")` 装饰器或继承 `BaseTool`；
- **Toolkit 组合**：
  - **典型 Toolkit**：`autogen-ext-tools-*` 提供 100+ 预制工具；

### 9.4 多 Agent 通信（Multi-Agent Communication）

- **Message Bus**：
  - **实现位置**：`autogen-core/src/autogen_core/_agent_runtime.py`；
  - **机制**：基于 Subscription 的发布订阅；
- **Shared State**：
  - **实现位置**：`autogen-core/src/autogen_core/_agent_state.py`；
  - **共享 State**：Agent 间共享状态；
- **Handoff**：
  - **典型实现**：`SwarmGroupChat`（Handoff 风格）；
  - **API**：`agent.on_message(...)` 中调用 `handoff(other_agent)`；
- **A2A 协议**：
  - **AutoGen 通过 Runtime RPC** 实现 A2A；
  - **典型实现**：`DistributedAgentRuntime`（跨进程通信）；
  - **协议**：`grpc` / `websocket`；
- **Remote Agent**：
  - **典型实现**：`RpcAgentClient` / `RpcAgentServer`；
  - **场景**：跨进程 / 跨机器 Agent 通信；

## 十、查询/计算/接口层（API & SDK）

- **Python API（核心）**：
  - `autogen_agentchat.agents.AssistantAgent`；
  - `autogen_agentchat.agents.UserProxyAgent`；
  - `autogen_agentchat.agents.CodeExecutorAgent`；
  - `autogen_agentchat.teams.RoundRobinGroupChat`；
  - `autogen_agentchat.teams.SelectorGroupChat`；
  - `autogen_agentchat.teams.Swarm`；
  - `autogen_agentchat.teams.MagenticOneGroupChat`；
  - `autogen_agentchat.conditions.MaxMessageTermination`；
  - `autogen_agentchat.conditions.TokenUsageTermination`；
  - `autogen_core.RoutedAgent`；
  - `autogen_core.ClosureAgent`；
  - `autogen_core.AgentRuntime`（`SingleThreadedAgentRuntime` / `DistributedAgentRuntime`）；
  - `autogen_core.TopicType` / `TypeSubscription` / `DefaultSubscription`；
  - `autogen_core.models.OpenAIChatCompletionClient`；
  - `autogen_core.models.AnthropicChatCompletionClient`；
  - `autogen_core.tools.FunctionTool`；
- **CLI**：
  - `magentic-one-cli`（复杂任务命令行）；
  - `autogen-studio`（Web Studio）；
- **AutoGen Studio**（Web IDE）：
  - 可视化设计 Agent / Team；
  - 调试 + 测试 + 监控；
  - **实现位置**：`autogen-studio/`；
- **Magentic-One CLI**：
  - 真实世界任务编排；
  - 复杂任务分解 + 执行；
- **第三方 SDK**：
  - **LangChain 集成**：通过 LangChain Adapter；
  - **Instructor 集成**：结构化输出；
  - **MCP 集成**：MCP Server / Client；
- **RPC 通信**：
  - **gRPC**：`autogen-core` 支持 gRPC；
  - **WebSocket**：`autogen-core` 支持 WebSocket；
  - **Redis / Kafka**：通过 `DistributedAgentRuntime` 扩展；

## 十一、生态与集成

- **LLM 集成**：OpenAI / Anthropic / Azure OpenAI / Google Gemini / Mistral / Ollama / vLLM / Together / Fireworks / LiteLLM（100+ 模型）；
- **Vector Store**：Chroma（默认）/ FAISS / Weaviate / Milvus / Qdrant / LanceDB / pgvector / Pinecone；
- **Tool 集成**：100+ 预制工具 + LangChain 全部 Tools + 自定义工具；
- **Code Executor**：本地 / Docker（生产推荐）；
- **Cache**：Disk / Redis；
- **Memory Backend**：Team Context（默认）/ Chroma / FAISS；
- **Observability**：OpenTelemetry（默认）/ LangSmith / Langfuse；
- **部署**：AutoGen Studio（Web）/ Docker / Kubernetes；
- **RPC Runtime**：gRPC / WebSocket / 自定义 Transport；
- **企业版**：AutoGen Enterprise（规划中）；
- **学术论文**：Multi-Agent Collaboration: A Survey（Microsoft Research）；
- **第三方库**：Instructor / LiteLLM / OpenTelemetry / Pydantic / tiktoken；

## 十二、源码学习路径

- **必读模块**（按学习顺序）：
  1. `python/packages/autogen-core/src/autogen_core/__init__.py`：核心导出；
  2. `python/packages/autogen-core/src/autogen_core/base.py`：Agent / Subscription 抽象；
  3. `python/packages/autogen-core/src/autogen_core/_agent_runtime.py`：AgentRuntime；
  4. `python/packages/autogen-core/src/autogen_core/_agent_instantiation.py`：Agent 实例化 + Subscription；
  5. `python/packages/autogen-core/src/autogen_core/_agent_type.py`：AgentType；
  6. `python/packages/autogen-core/src/autogen_core/models/__init__.py`：Model Client；
  7. `python/packages/autogen-core/src/autogen_core/models/_openai.py`：OpenAI Client；
  8. `python/packages/autogen-core/src/autogen_core/tools/_function_tool.py`：FunctionTool；
  9. `python/packages/autogen-core/src/autogen_core/cache_store.py`：CacheStore；
  10. `python/packages/autogen-agentchat/src/autogen_agentchat/__init__.py`：agentchat 导出；
  11. `python/packages/autogen-agentchat/src/autogen_agentchat/agents/assistant_agent.py`：AssistantAgent；
  12. `python/packages/autogen-agentchat/src/autogen_agentchat/agents/user_proxy_agent.py`：UserProxyAgent；
  13. `python/packages/autogen-agentchat/src/autogen_agentchat/teams/round_robin_group_chat.py`：RoundRobin；
  14. `python/packages/autogen-agentchat/src/autogen_agentchat/teams/selector_group_chat.py`：Selector；
  15. `python/packages/autogen-agentchat/src/autogen_agentchat/teams/swarm.py`：Swarm；
  16. `python/packages/autogen-agentchat/src/autogen_agentchat/conditions/max_message_condition.py`：终止条件；
  17. `python/packages/autogen-agentchat/src/autogen_agentchat/state/`：State 管理；
  18. `python/packages/autogen-magentic-one/src/autogen_magentic_one/`：Magentic-One；
  19. `python/packages/autogen-ext/src/autogen_ext/`：Extension；
  20. `python/packages/autogen-studio/`：Web Studio；
  21. `python/packages/autogen-magentic-one/magentic_one_cli/`：CLI；
- **LOC 与语言**：
  - `autogen-core/` ~50k LoC；
  - `autogen-agentchat/` ~30k LoC；
  - `autogen-ext/` ~30k LoC；
  - `autogen-magentic-one/` ~10k LoC；
  - `autogen-studio/` ~10k LoC（前端 TypeScript）；
  - 主语言：**Python**（85%）、**TypeScript**（15%，Studio 前端）；
- **构建系统**：
  - **Hatchling**（`pyproject.toml`）；
  - **uv**（依赖管理）；
  - **monorepo**（`python/packages/` 下多个子包）；
  - **CI/CD**：GitHub Actions；
  - **Azure DevOps**：内部 CI/CD；

## 十三、学习路径

- **入门（1 周）**：
  - 阅读官方 Tutorial（https://microsoft.github.io/autogen/）；
  - 跑通 `python/packages/autogen-agentchat/samples/`；
  - 理解 ConversableAgent / AssistantAgent / UserProxyAgent；
  - 跑通 GroupChat（RoundRobin / Selector）；
- **进阶（2-3 周）**：
  - 深入学习 Actor Model（`autogen-core`）；
  - 实战 RoutedAgent / ClosureAgent；
  - 学习 Subscription / TopicType；
  - 学习 Magentic-One（Orchestrator + Coder / WebSurfer / FileSurfer / Executor / ComputerTerminal）；
  - 学习 CodeExecutor + MCP；
- **高级（1-2 月）**：
  - 阅读源码（Actor Model + Runtime + GroupChat + Magentic-One）；
  - 自定义 Subscription（基于主题路由）；
  - 自定义 Model Client；
  - 自定义 Tool（FunctionTool / GenericTool）；
  - 自定义 Termination Condition；
  - DistributedAgentRuntime 分布式部署；
  - AutoGen Studio 二次开发；
- **推荐资源**：
  - 官方文档：https://microsoft.github.io/autogen/
  - GitHub：https://github.com/microsoft/autogen
  - AutoGen Studio：https://github.com/microsoft/autogen/tree/main/python/packages/autogen-studio
  - 论文：Multi-Agent Collaboration: A Survey（Microsoft Research）
  - 论文：AutoGen: Enabling Next-Gen LLM Applications via Multi-Agent Conversation
  - 博客：https://devblogs.microsoft.com/autogen/
  - Discord：https://discord.gg/autogen
  - Forum：https://github.com/microsoft/autogen/discussions
  - YouTube：https://www.youtube.com/@MicrosoftResearch

## 十四、风险与陷阱

- **学习曲线陡**：Actor Model / Subscription / Runtime 概念多，新人需 2-3 周入门；
- **0.4 重写破坏性升级**：从 0.2 升级到 0.4 几乎重写代码；
- **Magentic-One 不稳定**：复杂任务长 Ledger 维护成本高；
- **SelectorGroupChat 不稳定**：LLM-based Speaker Selection 可能选错；
- **Distributed Runtime 复杂**：gRPC / Redis 部署复杂；
- **Code Executor 沙箱**：本地执行有安全风险，必须用 Docker；
- **RAG 集成成本**：默认无 RAG，需手动集成 Chroma / FAISS；
- **MCP 不稳定**：MCP 协议还在演进；
- **Studio 功能缺失**：Studio 与 Python API 功能不对等；
- **Performance 问题**：Python 调度开销大，超大 Team（>10 Agent）性能下降；
- **LLM 成本**：ReAct 循环 + Multi-Step 模型易产生高 token 消耗；
- **Cache 一致性**：Disk / Redis Cache 在 LLM 更新时需手动清理；
- **Subscription 调试难**：消息路由难调试；
- **Terminate Condition 设计**：最大消息数 / Token 终止条件难调；
- **Computer Use 风险**：ComputerUseAgent 自动操作系统有安全风险；
- **Async / Await 错误处理**：异步编程对新人较难；

## 十六、参考资料

- 官方仓库：https://github.com/microsoft/autogen
- 官方文档：https://microsoft.github.io/autogen/
- AutoGen Studio：https://github.com/microsoft/autogen/tree/main/python/packages/autogen-studio
- Magentic-One：https://github.com/microsoft/autogen/tree/main/python/packages/autogen-magentic-one
- 论文：Multi-Agent Collaboration: A Survey（Microsoft Research）
- 论文：AutoGen: Enabling Next-Gen LLM Applications via Multi-Agent Conversation
- 论文：Magentic-One: A Generalist Multi-Agent System for Solving Complex Tasks
- 论文：RAG and Agent: A Survey
- 博客：https://devblogs.microsoft.com/autogen/
- Tutorial：https://microsoft.github.io/autogen/dev/user-guide/core-user-guide/
- Discord：https://discord.gg/autogen
- Forum：https://github.com/microsoft/autogen/discussions
- YouTube：https://www.youtube.com/@MicrosoftResearch
- 学术合作：https://www.microsoft.com/en-us/research/

## 十七、与本项目关联

AutoGen 作为「Actor Model + GroupChat + Magentic-One + AutoBuild」行业代表多 Agent 框架，与本项目（engineering/ 轨道 C/C++ 自研存储与向量索引）在「事件驱动运行时 + Subscription 路由 + GroupChat 对话 + Code Executor 沙箱」四个维度具有直接参考价值：

- **Actor Model ↔ 分布式 Runtime**：AutoGen 的 Actor Model（`autogen-core/_agent_runtime.py`）与本项目 Phase 9 分布式协调（`engineering/src/db/distributed/`）+ Raft 一致性（`engineering/src/db/consensus/`）+ Coordinator（`engineering/src/db/coordinator/`）的"**事件驱动 + 消息路由 + 状态机**"设计相似。本项目可参考 AutoGen "**订阅 + 发布 + 自动路由**" 思路。
- **Subscription ↔ 主题订阅**：AutoGen 的 Subscription（`autogen-core/base.py`）机制与本项目 WAL（`engineering/src/db/storage/log/`）的事件订阅模式相似——都是"**主题 + 类型 + 自动路由**"思路。
- **GroupChat ↔ 多表连接**：AutoGen 的 GroupChat（`autogen-agentchat/teams/`）与本项目 SQL 多表连接（`engineering/src/db/sql/`）的多节点协调相似。
- **Magentic-One ↔ 分布式事务**：Magentic-One 的"**自适应 + 重规划 + Ledger**" 与本项目分布式事务（`engineering/src/db/distributed/dist_txn.c`）的"**协调 + 重试 + 回滚**"机制相似——都是"**复杂任务分解 + 自适应协调**"思路。
- **Code Executor ↔ SQL Executor**：Code Executor（`autogen-ext/agents/code_executor_agent.py`）与本项目 SQL Executor（`engineering/src/db/executor/`）的"**执行 + 重试 + 回滚**"机制相似。
- **MCP ↔ DB 协议**：AutoGen 通过 MCP 连接外部系统；本项目实现 PG Wire Protocol（`engineering/src/server/pgwire/`）+ 自定义协议（`engineering/src/server/pg_server.h`），可对照 MCP "**标准化 + 可发现 + 可插拔**" 思路。
- **Distributed Runtime ↔ Phase 9 分布式**：DistributedAgentRuntime（gRPC + Redis） 与本项目 Raft（`engineering/src/db/consensus/raft.c`）+ 分布式事务（`engineering/src/db/distributed/dist_txn.c`）+ Shard 路由（`engineering/src/db/sharding/`）对照——本项目用 C 自研，AutoGen 用 Python + gRPC。
- **Tool 抽象 ↔ Index AM**：Tool 定义与本项目索引 AM（`engineering/src/db/index/`）相似——都是"**声明式 + 参数化 + 可调用**"。
- **OpenTelemetry ↔ APM**：AutoGen 的 OpenTelemetry（`autogen-core/telemetry/`）与本项目自定义 APM 协议对照。
- **Studio ↔ 自研 Studio**：AutoGen Studio Web IDE 与本项目自研工具对照。
- **学习建议**：阅读 AutoGen 源码重点关注「Actor Model + Subscription + GroupChat + Magentic-One 自适应」，与本项目 `engineering/src/db/distributed/` + `engineering/src/db/consensus/` 做架构对照，加深对「分布式 + 多 Agent + 自适应协调」的理解。