# MetaGPT Wiki（多 Agent 软件公司框架）

> 参考版本：`metagpt` Python 包（`metagpt/` 主包，单版本即社区版），参考 HEAD：b292a33a6 前的最终 release（MetaGPT 0.8+）。本 wiki 重点介绍 MetaGPT 作为「**软件工程 SOP（Standard Operating Procedure）+ 多角色协作 + 代码生成 + Memory + RAG + Subscription 发布订阅**」的多 Agent 软件公司框架定位、源码架构、四个 Agent 维度（角色协作 / 记忆 / 工具调用 / 人机协作）的深度展开，以及与本项目 `engineering/` 自研存储与向量索引的对照参考。

## 一、项目定位

MetaGPT 是由 **DeepWisdom（深度赋智，吴承霖）** 于 **2023 年 6 月**开源的 **「多 Agent 软件公司框架」**，定位是 **「MetaGPT: The Multi-Agent Framework for Software Engineering SOPs」**——把人类软件公司的角色分工（Product Manager / Architect / Project Manager / Engineer / QA）抽象为可执行的多 Agent 系统，让 Agent 按 SOP（Standard Operating Procedure）协作完成软件开发任务。

- **核心痛点**：
  - LangChain 链式 LCEL 缺少**角色化分工**（无 Product Manager / Engineer 等角色）；
  - AutoGen 缺少**软件工程 SOP**（缺少 PRD / 设计文档 / 代码 / Review 流程）；
  - 缺少**协作产物交接**（Message → Document → Code → Review）；
  - 缺少**统一架构抽象**（Team / Role / Action / Environment）；
  - 缺少**长任务规划**（Plan → Code → Test 完整流水线）；
  - 缺少**自研 Memory + RAG**（deepwisdom 团队自研）；
- **典型用户**：
  - **学术研究**：论文复现 / Agent SOP 探索；
  - **企业应用**：自动代码生成 / 文档生成；
  - **快速原型**：MVP / Hackathon 项目；
  - **复杂软件工程**：多模块系统设计 + 实现；
- **核心卖点**：
  - **Role 抽象**：Product Manager / Architect / Project Manager / Engineer / QA 等；
  - **Action 抽象**：WritePRD / WriteDesign / WriteCode / WriteTest / ReviewCode 等；
  - **Team 抽象**：Software Company（多角色协作）；
  - **Environment 抽象**：Environment + Roles + Actions 协作；
  - **SOP（Standard Operating Procedure）**：标准化工作流；
  - **Subscription 发布订阅**（Role 间消息路由）；
  - **Memory**（short / long / global）；
  - **RAG**（自研）；
  - **Provider**（多 LLM 路由：OpenAI / Anthropic / Gemini / Ollama / 自部署）；
  - **Document Store**（协作产物版本化）；
  - **Skills**（预制技能）；
  - **Strategy**（LLM 调用策略 / Selection）；

## 二、发展历史与社区

- **起源（2023-06）**：DeepWisdom 团队开源 MetaGPT v0.1，发表论文《MetaGPT: Meta Programming for A Multi-Agent Collaborative Framework》；
- **里程碑**：
  - 2023-06：v0.1（Software Company SOP + ProductManager / Architect / ProjectManager / Engineer / QA）；
  - 2023-12：v0.5（Subscription / Environment / RAG / Document Store）；
  - 2024-03：v0.7（Data Interpreter 引入 / 多模态 / Code Interpreter）；
  - 2024-06：v0.8（Provider / Strategy / Skills / Memory 完善）；
  - 2024-10：v0.8.x（Affiliation / Profile / Subscription 完善）；
  - 2025-02：v0.8.x（Ollama / Anthropic / Gemini 完善 / 多 Agent 框架）；
  - 2025-08：v0.8.x（数据解释器 / 学术研究 / 强化学习）；
  - 2026-03：v0.8.x（稳定 + 性能 + 学术 + 论文复现）；
- **维护状态**：**活跃**（DeepWisdom 团队 15+ 工程师，月均 60+ commits）；
- **社区**：
  - GitHub 50k+ star、`metagpt` PyPI 月下载量 100 万+；
  - Discord 3k+ 成员、GitHub Discussions；
  - 学术论文：MetaGPT: Meta Programming for A Multi-Agent Collaborative Framework（ICLR 2024）；
  - 学术论文：Data Interpreter: An LLM Agent For Data Science（DeepWisdom）；
- **关键贡献者**：吴承霖（创始人）、DeepWisdom 团队、贡献者 200+；

## 三、许可与商业限制

- **核心代码**：**MIT License**（`metagpt/`），可商用、修改、私有部署；
- **MetaGPT Cloud**：商业 SaaS（部分闭源）；
- **生态关系**：
  - **上游依赖**：
    - Python 3.10+；
    - OpenAI / Anthropic / Azure / Gemini / 豆包 / 通义千问；
    - Pydantic v2；
    - asyncio + aiohttp；
    - chroma / faiss（memory）；
  - **下游生态**：约 3000+ 学术研究 + 企业应用；

## 四、业界主要使用方与案例

- **国际**（公开）：
  - **多家初创公司**：MVP / Hackathon 加速；
  - **多家研究机构**：Agent SOP 实验；
- **国内**：
  - **多家互联网公司**：自动代码生成 / 文档生成；
  - **DeepWisdom 客户**：企业定制化 Agent；
- **典型场景**：
  - **学术研究**：论文复现 / Agent SOP 探索；
  - **企业应用**：自动代码生成 / 文档生成；
  - **快速原型**：MVP / Hackathon 项目；
  - **复杂软件工程**：多模块系统设计 + 实现；
  - **数据分析**：Data Interpreter（自动数据处理 / 建模 / 可视化）；
  - **多模态**：图像 / 视频 + 文本生成；
  - **代码生成**：单文件 / 多文件 / 完整项目；

## 五、一句话对比定位

> MetaGPT 是 **「DeepWisdom 出品 + 软件工程 SOP + 多角色协作 + ProductManager / Architect / Engineer + Subscription + RAG + Memory + 50k star + 学术研究 + MIT」**；与 **LangGraph（图状态机）**、**CrewAI（角色化 + Process）**、**AutoGen（GroupChat 对话）**、**OpenAI Agents（Handoff 简单协议）**、**Swarm（教学 Handoff）**、**AgentScope（多 Agent 通用）** 构成多 Agent 生态。MetaGPT 的差异化是 **「软件工程 SOP + 角色分工 + Message → Document → Code 协作 + 学术研究 + Data Interpreter」**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                  用户应用层（User Apps）                              │
│        CLI / Python API / Web UI / MetaGPT Cloud                    │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  High-Level API（metagpt/）                           │
│      Team / SoftwareCompany / DataInterpreter                         │
│      Role（ProductManager / Architect / ProjectManager / Engineer）  │
│      Action（WritePRD / WriteDesign / WriteCode / WriteTest）       │
│      Environment / Skills / Memory / RAG / Strategy                  │
│      Provider / Config / Settings / Profile                         │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Role / Action 抽象                                  │
│   roles/（ProductManager / Architect / ProjectManager / Engineer / QA）│
│   actions/（WritePRD / WriteDesign / WriteCode / WriteTest / ReviewCode）│
│   actions/action.py（Action 基类）                                    │
│   roles/role.py（Role 基类）                                          │
│   skills/（Skills 预制技能）                                          │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Team / Environment                                 │
│   team.py（Team 基类）                                             │
│   software_company.py（Software Company 默认 Team）               │
│   environment.py（Environment 基类 + Roles 注册）                │
│   context.py / context_mixin.py（Context 上下文）                │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Memory / RAG / Document Store                       │
│   memory/（short_term / long_term / global / vector）              │
│   rag/（RAG 工具集：Embeddings / Retriever / Reranker）            │
│   document_store/（Document 版本化：Git / DB / FS）               │
│   document.py（Document 抽象）                                    │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Subscription / Message Bus                         │
│   subscription.py（Subscription 基类）                              │
│   publish / subscribe（Role 间通信）                                  │
│   msg（Message 抽象）                                              │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Provider / Strategy / LLM                          │
│   provider/（OpenAI / Anthropic / Gemini / Ollama / 自部署）       │
│   strategy/（LLM 选择策略：cost / latency / quality）              │
│   llm.py（LLM 统一接口）                                            │
│   configs/（配置管理）                                              │
└─────────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  外部依赖（External Dependencies）                   │
│   OpenAI / Anthropic / Azure / Gemini / Ollama / tiktoken          │
│   chroma / faiss / aiohttp / pydantic                              │
└─────────────────────────────────────────────────────────────────────┘
```

进程模型：MetaGPT 是**纯 Python 用户态库**（无独立进程）；通过 asyncio + aiohttp 实现异步高并发。

## 七、Agent 类型专项（核心扩展章节）

### 7.1 角色协作模式（Role Collaboration）

MetaGPT 通过 **Role + Action + Environment + Subscription** 实现四种协作模式：

- **中心化（Orchestrator-Worker）**：
  - **典型实现**：ProjectManager 角色委派任务给 Engineer / Architect / QA；
  - **实现位置**：`metagpt/roles/project_manager.py` / `metagpt/team.py`；
  - **典型场景**：Scrum Master 风格分工；
- **对等（Peer-to-Peer / Blackboard）**：
  - **典型实现**：所有 Role 共享 Environment Blackboard；
  - **实现位置**：`metagpt/environment.py`（Environment 基类）；
  - **共享 State**：所有消息 / Document 共享；
- **分层（Hierarchical / Supervisor）**：
  - **典型实现**：ProductManager → Architect → ProjectManager → Engineer → QA 层级；
  - **实现位置**：`metagpt/software_company.py`（SoftwareCompany 默认配置）；
  - **状态隔离**：每个 Role 独立上下文；
- **市场（Market-based Bidding）**：
  - **典型实现**：自定义 Strategy 让 Role 报价接单；
  - **实现位置**：自定义 `Role._act()` 实现报价逻辑；
  - **不直接支持**，需用户自定义；
- **Subscription 发布订阅**：
  - **典型实现**：Role 通过 Subscription 接收其他 Role 的消息；
  - **实现位置**：`metagpt/subscription.py`；
  - **机制**：订阅 Topic / Role / Action；
  - **典型场景**：消息路由、解耦协作；
- **Data Interpreter（数据科学协作）**：
  - **典型实现**：`DataInterpreter`（多 Agent 数据科学）；
  - **实现位置**：`metagpt/roles/data_interpreter.py`；
  - **机制**：自动数据处理 / 建模 / 可视化；
  - **典型场景**：数据分析、机器学习；

### 7.2 记忆系统（Memory System）

MetaGPT 记忆分**三层 + Vector + Document**：

- **短期（Short-term Memory）**：
  - **实现位置**：`metagpt/memory/short_term.py`；
  - **存储**：当前 Role / Team 的最近 N 个 Message；
  - **API**：`role.set_memory(ShortTermMemory())`；
- **长期（Long-term Memory）**：
  - **实现位置**：`metagpt/memory/long_term.py`；
  - **存储**：Chroma / FAISS（OpenAI / 自定义 embeddings）；
  - **API**：`role.set_memory(LongTermMemory())`；
- **全局（Global Memory）**：
  - **实现位置**：`metagpt/memory/global_memory.py`；
  - **跨 Role / 跨 Team 共享**；
  - **API**：`team.set_global_memory(GlobalMemory())`；
- **向量记忆（Vector Memory）**：
  - **实现位置**：`metagpt/memory/long_term.py`（用 Chroma）；
  - **检索**：相似度召回历史消息；
- **文件记忆（File Memory）**：
  - **实现位置**：`metagpt/document_store/`；
  - **存储**：本地文件系统 + JSON / Markdown；
- **记忆压缩与检索**：
  - **Summarization**：在 Role 中加 `summarize_memory()`；
  - **RAG over Memory**：用 RAG 类 + Chroma 实现 RAG-over-Memory；
- **Document Store（文档版本化）**：
  - **实现位置**：`metagpt/document_store/`；
  - **版本化**：Git / DB / FS；
  - **API**：`document_store.save(document)` / `document_store.load(commit)`；

### 7.3 工具调用协议（Tool Calling Protocol）

MetaGPT 通过 **Tool / Skill / Action** 实现工具调用：

- **Function Calling（OpenAI / Anthropic / Gemini）**：
  - **实现位置**：`metagpt/provider/`（OpenAI / Anthropic / Gemini Provider）；
  - **支持模型**：OpenAI GPT-4 / Anthropic Claude / Google Gemini / Mistral / Ollama / 通义千问 / 豆包；
  - **绑定工具**：`role.use_tools([my_tool])`；
- **ReAct（Reasoning + Acting）**：
  - **典型实现**：`Role` 默认使用 ReAct 循环；
  - **实现位置**：`metagpt/roles/role.py`（`_act()` 方法）；
  - **循环**：每轮 Role 生成 Thought + Action（Tool 调用）→ 观察结果 → 下一轮；
- **CodeAct（生成代码执行）**：
  - **典型实现**：Engineer 角色调用 Python REPL / Docker 沙箱；
  - **实现位置**：`metagpt/roles/engineer.py` / `metagpt/actions/write_code.py`；
  - **沙箱**：本地 / Docker（生产推荐）；
- **Tool Use + Reflection**：
  - **典型实现**：QA 角色反思 Engineer 输出；
  - **实现位置**：`metagpt/roles/qa_engineer.py`（QA Review）；
- **Structured Outputs**：
  - **实现位置**：`metagpt/actions/action.py`（Pydantic 模型）；
  - **典型实现**：`class WriteCode(Action): ...`；
- **Custom Tool**：
  - **典型实现**：自定义 `Action` 类；
  - **API**：`class MyAction(Action): async def run(self, context): ...`；
- **Skills**：
  - **实现位置**：`metagpt/skills/`（预制技能）；
  - **典型实现**：搜索 / 摘要 / 翻译 / 校对；
- **Tool Error Handling**：
  - **实现位置**：`metagpt/actions/action.py`（`handle_error`）；
  - **Fallback**：自定义 `_handle_error()` 方法；
- **Toolkit 组合**：
  - **典型 Toolkit**：`metagpt.tools` 提供 100+ 预制工具；

### 7.4 人机协作（Human-in-the-Loop）

MetaGPT HITL 通过 **User Role / Custom Role / Approval** 实现：

- **UserProxy Role**：
  - **典型实现**：自定义 Role 让人类作为 Agent 参与；
  - **实现位置**：自定义 Role；
  - **API**：`user_role = UserRole()` 加入 Team；
- **Custom HITL Tool**：
  - **典型实现**：`HumanInputAction`（等待人工输入）；
  - **API**：在 Action 内调用 `input("请确认: ")`；
- **Document Approval**：
  - **典型实现**：PRD / Design 文档生成后等待人工审批；
  - **API**：在 Team 中插入 `ReviewHumanAction`；
- **Magentic-One 风格自适应 + Human**：
  - **典型实现**：自定义 Strategy + Human Override；
  - **API**：在 Action 内检测 + 调用人工；
- **Approval Workflow**：
  - **典型实现**：自定义 Tool（如 `approval_tool`）调用前等待审批；
- **CLI 交互**：
  - **典型实现**：`metagpt --interactive` 模式；
  - **API**：`metagpt/team.py` 集成 `input()`；
- **Custom HITL**：
  - **典型实现**：自定义 Role 加 `before_act` / `after_act` 钩子；
- **Audit Trail**：
  - **实现位置**：`metagpt/logs.py`（结构化日志）；
  - **API**：自动 trace 所有 Role / Action 调用；

## 九、查询/计算/接口层

### 9.1 Team 执行引擎（Team Execution Engine）

- **Team 编排**：
  - **实现位置**：`metagpt/team.py`；
  - **API**：`team = Team(hire=[role1, role2], env=env)`；
  - **执行**：`team.run(idea="...")` 异步执行；
- **Software Company 默认 Team**：
  - **实现位置**：`metagpt/software_company.py`；
  - **API**：`company = SoftwareCompany()`；
  - **默认角色**：ProductManager / Architect / ProjectManager / Engineer / QA；
  - **默认动作**：WritePRD / WriteDesign / WriteCode / WriteTest / ReviewCode；
- **Data Interpreter**：
  - **实现位置**：`metagpt/roles/data_interpreter.py`；
  - **API**：`di = DataInterpreter()`；
  - **场景**：数据分析 / 机器学习；
- **Environment**：
  - **实现位置**：`metagpt/environment.py`；
  - **API**：`env = Environment()`；
  - **角色注册**：`env.add_role(role)`；
  - **消息发布**：`env.publish_message(msg)`；

### 9.2 状态管理（State / Subscription / Message）

- **Role 状态**：
  - **实现位置**：`metagpt/roles/role.py`（Role 内置状态）；
  - **内容**：当前任务 / 历史消息 / Memory；
- **Team 状态**：
  - **实现位置**：`metagpt/team.py`（Team state）；
  - **内容**：所有 Role 状态 + Environment 状态；
- **Subscription**：
  - **实现位置**：`metagpt/subscription.py`；
  - **API**：`role.subscribe_to(topic)`；
- **Message**：
  - **实现位置**：`metagpt/environment.py`（msg）；
  - **API**：`msg = Message(content="...", role=..., cause_by=...)`；
- **Persistence**：
  - **API**：`team.save()` / `team.load(path)`；
  - **场景**：持久化 + 恢复；
- **Document 版本化**：
  - **实现位置**：`metagpt/document_store/`；
  - **API**：`document_store.save(document)` / `document_store.load(commit)`；

### 9.3 多模态工具（Multimodal Tools）

- **浏览器**：`metagpt.tools.browser`（Playwright / Selenium）；
- **代码执行**：`metagpt.tools.python_repl`（本地 / Docker）；
- **文件操作**：`metagpt.tools.file`（本地文件系统）；
- **API 调用**：`metagpt.tools.api`（REST API 通用工具）；
- **RAG 工具**：`metagpt.rag`（Chroma / FAISS）；
- **数据库**：`metagpt.tools.sql`（查询 SQL 数据库）；
- **搜索**：`metagpt.tools.search`（Tavily / Serper / DuckDuckGo）；
- **多模态**：`metagpt.tools.vision`（GPT-4V / Claude 3 / Gemini Vision）；
- **自定义**：`class MyTool(BaseTool): async def run(self, ...): ...`；

### 9.4 多 Agent 通信（Multi-Agent Communication）

- **Message Bus（Subscription）**：
  - **实现位置**：`metagpt/subscription.py`；
  - **机制**：基于 Topic / Role / Action 的发布订阅；
- **Shared Environment**：
  - **实现位置**：`metagpt/environment.py`；
  - **共享 State**：所有 Role 共享 Environment；
- **Handoff**：
  - **典型实现**：自定义 Role 实现 `_act()` 中委派给另一个 Role；
  - **API**：`await env.publish_message(Message(..., send_to=other_role))`；
- **A2A 协议**：
  - **MetaGPT 不直接实现 A2A**，但可通过自定义 Message + Subscription 实现跨进程通信；
  - **典型实现**：自定义 Action 通过 HTTP / gRPC 调用远程 Agent；

## 十、查询/计算/接口层（API & SDK）

- **Python API（核心）**：
  - `metagpt.team.Team`；
  - `metagpt.software_company.SoftwareCompany`；
  - `metagpt.roles.product_manager.ProductManager`；
  - `metagpt.roles.architect.Architect`；
  - `metagpt.roles.project_manager.ProjectManager`；
  - `metagpt.roles.engineer.Engineer`；
  - `metagpt.roles.qa_engineer.QAEngineer`；
  - `metagpt.roles.data_interpreter.DataInterpreter`；
  - `metagpt.actions.action.Action`；
  - `metagpt.environment.Environment`；
  - `metagpt.context.Context`；
  - `metagpt.memory.*`（ShortTerm / LongTerm / GlobalMemory）；
  - `metagpt.rag.RAG`；
  - `metagpt.provider.*`（OpenAI / Anthropic / Gemini / Ollama / 自部署）；
  - `metagpt.strategy.*`（LLM Selection Strategy）；
  - `metagpt.skills.*`（Skills）；
- **CLI**：
  - `metagpt "创建一个 TODO 应用"`；
  - `metagpt --interactive`；
  - `metagpt --company`；
- **Web UI**：
  - MetaGPT Cloud Web UI；
  - 可视化 Team / Role / Action / Document；
- **第三方 SDK**：
  - **LangChain 集成**：通过 LangChain Adapter；
  - **Instructor 集成**：结构化输出；
- **RPC 通信**：
  - 自定义 Transport（HTTP / gRPC）；

## 十一、生态与集成

- **LLM 集成**：OpenAI / Anthropic / Azure OpenAI / Google Gemini / Mistral / Ollama / vLLM / Together / Fireworks / 豆包 / 通义千问 / 文心一言 / 智谱 GLM；
- **Vector Store**：Chroma（默认）/ FAISS / Weaviate / Milvus / Qdrant / LanceDB / pgvector / Pinecone；
- **Tool 集成**：100+ 预制工具 + LangChain 全部 Tools + 自定义工具；
- **Code Executor**：本地 / Docker（生产推荐）；
- **Memory Backend**：Chroma（默认）/ SQLite / Redis / 自定义；
- **Document Store**：Git（默认）/ DB / FS / 自定义；
- **Observability**：结构化日志（`metagpt/logs.py`）/ OpenTelemetry / LangSmith / Langfuse；
- **部署**：Docker / Kubernetes / MetaGPT Cloud；
- **企业版**：MetaGPT Cloud + 私有部署；
- **Skills Marketplace**：100+ 预制 Skills；
- **学术合作**：ICLR 2024 / NeurIPS 2024 / ICML 2024；

## 十二、源码学习路径

- **必读模块**（按学习顺序）：
  1. `metagpt/__init__.py`：导出列表；
  2. `metagpt/startup.py`：启动入口；
  3. `metagpt/team.py`：Team 基类；
  4. `metagpt/software_company.py`：Software Company 默认 Team；
  5. `metagpt/environment.py`：Environment 基类；
  6. `metagpt/context.py` / `metagpt/context_mixin.py`：Context；
  7. `metagpt/roles/role.py`：Role 基类；
  8. `metagpt/roles/product_manager.py`：ProductManager；
  9. `metagpt/roles/architect.py`：Architect；
  10. `metagpt/roles/project_manager.py`：ProjectManager；
  11. `metagpt/roles/engineer.py`：Engineer；
  12. `metagpt/roles/qa_engineer.py`：QA Engineer；
  13. `metagpt/actions/action.py`：Action 基类；
  14. `metagpt/actions/write_prd.py`：WritePRD；
  15. `metagpt/actions/write_design.py`：WriteDesign；
  16. `metagpt/actions/write_code.py`：WriteCode；
  17. `metagpt/actions/write_test.py`：WriteTest；
  18. `metagpt/actions/review_code.py`：ReviewCode；
  19. `metagpt/subscription.py`：Subscription；
  20. `metagpt/memory/short_term.py`：ShortTerm Memory；
  21. `metagpt/memory/long_term.py`：LongTerm Memory；
  22. `metagpt/memory/global_memory.py`：Global Memory；
  23. `metagpt/rag/`：RAG 工具；
  24. `metagpt/document_store/`：Document 版本化；
  25. `metagpt/provider/`：多 LLM Provider；
  26. `metagpt/strategy/`：LLM Selection Strategy；
  27. `metagpt/skills/`：Skills；
  28. `metagpt/document.py`：Document；
  29. `metagpt/schema.py`：Schema；
  30. `metagpt/logs.py`：结构化日志；
  31. `metagpt/management/`：项目 / 文件管理；
- **LOC 与语言**：
  - `metagpt/` ~80k LoC；
  - `metagpt/roles/` ~10k LoC；
  - `metagpt/actions/` ~10k LoC；
  - `metagpt/memory/` ~5k LoC；
  - `metagpt/rag/` ~5k LoC；
  - 主语言：**Python**（100%）；
- **构建系统**：
  - **setuptools**（`setup.py`）；
  - **pip**（依赖管理）；
  - **单包**（`metagpt/` 一包）；
  - **CI/CD**：GitHub Actions；
  - **release-please**（发布管理）；

## 十三、学习路径

- **入门（1 周）**：
  - 阅读官方 Tutorial（https://docs.deepwisdom.ai/main/en/）；
  - 跑通 `metagpt "创建一个 TODO 应用"`；
  - 理解 SoftwareCompany + Role + Action + Environment；
  - 跑通 ProductManager / Architect / Engineer / QA 协作；
- **进阶（2-3 周）**：
  - 深入学习 Action（WritePRD / WriteDesign / WriteCode / WriteTest）；
  - 实战 Subscription 发布订阅；
  - 学习 Memory（ShortTerm / LongTerm / GlobalMemory）；
  - 学习 RAG（自研 RAG 工具）；
  - 学习 Document Store（Git / DB 版本化）；
  - 学习 Provider（OpenAI / Anthropic / Ollama）；
  - 学习 Strategy（LLM 选择）；
- **高级（1-2 月）**：
  - 阅读源码（Team + Role + Action + Environment + Subscription）；
  - 自定义 Role（继承 Role + 实现 _act）；
  - 自定义 Action（继承 Action + 实现 run）；
  - 自定义 Environment（继承 Environment + 实现 publish_message）；
  - 自定义 Memory / RAG；
  - 自定义 Subscription；
  - Data Interpreter 二次开发；
  - Skills Marketplace 开发；
- **推荐资源**：
  - 官方文档：https://docs.deepwisdom.ai/main/en/
  - GitHub：https://github.com/geekan/MetaGPT
  - 论文：MetaGPT: Meta Programming for A Multi-Agent Collaborative Framework（ICLR 2024）
  - 论文：Data Interpreter: An LLM Agent For Data Science
  - 博客：https://medium.com/@deepwisdom
  - Discord：https://discord.gg/metagpt
  - Forum：https://github.com/geekan/MetaGPT/discussions
  - YouTube：https://www.youtube.com/@DeepWisdomAI

## 十四、风险与陷阱

- **学习曲线陡**：Software Company SOP 概念多，新人需 1-2 周入门；
- **角色僵化**：默认角色分工偏软工开发，通用任务难适配；
- **Token 消耗大**：多 Role 协作产生高 token 消耗；
- **Subscription 调试难**：消息路由难调试；
- **Action 错误处理**：Action 异常难处理；
- **Memory 一致性**：多 Role 共享 Memory 时易冲突；
- **Document 版本化冲突**：Git Document Store 在并发下易冲突；
- **RAG 集成成本**：默认 RAG 文档少，需自定义；
- **Provider 兼容**：多 Provider 行为差异大；
- **Strategy 调优**：LLM Selection Strategy 难调；
- **Performance 问题**：Python 调度开销大，超大 Team（>10 Role）性能下降；
- **Code Executor 沙箱**：本地执行有安全风险，必须用 Docker；
- **多模态支持弱**：图像 / 视频 / 音频支持较少；
- **HITL 集成弱**：默认 HITL 支持较弱；
- **Studio 功能缺失**：无官方 Web Studio，需自建；
- **学术贡献者减少**：DeepWisdom 团队 2025 后维护速度下降；

## 十六、参考资料

- 官方仓库：https://github.com/geekan/MetaGPT
- 官方文档：https://docs.deepwisdom.ai/main/en/
- 论文：MetaGPT: Meta Programming for A Multi-Agent Collaborative Framework（ICLR 2024）
- 论文：Data Interpreter: An LLM Agent For Data Science
- 论文：Affordable AI Assistants with Knowledge Graph of Thoughts（DeepWisdom）
- 论文：AFlow: Automating Agentic Workflow Generation
- 博客：https://medium.com/@deepwisdom
- Tutorial：https://docs.deepwisdom.ai/main/en/tutorials/
- Discord：https://discord.gg/metagpt
- Forum：https://github.com/geekan/MetaGPT/discussions
- YouTube：https://www.youtube.com/@DeepWisdomAI
- MetaGPT Cloud：https://cloud.deepwisdom.ai/
- 学术合作：https://www.deepwisdom.ai/

## 十七、与本项目关联

MetaGPT 作为「软件工程 SOP + 多角色协作 + Message → Document → Code」行业代表多 Agent 框架，与本项目（engineering/ 轨道 C/C++ 自研存储与向量索引）在「角色化抽象 + SOP 流水线 + Document 版本化 + Subscription 路由」四个维度具有直接参考价值：

- **Role / Action 抽象 ↔ 索引 AM**：Role / Action 抽象与本项目索引 AM 抽象（`engineering/src/db/index/`）的"**能力声明 + 参数化 + 可组合**"思路相似。
- **Software Company SOP ↔ SQL Pipeline**：Software Company SOP（PRD → Design → Code → Test → Review）与本项目 SQL 执行流水线（解析 → 优化 → 执行 → 索引 → 优化器）相似——都是"**标准化输入 → 规范化中间产物 → 命令式执行**"。
- **Document Store ↔ WAL + 索引**：Document Store（`metagpt/document_store/`）的 Git 版本化与本项目 WAL（`engineering/src/db/storage/log/`）+ BTree 索引（`engineering/src/db/index/btree/`）的"**持久化 + 版本化 + 可重放**"机制相似。
- **Subscription ↔ 主题订阅**：Subscription（`metagpt/subscription.py`）机制与本项目协调器（`engineering/src/db/distributed/coordinator/`）的"**事件订阅 + 自动路由**"机制相似。
- **Memory 三层 ↔ 存储分层**：ShortTerm / LongTerm / GlobalMemory 与本项目多层存储（`engineering/src/db/storage/`）的多层抽象（buffer / log / catalog / blob）有相似性。
- **RAG ↔ 向量索引**：MetaGPT 的 RAG（`metagpt/rag/`）与本项目 HNSW / IVF / PQ 向量索引对照，本项目 wiki `docs/db-wiki/vector/` 已系统梳理。
- **Provider ↔ 多 LLM 路由**：MetaGPT Provider（`metagpt/provider/`）与本项目 LiteLLM 集成（`engineering/src/llm/llm.h`）对照。
- **Strategy ↔ GUC + 索引选择器**：Strategy（`metagpt/strategy/`）与本项目 GUC（`engineering/src/db/core/guc.c`）+ Index Selector（`engineering/src/db/index/index_selector.c`）的"**配置驱动 + 自动选择**"机制相似。
- **Skills Marketplace ↔ 算法市场**：Skills Marketplace 模式可启发本项目算法市场（`engineering/src/algo-prod/`）。
- **Code Executor ↔ SQL Executor**：Engineer 角色的 Code Executor（`metagpt/actions/write_code.py`）与本项目 SQL Executor（`engineering/src/db/executor/`）的"**代码生成 + 执行 + 测试**"机制相似。
- **Data Interpreter ↔ 自研 Data Pipeline**：Data Interpreter（`metagpt/roles/data_interpreter.py`）与本项目多模态 RAG 应用（`engineering/apps/multimodal_rag/`）对照。
- **学习建议**：阅读 MetaGPT 源码重点关注「Software Company SOP + Role / Action 抽象 + Subscription 路由 + Document 版本化」，与本项目 `engineering/src/db/sql/` + `engineering/src/db/storage/` 做架构对照，加深对「SOP 流水线 + 多 Agent + 持久化」的理解。