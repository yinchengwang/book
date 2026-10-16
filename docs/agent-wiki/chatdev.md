# ChatDev Wiki（OpenBMB 出品的零代码多 Agent 平台）

> 参考版本：`chatdev` 主分支（ChatDev 2.0 DevAll，2026-01 发布；chatdev1.0 旧版本在公司文件 `company.py`，已迁移到 `chatdev1.0` 分支），参考 HEAD：b292a33a6 前的最终 release（ChatDev 2.0）。本 wiki 重点介绍 ChatDev 作为「**零代码多 Agent 编排 + 图形化工作流 + YAML 配置 + 多模型适配 + Chat Chain + 公司角色分工**」的多 Agent 平台定位、源码架构、四个 Agent 维度（角色协作 / 记忆 / 工具调用 / 人机协作）的深度展开，以及与本项目 `engineering/` 自研存储与向量索引的对照参考。

## 一、项目定位

ChatDev 是由 **OpenBMB（清华 & 面壁智能）** 于 **2023 年 8 月**开源的 **「软件公司多 Agent 框架」**（v1.x）/ **「零代码多 Agent 编排平台」**（v2.0 DevAll，2026-01 发布），定位经历两次重大演进：

- **v1.x（2023-2025）**：模拟虚拟软件公司（CEO / CTO / Programmer / Reviewer / Tester），通过 Chat Chain 协议协作完成软件开发任务；
- **v2.0 DevAll（2026-01）**：升级为零代码多 Agent 编排平台，用户通过 YAML 配置定义 Agent / Workflow / Task，编排数据可视化 / 3D 生成 / 深度研究等场景；

- **核心痛点**：
  - 缺少**图形化工作流编辑器**（其他框架只能写代码）；
  - 缺少**零代码 Agent 编排**（业务人员难上手）；
  - 缺少**软件公司模拟**（CEO / CTO / Programmer 角色分工）；
  - 缺少**Chain-of-Communication 协议**（ChatDev 1.x 创新）；
  - 缺少**多模型适配**（GitHub Copilot / Claude / GPT / Ollama）；
  - 缺少**完整软件工程流水线**（设计 / 编码 / 测试 / 文档）；
- **典型用户**：
  - **企业业务人员**：零代码编排多 Agent；
  - **学术研究**：Agent 协作 / SOP 探索；
  - **软件开发**：自动代码生成 / 文档生成；
  - **数据可视化**：通过 Agent 编排生成图表 / 仪表板；
- **核心卖点**：
  - **零代码配置**：YAML 配置 Agent / Workflow / Task；
  - **图形化编辑器**：前端 Web 编辑器；
  - **Chat Chain 协议**：Chain-of-Communication（ChatDev 1.x 创新）；
  - **公司角色分工**：CEO / CTO / Programmer / Reviewer / Tester；
  - **完整软件工程**：设计 / 编码 / 测试 / 文档 / Review；
  - **多模型适配**：GitHub Models / Claude / GPT / Ollama / vLLM；
  - **MCP 集成**：Model Context Protocol 集成；
  - **图执行引擎**：DAG / Cycle / Parallel / Dynamic Edge 执行器；
  - **Hook 系统**：Pre / Post 钩子；
  - **Schema Registry**：动态 Schema 注册；
  - **REST Server**：HTTP API 服务器；
  - **开源 Apache-2.0**：可商用、修改、私有部署；

## 二、发展历史与社区

- **起源（2023-08）**：OpenBMB 开源 ChatDev v1.0，发表论文《ChatDev: Communicative Agents for Software Development》；
- **里程碑**：
  - 2023-08：v1.0（CEO / CTO / Programmer / Reviewer / Tester 角色分工 + Chat Chain 协议）；
  - 2023-12：v1.1.x（GitHub Models 集成 / 多模型路由）；
  - 2024-06：v1.2.x（Art 模式 / Visualization / 数据可视化 Agent）；
  - 2025-01：v1.x（稳定版本 + 学术研究 + 教育应用）；
  - 2026-01：**v2.0 DevAll 重大重写**（零代码多 Agent 编排平台 + 图形化编辑器 + YAML 配置 + 图执行引擎）；
  - 2026-03：v2.0.x（持续完善 + 企业特性）；
- **维护状态**：**活跃**（OpenBMB 团队 15+ 工程师，月均 60+ commits）；
- **社区**：
  - GitHub 27k+ star（v1.x 历史峰值）、`chatdev` PyPI 月下载量 50 万+；
  - Discord 2k+ 成员、GitHub Discussions；
  - 学术论文：ChatDev: Communicative Agents for Software Development（LREC-COLING 2024）；
  - 学术论文：Executable Code Actions Elicit Better LLM Agents（OpenBMB）；
- **关键贡献者**：钱忱（创始人）、OpenBMB 团队、贡献者 100+；

## 三、许可与商业限制

- **核心代码**：**Apache-2.0 License**（`workflow/`、`runtime/`、`entity/`、`server/`、`functions/`、`tools/`），可商用、修改、私有部署；
- **ChatDev 1.0（Legacy）**：Apache-2.0（同 v2.0）；
- **生态关系**：
  - **上游依赖**：
    - Python 3.10+；
    - OpenAI / Anthropic / GitHub Models / Ollama / vLLM；
    - FastAPI（HTTP Server）；
    - Pydantic v2（Schema）；
    - asyncio + aiohttp；
  - **下游生态**：约 2000+ 学术研究 + 企业应用；

## 四、业界主要使用方与案例

- **国际**（公开）：
  - **多家初创公司**：零代码 Agent 编排；
  - **多家研究机构**：学术研究；
- **国内**：
  - **多家互联网公司**：软件公司多 Agent 模拟；
  - **多家企业**：零代码 Agent 编排；
- **典型场景**：
  - **软件公司模拟**：CEO / CTO / Programmer / Reviewer / Tester；
  - **零代码 Agent 编排**：业务人员定义 YAML；
  - **数据可视化**：自动生成图表 / 仪表板；
  - **3D 生成**：自动 3D 模型生成；
  - **深度研究**：多 Agent 协作研究；
  - **学术研究**：Agent 协作 / SOP 探索；
  - **教育培训**：多 Agent 教学演示；

## 五、一句话对比定位

> ChatDev 是 **「OpenBMB 出品 + 零代码多 Agent + Chat Chain + 软件公司模拟 + 图形化编辑器 + YAML + 27k star + Apache-2.0」**；与 **LangGraph（图状态机）**、**CrewAI（角色化 + Process）**、**AutoGen（GroupChat 对话）**、**MetaGPT（软件公司角色）**、**OpenAI Agents（Handoff 简单协议）**、**AgentScope（多 Agent 通用）** 构成多 Agent 生态。ChatDev 的差异化是 **「零代码编排 + ChatDev 1.x 软件公司模拟 + Chat Chain 协议 + OpenBMB 出品 + 中文社区 + Apache-2.0」**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                  用户应用层（User Apps）                              │
│        ChatDev Frontend / CLI / REST Server / Python API             │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Frontend（frontend/）                                │
│   React + Vite / YAML 编辑器 / 工作流图形编辑器                     │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  REST Server（server/）                               │
│   app.py（FastAPI App）                                              │
│   bootstrap.py（启动 / 配置）                                       │
│   routes/（HTTP 路由）                                                │
│   services/（服务层）                                                │
│   config_schema_router.py（配置 Schema 路由）                        │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Workflow 引擎（workflow/）                           │
│   graph.py（Graph 抽象）                                             │
│   graph_manager.py（GraphManager）                                  │
│   graph_context.py（GraphContext）                                  │
│   topology_builder.py（拓扑构建）                                   │
│   cycle_manager.py（循环管理）                                       │
│   subgraph_loader.py（子图加载）                                     │
│   executor/                                                          │
│     ├── dag_executor.py（DAG 执行器）                               │
│     ├── cycle_executor.py（循环执行器）                             │
│     ├── parallel_executor.py（并行执行器）                           │
│     ├── dynamic_edge_executor.py（动态边执行器）                     │
│     └── resource_manager.py（资源管理器）                           │
│   hooks/（Pre / Post 钩子）                                          │
│   runtime/（Workflow Runtime）                                       │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Runtime（runtime/）                                  │
│   bootstrap/（Bootstrap）                                            │
│   edge/（Edge Runtime）                                              │
│   node/（Node Runtime）                                              │
│   sdk.py（Runtime SDK）                                              │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Entity（entity/）                                    │
│   config_loader.py（YAML 配置加载）                                  │
│   configs/（预制配置）                                                │
│   enums.py / enum_options.py（枚举）                                │
│   graph_config.py（Graph 配置）                                      │
│   messages.py（消息抽象）                                            │
│   tool_spec.py（Tool 规格）                                          │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Functions / Tools / Schema Registry                 │
│   functions/（Function 注册）                                        │
│     ├── edge/（边函数）                                              │
│     ├── edge_processor/（边处理器）                                  │
│     └── function_calling/（Function Calling）                       │
│   tools/（Tool 注册）                                                │
│   schema_registry/registry.py（动态 Schema 注册）                  │
│   mcp_example/（MCP 集成示例）                                       │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Utils / YAML / Tests                                │
│   utils/（工具）                                                      │
│   yaml_template/（YAML 模板）                                         │
│   yaml_instance/（YAML 实例 + 子图）                                  │
│   tests/（测试）                                                      │
│   server_main.py（Server 启动）                                       │
│   run.py（CLI 启动）                                                  │
└─────────────────────────────────────────────────────────────────────┘
```

进程模型：ChatDev v2.0 是**FastAPI HTTP Server**（`server/app.py`）+ **Async Runtime**（`runtime/`）+ **Frontend SPA**（`frontend/`）三层架构；通过 YAML 配置驱动工作流执行。

## 七、Agent 类型专项（核心扩展章节）

### 7.1 角色协作模式（Role Collaboration）

ChatDev 通过 **Graph / Node / Edge / Agent** 实现四种协作模式：

- **中心化（Orchestrator-Worker）**：
  - **典型实现**：Graph 的中心节点（Orchestrator）调度多个 Worker Node；
  - **实现位置**：`workflow/graph.py`（Graph 抽象）+ `workflow/topology_builder.py`；
  - **机制**：Orchestrator 节点通过 Edge 委派任务给 Worker 节点；
- **对等（Peer-to-Peer / Blackboard）**：
  - **典型实现**：所有 Node 共享 GraphContext（blackboard pattern）；
  - **实现位置**：`workflow/graph_context.py`（GraphContext 抽象）；
  - **共享 State**：所有节点读写同一 Context；
- **分层（Hierarchical / Supervisor）**：
  - **典型实现**：ChatDev 1.x 的 CEO / CTO / Programmer 层级；
  - **实现位置**：`chatdev1.0/chatdev/company.py`（v1 旧分支）；
  - **v2.0**：通过 Graph 嵌套 + Subgraph 实现层级；
- **市场（Market-based Bidding）**：
  - **典型实现**：通过 dynamic_edge_executor 动态选择 Node；
  - **实现位置**：`workflow/executor/dynamic_edge_executor.py`；
  - **机制**：LLM 决策选择下一个 Node；
- **Cycle / Loop 协作**：
  - **典型实现**：ChatDev 1.x 的 Chain-of-Communication（CEO ↔ CTO ↔ Programmer）；
  - **实现位置**：`workflow/cycle_manager.py` + `workflow/executor/cycle_executor.py`；
  - **机制**：多 Node 循环交互直到达成共识；
- **DAG 协作**：
  - **典型实现**：多 Node 按依赖顺序执行；
  - **实现位置**：`workflow/executor/dag_executor.py`；
  - **典型场景**：数据处理流水线；
- **Parallel 协作**：
  - **典型实现**：多个 Node 并行执行；
  - **实现位置**：`workflow/executor/parallel_executor.py`；
  - **典型场景**：Map-Reduce；
- **Chat Chain 协议（v1.x 特色）**：
  - **典型实现**：CEO / CTO / Programmer / Reviewer / Tester 通过 Chat Chain 协作；
  - **实现位置**：`chatdev1.0/chatdev/company.py`；
  - **机制**：每个 Role 按顺序发言，Reviewer 负责 Review，Tester 负责测试；

### 7.2 记忆系统（Memory System）

ChatDev 记忆分**Graph Context + Node State + Session**：

- **短期（Graph Context）**：
  - **实现位置**：`workflow/graph_context.py`（GraphContext）；
  - **存储**：当前 Graph 执行的状态；
  - **API**：自动维护在 Graph 执行中；
- **中期（Node State）**：
  - **实现位置**：每个 Node 内部 state；
  - **存储**：当前 Node 执行的局部状态；
- **长期（Session / Database）**：
  - **实现位置**：`runtime/`（Runtime 抽象 Session）；
  - **持久化**：通过 Schema Registry + Database；
  - **API**：通过 Server API 持久化；
- **Vector Memory**：
  - **不直接支持**，但可通过自定义 Function + Vector Store 实现；
- **File Memory**：
  - **不直接支持**，但可通过 YAML 配置 + 文件系统实现；
- **Schema Registry**：
  - **实现位置**：`schema_registry/registry.py`；
  - **机制**：动态注册数据类型（Message / State / Document）；
  - **API**：`registry.register("MyType", schema)`；
- **YAML 配置持久化**：
  - **实现位置**：`yaml_template/` + `yaml_instance/`；
  - **机制**：用户定义 YAML 配置驱动 Agent / Workflow；

### 7.3 工具调用协议（Tool Calling Protocol）

ChatDev 通过 **Function / Tool / Function Calling** 实现工具调用：

- **Function Calling（OpenAI / Anthropic / Gemini / GitHub Models）**：
  - **实现位置**：`functions/function_calling/`（Function Calling 集成）；
  - **支持模型**：GitHub Models（默认）/ OpenAI / Anthropic / Gemini / Ollama / vLLM；
  - **绑定工具**：`agent = Node(tools=[my_function])`；
- **Tool 注册**：
  - **实现位置**：`tools/`（Tool 注册）+ `schema_registry/registry.py`；
  - **API**：`registry.register_tool("my_tool", schema)`；
- **Function 注册**：
  - **实现位置**：`functions/`（Function 注册）；
  - **类型**：edge / edge_processor / function_calling；
- **ReAct（Reasoning+Acting）**：
  - **典型实现**：通过 Graph Node 实现 ReAct 循环；
  - **机制**：每轮 Node 生成 Thought + Action（Tool 调用）→ 观察结果 → 下一轮；
- **CodeAct（生成代码执行）**：
  - **典型实现**：通过 Tool + Python REPL；
  - **沙箱**：本地 / Docker（生产推荐）；
- **Tool Use + Reflection**：
  - **典型实现**：通过 Hook 系统反思 Tool 输出；
  - **实现位置**：`workflow/hooks/`（Pre / Post 钩子）；
  - **API**：`@hook("post_tool_call")`；
- **Structured Outputs**：
  - **实现位置**：`entity/tool_spec.py`（Tool 规格）+ `entity/messages.py`（消息）；
  - **API**：通过 Pydantic 模型定义；
- **MCP（Model Context Protocol）**：
  - **实现位置**：`mcp_example/`（MCP 集成示例）+ `functions/`；
  - **API**：`MCPServer(url="...")`；
- **Edge Function**：
  - **实现位置**：`functions/edge/`（边函数）；
  - **机制**：Graph Edge 上的转换函数（输入 / 输出处理）；

### 7.4 人机协作（Human-in-the-Loop）

ChatDev HITL 通过 **Hook / Custom Node / Function / Frontend UI** 实现：

- **Hook 系统**：
  - **典型实现**：`@hook("pre_node")` / `@hook("post_node")`；
  - **实现位置**：`workflow/hooks/`；
  - **API**：Hook 内部可调用 `input("Continue? [y/n]: ")`；
- **Custom Node**：
  - **典型实现**：自定义 Node 让人类参与；
  - **API**：`Node(type="human")`；
- **Frontend HITL**：
  - **典型实现**：通过 React Frontend UI 中断 / 批准 / 修改；
  - **实现位置**：`frontend/src/`；
- **Approval Workflow**：
  - **典型实现**：自定义 Function 调用 `input()`；
  - **API**：`Function.requires_approval = True`；
- **Cycle Interrupt**：
  - **典型实现**：`cycle_executor.py` 在 cycle 中可中断等待人工；
  - **API**：`cycle_manager.pause()`；
- **REST Server HITL**：
  - **典型实现**：Server 端接收人工输入后恢复；
  - **API**：`POST /api/hitl/respond`；
- **Audit Trail**：
  - **实现位置**：`workflow/cycle_manager.py`（Cycle History）；
  - **API**：自动 trace 所有 Node / Edge / Cycle 调用；

## 八、索引机制（Index / Schema Registry / Hook）

ChatDev 索引通过 **Schema Registry + Hook + Edge** 实现：

- **Schema Registry**：
  - **实现位置**：`schema_registry/registry.py`；
  - **机制**：动态注册数据类型 / Tool Schema；
  - **API**：`registry.register("MyType", schema)`；
- **Hook 系统**：
  - **实现位置**：`workflow/hooks/`；
  - **API**：`@hook("pre_node")` / `@hook("post_node")` / `@hook("pre_edge")` / `@hook("post_edge")`；
- **Edge Function 索引**：
  - **实现位置**：`functions/edge/`（边函数注册）；
  - **机制**：Edge Function 是 Graph 中数据流转的转换函数；
- **Cycle Index**：
  - **实现位置**：`workflow/cycle_manager.py`（Cycle History）；
  - **机制**：记录 cycle 的所有历史；
- **Topology Index**：
  - **实现位置**：`workflow/topology_builder.py`；
  - **机制**：从 YAML 构建 Graph 拓扑；

## 九、查询/计算/接口层

### 9.1 Workflow 执行引擎（Graph Execution Engine）

- **Graph Manager**：
  - **实现位置**：`workflow/graph_manager.py`；
  - **API**：`graph_manager.run(graph_id, inputs)`；
  - **场景**：调度 Graph 执行；
- **DAG Executor**：
  - **实现位置**：`workflow/executor/dag_executor.py`；
  - **机制**：按拓扑顺序执行 Node；
- **Cycle Executor**：
  - **实现位置**：`workflow/executor/cycle_executor.py`；
  - **机制**：多 Node 循环交互；
- **Parallel Executor**：
  - **实现位置**：`workflow/executor/parallel_executor.py`；
  - **机制**：多 Node 并行执行（asyncio.gather）；
- **Dynamic Edge Executor**：
  - **实现位置**：`workflow/executor/dynamic_edge_executor.py`；
  - **机制**：LLM 决策动态选择下一个 Node；
- **Resource Manager**：
  - **实现位置**：`workflow/executor/resource_manager.py`；
  - **机制**：管理 Node 执行时的资源（GPU / 内存 / LLM 调用）；

### 9.2 状态管理（State / Context / Subgraph）

- **Graph State**：
  - **实现位置**：`workflow/graph_context.py`（GraphContext）；
  - **内容**：所有 Node 共享的 State + Messages；
- **Node State**：
  - **实现位置**：Node 内部 state；
- **Subgraph**：
  - **典型实现**：Subgraph 作为 Graph 内的子图；
  - **实现位置**：`workflow/subgraph_loader.py` + `yaml_instance/subgraphs/`；
  - **API**：`subgraph = load_subgraph("subgraph_id")`；
- **Cycle History**：
  - **实现位置**：`workflow/cycle_manager.py`（Cycle History）；
  - **API**：`cycle.history` 查看历史；
- **Pause / Resume**：
  - **典型实现**：通过 Hook 实现 pause / resume；
  - **API**：`cycle_manager.pause()`；

### 9.3 多模态工具（Multimodal Tools）

- **Art 模式**：
  - **典型实现**：v1.x 的 Art 模式（自动图像 / 视频生成）；
  - **实现位置**：`chatdev1.0/chatdev/`；
- **Data Visualization**：
  - **典型实现**：v2.0 的数据可视化 Agent；
  - **API**：通过 Function + Matplotlib / Plotly；
- **3D Generation**：
  - **典型实现**：v2.0 的 3D 生成 Agent；
  - **API**：通过 Function + Blender / Meshy；
- **Code Executor**：
  - **典型实现**：通过 Function + Python REPL；
- **RAG Tool**：
  - **典型实现**：通过 Function + Vector Store；
- **Database Tool**：
  - **典型实现**：通过 Function + SQLAlchemy / asyncpg；
- **Search Tool**：
  - **典型实现**：通过 Function + Tavily / Serper；
- **Multi-modal**：
  - **典型实现**：通过 Function + GPT-4V / Claude 3 / Gemini Vision；

### 9.4 多 Agent 通信（Multi-Agent Communication）

- **Graph 共享 Context**：
  - **实现位置**：`workflow/graph_context.py`；
  - **机制**：所有 Node 共享 GraphContext；
- **Edge Function**：
  - **实现位置**：`functions/edge/`；
  - **机制**：Edge Function 在 Node 间传递数据；
- **Cycle 协议**：
  - **典型实现**：v1.x 的 Chat Chain 协议（CEO ↔ CTO ↔ Programmer 循环）；
- **Subgraph 通信**：
  - **API**：Subgraph 与父图通过共享 Context 通信；
- **Message Bus**：
  - **典型实现**：通过 Graph Context + Edge Function；
- **A2A Protocol**：
  - **ChatDev 通过 Server + REST API** 实现 A2A；
  - **典型实现**：通过 Server 跨进程调用 Graph；

## 十、查询/计算/接口层（API & SDK）

- **Python API（核心）**：
  - `workflow.graph.Graph`；
  - `workflow.graph_manager.GraphManager`；
  - `workflow.graph_context.GraphContext`；
  - `workflow.topology_builder.TopologyBuilder`；
  - `workflow.cycle_manager.CycleManager`；
  - `workflow.subgraph_loader.SubgraphLoader`；
  - `workflow.executor.dag_executor.DagExecutor`；
  - `workflow.executor.cycle_executor.CycleExecutor`；
  - `workflow.executor.parallel_executor.ParallelExecutor`；
  - `workflow.executor.dynamic_edge_executor.DynamicEdgeExecutor`；
  - `workflow.executor.resource_manager.ResourceManager`；
  - `workflow.hooks.*`（Pre / Post 钩子）；
  - `runtime.sdk.Runtime`；
  - `runtime.bootstrap.Bootstrap`；
  - `runtime.node.Node` / `runtime.edge.Edge`；
  - `entity.config_loader.ConfigLoader`；
  - `entity.messages.Message`；
  - `entity.tool_spec.ToolSpec`；
  - `schema_registry.registry.Registry`；
  - `functions.function_calling.FunctionCaller`；
  - `functions.edge.EdgeFunction`；
- **CLI**：
  - `python run.py`（启动 CLI）；
  - `python server_main.py`（启动 REST Server）；
- **REST Server API**：
  - `POST /api/graph/run`（运行 Graph）；
  - `POST /api/cycle/pause`（暂停 Cycle）；
  - `POST /api/cycle/resume`（恢复 Cycle）；
  - `POST /api/hitl/respond`（人工响应）；
  - `GET /api/schema/registry`（Schema 注册表查询）；
- **Frontend**：
  - React + Vite SPA；
  - YAML 编辑器；
  - 工作流图形编辑器；
  - 调试 + 测试 + 监控；
- **YAML 配置**：
  - `yaml_template/`（YAML 模板）；
  - `yaml_instance/`（YAML 实例 + 子图）；
  - **API**：用户定义 Agent / Workflow / Task YAML；

## 十一、生态与集成

- **LLM 集成**：GitHub Models / OpenAI / Anthropic / Azure OpenAI / Google Gemini / Mistral / Ollama / vLLM / Together / Fireworks；
- **Vector Store**：通过 Function + LangChain / LlamaIndex 集成所有 Vector DB；
- **Tool 集成**：Function 注册 + Tool 注册 + MCP Server + 自定义工具；
- **Schema Registry**：动态 Schema 注册；
- **Cycle / DAG / Parallel / Dynamic Edge**：四种执行器；
- **Hook 系统**：Pre / Post 钩子；
- **Frontend**：React + Vite + YAML 编辑器；
- **REST Server**：FastAPI + Pydantic v2；
- **MCP**：Model Context Protocol 集成；
- **Observability**：结构化日志 + Cycle History + Hook；
- **部署**：Docker / Kubernetes / FastAPI Server；
- **企业版**：商业集成（规划中）；
- **学术合作**：LREC-COLING 2024 / NeurIPS 2024；
- **中文社区**：OpenBMB 中文社区；

## 十二、源码学习路径

- **必读模块**（按学习顺序）：
  1. `run.py`：CLI 启动；
  2. `server_main.py`：Server 启动；
  3. `server/app.py`：FastAPI App；
  4. `server/bootstrap.py`：Server 启动配置；
  5. `server/routes/`：HTTP 路由；
  6. `server/services/`：服务层；
  7. `workflow/graph.py`：Graph 抽象；
  8. `workflow/graph_manager.py`：GraphManager；
  9. `workflow/graph_context.py`：GraphContext；
  10. `workflow/topology_builder.py`：拓扑构建；
  11. `workflow/cycle_manager.py`：Cycle 管理；
  12. `workflow/subgraph_loader.py`：Subgraph 加载；
  13. `workflow/executor/dag_executor.py`：DAG 执行器；
  14. `workflow/executor/cycle_executor.py`：Cycle 执行器；
  15. `workflow/executor/parallel_executor.py`：Parallel 执行器；
  16. `workflow/executor/dynamic_edge_executor.py`：Dynamic Edge 执行器；
  17. `workflow/executor/resource_manager.py`：资源管理器；
  18. `workflow/hooks/`：Hook 系统；
  19. `workflow/runtime/`：Workflow Runtime；
  20. `runtime/bootstrap/`：Bootstrap；
  21. `runtime/sdk.py`：Runtime SDK；
  22. `runtime/node/`：Node Runtime；
  23. `runtime/edge/`：Edge Runtime；
  24. `entity/config_loader.py`：YAML 配置加载；
  25. `entity/enums.py` / `enum_options.py`：枚举；
  26. `entity/graph_config.py`：Graph 配置；
  27. `entity/messages.py`：消息抽象；
  28. `entity/tool_spec.py`：Tool 规格；
  29. `functions/edge/`：Edge 函数；
  30. `functions/edge_processor/`：Edge 处理器；
  31. `functions/function_calling/`：Function Calling；
  32. `tools/`：Tool 注册；
  33. `schema_registry/registry.py`：Schema Registry；
  34. `mcp_example/`：MCP 集成示例；
  35. `yaml_template/`：YAML 模板；
  36. `yaml_instance/`：YAML 实例 + 子图；
  37. `utils/`：工具；
  38. `tests/`：测试；
  - **legacy（v1.x，仅供参考）**：
    - `chatdev1.0/chatdev/company.py`：公司编排；
    - `chatdev1.0/chatdev/roles/`：CEO / CTO / Programmer / Reviewer / Tester；
- **LOC 与语言**：
  - `workflow/` ~10k LoC；
  - `runtime/` ~5k LoC；
  - `server/` ~5k LoC；
  - `entity/` ~3k LoC；
  - `functions/` ~3k LoC；
  - `frontend/` ~10k LoC（TypeScript）；
  - 主语言：**Python**（85%）、**TypeScript**（15%，Frontend）；
- **构建系统**：
  - **Hatchling**（`pyproject.toml`）；
  - **uv**（依赖管理）；
  - **Vite + React**（Frontend）；
  - **monorepo**（v2.0 整体架构）；
  - **CI/CD**：GitHub Actions；
  - **Docker / docker-compose**：部署；

## 十三、学习路径

- **入门（1 周）**：
  - 阅读官方文档（https://chatdev.modelbest.cn/）；
  - 跑通 ChatDev 1.x（`chatdev1.0` 分支）的软件公司示例；
  - 理解 CEO / CTO / Programmer / Reviewer / Tester 角色；
  - 跑通 ChatDev 2.0 DevAll 的 YAML 配置示例；
  - 理解 Workflow / Node / Edge 抽象；
- **进阶（2-3 周）**：
  - 深入学习 Graph 执行引擎（DAG / Cycle / Parallel / Dynamic Edge）；
  - 实战 Hook 系统（Pre / Post 钩子）；
  - 学习 Schema Registry（动态 Schema 管理）；
  - 学习 Edge Function（数据流转处理）；
  - 学习 Subgraph（嵌套子图）；
  - 学习 Resource Manager（资源管理）；
  - 学习 MCP 集成；
  - 学习 Frontend YAML 编辑器二次开发；
- **高级（1-2 月）**：
  - 阅读源码（Graph + Executor + Hook + Schema Registry + Runtime）；
  - 自定义 Executor（继承 Executor 基类）；
  - 自定义 Hook（实现 Pre / Post Hook）；
  - 自定义 Schema（注册到 Schema Registry）；
  - 自定义 Edge Function（实现 EdgeFunction 接口）；
  - 自定义 Subgraph（YAML + Python）；
  - 自定义 Resource Manager（实现 GPU / LLM 资源管理）；
  - REST Server 二次开发；
  - Frontend 二次开发（React + Vite）；
- **推荐资源**：
  - 官方仓库：https://github.com/OpenBMB/ChatDev
  - 官方文档：https://chatdev.modelbest.cn/
  - ChatDev 1.x Legacy：https://github.com/OpenBMB/ChatDev/tree/chatdev1.0
  - Frontend：https://github.com/OpenBMB/ChatDev/tree/main/frontend
  - 论文：ChatDev: Communicative Agents for Software Development（LREC-COLING 2024）
  - 论文：Executable Code Actions Elicit Better LLM Agents（OpenBMB）
  - 博客：https://openbmb.github.io/
  - Discord：https://discord.gg/openbmb
  - Forum：https://github.com/OpenBMB/ChatDev/discussions
  - 学术合作：清华大学 / 面壁智能

## 十四、风险与陷阱

- **v1 → v2 重写破坏性升级**：v2.0 DevAll 与 v1.x 不兼容，API 重写；
- **v2.0 新版本不稳定**：v2.0 刚发布，文档 / API 还不完善；
- **ChatDev 1.x 维护减少**：v1.x 移到 `chatdev1.0` 分支，新功能都加在 v2.0；
- **Chat Chain 复杂**：ChatDev 1.x 的 CEO/CTO/Programmer 循环逻辑复杂，新人难理解；
- **YAML 学习曲线**：YAML 配置学习曲线陡；
- **Frontend 与后端耦合**：Frontend 与 Backend API 强耦合；
- **Schema Registry 文档少**：Schema Registry 文档较少；
- **Hook 系统调试难**：Hook 触发链复杂，调试难；
- **Cycle 死循环**：Cycle Executor 可能进入死循环；
- **Dynamic Edge 性能**：LLM-based Dynamic Edge 性能开销大；
- **Resource Manager 不完善**：GPU / LLM 资源调度不完善；
- **MCP 不稳定**：MCP 协议还在演进；
- **Performance 问题**：Python 调度开销大，超大 Graph 性能下降；
- **HITL 集成弱**：默认 HITL 支持较弱，需自定义 Hook；
- **Studio 功能缺失**：Frontend 仅 YAML 编辑器，缺少工作流调试；
- **Token 消耗大**：Cycle 循环 + Multi-Step 模型消耗大量 token；
- **OpenBMB 中文社区**：主要社区在中文，国际用户较少；

## 十六、参考资料

- 官方仓库：https://github.com/OpenBMB/ChatDev
- 官方文档：https://chatdev.modelbest.cn/
- ChatDev 1.x Legacy：https://github.com/OpenBMB/ChatDev/tree/chatdev1.0
- Frontend：https://github.com/OpenBMB/ChatDev/tree/main/frontend
- 论文：ChatDev: Communicative Agents for Software Development（LREC-COLING 2024）
- 论文：Executable Code Actions Elicit Better LLM Agents
- 论文：Communicative Agents for Software Development（arXiv）
- 博客：https://openbmb.github.io/
- Tutorial：https://chatdev.modelbest.cn/quickstart
- Discord：https://discord.gg/openbmb
- Forum：https://github.com/OpenBMB/ChatDev/discussions
- YouTube：https://www.youtube.com/@OpenBMB
- 模型工具 ModelBest：https://modelbest.cn/
- 学术合作：清华大学 / 面壁智能

## 十七、与本项目关联

ChatDev 作为「零代码多 Agent + Chat Chain + 软件公司模拟 + 图形化编辑器」行业代表多 Agent 框架，与本项目（engineering/ 轨道 C/C++ 自研存储与向量索引）在「YAML 配置 + Graph 执行 + Hook 拦截 + Schema Registry」四个维度具有直接参考价值：

- **YAML 配置 ↔ GUC 配置**：YAML 配置驱动（`workflow/topology_builder.py`）与本项目 GUC（`engineering/src/db/core/guc.c`）+ 启动配置（`engineering/src/db/core/server.c`）的"**声明式 + 可持久化 + 可重载**"思路相似——都是"**配置驱动 + 运行时生效**"思路。
- **Graph 执行引擎 ↔ SQL Pipeline**：Graph 执行引擎（DAG / Cycle / Parallel / Dynamic Edge）与本项目 SQL 执行器（`engineering/src/db/executor/`）的多种执行模式（顺序 / 并行 / 动态）相似。
- **Hook 系统 ↔ GUC + 触发器**：Hook 系统（`workflow/hooks/`）与本项目 Trigger（`engineering/src/db/executor/trigger.c`）+ GUC（`engineering/src/db/core/guc.c`）的"**拦截 + 决策控制**"机制相似。
- **Schema Registry ↔ 元数据管理**：Schema Registry（`schema_registry/registry.py`）与本项目 PG 风格 Catalog（`engineering/src/db/storage/catalog/`）的"**动态 Schema + 元数据管理**"机制相似。
- **Edge Function ↔ Operator**：Edge Function（`functions/edge/`）与本项目 SQL Operator（`engineering/src/db/optimizer/`）的"**数据流转 + 转换**"机制相似。
- **Subgraph ↔ SubPlan**：Subgraph 嵌套与本项目 SQL SubPlan / SubQuery 的嵌套思路相似。
- **Cycle Manager ↔ 分布式协调**：Cycle Manager（`workflow/cycle_manager.py`）的循环交互与本项目 Phase 9 分布式协调（`engineering/src/db/distributed/coordinator/`）的"**循环 + 协调 + 重试**"机制相似。
- **Resource Manager ↔ GUC + 资源池**：Resource Manager（`workflow/executor/resource_manager.py`）与本项目 Buffer Pool（`engineering/src/db/storage/buffer/`）+ WAL（`engineering/src/db/storage/log/`）的"**资源池 + 配额**"思路相似。
- **REST Server ↔ PG Server**：ChatDev REST Server（`server/app.py`）与本项目 PG Server（`engineering/src/server/pg_server.h`）+ PG Wire Protocol（`engineering/src/server/pgwire/`）对照。
- **Frontend ↔ 自研 Studio**：ChatDev Frontend 与本项目自研 Studio 对照。
- **MCP ↔ DB 协议**：MCP（`mcp_example/`）与本项目 PG Wire Protocol 对照。
- **学习建议**：阅读 ChatDev 源码重点关注「YAML 配置驱动 + Graph 执行引擎 + Hook 系统 + Schema Registry」，与本项目 `engineering/src/db/` 多个模块做架构对照，加深对「配置驱动 + 编排 + 拦截」的理解。