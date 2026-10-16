# CrewAI Wiki（多 Agent 协作框架）

> 参考版本：`crewai` Python 包（`lib/crewai/src/crewai/`，2026 现代化版本，含 Agents 2.0 / Crews 2.0 / Flows / RAG / Memory / MCP / Skills / Hooks / CLI），参考 HEAD：b292a33a6 前的最终 release（crewai 0.130+/crewai-tools 0.40+）。本 wiki 重点介绍 CrewAI 作为「**角色化（Role/Goal/Backstory）+ 任务化（Task/Agent）+ 流程化（Crew/Process）+ 工具化（Tools/Toolkit）**」的多 Agent 协作框架定位、源码架构、四个 Agent 维度（角色协作 / 记忆 / 工具调用 / 人机协作）的深度展开，以及与本项目 `engineering/` 自研存储与向量索引的对照参考。

## 一、项目定位

CrewAI 是由 **João Moura**（巴西开发者）于 **2023 年 11 月**开源的 **「多 Agent 协作框架」**，定位是 **「Role-based, Crew-orchestrated, Task-driven multi-agent framework」**——以"**Crew（团队）+ Agent（角色）+ Process（流程）+ Tools（工具）**"四要素为核心，让用户用最少的代码组装出可生产部署的多 Agent 系统。

- **核心痛点**：
  - LangChain 链式 LCEL 缺少**角色化抽象**（Agent 没有 Goal / Backstory / Tools 明确分工）；
  - LangGraph 缺少**开箱即用的协作模式**（需手动写 StateGraph）；
  - AutoGen GroupChat 缺少**清晰的 Process 控制**（sequential / hierarchical / consensus）；
  - 缺少**统一的企业级特性**（Memory / RAG / MCP / Hooks / Skills / CLI）；
  - 缺少**Flow（事件驱动工作流）** 与 Crew 的整合；
- **典型用户**：
  - **企业应用开发者**：构建可生产的多 Agent 系统；
  - **内容创作**：写手 / 编辑 / 校对协作；
  - **数据分析**：分析师 / 工程师 / 报告员协作；
  - **客户支持**：一线 / 二线 / 升级协作；
  - **研究型 Agent**：检索 / 综合 / 验证协作；
- **核心卖点**：
  - **Role-based Agent**：Role / Goal / Backstory / Tools 明确分工；
  - **Crew Process**：sequential / hierarchical / consensus 三种异步编排；
  - **Task**：description / expected_output / agent / context / tools / async_execution；
  - **Tools / Toolkits**：开箱即用的 100+ 工具；
  - **Memory**：Short-term / Long-term / Entity / Contextual Memory；
  - **RAG**：内置 RAG 工具（`from langchain_community.tools import ...`）；
  - **Flows**：事件驱动工作流（@start / @listen / @router）；
  - **MCP**：Model Context Protocol 集成；
  - **Skills**：预制 Agent 技能；
  - **Hooks**：Pre/Post 钩子；
  - **CLI**：`crewai create / run / test / deploy / chat / flow`；
  - **CrewAI Platform**：托管 + 监控 + 可观测性；

## 二、发展历史与社区

- **起源（2023-11）**：João Moura 在巴西开源 CrewAI v0.1，定位「Role-based multi-agent framework」；
- **里程碑**：
  - 2023-11：v0.1（Agent / Task / Crew / Tool 基本抽象）；
  - 2024-03：v0.10（Memory / RAG / Hierarchical Process）；
  - 2024-08：v0.30（CrewAI Tools 大幅增加 / Async / AsyncExecution）；
  - 2024-12：v0.70（CrewAI Flows 引入 / 事件驱动工作流）；
  - 2025-03：v0.85（MCP 集成 / Hooks / Skills）；
  - 2025-08：v0.100（CrewAI Enterprise / CrewAI AMP / Crews 2.0 预览）；
  - 2025-12：v0.110+（Agents 2.0 / Training / Reasoning / Planner）；
  - 2026-03：v0.130+（多模态 / Replit / Skills Marketplace）；
- **维护状态**：**非常活跃**（CrewAI Inc. 团队 50+ 工程师，月均 200+ commits）；
- **社区**：
  - GitHub 25k+ star、`crewai` PyPI 月下载量 800 万+（2025 年起超越 LangChain 部分子包）；
  - Discord 6k+ 成员、Forum、YouTube 频道；
  - CrewAI AMP（Advanced Multi-agent Platform）；
- **关键贡献者**：João Moura（创始人）、CrewAI 团队 50+、社区 1000+ 贡献者；

## 三、许可与商业限制

- **核心代码**：**MIT License**（`lib/crewai/`、`lib/crewai-tools/`、`lib/crewai-core/`、`lib/crewai-files/`），可商用、修改、私有部署；
- **CrewAI Platform**：商业 SaaS（AMP + 多模态监控）；
- **CrewAI Enterprise**：商业版本（合规 + 安全 + 私有部署 + SSO）；
- **生态关系**：
  - **上游依赖**：
    - Python 3.10+；
    - LangChain（基础 LLM 抽象）；
    - Pydantic v2（数据验证）；
    - LiteLLM（多 LLM 路由）；
    - Instructor（结构化输出）；
    - Chroma / FAISS（向量库）；
  - **下游生态**：约 10000+ 企业使用 CrewAI，包括 IBM、Uber、Klarna、Oracle、DocuSign；

## 四、业界主要使用方与案例

- **国际**（公开）：
  - **Oracle**：企业 SaaS 多 Agent；
  - **DocuSign**：合同自动化 Agent；
  - **KPMG**：合规审计 Agent；
  - **PwC**：财务报告 Agent；
  - **多家金融机构**：客户尽调 / 风险评估 / 反欺诈；
  - **多家互联网公司**：内容生产 / 客服 / 营销自动化；
- **国内**：
  - 多家互联网公司内容生产 / 营销；
  - 多家企业内部知识库研究型 Agent；
- **典型场景**：
  - **内容创作**：研究 → 写手 → 编辑 → 校对协作；
  - **数据分析**：数据工程师 → 分析师 → 报告员协作；
  - **客户支持**：一线 → 二线 → 升级协作；
  - **销售自动化**：线索 → 跟进 → 谈判 → 合同协作；
  - **代码生成**：DBA + 后端 + 前端 + 测试协作；
  - **生产级系统**：Flow + Crew + Memory + RAG + Tools 整合；

## 五、一句话对比定位

> CrewAI 是 **「Role-based + Crew + Process + Tools + Flow + Memory + RAG + MCP + Skills + Hooks + CLI + 25k star + 企业级 + Python + MIT」**；与 **LangGraph（图状态机）**、**AutoGen（GroupChat 对话）**、**MetaGPT（软件公司角色）**、**OpenAI Agents（Handoff 简单协议）**、**Swarm（教学 Handoff）**、**AgentScope（多 Agent 通用）** 构成多 Agent 生态。CrewAI 的差异化是 **「角色化抽象 + Process 控制 + Flow 事件驱动 + 企业级特性 + 25k star + 上升势头猛」**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                  用户应用层（User Apps）                              │
│        CrewAI Studio / CLI / Python API / AMP Cloud / Enterprise     │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Public API（lib/crewai/src/crewai/）                │
│      Agent / Task / Crew / Flow / Process / Tool / Memory / RAG       │
│      LLM / LiteLLM / AgentOps / Observability / Hooks / Skills        │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Agent 执行层（agents/ + agent/）                     │
│   agent/（Crews 2.0 Agent 抽象）                                     │
│   agents/（Agents 2.0 多 Agent 实现）                                │
│   execution.py（Agent 执行 / 工具调用 / LLM 调用）                   │
│   lite_agent.py / lite_agent_output.py（轻量 Agent）               │
│   cli/（CrewAI CLI：create / run / test / deploy / chat / flow）    │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Task / Crew / Process 抽象                           │
│   task.py / tasks/（Task 抽象：description / expected_output / agent）  │
│   crew.py / crews/（Crew 编排：Process / Flow / Memory / RAG）      │
│   process.py（Sequential / Hierarchical / Consensus）               │
│   flows/（事件驱动工作流：@start / @listen / @router / state）       │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Tools / Toolkits（tools/）                          │
│   tools/（100+ 内置工具）                                            │
│   agents/（Agent-as-Tool）/ skills/（Skills 预制技能）              │
│   mcp/（MCP 集成）/ rag/（RAG 工具）                                │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Memory / Knowledge / State                          │
│   memory/（ShortTerm / LongTerm / Entity / Contextual）             │
│   knowledge/（RAG 知识源：PDF / Web / DB / Confluence）             │
│   state/（Flow 状态持久化）                                          │
│   events/（事件总线）/ hooks/（Pre / Post 钩子）                    │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  LLM / Security / Telemetry / Observability           │
│   llm.py / llms/（多 LLM 路由：OpenAI / Anthropic / Gemini / Ollama）│
│   security/（安全策略）/ auth/（认证）                              │
│   telemetry/（OpenTelemetry）/ monitoring/                          │
└─────────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  外部依赖（External Dependencies）                   │
│   LangChain / LiteLLM / Pydantic / Chroma / FAISS / Instructor     │
└─────────────────────────────────────────────────────────────────────┘
```

进程模型：CrewAI 是**纯 Python 用户态库**（无独立进程）；CLI 工具（`crewai run/test/deploy`）启动 LangGraph；AMP Cloud 是商业托管服务。

## 七、Agent 类型专项（核心扩展章节）

### 7.1 角色协作模式（Role Collaboration）

CrewAI 通过 **Agent（角色）+ Crew（团队）+ Process（流程）** 实现四种协作模式：

- **中心化（Orchestrator-Worker）**：
  - **典型实现**：Hierarchical Process（Manager Agent 委派 Worker）；
  - **实现位置**：`process.py`（`hierarchical` mode）/ `crew.py`（`manager_llm` 参数）；
  - **Manager Agent**：自动根据 Agent 描述选择委派目标；
  - **示例**：`examples/hierarchical_crew/`；
- **对等（Peer-to-Peer / Sequential）**：
  - **典型实现**：Sequential Process（任务按顺序依次执行，下游任务的 context 是上游输出）；
  - **实现位置**：`process.py`（`sequential` mode）；
  - **State 共享**：通过 Task `context` 参数显式声明；
  - **典型场景**：流水线型生产工作流；
- **分层（Hierarchical / Supervisor）**：
  - **典型实现**：Manager Agent → Worker Agent 们（LLM 自动委派）；
  - **实现位置**：`crew.py`（`Process.hierarchical` + `manager_llm`）；
  - **Manager 决策**：LLM 分析任务描述，选择合适的 Worker；
  - **典型场景**：复杂任务委派；
- **市场（Market-based Bidding）**：
  - **典型实现**：在 custom Process 中实现 Agent 报价机制；
  - **实现位置**：自定义 `Crew` 类；
  - **不直接支持**，需用户自定义；
- **Flow 事件驱动**：
  - **典型实现**：`@start() / @listen() / @router() / @and_() / @or_()` 装饰器；
  - **实现位置**：`flows/`（Flow 引擎 / State / Events）；
  - **事件循环**：Flow 自动监听状态变化触发后续方法；
  - **典型场景**：复杂工作流（条件分支 + 并行 + 状态）；

### 7.2 记忆系统（Memory System）

CrewAI 内置**四层记忆**：

- **短期（Short-term Memory）**：
  - **实现位置**：`memory/short_term/`；
  - **存储**：最近 N 个任务输出（默认 5 个）；
  - **作用**：当前 Crew 执行的上下文；
  - **API**：`crew = Crew(..., short_term_memory=ShortTermMemory())`；
- **长期（Long-term Memory）**：
  - **实现位置**：`memory/long_term/`；
  - **存储**：Chroma / SQLite（OpenAI embeddings）；
  - **作用**：跨会话保留任务输出；
  - **API**：`crew = Crew(..., long_term_memory=LongTermMemory(storage=...))`；
- **实体（Entity Memory）**：
  - **实现位置**：`memory/entity/`；
  - **存储**：从任务输出中抽取实体（人名 / 公司 / 产品）；
  - **API**：`crew = Crew(..., entity_memory=EntityMemory())`；
- **上下文（Contextual Memory）**：
  - **实现位置**：`memory/contextual/`；
  - **存储**：任务上下文 + RAG 检索；
  - **API**：`crew = Crew(..., contextual_memory=ContextualMemory())`；
- **向量记忆（Vector Memory）**：
  - **实现位置**：`memory/long_term/`（用 Chroma）；
  - **检索**：相似度召回历史任务输出；
- **文件记忆（File Memory）**：
  - **实现位置**：`memory/file/`；
  - **存储**：本地文件系统 + JSON；
- **记忆压缩与检索**：
  - **Summarization**：在 Task 中加 `summarize_output=True`；
  - **RAG over Memory**：用 `RAG` 类 + Chroma 实现 RAG-over-Memory；
- **Knowledge 知识库**：
  - **实现位置**：`knowledge/`（CrewAI Knowledge Source）；
  - **源**：PDF / Web / DB / Confluence / Notion；
  - **API**：`crew = Crew(..., knowledge_sources=[pdf_source, web_source])`；

### 7.3 工具调用协议（Tool Calling Protocol）

CrewAI 通过 **Tool / Toolkit / Agent-as-Tool** 实现工具调用：

- **Function Calling（OpenAI / Anthropic / Gemini）**：
  - **实现位置**：`llm.py`（统一 LLM 抽象）+ `execution.py`（Tool 调用）；
  - **支持模型**：OpenAI GPT-4 / Anthropic Claude / Google Gemini / Cohere / Mistral / Ollama / vLLM；
  - **绑定工具**：`llm.bind_tools(tools)` 或 `ChatOpenAI(model="...", tools=...)`；
- **ReAct（Reasoning + Acting）**：
  - **典型实现**：Agent 默认使用 ReAct 循环（Thought → Action → Observation）；
  - **实现位置**：`agent/agent.py`（`reasoning` 参数）；
  - **循环**：每轮 Agent 生成 Thought + Action（Tool 调用）→ 观察结果 → 下一轮；
- **CodeAct（生成代码执行）**：
  - **典型实现**：`CodeInterpreterTool`（本地 PythonREPL）或 `E2BCodeInterpreterTool`（E2B sandbox）；
  - **实现位置**：`crewai_tools/tools/code_interpreter_tool/`；
  - **沙箱**：本地 / E2B（生产推荐）；
- **Tool Use + Reflection**：
  - **典型实现**：在 Agent 中设 `reflection=True`；
  - **实现位置**：`agent/agent.py`（reflection 参数）；
  - **机制**：Agent 完成任务后反思并修正；
- **Structured Outputs**：
  - **实现位置**：`agent_output.py`（Pydantic 模型）+ `pydantic_output_parser`；
  - **典型实现**：`class MyOutput(BaseModel): result: str; agent = Agent(..., output_pydantic=MyOutput)`；
- **MCP（Model Context Protocol）**：
  - **实现位置**：`mcp/`（MCP 客户端 / 服务器）；
  - **典型实现**：`MCPServer` 类支持 SSE / Stdio 模式；
  - **典型场景**：连接外部 MCP 服务器（如 filesystem / GitHub / Slack）；
- **Skills（预制技能）**：
  - **实现位置**：`skills/`（Skills Marketplace）；
  - **典型实现**：搜索 / 摘要 / 翻译等预制技能；
- **Hooks（Pre / Post）**：
  - **实现位置**：`hooks/`（Pre / Post 钩子）；
  - **典型实现**：`@before_step / @after_step` 装饰器；
- **Tool Error Handling**：
  - **实现位置**：`tools/base_tool.py`（`handle_tool_call_error`）；
  - **Fallback**：`Tool(handle_tool_call_error=lambda e: "fallback result")`；

### 7.4 人机协作（Human-in-the-Loop）

CrewAI HITL 通过 **HumanInput** + **Flow** + **Crew custom** 实现：

- **HumanInput Tool**：
  - **典型实现**：`HumanInputTool()`（在 Crew 中加入，Agent 需要时调用）；
  - **实现位置**：`crewai_tools/tools/human_input_tool/`；
  - **API**：CLI 提示 / Web UI 输入；
- **Crew custom human_input**：
  - **典型实现**：自定义 Task 加 `human_input=True`；
  - **API**：`Task(..., human_input=True)`（任务执行前等待人工输入）；
- **Flow + Human Approval**：
  - **典型实现**：`@router(after_commit)` 在 Flow 内等待人工批准；
  - **API**：Flow 监听状态变化触发人工审批节点；
- **Approval Workflow**：
  - **典型实现**：自定义 Tool（如 `approval_tool`）调用前等待审批；
  - **API**：Tool 内部调用 `input("Approve? [y/n]: ")`；
- **CLI 交互**：
  - **典型实现**：`crewai chat`（与运行中的 Crew 交互）；
  - **实现位置**：`cli/chat.py`；
- **Custom HITL**：
  - **典型实现**：自定义 Agent 加 `before_execute` / `after_execute` 钩子；
  - **API**：`Hooks.pre_/post_ hooks`；
- **AMP（Advanced Multi-agent Platform）**：
  - **托管 HITL**：AMP Cloud 提供人在回路 UI；
  - **审批流**：AMP 内置审批工作流；
- **Audit Trail**：
  - **实现位置**：`telemetry/`（OpenTelemetry）+ `observability/`；
  - **API**：自动 trace 所有 Agent / Task / Tool 调用；

## 八、索引机制（Index）

CrewAI 索引通过 **Knowledge Source** + **Memory** 实现：

- **Knowledge Source 索引**：
  - **PDF**：PyPDF 解析 → 切分 → 嵌入（OpenAI）→ Chroma；
  - **Web**：WebBaseLoader / FireCrawl → 切分 → 嵌入 → Chroma；
  - **DB**：SQLDatabase → 查询 → 嵌入 → Chroma；
  - **Confluence / Notion**：API → 嵌入 → Chroma；
- **Memory 索引**：
  - **Long-term Memory**：Chroma + OpenAI embeddings；
  - **Short-term Memory**：内存 deque；
  - **Entity Memory**：spaCy NER → 实体去重 → 存储；
- **Knowledge Embedding Model**：
  - OpenAI `text-embedding-3-small/large`；
  - HuggingFace SentenceTransformers；
  - Cohere embed；
  - Ollama nomic-embed-text；
- **Knowledge Chunking**：
  - RecursiveCharacterTextSplitter；
  - SemanticSplitter；
  - TokenSplitter（tiktoken）；
  - 默认 chunk_size 500-1500 token；
- **Knowledge Search**：
  - **Similarity Search**：默认；
  - **MMR**：Maximal Marginal Relevance；
  - **Hybrid**：Vector + BM25；
- **Knowledge Update**：
  - `knowledge.add_source(source)` / `knowledge.remove_source(source)`；
  - 自动 incremental indexing；

## 九、查询/计算/接口层

### 9.1 Crew 执行引擎（Crew Execution Engine）

- **Crew 编排**：
  - **实现位置**：`crew.py`（`Crew.kickoff()` / `kickoff_async()` / `kickoff_for_each()`）；
  - **Process**：Sequential / Hierarchical / Consensus；
  - **Flow**：@start / @listen / @router 事件驱动；
- **Task 执行**：
  - **实现位置**：`task.py`（`Task.execute()` / `execute_async()`）；
  - **Agent 选择**：根据 `agent` 参数；
  - **Tools 调用**：根据 `tools` 列表；
  - **Async Execution**：多个 Task 并行执行（`async_execution=True`）；
- **Async / Parallel**：
  - **实现位置**：`execution.py`（`AsyncExecutor`）+ `concurrent.futures`；
  - **典型实现**：`Task(async_execution=True)` + `kickoff_async()`；
- **Consensus Process**：
  - **典型实现**：多个 Agent 投票达成共识；
  - **实现位置**：`processes/custom_consensus.py`（用户自定义）；
  - **场景**：决策类任务；

### 9.2 状态管理（State / Flow / Memory / Crew）

- **Crew 状态**：
  - **任务状态**：pending / running / completed / failed；
  - **输出**：每个 Task 的 output；
  - **Context**：上下游 Task 的输出（自动 / 手动）；
- **Flow 状态**：
  - **实现位置**：`flows/state.py`（Flow State 基类）；
  - **持久化**：自动 pickle / JSON 保存到 `flow.state` 文件；
  - **API**：`flow.state()`（访问状态）；
- **Memory 状态**：
  - **Short-term**：当前 Crew 的 N 个最近输出；
  - **Long-term**：Chroma 中的所有历史；
  - **Entity**：所有提取的实体；
- **Task Context**：
  - **API**：`task = Task(..., context=[other_task1, other_task2])`；
  - **自动注入**：上游任务的 output 自动注入下游任务的 context；
- **Run**：
  - **单次执行**：`crew.kickoff(inputs={...})`；
  - **异步执行**：`await crew.kickoff_async(inputs={...})`；
  - **批量执行**：`crew.kickoff_for_each(inputs_list)`；

### 9.3 多模态工具（Multimodal Tools）

- **浏览器**：`ScrapeWebsiteTool` / `SeleniumScrapingTool` / `FirecrawlScrapeWebsiteTool`；
- **代码执行**：`CodeInterpreterTool` / `E2BCodeInterpreterTool`；
- **文件操作**：`FileReadTool` / `FileWriteTool` / `DirectoryReadTool`；
- **API 调用**：`APITool`（REST API 通用工具）；
- **RAG 工具**：`RagTool`（从 PDF / Web / DB 检索）；
- **数据库**：`SQLDatabaseTool`（查询 SQL 数据库）；
- **搜索**：`SerperDevTool` / `TavilySearchTool` / `DuckDuckGoSearchTool`；
- **多模态**：`VisionTool` / `DALL-ETool` / `StableDiffusionTool`；
- **自定义**：`@tool("my_tool")` 装饰器或继承 `BaseTool`；
- **Toolkit 组合**：
  - **典型 Toolkit**：`crewai_tools` 提供 100+ 预制工具；
  - **组合使用**：Agent 可配置多个 Toolkit；

### 9.4 多 Agent 通信（Multi-Agent Communication）

- **共享 State（blackboard）**：
  - **实现位置**：`crew.py`（`Crew.state`）；
  - **共享机制**：所有 Agent 通过 Task output + context 共享；
- **消息总线（messages channel）**：
  - **实现位置**：`memory/short_term/`（消息队列）；
  - **典型实现**：Agent 输出作为消息传递；
- **Hierarchical Manager 委派**：
  - **典型实现**：Manager Agent 通过 LLM 决定委派给哪个 Worker；
  - **API**：`crew = Crew(process=Process.hierarchical, manager_llm=...)`；
- **Handoff（Handoffs）**：
  - **典型实现**：在 custom Crew 中实现 Agent 间 handoff；
  - **API**：自定义 routing 逻辑；
- **A2A 协议**：
  - **CrewAI 通过 MCP + Remote Crew** 实现 A2A；
  - **典型实现**：Remote Crew 作为 MCP Server 或 HTTP Server；

## 十、查询/计算/接口层（API & SDK）

- **Python API（核心）**：
  - `crewai.Agent`（`role / goal / tools / llm / memory / allow_delegation`）；
  - `crewai.Task`（`description / expected_output / agent / tools / context / async_execution`）；
  - `crewai.Crew`（`agents / tasks / process / memory / knowledge_sources / planning`）；
  - `crewai.Process`（`sequential / hierarchical`）；
  - `crewai.Flow` + `@start / @listen / @router`；
  - `crewai.AgentOutput` / `crewai.HumanInput`；
  - `crewai.LLM`（`model / temperature / api_key / base_url`）；
  - `crewai.Tool` / `crewai.BaseTool` / `crewai.tool` 装饰器；
  - `crewai.Memory`（Short / Long / Entity / Contextual）；
  - `crewai.Knowledge` / `crewai.KnowledgeSource`（PDF / Web / DB）；
- **CLI**：
  - `crewai create my_crew`（创建项目）；
  - `crewai run`（运行）；
  - `crewai test`（测试）；
  - `crewai deploy`（部署到 AMP）；
  - `crewai chat`（交互对话）；
  - `crewai flow`（运行 Flow）；
  - `crewai train`（训练 Agent）；
- **CrewAI Studio**（Web IDE）：
  - 可视化设计 Crew / Agent / Task / Flow；
  - 调试 + 测试 + 监控；
- **AMP Cloud（商业托管）**：
  - **部署**：一键部署 Crew / Flow；
  - **监控**：OpenTelemetry + 自定义监控；
  - **Scaling**：水平扩展；
  - **Approval**：人在回路审批 UI；
- **CrewAI Enterprise**：
  - **私有部署**：On-prem / VPC；
  - **合规**：SOC2 / HIPAA / GDPR；
  - **安全**：SSO / RBAC / Audit；
- **第三方 SDK**：
  - **LangChain 集成**：所有 LangChain Tools 都可用；
  - **LangGraph 集成**：LangGraph 作为 LLM 抽象；
  - **Instructor 集成**：结构化输出；
  - **MCP 集成**：MCP Server / Client；

## 十一、生态与集成

- **LLM 集成**：OpenAI / Anthropic / Google / Cohere / Mistral / Ollama / vLLM / Together / Fireworks / DeepSeek / Qwen；
- **Vector Store**：Chroma（默认）/ FAISS / Weaviate / Milvus / Qdrant / LanceDB / Pinecone / pgvector；
- **Tool 集成**：100+ 预制工具 + LangChain 全部 Tools + 自定义工具；
- **Memory Backend**：Chroma（默认）/ SQLite / Redis / 自定义；
- **Knowledge Source**：PDF / Web / DB / Confluence / Notion / GitHub / Slack；
- **MCP Server**：filesystem / GitHub / Slack / Google Drive / 自定义；
- **Observability**：OpenTelemetry（默认）/ LangSmith / Langfuse / CrewAI Observability；
- **部署**：AMP Cloud（商业托管）/ Docker / Kubernetes / CrewAI Enterprise（私有部署）；
- **企业版**：CrewAI Enterprise + AMP + 私有部署 + 合规 + 安全；
- **Skills Marketplace**：100+ 预制 Skills（搜索 / 摘要 / 翻译 / 校对）；
- **Agent Marketplace**：社区 Agent 模板（销售 / 客服 / 营销 / 财务 / HR）；
- **Flow Marketplace**：社区 Flow 模板（CRM / ERP / 营销自动化）；

## 十二、源码学习路径

- **必读模块**（按学习顺序）：
  1. `lib/crewai/src/crewai/__init__.py`：导出列表；
  2. `lib/crewai/src/crewai/agent.py`：Agent 抽象；
  3. `lib/crewai/src/crewai/lite_agent.py`：轻量 Agent；
  4. `lib/crewai/src/crewai/agents/`：Agents 2.0 多 Agent 实现；
  5. `lib/crewai/src/crewai/task.py`：Task 抽象；
  6. `lib/crewai/src/crewai/tasks/`：Tasks 2.0 实现；
  7. `lib/crewai/src/crewai/crew.py`：Crew 编排；
  8. `lib/crewai/src/crewai/crews/`：Crews 2.0；
  9. `lib/crewai/src/crewai/process.py`：Sequential / Hierarchical Process；
  10. `lib/crewai/src/crewai/execution.py`：Agent 执行；
  11. `lib/crewai/src/crewai/flow/`：Flow 事件驱动；
  12. `lib/crewai/src/crewai/llm.py`：LLM 抽象；
  13. `lib/crewai/src/crewai/llms/`：多 LLM 路由；
  14. `lib/crewai/src/crewai/tools/`：Tool 抽象；
  15. `lib/crewai/src/crewai/memory/`：四层记忆；
  16. `lib/crewai/src/crewai/knowledge/`：Knowledge Source；
  17. `lib/crewai/src/crewai/rag/`：RAG 工具；
  18. `lib/crewai/src/crewai/mcp/`：MCP 集成；
  19. `lib/crewai/src/crewai/skills/`：Skills；
  20. `lib/crewai/src/crewai/hooks/`：Hooks；
  21. `lib/crewai/src/crewai/events/`：事件总线；
  22. `lib/crewai/src/crewai/state/`：Flow State；
  23. `lib/crewai/src/crewai/cli/`：CLI；
  24. `lib/crewai/src/crewai/security/`：安全策略；
  25. `lib/crewai/src/crewai/telemetry/`：OpenTelemetry；
- **LOC 与语言**：
  - `lib/crewai/` ~80k LoC；
  - `lib/crewai-tools/` ~30k LoC；
  - `lib/crewai-core/` ~10k LoC；
  - `lib/crewai-files/` ~5k LoC；
  - 主语言：**Python**（100%）；
- **构建系统**：
  - **Hatchling**（`pyproject.toml`）；
  - **uv**（依赖管理）；
  - **monorepo**（`lib/crewai/`、`lib/crewai-tools/`、`lib/crewai-core/`、`lib/crewai-files/` 四包）；
  - **CI/CD**：GitHub Actions；
  - **release-please**（发布管理）；

## 十三、学习路径

- **入门（1 周）**：
  - 阅读官方 Tutorial（https://docs.crewai.com/）；
  - 跑通 `crewai create my_first_crew`；
  - 理解 Agent / Task / Crew / Tool 抽象；
  - 跑通 Sequential / Hierarchical 两种 Process；
- **进阶（2-3 周）**：
  - 深入学习 Process（Sequential / Hierarchical）；
  - 实战 Memory（Short / Long / Entity / Contextual）；
  - 学习 RAG（Knowledge Source + RAG Tool）；
  - 学习 Flow（@start / @listen / @router）；
  - 学习 MCP（MCPServer / MCPClient）；
- **高级（1-2 月）**：
  - 阅读源码（Crew + Process + Flow + Memory）；
  - 自定义 Process（Consensus / Bidding）；
  - 自定义 Tool（继承 BaseTool）；
  - 自定义 Memory（实现 BaseMemory）；
  - 自定义 Knowledge Source；
  - AMP 部署 + CrewAI Enterprise；
  - Hooks / Skills Marketplace；
- **推荐资源**：
  - 官方文档：https://docs.crewai.com/
  - GitHub：https://github.com/crewAIInc/crewAI
  - CrewAI Studio：https://studio.crewai.com/
  - AMP：https://amp.crewai.com/
  - 论文：Multi-Agent Collaboration: A Survey；
  - 博客：https://blog.crewai.com/
  - Tutorial：https://docs.crewai.com/getting-started/
  - Discord：https://discord.gg/crewai

## 十四、风险与陷阱

- **学习曲线陡**：Agent / Task / Crew / Process / Flow / Memory 概念多，新人需 1-2 周入门；
- **Manager Agent 委派不稳定**：LLM 自动委派可能选错 Worker，需 fallback；
- **Memory 成本**：Long-term Memory 持续写 Chroma，存储 + 嵌入成本高；
- **Task Context 滥用**：所有 Task context 注入下游，下游 context 可能爆炸；
- **Hierarchical Process 不稳定**：Manager Agent 决策影响整个流程；
- **Flow 状态持久化**：自动 pickle 可能反序列化失败（Python 版本兼容）；
- **Tools 错误处理**：默认吞掉异常，需自定义 handle_tool_call_error；
- **Async Execution 异常处理**：并行 Task 异常难处理；
- **AMP 锁定**：使用 AMP 后切换到自部署成本高；
- **LiteLLM 版本兼容**：多 LLM 路由版本敏感；
- **Pydantic v2 兼容**：部分 LangChain Tools 仍用 Pydantic v1；
- **MCP Server 不稳定**：MCP 协议还在演进；
- **Skills Marketplace 质量**：社区技能质量参差；
- **CLI 调试难**：Flow 错误难以定位；
- **企业版价格高**：CrewAI Enterprise 价格高于 LangSmith；
- **Performance 问题**：Python 调度开销大，超大 Crew（>10 Agent）性能下降；
- **Memory Schema 迁移**：Schema 变更需手动迁移；

## 十六、参考资料

- 官方仓库：https://github.com/crewAIInc/crewAI
- 官方文档：https://docs.crewai.com/
- CrewAI Studio：https://studio.crewai.com/
- AMP：https://amp.crewai.com/
- CrewAI Enterprise：https://enterprise.crewai.com/
- 论文：Multi-Agent Collaboration: A Survey
- 博客：https://blog.crewai.com/
- Tutorial：https://docs.crewai.com/getting-started/
- Discord：https://discord.gg/crewai
- Forum：https://forum.crewai.com/
- YouTube：https://www.youtube.com/@crewai
- CrewAI Academy：https://academy.crewai.com/

## 十七、与本项目关联

CrewAI 作为「Role-based + Process + Flow + Memory + RAG + MCP」行业代表多 Agent 框架，与本项目（engineering/ 轨道 C/C++ 自研存储与向量索引）在「角色化抽象 + Process 编排 + Memory 持久化 + Flow 事件驱动」四个维度具有直接参考价值：

- **Role-based Agent ↔ Index Access Method**：抽象的 Role / Goal / Backstory 与本项目的 PG 风格 Index AM 抽象（`engineering/src/db/index/`）有相似性——都是"**声明式能力 + 命令式调用 + 持久化元数据**"。
- **Process Sequential / Hierarchical ↔ SQL 执行器**：Process 抽象与本项目 SQL 执行器（`engineering/src/db/sql/`）的顺序 / 并行 / 分布式执行有相似性；本项目 Sequential 与 SQL Parallelism 的层状调度可对照学习。
- **Memory 四层 ↔ 存储分层**：CrewAI 的 Short-term / Long-term / Entity / Contextual Memory 与本项目的多层存储（`engineering/src/db/storage/`）—— Hot 热 / Cold 冷 / Blob 二元大对象 / mmview 多模态视图都有对应关系，本项目可参考 CrewAI "**近 + 远 + 实体 + 上下文**" 四层设计。
- **Flow 事件驱动 ↔ WAL 流式**：Flow 装饰器（@start / @listen / @router）与本项目 WAL 协调（`engineering/src/db/storage/log/`）+ WAL Buffer（`engineering/src/db/storage/wal_buf/`）的事件驱动相似。本项目 WAL 在「记录 + 刷盘 + 重放」事件流上可参考 Flow 事件总线（`crewai/events/`）。
- **RAG Knowledge ↔ 向量索引**：CrewAI 的 RAG Knowledge Source 与本项目 HNSW / IVF / PQ 向量索引对照，本项目 wiki `docs/db-wiki/vector/` 已系统梳理。
- **MCP ↔ DB 协议**：CrewAI 通过 MCP 连接外部系统；本项目实现 PG Wire Protocol（`engineering/src/server/pgwire/`）让客户端像 PG 一样调用，可对照 MCP "**标准化 + 可发现 + 可插拔**" 思路。
- **Toolkit 组合 ↔ 索引组合**：Toolkit 抽象（多 Tool 打包）与本项目索引组合（`engineering/src/db/index/`）的多索引协调相似。
- **Skills Marketplace ↔ 插件市场**：Skills Marketplace 模式可启发本项目算法市场（`engineering/src/algo-prod/`）。
- **Hooks ↔ GUC**：CrewAI Hooks（`crewai/hooks/`）与本项目 GUC 配置系统（`engineering/src/db/core/guc.c`）都是"**运行时拦截 + 决策控制**"思路。
- **AMP Cloud ↔ 自研云**：CrewAI AMP 托管模式与本项目自部署（`engineering/src/db/core/db_server.c`）对照。
- **学习建议**：阅读 CrewAI 源码重点关注「Agent 抽象 + Process 编排 + Flow 事件驱动 + Memory 持久化」，与本项目 `engineering/src/db/sql/` + `engineering/src/db/storage/` 做架构对照，加深对「多 Agent + 持久化 + 编排」的理解。