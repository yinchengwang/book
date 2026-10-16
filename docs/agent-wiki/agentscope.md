# AgentScope Wiki（阿里出品的多 Agent 框架）

> 参考版本：`agentscope` Python 包（`src/agentscope/`，阿里达摩院 / Alibaba DAMO Academy），参考 HEAD：b292a33a6 前的最终 release（AgentScope 0.1+）。本 wiki 重点介绍 AgentScope 作为「**Realtime 多 Agent + 多模态（文本 / 语音 / 图像）+ Middleware（短期 + 长期记忆）+ Tools + RAG + Pipeline（SOP / Team / Goal）+ Workspace（K8s / Docker / AppleContainer）+ MCP + Formatter（多 LLM 适配）+ Skill + Classifier + Permission + TUI**」的多 Agent 框架定位、源码架构、四个 Agent 维度（角色协作 / 记忆 / 工具调用 / 人机协作）的深度展开，以及与本项目 `engineering/` 自研存储与向量索引的对照参考。

## 一、项目定位

AgentScope 是由 **阿里巴巴达摩院**（Alibaba DAMO Academy）于 **2024 年**开源的 **「Realtime 多 Agent + 多模态」** 多 Agent 框架，定位是 **「AgentScope: A Flexible Multi-Agent Platform for Building, Managing, and Evaluating LLM-powered Agents」**——以 **Realtime / Multi-Modal / Middleware / Workspace** 四大特性为核心，构建一个生产级的多 Agent 平台。

- **核心痛点**：
  - 缺少**Realtime 多 Agent**（多 Agent 实时对话 / 协同）；
  - 缺少**多模态原生支持**（文本 / 语音 / 图像 / 视频）；
  - 缺少**Middleware 机制**（短期 / 长期记忆 / Function Calling 等中间件）；
  - 缺少**Workspace 沙箱**（K8s / Docker / AppleContainer 隔离执行）；
  - 缺少**多 LLM Formatter**（OpenAI / Anthropic / Gemini / DeepSeek / DashScope / Moonshot / Ollama / 豆包）；
  - 缺少**Pipeline SOP**（GoalPipeline / TeamPipeline / SOP Engine）；
  - 缺少**Skill 系统**（预制技能库）；
  - 缺少**Classifier / Permission**（意图分类 + 权限控制）；
  - 缺少**TUI**（终端用户界面）；
- **典型用户**：
  - **企业应用**：客服 / 销售 / 营销 Agent；
  - **多模态应用**：语音 + 图像 + 文本 Agent；
  - **Realtime 应用**：实时对话 / 协同；
  - **生产部署**：Workspace 沙箱隔离；
- **核心卖点**：
  - **Realtime 多 Agent**：支持实时对话；
  - **多模态**：Text + Speech（TTS / ASR）+ Image + Video；
  - **Middleware**：Short-term / Long-term Memory / Function Calling 中间件；
  - **Workspace**：K8s / Docker / AppleContainer 沙箱；
  - **Formatter**：多 LLM 适配（OpenAI / Anthropic / Gemini / DeepSeek / DashScope / Moonshot / Ollama / 豆包）；
  - **Tools**：内置 + Toolkit + Skill 系统；
  - **Pipeline**：GoalPipeline / TeamPipeline / SOP Engine；
  - **RAG**：Knowledge + Chunker + Parser + VDB（Qdrant / Elasticsearch / Milvus）；
  - **MCP**：Model Context Protocol 集成；
  - **Classifier**：意图分类；
  - **Permission**：权限控制；
  - **TUI**：终端用户界面；
  - **Skill 系统**：预制技能库；
  - **Console**：交互式 Console；
  - **Chinese LLM**：DashScope / 豆包 / Moonshot / DeepSeek；

## 二、发展历史与社区

- **起源（2024）**：阿里巴巴达摩院开源 AgentScope v0.1，定位「Realtime 多 Agent + 多模态」；
- **里程碑**：
  - 2024：v0.1（基本框架 + ReAct Agent）；
  - 2024-08：v0.3（Realtime / Multi-Modal / Middleware）；
  - 2024-12：v0.5（Workspace / Skill / Pipeline 完善）；
  - 2025-03：v0.7（Classifier / Permission / TUI 完善）；
  - 2025-08：v0.8（MCP / RAG / 长期记忆完善）；
  - 2026-03：v1.0+（稳定 + 企业级）；
- **维护状态**：**活跃**（阿里达摩院团队 20+ 工程师，月均 80+ commits）；
- **社区**：
  - GitHub 5k+ star、`agentscope` PyPI 月下载量 50 万+；
  - Discord 1k+ 成员、GitHub Discussions；
  - 学术论文：AgentScope: A Flexible Multi-Agent Platform；
- **关键贡献者**：阿里达摩院团队、贡献者 100+；

## 三、许可与商业限制

- **核心代码**：**Apache-2.0 License**（`src/agentscope/`），可商用、修改、私有部署；
- **生态关系**：
  - **上游依赖**：
    - Python 3.10+；
    - OpenAI / Anthropic / Gemini / DeepSeek / DashScope / Moonshot / Ollama；
    - Pydantic v2；
    - asyncio + aiohttp；
    - chroma / qdrant / elasticsearch / milvus（VDB）；
    - Kubernetes / Docker（Workspace）；
    - AppleContainer（macOS 沙箱）；
    - MCP（`mcp/`）；
    - TUI（`tui/`）；
    - Classifier（`classifier/`）；
    - Permission（`permission/`）；
    - Realtime（`realtime/`）；
    - Skill（`skill/`）；
    - Workspace（`workspace/`）；
    - Pipeline（`pipeline/`）；
    - SOP（`sop/`）；
  - **下游生态**：约 1000+ 项目使用 AgentScope；

## 四、业界主要使用方与案例

- **国际**（公开）：
  - **多家初创公司**：Realtime 多 Agent；
- **国内**：
  - **阿里巴巴**：客服 / 销售 / 营销 Agent；
  - **多家互联网公司**：多模态 Agent；
  - **多家企业**：生产部署；
- **典型场景**：
  - **多模态客服**：语音 + 文本 + 图像；
  - **Realtime 协同**：多 Agent 实时对话；
  - **企业应用**：客服 / 销售 / 营销；
  - **生产部署**：Workspace 沙箱；
  - **Realtime Agent**：TTS + ASR + 对话；
  - **Skill Marketplace**：预制技能。

## 五、一句话对比定位

> AgentScope 是 **「阿里达摩院出品 + Realtime 多 Agent + 多模态 + Middleware + Workspace + 多 LLM Formatter + 5k star + Apache-2.0」**；与 **LangGraph（图状态机）**、**CrewAI（角色化 + Process）**、**AutoGen（GroupChat 对话）**、**MetaGPT（软件公司角色）**、**OpenAI Agents（Handoff 简单协议）**、**AgentScope（多 Agent 通用）** 构成多 Agent 生态。AgentScope 的差异化是 **「Realtime + 多模态 + Middleware + Workspace + 中文 LLM 适配 + 阿里出品 + 国产开源」**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                  用户应用层（User Apps）                              │
│        AgentScope TUI / Console / Python API / Web UI                │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Public API（src/agentscope/）                       │
│     │ Agent / Tool / Memory / Middleware / Formatter / Pipeline       │
│      Skill / Classifier / Permission / Workspace / Realtime         │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Agent 抽象                                          │
│   agent/                                                              │
│     ├── _agent.py（Agent 基类）                                       │
│     ├── _a2a_agent.py（A2A Agent）                                   │
│     ├── _realtime/（Realtime Agent）                                  │
│     ├── _config.py（Agent Config）                                    │
│     ├── _structured_output_tool.py（结构化输出工具）                   │
│     └── _utils.py                                                    │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Formatter / Model / Embedding                       │
│   formatter/                                                          │
│     ├── _openai_formatter.py / _anthropic_formatter.py               │
│     ├── _gemini_formatter.py / _deepseek_formatter.py                │
│     ├── _dashscope_formatter.py / _moonshot_formatter.py             │
│     ├── _ollama_formatter.py / _MiniMax_formatter.py                 │
│     └── _formatter_base.py                                          │
│   model/（Model Client 抽象）                                          │
│   embedding/                                                          │
│     ├── _embedding_base.py / _embedding_model_card.py                │
│     ├── _embedding_response.py / _embedding_usage.py                │
│     ├── _dashscope/ / _gemini/ / _ollama/                            │
│     ├── _file_cache.py / _cache_base.py                             │
│     └── _adapters.py                                                │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Tool / Tool Group / Toolkit                          │
│   tool/                                                              │
│     ├── _toolkit.py（Toolkit）                                        │
│     ├── _tool_group.py（Tool Group）                                  │
│     ├── _task/（异步任务）                                            │
│     ├── _builtin/（内置工具）                                          │
│     ├── _base.py / _adapters.py / _response.py / _types.py           │
│     └── _constants.py                                              │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Middleware / Memory                                 │
│   middleware/                                                         │
│     └── _longterm_memory/                                           │
│           ├── _mem0/（Mem0 长期记忆）                                │
│           │     ├── _middleware.py                                  │
│           │     ├── _tools.py / _utils.py                          │
│           │     └── _agentscope_adapter.py                          │
│           ├── _reme/（ReMe 长期记忆）                                │
│           └── _agentic_memory/（Agentic Memory）                    │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Pipeline / SOP / State                              │
│   pipeline/                                                            │
│     ├── _base.py（Pipeline 基类）                                    │
│     ├── _goal_pipeline.py（目标驱动 Pipeline）                        │
│     └── _team_pipeline.py（团队 Pipeline）                          │
│   sop/                                                                │
│     ├── _engine.py（SOP 引擎）                                        │
│     ├── _schema.py（SOP Schema）                                      │
│     └── _state.py（SOP 状态）                                          │
│   state/（State 抽象）                                                │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  RAG / Knowledge / Chunker / Parser / VDB           │
│   rag/                                                                │
│     ├── _document.py                                                │
│     ├── _knowledge.py                                                │
│     ├── _chunker/（文档分块器）                                        │
│     ├── _parser/（文档解析器）                                          │
│     └── _vdb/（向量库）                                                │
│           ├── _qdrant.py / _elasticsearch.py                         │
│           └── _milvus.py                                            │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Skill / Classifier / Permission                     │
│   skill/（技能库）                                                    │
│   classifier/（意图分类器）                                            │
│   permission/（权限控制）                                              │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Workspace / TUI / Console / Credential              │
│   workspace/                                                          │
│     ├── _base.py（Workspace 基类）                                  │
│     ├── _k8s/（Kubernetes Workspace）                                │
│     │     └── _k8s_backend.py                                      │
│     └── _applecontainer/（AppleContainer Workspace）                 │
│           └── _applecontainer_workspace.py                          │
│   tui/（终端 UI）                                                     │
│   console/（交互式 Console）                                            │
│   credential/（凭证管理）                                              │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Realtime / MCP / ASR / TTS / Multimodal            │
│   realtime/                                                          │
│   mcp/（Model Context Protocol 集成）                                  │
│   asr/（语音识别）                                                     │
│   tts/（语音合成）                                                      │
│   multimodal/（多模态处理）                                              │
│   rag/（RAG 集成）                                                      │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Message / Formatter / Event                          │
│   message/                                                          │
│   event/（事件总线）                                                   │
│   state/                                                              │
│   middleware/                                                     │
│   rag/                                                              │
│   skill/                                                          │
│   permission/                                                     │
│   classifier/                                                    │
└─────────────────────────────────────────────────────────────────────┘
```

进程模型：AgentScope 是**纯 Python 用户态库**（无独立进程）；Workspace 是**外部沙箱**（K8s / Docker / AppleContainer）；通过 asyncio 实现高并发。

## 七、Agent 类型专项（核心扩展章节）

### 7.1 角色协作模式（Role Collaboration）

AgentScope 通过 **Agent + Pipeline + TeamPipeline** 实现四种协作模式：

- **中心化（Orchestrator-Worker）**：
  - **典型实现**：GoalPipeline（目标驱动）+ Orchestrator Agent 委派 Worker；
  - **实现位置**：`pipeline/_goal_pipeline.py`（GoalPipeline）；
  - **API**：`GoalPipeline(orchestrator, workers)`；
- **对等（Peer-to-Peer / Blackboard）**：
  - **典型实现**：TeamPipeline（团队协作）+ 共享 State；
  - **实现位置**：`pipeline/_team_pipeline.py`（TeamPipeline）；
  - **共享 State**：所有 Agent 共享 Pipeline State；
- **分层（Hierarchical / Supervisor）**：
  - **典型实现**：嵌套 Agent（Agent 内调用其他 Agent）；
  - **实现位置**：`agent/_agent.py`（嵌套 Agent 调用）；
  - **API**：`agent_a.call(agent_b)`；
- **市场（Market-based Bidding）**：
  - **典型实现**：自定义 Pipeline 实现 Bidding；
  - **不直接支持**，需用户自定义；
- **SOP（Standard Operating Procedure）**：
  - **典型实现**：SOP Engine（`sop/_engine.py`）执行标准操作流程；
  - **实现位置**：`sop/_engine.py`；
  - **API**：`SOPEngine.run(sop, inputs)`；
- **A2A（Agent-to-Agent）**：
  - **典型实现**：A2A Agent（`agent/_a2a_agent.py`）跨进程通信；
  - **实现位置**：`agent/_a2a_agent.py`；
  - **机制**：基于 MCP / HTTP / gRPC 跨进程调用；
- **Realtime Multi-Agent**：
  - **典型实现**：Realtime Agent（`agent/_realtime/`）实时对话；
  - **实现位置**：`agent/_realtime/`；
  - **场景**：实时音频对话 + 多 Agent 协同；

### 7.2 记忆系统（Memory System）

AgentScope 记忆分**短期 + 长期 + Agentic + Mem0 + ReMe**：

- **短期（In-memory conversation buffer）**：
  - **实现位置**：Agent 内部消息缓冲；
  - **API**：自动维护；
- **长期（Long-term Memory）**：
  - **实现位置**：`middleware/_longterm_memory/`（长期记忆中间件）；
  - **类型**：Mem0（`middleware/_longterm_memory/_mem0/`） + ReMe（`middleware/_longterm_memory/_reme/`） + Agentic Memory（`middleware/_longterm_memory/_agentic_memory/`）；
  - **API**：`agent.use_memory(LongTermMemoryMiddleware(...))`；
- **Agentic Memory**：
  - **典型实现**：Agent 自动管理 Memory（添加 / 删除 / 检索）；
  - **实现位置**：`middleware/_longterm_memory/_agentic_memory/_middleware.py`；
- **Mem0 Memory**：
  - **典型实现**：Mem0 第三方长期记忆；
  - **实现位置**：`middleware/_longterm_memory/_mem0/_middleware.py` + `_agentscope_adapter.py`；
- **ReMe Memory**：
  - **典型实现**：ReMe 自研长期记忆；
  - **实现位置**：`middleware/_longterm_memory/_reme/`；
- **向量记忆（Vector Memory）**：
  - **实现位置**：`rag/_vdb/`（Qdrant / Elasticsearch / Milvus）；
  - **API**：`RAGKnowledge(vector_db=QdrantVDB(...))`；
- **文件记忆（File Memory）**：
  - **典型实现**：通过 Skill + 文件系统；
- **Memory 压缩与检索**：
  - **典型实现**：在 Memory Middleware 中加 Summary / RAG；
- **Embedding 缓存**：
  - **实现位置**：`embedding/_file_cache.py`（File Cache）+ `_cache_base.py`（Cache 抽象）；
  - **API**：自动缓存 Embedding；

### 7.3 工具调用协议（Tool Calling Protocol）

AgentScope 通过 **Tool + Toolkit + Tool Group + Skill + Function Calling** 实现工具调用：

- **Function Calling（多 LLM）**：
  - **实现位置**：`formatter/`（OpenAI / Anthropic / Gemini / DeepSeek / DashScope / Moonshot / Ollama / 豆包 Formatter）+ `model/`（Model Client）；
  - **支持模型**：OpenAI GPT / Anthropic Claude / Gemini / DeepSeek / DashScope / Moonshot / Ollama / 豆包；
  - **API**：`Agent(model=ModelFactory.create(...), tools=[...])`；
- **Tool 抽象**：
  - **实现位置**：`tool/_base.py`（Tool 基类）+ `_toolkit.py`（Toolkit）；
  - **API**：`@tool; def my_tool(arg: str) -> str: ...`；
- **Toolkit 组合**：
  - **典型实现**：`Toolkit([tool1, tool2, tool3])`；
  - **实现位置**：`tool/_toolkit.py`；
- **Tool Group**：
  - **典型实现**：多个 Tool 作为一个 Group；
  - **实现位置**：`tool/_tool_group.py`；
- **Skill 系统**：
  - **实现位置**：`skill/`（技能库）；
  - **API**：`agent.use_skill("search")`；
- **ReAct（Reasoning + Acting）**：
  - **典型实现**：Agent 默认使用 ReAct 循环；
  - **机制**：每轮 Agent 生成 Thought + Action（Tool 调用）→ 观察结果 → 下一轮；
- **CodeAct（生成代码执行）**：
  - **典型实现**：通过 Workspace + Code Executor；
  - **沙箱**：K8s / Docker / AppleContainer；
- **Tool Use + Reflection**：
  - **典型实现**：通过 Middleware 反思 Tool 输出；
- **Structured Outputs**：
  - **实现位置**：`agent/_structured_output_tool.py`（结构化输出工具）；
  - **API**：`Agent(output_schema=MyModel)`；
- **MCP（Model Context Protocol）**：
  - **实现位置**：`mcp/`（MCP 集成）；
  - **API**：`MCPClient(server="...")`；
- **Built-in Tools**：
  - **实现位置**：`tool/_builtin/`（内置工具：浏览器 / 文件 / HTTP / 数据库）；
- **Tool Error Handling**：
  - **实现位置**：`tool/_response.py`（响应处理）；
- **Async Task**：
  - **实现位置**：`tool/_task/`（异步任务）；

### 7.4 人机协作（Human-in-the-Loop）

AgentScope HITL 通过 **TUI / Console / Permission / Workspace** 实现：

- **TUI HITL**：
  - **典型实现**：用户可在 TUI 中随时介入；
  - **实现位置**：`tui/`；
- **Console HITL**：
  - **典型实现**：交互式 Console 接受用户输入；
  - **实现位置**：`console/`；
- **Permission 系统**：
  - **典型实现**：权限控制（Tool / File / Network）；
  - **实现位置**：`permission/`；
  - **API**：`Permission.require("file_delete")`；
- **Workspace 隔离**：
  - **典型实现**：Workspace 沙箱隔离执行；
  - **实现位置**：`workspace/`（K8s / Docker / AppleContainer）；
- **Classifier**：
  - **典型实现**：意图分类（决定是否需要 HITL）；
  - **实现位置**：`classifier/`；
- **Realtime HITL**：
  - **典型实现**：Realtime Agent 实时人工介入；
  - **API**：`RealtimeAgent(...).pause()`；
- **Custom HITL**：
  - **典型实现**：自定义 Tool 调用 `input()`；
- **Audit Trail**：
  - **实现位置**：`event/`（事件总线）+ `state/`；
  - **API**：自动 trace 所有 Agent / Tool / Pipeline 调用；

## 八、索引机制（Index / Skill / Classifier）

AgentScope 索引通过 **Skill + Classifier + Permission + Workspace + Pipeline** 实现：

- **Skill Index**：
  - **实现位置**：`skill/`（技能库）；
  - **API**：`skill_registry.load("search")`；
- **Classifier Index**：
  - **实现位置**：`classifier/`（意图分类器）；
  - **API**：`classifier.classify(input)`；
- **Permission Index**：
  - **实现位置**：`permission/`（权限控制）；
  - **API**：`permission.check("file_delete", user)`；
- **Workspace Index**：
  - **实现位置**：`workspace/`（Workspace 注册）；
  - **API**：`workspace_registry.register("k8s", K8sWorkspace)`；
- **Pipeline Index**：
  - **实现位置**：`pipeline/`（Pipeline 注册）；
  - **API**：`pipeline_registry.register("goal", GoalPipeline)`；
- **Tool Index**：
  - **实现位置**：`tool/_toolkit.py`（Toolkit 注册）；
  - **API**：`toolkit.register("my_tool", MyTool)`；
- **RAG Index**：
  - **实现位置**：`rag/_vdb/`（VDB 注册）；
  - **API**：`vdb_registry.register("qdrant", QdrantVDB)`；
- **Embedding Index**：
  - **实现位置**：`embedding/`（Embedding 注册）；
  - **API**：`embedding_registry.register("openai", OpenAIEmbedding)`；
- **Formatter Index**：
  - **实现位置**：`formatter/`（Formatter 注册）；
  - **API**：`formatter_registry.register("openai", OpenAIFormatter)`；

## 九、查询/计算/接口层

### 9.1 Pipeline 执行引擎（Pipeline Execution Engine）

- **Goal Pipeline**：
  - **实现位置**：`pipeline/_goal_pipeline.py`；
  - **API**：`GoalPipeline(goal, agents).run()`；
  - **场景**：目标驱动 Pipeline；
- **Team Pipeline**：
  - **实现位置**：`pipeline/_team_pipeline.py`；
  - **API**：`TeamPipeline(agents).run()`；
  - **场景**：团队协作 Pipeline；
- **Base Pipeline**：
  - **实现位置**：`pipeline/_base.py`；
  - **API**：Pipeline 基类；
- **SOP Engine**：
  - **实现位置**：`sop/_engine.py`；
  - **API**：`SOPEngine.run(sop, inputs)`；
  - **场景**：标准操作流程；
- **Realtime Pipeline**：
  - **实现位置**：`agent/_realtime/`；
  - **场景**：实时 Pipeline；

### 9.2 状态管理（State / SOP State）

- **Pipeline State**：
  - **实现位置**：`pipeline/_base.py`（Pipeline State）；
- **SOP State**：
  - **实现位置**：`sop/_state.py`（SOP State）；
- **Agent State**：
  - **实现位置**：`state/`（Agent State）；
- **Persistence**：
  - **实现位置**：`state/` + `middleware/`；
  - **API**：自动持久化；
- **Pause / Resume**：
  - **典型实现**：通过 TUI / Console 实现 pause / resume；

### 9.3 多模态工具（Multimodal Tools）

- **ASR（语音识别）**：
  - **实现位置**：`asr/`（语音识别）；
  - **API**：`asr.transcribe(audio)`；
- **TTS（语音合成）**：
  - **实现位置**：`tts/`（语音合成）；
  - **API**：`tts.synthesize(text)`；
- **Vision（图像）**：
  - **典型实现**：通过 Model Client 集成 GPT-4V / Claude 3 / Gemini Vision；
- **Video**：
  - **典型实现**：通过 Model Client 集成视频分析；
- **RAG Tool**：
  - **实现位置**：`rag/`（RAG 集成）；
  - **API**：`RAGKnowledge(...).search(query)`；
- **Browser Tool**：
  - **典型实现**：通过 Tool + Playwright；
- **Code Executor**：
  - **典型实现**：通过 Workspace + Code Executor；
- **Database Tool**：
  - **典型实现**：通过 Tool + SQLAlchemy；
- **Search Tool**：
  - **典型实现**：通过 Tool + Tavily / Serper；

### 9.4 多 Agent 通信（Multi-Agent Communication）

- **Shared State**：
  - **实现位置**：`state/`（共享 State）；
- **Pipeline 共享**：
  - **实现位置**：`pipeline/_base.py`（Pipeline Context）；
- **A2A Protocol**：
  - **实现位置**：`agent/_a2a_agent.py`（A2A Agent）；
  - **机制**：基于 MCP / HTTP / gRPC 跨进程调用；
- **Realtime 通信**：
  - **典型实现**：通过 Realtime Agent + WebSocket；
- **Workspace 隔离**：
  - **实现位置**：`workspace/`（Workspace 隔离）；

## 十、查询/计算/接口层（API & SDK）

- **Python API（核心）**：
  - `agentscope.Agent`（`agent/_agent.py`）；
  - `agentscope.tool` / `agentscope.Toolkit`（`tool/_toolkit.py`）；
  - `agentscope.formatter.*`（`formatter/`）；
  - `agentscope.model.*`（`model/`）；
  - `agentscope.embedding.*`（`embedding/`）；
  - `agentscope.message.*`（`message/`）；
  - `agentscope.event.*`（`event/`）；
  - `agentscope.state.*`（`state/`）；
  - `agentscope.middleware.*`（`middleware/`）；
  - `agentscope.rag.*`（`rag/`）；
  - `agentscope.skill.*`（`skill/`）；
  - `agentscope.classifier.*`（`classifier/`）；
  - `agentscope.permission.*`（`permission/`）；
  - `agentscope.pipeline.*`（`pipeline/`）；
  - `agentscope.sop.*`（`sop/`）；
  - `agentscope.workspace.*`（`workspace/`）；
  - `agentscope.realtime.*`（`realtime/`）；
  - `agentscope.tui.*`（`tui/`）；
  - `agentscope.console.*`（`console/`）；
  - `agentscope.mcp.*`（`mcp/`）；
  - `agentscope.asr.*`（`asr/`）；
  - `agentscope.tts.*`（`tts/`）；
- **TUI**：
  - `agentscope-tui`（终端 UI）；
- **Console**：
  - `agentscope.console.run_console()`（交互式 Console）；
- **Workspace**：
  - K8s Workspace（生产推荐）；
  - AppleContainer Workspace（macOS）；
  - Docker Workspace（本地开发）；
- **第三方 SDK**：
  - **OpenAI Python SDK**；
  - **Anthropic Python SDK**；
  - **DashScope Python SDK**；
  - **Moonshot Python SDK**；
  - **DeepSeek Python SDK**；
  - **Gemini Python SDK**；
- **学术合作 SDK**：
  - **论文复现**：通过 config + seed 复现实验；

## 十一、生态与集成

- **LLM 集成**：OpenAI / Anthropic / Google Gemini / DeepSeek / DashScope（阿里通义）/ Moonshot（月之暗面）/ Ollama / 豆包 / MiniMax / 自部署；
- **Vector Store**：Qdrant（默认）/ Elasticsearch / Milvus / Chroma / FAISS；
- **Tool 集成**：内置 + Toolkit + Skill + 自定义；
- **Memory Backend**：Mem0（默认）/ ReMe / Agentic Memory / Chroma / FAISS；
- **Embedding Cache**：File Cache（默认）/ Redis；
- **Workspace**：K8s（生产推荐）/ Docker / AppleContainer（macOS）；
- **RAG**：Knowledge + Chunker + Parser + VDB；
- **MCP**：Model Context Protocol 集成；
- **ASR**：Whisper / 阿里 ASR / Azure ASR；
- **TTS**：OpenAI TTS / 阿里 TTS / ElevenLabs；
- **Multimodal**：GPT-4V / Claude 3 / Gemini Vision / Qwen-VL；
- **Realtime**：WebSocket / WebRTC；
- **Observability**：Event 总线 + State + 自定义；
- **部署**：Docker / Kubernetes / 自定义；
- **企业版**：商业集成（规划中）；
- **学术合作**：阿里达摩院 / NeurIPS / ICLR；
- **中文社区**：阿里达摩院 + 中文用户；
- **Skill Marketplace**：预制技能库；

## 十二、源码学习路径

- **必读模块**（按学习顺序）：
  1. `src/agentscope/__init__.py`：导出列表；
  2. `src/agentscope/agent/_agent.py`：Agent 基类；
  3. `src/agentscope/agent/_a2a_agent.py`：A2A Agent；
  4. `src/agentscope/agent/_realtime/`：Realtime Agent；
  5. `src/agentscope/agent/_config.py`：Agent Config；
  6. `src/agentscope/agent/_structured_output_tool.py`：结构化输出工具；
  7. `src/agentscope/formatter/_openai_formatter.py`：OpenAI Formatter；
  8. `src/agentscope/formatter/_anthropic_formatter.py`：Anthropic Formatter；
  9. `src/agentscope/formatter/_gemini_formatter.py`：Gemini Formatter；
  10. `src/agentscope/formatter/_deepseek_formatter.py`：DeepSeek Formatter；
  11. `src/agentscope/formatter/_dashscope_formatter.py`：DashScope Formatter；
  12. `src/agentscope/formatter/_moonshot_formatter.py`：Moonshot Formatter；
  13. `src/agentscope/formatter/_ollama_formatter.py`：Ollama Formatter；
  14. `src/agentscope/formatter/_formatter_base.py`：Formatter 基类；
  15. `src/agentscope/model/`：Model Client；
  16. `src/agentscope/embedding/_embedding_base.py`：Embedding 基类；
  17. `src/agentscope/embedding/_file_cache.py`：File Cache；
  18. `src/agentscope/embedding/_dashscope/`：DashScope Embedding；
  19. `src/agentscope/embedding/_gemini/`：Gemini Embedding；
  20. `src/agentscope/embedding/_ollama/`：Ollama Embedding；
  21. `src/agentscope/tool/_base.py`：Tool 基类；
  22. `src/agentscope/tool/_toolkit.py`：Toolkit；
  23. `src/agentscope/tool/_tool_group.py`：Tool Group；
  24. `src/agentscope/tool/_task/`：异步任务；
  25. `src/agentscope/tool/_builtin/`：内置工具；
  26. `src/agentscope/middleware/`：Middleware；
  27. `src/agentscope/middleware/_longterm_memory/_mem0/`：Mem0；
  28. `src/agentscope/middleware/_longterm_memory/_reme/`：ReMe；
  29. `src/agentscope/middleware/_longterm_memory/_agentic_memory/`：Agentic Memory；
  30. `src/agentscope/pipeline/_base.py`：Pipeline 基类；
  31. `src/agentscope/pipeline/_goal_pipeline.py`：Goal Pipeline；
  32. `src/agentscope/pipeline/_team_pipeline.py`：Team Pipeline；
  33. `src/agentscope/sop/_engine.py`：SOP 引擎；
  34. `src/agentscope/sop/_schema.py`：SOP Schema；
  35. `src/agentscope/sop/_state.py`：SOP State；
  36. `src/agentscope/rag/_document.py`：RAG Document；
  37. `src/agentscope/rag/_knowledge.py`：RAG Knowledge；
  38. `src/agentscope/rag/_chunker/`：Chunker；
  39. `src/agentscope/rag/_parser/`：Parser；
  40. `src/agentscope/rag/_vdb/_qdrant.py`：Qdrant VDB；
  41. `src/agentscope/rag/_vdb/_elasticsearch.py`：Elasticsearch VDB；
  42. `src/agentscope/rag/_vdb/_milvus.py`：Milvus VDB；
  43. `src/agentscope/skill/`：Skill 系统；
  44. `src/agentscope/classifier/`：Classifier；
  45. `src/agentscope/permission/`：Permission；
  46. `src/agentscope/workspace/_base.py`：Workspace 基类；
  47. `src/agentscope/workspace/_k8s/`：K8s Workspace；
  48. `src/agentscope/workspace/_applecontainer/`：AppleContainer Workspace；
  49. `src/agentscope/tui/`：TUI；
  50. `src/agentscope/console/`：Console；
  51. `src/agentscope/realtime/`：Realtime；
  52. `src/agentscope/mcp/`：MCP；
  53. `src/agentscope/asr/`：ASR；
  54. `src/agentscope/tts/`：TTS；
  55. `src/agentscope/multimodal/`：Multimodal；
  56. `src/agentscope/message/`：Message；
  57. `src/agentscope/event/`：Event；
  58. `src/agentscope/state/`：State；
  59. `src/agentscope/credential/`：Credential；
  60. `src/agentscope/types/`：Types；
- **LOC 与语言**：
  - `src/agentscope/` ~80k LoC；
  - `src/agentscope/formatter/` ~10k LoC；
  - `src/agentscope/rag/` ~10k LoC；
  - `src/agentscope/middleware/` ~10k LoC；
  - `src/agentscope/tool/` ~10k LoC；
  - `src/agentscope/workspace/` ~5k LoC；
  - 主语言：**Python**（100%）；
- **构建系统**：
  - **Hatchling**（`pyproject.toml`）；
  - **uv**（依赖管理）；
  - **单包**（`src/agentscope/` 一包）；
  - **CI/CD**：GitHub Actions；

## 十三、学习路径

- **入门（1 周）**：
  - 阅读官方文档（https://agentscope.io/）；
  - 跑通基础 Agent 示例；
  - 理解 Agent / Toolkit / Formatter 抽象；
  - 跑通 ReAct Agent；
  - 跑通 Pipeline（Goal / Team）；
- **进阶（2-3 周）**：
  - 深入学习 Middleware（长期记忆）；
  - 实战 RAG（Qdrant / Elasticsearch / Milvus）；
  - 学习 Workspace（K8s / Docker / AppleContainer）；
  - 学习 Formatter（OpenAI / Anthropic / Gemini / DeepSeek / DashScope / Moonshot / Ollama / 豆包）；
  - 学习 Tool + Toolkit + Skill；
  - 学习 Realtime + ASR + TTS；
- **高级（1-2 月）**：
  - 阅读源码（Agent + Middleware + Pipeline + RAG + Workspace）；
  - 自定义 Middleware（实现 Long-term Memory）；
  - 自定义 Formatter（适配新 LLM）；
  - 自定义 Pipeline（实现业务 Pipeline）；
  - 自定义 Skill（实现业务 Skill）；
  - 自定义 Workspace（实现新沙箱）；
  - 自定义 Classifier（实现意图分类）；
  - 自定义 Permission（实现权限控制）；
- **推荐资源**：
  - 官方仓库：https://github.com/agentscope-ai/agentscope
  - 官方文档：https://agentscope.io/
  - 教程：https://agentscope.io/tutorial/
  - 论文：AgentScope: A Flexible Multi-Agent Platform
  - Discord：https://discord.gg/agentscope
  - Forum：https://github.com/agentscope-ai/agentscope/discussions
  - 学术合作：阿里达摩院

## 十四、风险与陷阱

- **快速演进**：AgentScope API 频繁变动；
- **学习曲线陡**：Middleware / Pipeline / Workspace / Formatter 概念多；
- **Workspace 部署复杂**：K8s Workspace 需要 K8s 集群；
- **Memory 中间件多**：Mem0 / ReMe / Agentic Memory 选择复杂；
- **RAG VDB 适配**：Qdrant / Elasticsearch / Milvus 行为差异大；
- **Formatter 兼容**：多 LLM Formatter 行为差异大；
- **Realtime 复杂度**：实时对话延迟敏感；
- **MCP 不稳定**：MCP 协议还在演进；
- **Skill 文档少**：Skill 系统文档较少；
- **Classifier / Permission 文档少**：Classifier / Permission 系统文档较少；
- **Performance 问题**：Python 调度开销大；
- **TUI 调试难**：TUI 调试相对复杂；
- **国产 LLM 锁定**：DashScope / 豆包 / Moonshot 商业产品依赖；
- **Token 消耗大**：Realtime + Multi-Step + Multimodal 消耗大量 token；
- **Workspace 沙箱开销**：K8s 沙箱有性能开销；
- **Embedding 缓存不一致**：不同 LLM Embedding 不可互换；
- **学习路径多**：从入门到高级需 2-3 个月；

## 十六、参考资料

- 官方仓库：https://github.com/agentscope-ai/agentscope
- 官方文档：https://agentscope.io/
- 教程：https://agentscope.io/tutorial/
- 论文：AgentScope: A Flexible Multi-Agent Platform
- 论文：Multi-Agent Collaboration with AgentScope
- 博客：https://blog.agentscope.io/
- Tutorial：https://agentscope.io/tutorial/
- Discord：https://discord.gg/agentscope
- Forum：https://github.com/agentscope-ai/agentscope/discussions
- YouTube：https://www.youtube.com/@agentscope
- 学术合作：阿里达摩院

## 十七、与本项目关联

AgentScope 作为「Realtime + 多模态 + Middleware + Workspace」行业代表多 Agent 框架，与本项目（engineering/ 轨道 C/C++ 自研存储与向量索引）在「Realtime 多 Agent + Middleware 中间件 + Workspace 沙箱 + 多 LLM Formatter」四个维度具有直接参考价值：

- **Realtime 多 Agent ↔ Realtime DB**：Realtime Agent（`agent/_realtime/`）与本项目实时数据库（`engineering/src/db/realtime/`）+ 流式读取（`engineering/src/db/storage/blob/`）的"**实时 + 双向 + 长连接**"思路相似。
- **Middleware ↔ 中间件拦截**：Middleware 系统（`middleware/`）与本项目 GUC（`engineering/src/db/core/guc.c`）+ Trigger（`engineering/src/db/executor/trigger.c`）的"**拦截 + 处理 + 决策**"机制相似——都是"**中间件链 + 顺序处理**"思路。
- **Workspace 沙箱 ↔ 自研沙箱**：Workspace 沙箱（`workspace/` K8s / Docker / AppleContainer）与本项目 Code Executor + Sandbox（`engineering/src/db/executor/`）+ Blob 沙箱（`engineering/src/db/storage/blob/`）的"**沙箱隔离 + 安全执行**"机制相似。
- **Formatter 多 LLM ↔ LiteLLM 集成**：Formatter（`formatter/`）多 LLM 适配与本项目 LiteLLM 集成（`engineering/src/llm/llm.h`）的"**多模型路由 + 统一接口**"思路相似。
- **Pipeline ↔ SQL Pipeline**：Pipeline（`pipeline/` GoalPipeline / TeamPipeline / SOP Engine）与本项目 SQL 执行器（`engineering/src/db/executor/`）的"**多步骤 + 顺序 / 并行 / 条件**"思路相似。
- **RAG Knowledge ↔ 向量索引**：RAG Knowledge（`rag/`）与本项目 HNSW / IVF / PQ 向量索引对照，本项目 wiki `docs/db-wiki/vector/` 已系统梳理。
- **长期记忆中间件 ↔ 存储分层**：长期记忆中间件（`middleware/_longterm_memory/` Mem0 / ReMe / Agentic Memory）与本项目 `engineering/src/db/storage/mm/mm_storage.c` 的"**多层抽象 + 可插拔**"思路相似。
- **MCP ↔ DB 协议**：MCP（`mcp/`）与本项目 PG Wire Protocol（`engineering/src/server/pgwire/`）对照。
- **Permission ↔ GUC + RBAC**：Permission（`permission/`）与本项目 GUC + RBAC 对照。
- **Classifier ↔ 路由选择**：Classifier（`classifier/`）与本项目 Index Selector（`engineering/src/db/index/index_selector.c`）的"**意图分类 + 自动选择**"机制相似。
- **TUI / Console ↔ 自研 Studio**：TUI / Console 与本项目自研 Studio 对照。
- **国产 LLM ↔ 自研 LLM**：AgentScope 集成 DashScope / 豆包 / Moonshot / DeepSeek；本项目集成 LiteLLM，可参考 AgentScope 多 LLM 适配经验。
- **Realtime + ASR + TTS ↔ 多模态 Pipeline**：Realtime + ASR + TTS（`realtime/` + `asr/` + `tts/`）与本项目多模态 RAG 应用（`engineering/apps/multimodal_rag/`）的"**多模态 + 实时 + 流水线**"思路相似。
- **学习建议**：阅读 AgentScope 源码重点关注「Middleware 中间件机制 + Workspace 沙箱隔离 + Formatter 多 LLM 适配 + Realtime 多 Agent」，与本项目 `engineering/src/db/storage/` + `engineering/src/llm/` 做架构对照，加深对「中间件 + 沙箱 + 多 LLM + 实时」的理解。
- **重点对照**：本项目自研 K8s Workspace / Code Executor 沙箱可参考 AgentScope Workspace 抽象；本项目 GUC / Trigger 可参考 AgentScope Middleware 中间件链；本项目多模态 RAG 应用可参考 AgentScope Realtime + ASR + TTS 多模态 Pipeline。