# OpenAI Agents Wiki（OpenAI 官方 Agent SDK）

> 参考版本：`openai-agents` Python 包（`src/agents/`），参考 HEAD：b292a33a6 前的最终 release（openai-agents 0.1+/0.2+）。本 wiki 重点介绍 OpenAI Agents 作为「**Handoff 协议 + Guardrail + Session Memory + Tools / Function Calling + Tracing + Computer Use + LiteLLM 多模型**」的 OpenAI 官方 Agent SDK 定位、源码架构、四个 Agent 维度（角色协作 / 记忆 / 工具调用 / 人机协作）的深度展开，以及与本项目 `engineering/` 自研存储与向量索引的对照参考。

## 一、项目定位

OpenAI Agents SDK（也称 `openai-agents-python`）是 **OpenAI** 于 **2025 年 3 月**开源的 **「官方轻量级 Agent 框架」**，定位是 **「A lightweight, powerful framework for multi-agent workflows」**——从 Swarm（教学 Handoff）演化而来，作为 OpenAI Agents API 的官方 SDK，支持 Handoff / Guardrail / Session / Tracing / Computer Use 等核心能力。

- **核心痛点**：
  - 缺少 OpenAI 官方 Agent 框架（之前用 LangChain / AutoGen / LlamaIndex 都缺官方支持）；
  - Swarm 教学版本不可生产（缺少 Tracing / Guardrail / Computer Use）；
  - 缺少原生 Handoff 协议（Orchestra / CrewAI 都需要自定义）；
  - 缺少原生 Session / Memory（不是 Session）；
  - 缺少原生 Guardrail（内容安全 / 输入校验）；
  - 缺少原生 Computer Use（CUA）支持；
  - 缺少原生 Tracing 可观测性；
- **典型用户**：
  - **OpenAI 生态用户**：使用 OpenAI API 的开发者；
  - **轻量 Agent**（单 Agent + 工具）；
  - **多 Agent**（Handoff 协作）；
  - **Computer Use**（自动操作电脑）；
  - **生产部署**：Tracing / Guardrail / Session 全套；
- **核心卖点**：
  - **Handoff 协议**：原生 Agent 间 handoff（handoffs / handoff_tool）；
  - **Guardrail**：输入 / 输出校验（input_guardrail / output_guardrail）；
  - **Session Memory**：会话持久化（SQLiteSession / RedisSession / 自定义）；
  - **Tools / Function Calling**：原生 Function Calling + AgentTool + ComputerUseTool；
  - **Tracing**：原生 Tracing（Trace / Span / Custom）；
  - **Computer Use**：OpenAI CUA（Computer Use Agent）；
  - **LiteLLM 兼容**：通过 LiteLLM 支持多 LLM（Anthropic / Gemini / 自定义）；
  - **MCP**：Model Context Protocol 集成；
  - **Realtime API**：Realtime API 支持（音频对话 Agent）；
  - **Voice Pipeline**：Voice Pipeline（TTS + STT）；

## 二、发展历史与社区

- **起源（2025-03）**：OpenAI 开源 openai-agents-python v0.1，从 Swarm 教学版演化而来；
- **里程碑**：
  - 2025-03：v0.1（Agent / Handoff / Guardrail / Session / Tracing）；
  - 2025-04：v0.1.x（Computer Use Tool / MCP / Voice Pipeline）；
  - 2025-06：v0.1.x（Realtime API / LiteLLM 兼容 / 多模型支持）；
  - 2025-08：v0.2.x（Tracing 增强 / Visualizer / OpenAI Dashboard 集成）；
  - 2025-10：v0.2.x（Computer Use 完善 / Anthropic Claude 集成 / Gemini 集成）；
  - 2026-03：v0.3+（性能优化 + 文档完善 + 企业特性）；
- **维护状态**：**非常活跃**（OpenAI 团队 30+ 工程师，月均 100+ commits）；
- **社区**：
  - GitHub 8k+ star、`openai-agents` PyPI 月下载量 300 万+；
  - Discord 5k+ 成员、Forum、Cookbook；
  - OpenAI Cookbook 大量示例；
- **关键贡献者**：OpenAI 团队、Swarm 团队、贡献者 500+；

## 三、许可与商业限制

- **核心代码**：**MIT License**（`src/agents/`），可商用、修改、私有部署；
- **OpenAI Platform 集成**：商业（Tracing / Realtime API）；
- **生态关系**：
  - **上游依赖**：
    - Python 3.10+；
    - OpenAI Python SDK；
    - LiteLLM（多 LLM 路由）；
    - Pydantic v2；
    - tiktoken；
    - openai-agents-realtime（Realtime API）；
  - **下游生态**：约 10000+ 项目使用 OpenAI Agents SDK；

## 四、业界主要使用方与案例

- **国际**（公开）：
  - **多家初创公司**：MVP / Hackathon 加速；
  - **多家企业**：客服 / 销售 / 营销 Agent；
  - **多家金融机构**：合规审批 / 风险评估；
- **国内**：
  - 多家互联网公司内容生产 / 营销；
  - 多家企业内部知识库研究型 Agent；
- **典型场景**：
  - **轻量 Agent**：单 Agent + Function Calling；
  - **多 Agent**：Handoff 协作；
  - **Computer Use**：自动浏览器 / IDE 操作；
  - **客服 / 销售**：多 Agent 协作；
  - **Realtime API**：实时音频对话；
  - **生产级系统**：Tracing + Guardrail + Session 全套；
  - **多 LLM**：通过 LiteLLM 集成 Anthropic / Gemini；

## 五、一句话对比定位

> OpenAI Agents 是 **「OpenAI 官方出品 + Handoff + Guardrail + Session Memory + Tracing + Computer Use + LiteLLM + Realtime + 8k star + MIT」**；与 **LangGraph（图状态机）**、**CrewAI（角色化 + Process）**、**AutoGen（GroupChat 对话）**、**MetaGPT（软件公司角色）**、**Swarm（教学 Handoff）**、**AgentScope（多 Agent 通用）** 构成多 Agent 生态。OpenAI Agents 的差异化是 **「OpenAI 官方出品 + Handoff 协议 + Guardrail + Session + Computer Use + Realtime + Tracing 一体化」**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                  用户应用层（User Apps）                              │
│        OpenAI Dashboard / CLI / Python API / Cookbook                │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Public API（src/agents/）                            │
│      Agent / Runner / Handoff / Guardrail / Session / Tracing        │
│      Tool / FunctionTool / AgentTool / ComputerUseTool / MCPTool    │
│      Run / RunResult / RunContext / Usage / Item / Message          │
│      RealtimeRunner / VoicePipeline / Runner                        │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Agent / Handoff 抽象                                │
│   agent.py（Agent 类：instructions / tools / handoffs / model）       │
│   _public_agent.py（Public Agent 封装）                              │
│   handoffs/（Handoff 类 + Handoff Tool + 输入 Schema）              │
│   lifecycle.py（Agent Lifecycle）                                   │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Runner / Run / State                                │
│   run.py（Runner.run / run_sync / run_streamed）                    │
│   _run_state_agent_identity.py（RunState + AgentIdentity）          │
│   agent_tool_state.py / agent_tool_input.py（Agent-as-Tool State）  │
│   items.py（RunItem / MessageOutputItem / ToolCallItem / HandoffCallItem）│
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Tools / Function Schema                            │
│   function_schema.py（Function Calling Schema 生成）                │
│   decorators.py（@function_tool 装饰器）                            │
│   computer.py（Computer Use Tool / CUA）                            │
│   editor.py（Editor Tool / apply_diff）                             │
│   apply_diff.py（Diff 应用工具）                                    │
│   mcp/（Model Context Protocol 集成）                                │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Memory / Session                                    │
│   memory/（SessionMemory 抽象）                                     │
│   SQLiteSession / RedisSession / EncryptedSession                  │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Guardrail / Tracing                                 │
│   guardrail.py（InputGuardrail / OutputGuardrail）                  │
│   _debug.py（Debug 信息）                                            │
│   tracing/（Tracing 抽象 / Trace / Span）                            │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Realtime / Voice / Extensions                      │
│   realtime/（RealtimeRunner / RealtimeAgent / RealtimeSession）    │
│   voice/（VoicePipeline / STT / TTS）                                │
│   extensions/（扩展集成）                                            │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Model Client / LiteLLM / Provider                   │
│   model_settings.py（ModelSettings：temperature / tool_choice / reasoning）│
│   exceptions.py（AgentException / MaxTurnsExceeded）                │
│   logger.py（Logger）                                                │
└─────────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  外部依赖（External Dependencies）                   │
│   OpenAI Python SDK / LiteLLM / Pydantic / tiktoken                │
│   SQLite / Redis / Anthropic / Gemini / Ollama                      │
└─────────────────────────────────────────────────────────────────────┘
```

进程模型：OpenAI Agents 是**纯 Python 用户态库**（无独立进程）；Realtime API 通过 WebSocket 实时通信。

## 七、Agent 类型专项（核心扩展章节）

### 7.1 角色协作模式（Role Collaboration）

OpenAI Agents 通过 **Agent + Handoff** 实现四种协作模式：

- **中心化（Orchestrator-Worker）**：
  - **典型实现**：Orchestrator Agent 通过工具调用 AgentTool（其他 Agent 作为工具）；
  - **实现位置**：`src/agents/agent.py`（`agent.as_tool()` 方法）/ `handoffs/`；
  - **API**：`specialist_tool = specialist_agent.as_tool(tool_name="specialist")`；
- **对等（Peer-to-Peer / Handoff）**：
  - **典型实现**：Agent 通过 Handoff 主动交接给另一个 Agent；
  - **实现位置**：`src/agents/handoffs/`（`handoff()` 函数）；
  - **API**：`triage_agent = Agent(handoffs=[billing_agent, technical_agent])`；
  - **机制**：LLM 决策调用 handoff() 函数；
- **分层（Hierarchical / Supervisor）**：
  - **典型实现**：Supervisor Agent → Worker Agent（Worker 不互相 handoff）；
  - **实现位置**：自定义 handoff graph；
  - **API**：通过 `as_tool()` 包装 Worker；
- **市场（Market-based Bidding）**：
  - **典型实现**：Orchestrator 通过 AgentTool 报价选择；
  - **实现位置**：自定义 Orchestrator Agent；
  - **API**：自定义选择逻辑；
- **Handoff Input Filter**：
  - **典型实现**：handoff 时过滤 / 转换输入；
  - **实现位置**：`src/agents/handoffs/`；
  - **API**：`handoff(billing_agent, input_filter=filter_fn)`；
  - **典型场景**：handoff 时清理历史；
- **Run Context**：
  - **典型位置**：`src/agents/run.py`（RunContextWrapper）；
  - **API**：所有 Tool / Guardrail 接收 `RunContext`（用户数据、State）；

### 7.2 记忆系统（Memory System）

OpenAI Agents 记忆分**两层 + Session**：

- **短期（In-memory conversation buffer）**：
  - **实现位置**：`RunContextWrapper.context`；
  - **内容**：当前 Run 的消息历史；
  - **API**：自动维护在 Runner.run() 调用中；
- **长期（Session Memory）**：
  - **实现位置**：`src/agents/memory/`；
  - **类型**：SQLiteSession（本地）/ RedisSession（分布式）/ EncryptedSession（加密）；
  - **API**：`session = SQLiteSession("user_123"); result = await Runner.run(agent, "hi", handle_session=session)`；
  - **持久化**：自动跨 Run 保留消息；
- **Vector Memory**：
  - **不直接支持**，但可通过 Session + 自定义 Vector Store 实现；
- **File Memory**：
  - **不直接支持**，但可通过 Session + 文件系统实现；
- **Memory 压缩与检索**：
  - **不直接支持**，但可通过自定义 Tools 实现；
  - **典型实现**：自定义 Tool 调用 Vector Store；
- **Run Item History**：
  - **实现位置**：`src/agents/items.py`（RunItem / MessageOutputItem / ToolCallItem / HandoffCallItem）；
  - **持久化**：通过 Session 自动持久化；
  - **API**：`result = await Runner.run(...)` 返回 `RunResult`；
  - **类型**：MessageOutputItem / ToolCallItem / ToolCallOutputItem / HandoffCallItem / HandoffOutputItem；

### 7.3 工具调用协议（Tool Calling Protocol）

OpenAI Agents 通过 **Tool / FunctionTool / AgentTool / ComputerUseTool / MCPTool** 实现工具调用：

- **Function Calling（OpenAI / Anthropic / Gemini）**：
  - **实现位置**：`src/agents/function_schema.py`（Function Calling Schema 生成）+ `decorators.py`（`@function_tool` 装饰器）；
  - **支持模型**：OpenAI GPT-4 / Anthropic Claude（通过 LiteLLM）/ Google Gemini（通过 LiteLLM）/ Ollama（通过 LiteLLM）；
  - **绑定工具**：`agent = Agent(name="...", tools=[my_function_tool])`；
- **@function_tool 装饰器**：
  - **API**：`@function_tool; async def my_tool(arg: str) -> str: ...`；
  - **自动生成 Schema**：从 Python 类型注解自动生成 JSON Schema；
- **ReAct（Reasoning + Acting）**：
  - **典型实现**：Agent 默认使用 ReAct 循环；
  - **实现位置**：`src/agents/run.py`；
  - **循环**：每轮 Agent 生成 Thought + Action（Tool 调用）→ 观察结果 → 下一轮；
- **CodeAct（生成代码执行）**：
  - **典型实现**：通过 FunctionTool + Python REPL；
  - **API**：自定义 FunctionTool 调用 Python REPL；
  - **沙箱**：本地 / Docker（生产推荐）；
- **Tool Use + Reflection**：
  - **典型实现**：通过 OutputGuardrail 反思 Tool 输出；
  - **API**：`output_guardrail=my_guardrail`；
- **Structured Outputs**：
  - **实现位置**：`src/agents/agent_output.py`（Pydantic 模型）；
  - **典型实现**：`agent = Agent(..., output_type=MyModel)`；
  - **支持**：OpenAI Structured Outputs API；
- **Computer Use**：
  - **实现位置**：`src/agents/computer.py`（ComputerUseTool）；
  - **API**：`computer = Computer(use="openai")`；
  - **模型**：OpenAI CUA（Computer Use Agent）；
  - **典型场景**：浏览器自动化 / IDE 自动化 / 桌面自动化；
- **Editor Tool**：
  - **实现位置**：`src/agents/editor.py`；
  - **API**：`agent = Agent(tools=[EditorTool()])`；
  - **场景**：代码编辑 / 文件修改；
- **MCP（Model Context Protocol）**：
  - **实现位置**：`src/agents/mcp/`；
  - **API**：`server = MCPServerSse(url="...")` 或 `server = MCPServerStdio(command="...")`；
  - **典型场景**：连接外部 MCP 服务器；
- **Tool Error Handling**：
  - **实现位置**：`src/agents/exceptions.py`；
  - **Fallback**：Tool 内部 try/except；
  - **Retry**：通过 `max_turns` 配置；
- **Toolkit 组合**：
  - **典型**：Agent 可配置多个 Tool + AgentTool + MCPTool + ComputerUseTool；

### 7.4 人机协作（Human-in-the-Loop）

OpenAI Agents HITL 通过 **Guardrail / Session / Function Tool** 实现：

- **InputGuardrail（输入校验）**：
  - **典型实现**：在 Agent 运行前验证用户输入；
  - **实现位置**：`src/agents/guardrail.py`；
  - **API**：`agent = Agent(..., input_guardrails=[my_guardrail])`；
  - **机制**：Guardrail 可抛 `InputGuardrailTripwireTriggered` 中断执行；
- **OutputGuardrail（输出校验）**：
  - **典型实现**：在 Agent 输出后验证结果；
  - **API**：`agent = Agent(..., output_guardrails=[my_guardrail])`；
  - **机制**：Guardrail 可抛 `OutputGuardrailTripwireTriggered` 中断执行；
- **Approval Workflow**：
  - **典型实现**：自定义 FunctionTool 调用 `input("Approve? [y/n]: ")`；
  - **API**：Tool 内部调用 `input()`；
- **Custom HITL**：
  - **典型实现**：在 Tool / Agent 中调用 `input()` 等待人工输入；
- **Session-based HITL**：
  - **典型实现**：通过 Session 持久化对话状态，等待人工回复；
- **Computer Use Approval**：
  - **典型实现**：ComputerUseTool 调用前可请求审批；
- **Tracing & Audit Trail**：
  - **实现位置**：`src/agents/tracing/`；
  - **API**：自动 trace 所有 Agent / Tool / Handoff 调用；
  - **可视化**：OpenAI Dashboard 自动展示；
- **Realtime HITL**：
  - **典型实现**：Realtime API 实时音频对话；
  - **API**：`RealtimeRunner.run()` 自动中断 / 恢复；
- **Voice Pipeline HITL**：
  - **典型实现**：Voice Pipeline（TTS + STT）+ 人工介入；
- **Studio / Dashboard**：
  - **OpenAI Dashboard**：可视化 trace / debug；
  - **API**：自动集成；

## 八、索引机制（Index / Session）

OpenAI Agents 索引通过 **Session** 实现：

- **Session**：
  - **实现位置**：`src/agents/memory/`；
  - **类型**：SQLiteSession / RedisSession / EncryptedSession / 自定义 Session；
  - **API**：`session.add(...)` / `session.get_items(...)` / `session.clear_session()`；
- **Run Item**：
  - **实现位置**：`src/agents/items.py`；
  - **类型**：MessageOutputItem / ToolCallItem / ToolCallOutputItem / HandoffCallItem / HandoffOutputItem；
- **Session Persistence**：
  - **持久化**：自动写入 Session Storage（SQLite / Redis）；
  - **检索**：按 thread_id 检索历史 Run Items；
- **Tracing**：
  - **实现位置**：`src/agents/tracing/`；
  - **API**：`trace = await Runner.run(...); trace.export()`；
  - **持久化**：OpenAI Dashboard 自动展示；

## 九、查询/计算/接口层

### 9.1 Runner 执行引擎（Runner Execution Engine）

- **Runner.run()**：
  - **API**：`result = await Runner.run(agent, "hi")`；
  - **异步执行**：默认异步；
- **Runner.run_sync()**：
  - **API**：`result = Runner.run_sync(agent, "hi")`；
  - **同步执行**：阻塞；
- **Runner.run_streamed()**：
  - **API**：`async for event in Runner.run_streamed(agent, "hi"): print(event)`；
  - **流式执行**：逐事件返回；
- **RunConfig**：
  - **实现位置**：`src/agents/run.py`（RunConfig 类）；
  - **API**：`run_config = RunConfig(model="gpt-4o", max_turns=10)`；
- **Max Turns**：
  - **API**：`max_turns=10`（最大推理轮次）；
  - **场景**：避免无限循环；
- **RealtimeRunner**：
  - **实现位置**：`src/agents/realtime/`；
  - **API**：`runner = RealtimeRunner(agent); await runner.run()`；
  - **场景**：Realtime API 实时音频对话；
- **VoicePipeline**：
  - **实现位置**：`src/agents/voice/`；
  - **API**：`pipeline = VoicePipeline(workflow=SingleAgentWorkflow(agent))`；
  - **场景**：TTS + STT + Agent 流水线；

### 9.2 状态管理（State / Session / RunContext）

- **RunContext**：
  - **实现位置**：`src/agents/run.py`（RunContextWrapper）；
  - **API**：所有 Tool / Guardrail 接收 `RunContext`；
  - **作用**：跨 Agent / Tool 传递上下文；
- **Run State**：
  - **实现位置**：`src/agents/items.py`（RunItem）；
  - **内容**：当前 Run 的所有事件；
  - **API**：`result.new_items` 列出 RunItem；
- **Session State**：
  - **实现位置**：`src/agents/memory/`；
  - **API**：`session.get_items(...)` 检索历史；
- **Handoff State**：
  - **实现位置**：`src/agents/handoffs/`；
  - **机制**：handoff 时携带 context；
- **Agent Tool State**：
  - **实现位置**：`src/agents/agent_tool_state.py` / `agent_tool_input.py`；
  - **机制**：Agent-as-Tool 时携带 state；

### 9.3 多模态工具（Multimodal Tools）

- **Computer Use**：
  - **实现位置**：`src/agents/computer.py`；
  - **API**：`computer = Computer(use="openai", display_width=1920, display_height=1080)`；
  - **场景**：浏览器 / IDE / 桌面自动化；
- **Browser Tool**：
  - **典型实现**：通过 MCP Browser Server；
  - **API**：`MCPServerSse(url="http://localhost:8080")`；
- **Code Executor**：
  - **典型实现**：通过 FunctionTool + Python REPL；
  - **API**：自定义 FunctionTool；
- **File Operations**：
  - **典型实现**：通过 FunctionTool + Python 文件 API；
- **RAG Tool**：
  - **典型实现**：通过 FunctionTool + Vector Store；
- **Database Tool**：
  - **典型实现**：通过 FunctionTool + SQLAlchemy / asyncpg；
- **Search Tool**：
  - **典型实现**：通过 FunctionTool + Tavily / Serper；
- **Multi-modal**：
  - **API**：OpenAI Vision API（gpt-4o vision）；

### 9.4 多 Agent 通信（Multi-Agent Communication）

- **Handoff**：
  - **实现位置**：`src/agents/handoffs/`；
  - **API**：`handoff(other_agent)` 函数；
  - **机制**：Agent 调用 handoff() 主动交接；
- **Agent-as-Tool**：
  - **典型实现**：`specialist_agent.as_tool(...)`；
  - **API**：其他 Agent 作为 Tool 调用；
- **Shared Session**：
  - **典型实现**：所有 Agent 共享同一 Session；
  - **API**：`session = SQLiteSession(...)` 传给所有 Runner.run();
- **RUN CONTEXT**：
  - **典型实现**：通过 RunContextWrapper 传递上下文；
  - **API**：Tool / Guardrail 接收 `RunContext`；
- **A2A Protocol**：
  - **OpenAI 通过 MCP + Realtime API** 实现 A2A；
  - **典型实现**：Remote Agent 作为 MCP Server；

## 十、查询/计算/接口层（API & SDK）

- **Python API（核心）**：
  - `agents.Agent`；
  - `agents.Runner`（`run / run_sync / run_streamed`）；
  - `agents.Handoff`；
  - `agents.Guardrail`（Input / Output）；
  - `agents.Session`（SQLiteSession / RedisSession / EncryptedSession）；
  - `agents.Tool` / `agents.FunctionTool` / `@function_tool`；
  - `agents.Computer` / `agents.ComputerUseTool`；
  - `agents.Editor`；
  - `agents.MCPServer` / `agents.MCPServerSse` / `agents.MCPServerStdio`；
  - `agents.Trace` / `agents.Span`；
  - `agents.RealtimeRunner` / `agents.RealtimeAgent` / `agents.RealtimeSession`；
  - `agents.VoicePipeline` / `agents.VoiceWorkflow`；
  - `agents.RunItem` / `agents.MessageOutputItem` / `agents.ToolCallItem` / `agents.HandoffCallItem`；
  - `agents.RunContextWrapper`；
- **LiteLLM 集成**：
  - `litellm` 包；
  - **API**：`model = LitellmModel(model="claude-3-5-sonnet", api_key=...)`；
- **第三方 SDK**：
  - **OpenAI Python SDK**（`openai` 包）；
  - **MCP**（`mcp` 包）；
- **Tracing 可视化**：
  - **OpenAI Dashboard**：自动展示 trace；
  - **Logfire**：`logfire.configure(); pplk trace()` 集成；
  - **OpenTelemetry**：`set_tracing_export_api(OpenTelemetry)` 集成；

## 十一、生态与集成

- **LLM 集成**：OpenAI GPT-4（默认）/ Anthropic Claude（通过 LiteLLM）/ Google Gemini（通过 LiteLLM）/ Mistral / Ollama / 自部署（通过 LiteLLM）；
- **Vector Store**：通过 FunctionTool + LangChain / LlamaIndex 集成所有 Vector DB；
- **Tool 集成**：FunctionTool + AgentTool + ComputerUseTool + MCPTool + EditorTool；
- **Memory Backend**：SQLite（默认）/ Redis / 自定义；
- **Observability**：OpenAI Dashboard（默认）/ Logfire / OpenTelemetry / 自定义 Tracer；
- **部署**：Docker / Kubernetes / OpenAI Cloud；
- **企业版**：OpenAI Enterprise（Tracing / Realtime / Function Calling 增强）；
- **Cookbook**：OpenAI Cookbook 100+ 示例；
- **Realtime API**：音频对话 Agent；
- **Voice Pipeline**：TTS + STT Agent；
- **Computer Use**：浏览器 / IDE / 桌面自动化；
- **MCP 集成**：filesystem / GitHub / Slack / Google Drive / 自定义；

## 十二、源码学习路径

- **必读模块**（按学习顺序）：
  1. `src/agents/__init__.py`：导出列表；
  2. `src/agents/agent.py`：Agent 类（核心）；
  3. `src/agents/_public_agent.py`：Public Agent 封装；
  4. `src/agents/run.py`：Runner（核心）；
  5. `src/agents/_config.py`：Config；
  6. `src/agents/_config_coercion.py`：Config 强制转换；
  7. `src/agents/decorators.py`：`@function_tool` 装饰器；
  8. `src/agents/function_schema.py`：Function Schema 生成；
  9. `src/agents/model_settings.py`：ModelSettings；
  10. `src/agents/items.py`：RunItem（Message / Tool / Handoff）；
  11. `src/agents/agent_output.py`：Pydantic 输出；
  12. `src/agents/agent_tool_input.py` / `agent_tool_state.py`：Agent-as-Tool；
  13. `src/agents/_run_state_agent_identity.py`：RunState + AgentIdentity；
  14. `src/agents/_tool_identity.py` / `_tool_invocation.py`：Tool 调用；
  15. `src/agents/_function_tool_arguments.py`：Tool 参数；
  16. `src/agents/guardrail.py`：Guardrail（input / output）；
  17. `src/agents/handoffs/`：Handoff 协议；
  18. `src/agents/computer.py`：Computer Use Tool；
  19. `src/agents/editor.py`：Editor Tool；
  20. `src/agents/apply_diff.py`：Diff 应用；
  21. `src/agents/memory/`：Session Memory（SQLiteSession / RedisSession）；
  22. `src/agents/mcp/`：MCP Server / Client；
  23. `src/agents/tracing/`：Tracing；
  24. `src/agents/_debug.py`：Debug；
  25. `src/agents/logger.py`：Logger；
  26. `src/agents/exceptions.py`：Exceptions；
  27. `src/agents/lifecycle.py`：Lifecycle；
  28. `src/agents/realtime/`：RealtimeRunner / RealtimeAgent；
  29. `src/agents/voice/`：VoicePipeline / STT / TTS；
  30. `src/agents/extensions/`：Extensions；
- **LOC 与语言**：
  - `src/agents/` ~50k LoC；
  - `src/agents/extensions/` ~10k LoC；
  - `src/agents/realtime/` ~10k LoC；
  - `src/agents/voice/` ~5k LoC；
  - `src/agents/memory/` ~5k LoC；
  - 主语言：**Python**（95%）、**TypeScript**（5%，openai-agents-js 单独）；
- **构建系统**：
  - **Hatchling**（`pyproject.toml`）；
  - **uv**（依赖管理）；
  - **单包**（`src/agents/` 一包）；
  - **CI/CD**：GitHub Actions；
  - **release-please**（发布管理）；

## 十三、学习路径

- **入门（1 周）**：
  - 阅读官方 Cookbook（https://cookbook.openai.com/）；
  - 跑通 Basic Agent 示例；
  - 理解 Agent / Runner / Handoff 抽象；
  - 跑通 Function Tool 单 Agent 示例；
  - 跑通 Handoff 多 Agent 示例；
- **进阶（2-3 周）**：
  - 深入学习 Guardrail（Input / Output）；
  - 实战 Session（SQLiteSession / RedisSession）；
  - 学习 Tracing（OpenAI Dashboard / Logfire）；
  - 学习 Computer Use Tool；
  - 学习 MCP（MCPServer / MCPClient）；
  - 学习 LiteLLM（多 LLM 路由）；
- **高级（1-2 月）**：
  - 阅读源码（Agent / Runner / Handoff / Guardrail / Session）；
  - 自定义 Guardrail（InputGuardrail / OutputGuardrail）；
  - 自定义 Session（继承 Session 基类）；
  - 自定义 Tool（继承 Tool / FunctionTool）；
  - 自定义 Computer（实现 Mouse / Keyboard 接口）；
  - Realtime API 集成；
  - Voice Pipeline 集成；
  - OpenAI Dashboard 集成；
- **推荐资源**：
  - 官方文档：https://openai.github.io/openai-agents-python/
  - GitHub：https://github.com/openai/openai-agents-python
  - Cookbook：https://cookbook.openai.com/
  - OpenAI Dashboard：https://platform.openai.com/dashboard
  - Realtime API：https://platform.openai.com/docs/guides/realtime
  - Voice Pipeline：https://openai.github.io/openai-agents-python/voice/
  - LiteLLM：https://docs.litellm.ai/
  - Discord：https://discord.gg/openai

## 十四、风险与陷阱

- **OpenAI 绑定**：默认依赖 OpenAI API，需通过 LiteLLM 转 Anthropic / Gemini；
- **LiteLLM 兼容性**：多 LLM 路由版本敏感；
- **Realtime API 成本**：Realtime API 价格较高；
- **Computer Use 风险**：自动操作系统有安全风险，必须 sandbox；
- **Session 持久化**：SQLiteSession 默认本地，生产需 RedisSession；
- **Guardrail 误杀**：Guardrail 阈值需调，避免误杀；
- **Handoff 死循环**：Agent 间 handoff 可能形成环路；
- **Tracing 成本**：Tracing 详细级别高时 token 多；
- **Tool 错误处理**：Tool 内部需 try/except；
- **Max Turns 限制**：默认 max_turns=10 需根据任务调整；
- **Performance 问题**：Python 调度开销大；
- **Voice Pipeline 延迟**：STT + TTS 延迟较高；
- **MCP 不稳定**：MCP 协议还在演进；
- **Cookbook 示例错误**：部分 Cookbook 示例代码可能 outdated；
- **Studio 功能缺失**：无官方 Web Studio（除 Dashboard）；

## 十六、参考资料

- 官方仓库：https://github.com/openai/openai-agents-python
- 官方文档：https://openai.github.io/openai-agents-python/
- Cookbook：https://cookbook.openai.com/
- OpenAI Dashboard：https://platform.openai.com/dashboard
- Realtime API：https://platform.openai.com/docs/guides/realtime
- Voice Pipeline：https://openai.github.io/openai-agents-python/voice/
- LiteLLM：https://docs.litellm.ai/
- Swarm（前身）：https://github.com/openai/swarm
- 论文：Realtime API 文档
- 论文：Computer Use (CUA) 文档
- Discord：https://discord.gg/openai
- Forum：https://community.openai.com/

## 十七、与本项目关联

OpenAI Agents 作为「OpenAI 官方 + Handoff + Guardrail + Session + Tracing + Computer Use」行业代表 Agent SDK，与本项目（engineering/ 轨道 C/C++ 自研存储与向量索引）在「轻量级 Agent + Handoff 协议 + Session 持久化 + Guardrail 拦截」四个维度具有直接参考价值：

- **Agent + Handoff ↔ Index AM**：Agent 的轻量级抽象与本项目索引 AM（`engineering/src/db/index/`）的轻量级接口相似——都是"**声明式 + 参数化 + 可组合**"。
- **Handoff 协议 ↔ Handoff Routing**：Handoff 协议（`src/agents/handoffs/`）与本项目 SQL 优化器的 Handoff Routing（如子查询下推）思路相似——都是"**控制权转移 + 上下文保留**"。
- **Session Memory ↔ WAL Buffer**：Session Memory（`src/agents/memory/`）与本项目 WAL Buffer（`engineering/src/db/storage/wal_buf/`）的"**持久化 + 跨会话保留**"机制相似。
- **Guardrail ↔ GUC + 触发器**：Guardrail（`src/agents/guardrail.py`）与本项目 GUC（`engineering/src/db/core/guc.c`）+ Trigger（`engineering/src/db/executor/trigger.c`）的"**拦截 + 验证 + 决策**"机制相似。
- **Computer Use ↔ Code Executor**：Computer Use Tool（`src/agents/computer.py`）与本项目 Code Executor（`engineering/src/db/executor/`）+ Sandbox 沙箱的"**代码生成 + 执行 + 限制**"机制相似。
- **Function Tool ↔ Index 工具**：Function Tool（`src/agents/function_schema.py`）与本项目索引工具（`engineering/src/db/index/`）的"**Schema 声明 + 自动生成 + 可调用**"机制相似。
- **Tracing ↔ APM**：Tracing（`src/agents/tracing/`）与本项目自定义 APM 协议对照——都是"**Span + Trace + 观测**"思路。
- **MCP ↔ DB 协议**：MCP（`src/agents/mcp/`）与本项目 PG Wire Protocol（`engineering/src/server/pgwire/`）+ 自定义协议（`engineering/src/server/pg_server.h`）对照。
- **Realtime API ↔ Realtime DB**：Realtime API（`src/agents/realtime/`）与本项目实时数据库（`engineering/src/db/realtime/`）的"**实时 + 双向 + 长连接**"思路相似。
- **Voice Pipeline ↔ 多模态 Pipeline**：Voice Pipeline（`src/agents/voice/`）与本项目多模态 RAG 应用（`engineering/apps/multimodal_rag/`）的"**多模态 + 流水线 + 异步**"思路相似。
- **LiteLLM ↔ Multi-LLM Gateway**：LiteLLM（通过 LiteLLM SDK） 与本项目 LiteLLM 集成（`engineering/src/llm/llm.h`）对照——都是"**多模型路由 + 统一接口**"。
- **学习建议**：阅读 OpenAI Agents 源码重点关注「Agent 抽象 + Runner 执行 + Handoff 协议 + Guardrail 拦截 + Session 持久化」，与本项目 `engineering/src/db/index/` + `engineering/src/db/executor/` 做架构对照，加深对「轻量级 Agent + 拦截 + 持久化」的理解。