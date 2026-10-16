# LangGraph Wiki（LangChain 出品的图编排 Agent 框架）

> 参考版本：`libs/langgraph`（Python 包 `langgraph`，与 `langgraph-checkpoint`、`langgraph-sdk` 三个 PyPI 包共仓），参考 HEAD：b292a33a6 前的最终 release（langgraph 0.2+/langgraph-checkpoint 2.0+）。本 wiki 重点介绍 LangGraph 作为「图编排 + 状态机 + 持久化 Checkpoint + Pregel 执行引擎」的工业级 Agent 框架定位、源码架构、四个 Agent 维度（角色协作 / 记忆 / 工具调用 / 人机协作）的深度展开，以及与本项目 `engineering/` 自研存储与向量索引的对照参考。

## 一、项目定位

LangGraph 是由 **LangChain Inc.**（Harrison Chase 团队）于 **2024 年 6 月**开源的 **「图编排 + 状态化 + 持久化 + 流式」** Agent 框架，定位是 **「Production-grade orchestration for long-running, stateful agents」**——填补 LangChain Expression Language（LCEL）缺少「显式图 + 持久化 + 人在回路」的工程空白。

- **核心痛点**：
  - LangChain 链式 LCEL 难以表达**循环**（agent 需要循环重试、ReAct 循环、自我修正）；
  - 缺少**细粒度状态管理**（state / channel / checkpoint），长任务状态丢失；
  - 缺少**持久化与可恢复**（run-by-run persistence），断电/重启即崩；
  - 缺少**人在回路**（human-in-the-loop interrupt）原生支持；
  - 缺少**多角色并行执行**（map-reduce / send API / branch）；
  - 缺少**流式观测**（stream / astream events）；
- **典型用户**：
  - **Agent 应用开发者**：构建可生产部署的 Agent；
  - **多 Agent 系统**：编排 Supervisor / Worker；
  - **长任务工作流**：可中断、可恢复的研究型 Agent；
  - **企业应用**：审计、合规、人在回路审批；
- **核心卖点**：
  - **StateGraph** 抽象（节点 / 边 / 状态通道 channel）；
  - **Pregel** 执行引擎（BSP 模型的图执行）；
  - **Checkpoint** 持久化（线程级 + 跨线程记忆）；
  - **Interrupt / Resume**（人在回路）；
  - **Send API**（map-reduce / dynamic fan-out）；
  - **Streaming**（token / values / updates / events / debug / custom）；

## 二、发展历史与社区

- **起源（2024-06）**：LangChain 团队开源 LangGraph v0.1，定位「Stateful graph orchestration」；
- **里程碑**：
  - 2024-06：v0.1（StateGraph + 基础节点）；
  - 2024-08：v0.2（Checkpoint 抽象 / Memory Send / SQLite / Postgres Checkpointer）；
  - 2024-10：v0.3（Pregel 重构 / Streaming API 完善）；
  - 2024-12：v0.4（LangGraph Studio / Studio Desktop / Studio Web）；
  - 2025-02：v0.5+（HITL Interrupt 完善 / Command API）；
  - 2025-06：v0.6+（Send API 完善 / RemoteGraph / LangGraph SDK）；
- **维护状态**：**活跃**（LangChain Inc. 团队 30+ 工程师，月均 100+ commits）；
- **社区**：
  - GitHub 13k+ star、`langgraph` PyPI 月下载量 500 万+；
  - Discord 5k+ 成员、LangChain Forum；
  - LangGraph Studio（桌面 / Web 可视化 IDE）；
- **关键贡献者**：Harrison Chase（创始人）、LangChain 团队、Nuno Campos (Developer), Eugene Yurtsev, Erick Friis, William King;

## 四、业界主要使用方与案例

- **国际**（公开）：
  - **Uber**：客服 Agent 多步骤决策；
  - **LinkedIn**：研究型 Agent 长任务编排；
  - **Klarna**：客服 Agent 人在回路审批；
  - **Replit**：Code Agent 工作流编排；
  - **Elastic**：Elastic AI Agent 多步骤研究；
  - **多家金融机构**：合规审批工作流；
- **国内**：
  - 多家互联网公司客服 Agent；
  - 多家企业内部知识库研究型 Agent；
- **典型场景**：
  - **多步骤研究 Agent**：ReAct 循环 + 工具调用；
  - **人在回路审批**：自动批 → 人工 review → 继续；
  - **多 Agent Supervisor**：Supervisor 调度 Worker；
  - **长任务 Workflow**：可中断、可恢复；
  - **RAG + Agent**：向量检索 → LLM 推理 → 工具调用 → 生成；
  - **生产级系统**：Checkpoint 持久化 + 审计；

## 五、一句话对比定位

> LangGraph 是 **「LangChain 出品 + 图编排 + StateGraph + Pregel 执行引擎 + Checkpoint 持久化 + HITL + Streaming + Python + MIT + 13k star + 生产级」**；与 **CrewAI（Crew 协作）**、**AutoGen（GroupChat 对话）**、**MetaGPT（软件公司角色）**、**OpenAI Agents（Handoff 简单协议）**、**Swarm（教学 Handoff）** 构成多 Agent 生态。LangGraph 的差异化是 **「图编排 + 持久化 + 人在回路 + 流式 + 工业级 + LangChain 生态完整」**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                  用户应用层（User Apps）                              │
│        LangGraph Studio / CLI / Python API / SDK / RemoteGraph      │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Public API（langgraph.graph / langgraph.pregel）   │
│      StateGraph / MessageGraph / add_node / add_edge / add_branch      │
│      compile() → Pregel 实例                                        │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Pregel 执行引擎（langgraph.pregel）                │
│   _algo.py（同步 / 异步 / 流式算法）                                  │
│   _loop.py（Pregel Loop 主循环）                                     │
│   _executor.py（节点执行器 / ConcurrentExecutor）                    │
│   _runner.py（Pregel Runner）                                       │
│   _call.py / _read.py / _write.py / _messages.py（消息 / 状态读写） │
│   _checkpoint.py（Checkpoint 协调）                                  │
│   _remote_run_stream.py（RemoteGraph 远程调用）                     │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  State / Channel 抽象（langgraph.graph / channels）  │
│   state.py（StateGraph 节点 / 边 / 分支）                           │
│   channels/（LastValue / Topic / BinaryOperator / NamedBarrier）     │
│   message.py / ui.py（消息 / UI 协议）                             │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Checkpoint 抽象（langgraph.checkpoint）             │
│   base/（Checkpoint / Metadata / Serializer 抽象）                  │
│   memory/（InMemorySaver）                                          │
│   serde/（Serializer for State）                                    │
│   外部实现：Postgres / SQLite / Redis Checkpointer                  │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌──────────┬─────────────────┼─────────────────┬───────────────────────┐
│          │                 │                 │                       │
┌──────────▼──────┐ ┌─────────▼────────┐ ┌────▼──────────────┐ ┌─────▼──────┐
│  Postgres      │ │  SQLite          │ │  Redis            │ │  InMemory  │
│  Checkpointer  │ │  Checkpointer    │ │  Checkpointer     │ │  Saver     │
│  PostgresSaver │ │  SqliteSaver     │ │                   │ │            │
└──────────┬──────┘ └─────────┬────────┘ └────┬──────────────┘ └─────┬──────┘
           │                  │                │                   │
┌──────────▼──────────────────▼────────────────▼───────────────────▼───────┐
│                  Store / Cross-thread Memory（langgraph.store）         │
│   InMemoryStore / PostgresStore / 向量记忆 / 命名空间 / 检索          │
└─────────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Func / Managed / Streaming                           │
│   func/（@entrypoint / @task 函数式 API）                            │
│   managed/（IsLastStep / RemainingSteps 管理）                       │
│   stream/（Streaming API / events / values / updates）                │
└─────────────────────────────────────────────────────────────────────┘
```

进程模型：LangGraph 是**纯 Python 用户态库**（无独立进程），Pregel 引擎在用户进程内同步 / 异步执行；Checkpoint / Store 是外部持久化（Postgres / SQLite / Redis）。

## 七、Agent 类型专项（核心扩展章节）

### 7.1 角色协作模式（Role Collaboration）

LangGraph 通过 **Pregel BSP 模型**（Bulk Synchronous Parallel）+ **State 主题** + **Send API** 实现四种协作模式：

- **中心化（Orchestrator-Worker）**：
  - **典型实现**：Supervisor 节点调度多个 Worker；
  - **实现位置**：`Reference/ai/agent/langgraph/libs/langgraph/langgraph/graph/state.py`（`add_node` / `add_edge` / `add_branch` 决定哪个 Worker 处理）；
  - **状态共享**：通过 channel 共享（如 `messages` channel / `worker_results` channel）；
  - **示例**：`docs/docs/concepts/multi_agent.md`；
- **对等（Peer-to-Peer / Blackboard）**：
  - **典型实现**：所有节点共享同一份 State（blackboard pattern）；
  - **实现位置**：`libs/langgraph/langgraph/graph/state.py`（`StateGraph` 本质即 blackboard，所有节点读写同一状态）；
  - **通道机制**：`channels/`（LastValue / Topic / BinaryOperatorAggregate 等）；
  - **示例**：Multi-Agent Debate；
- **分层（Hierarchical / Supervisor）**：
  - **典型实现**：Root Supervisor → Sub-Supervisor → Worker；
  - **实现位置**：`libs/langgraph/langgraph/pregel/_algo.py`（支持嵌套 Pregel / SubGraph）；
  - **状态隔离**：每个 SubGraph 独立状态空间；
  - **示例**：`examples/multi_agent/hierarchical_agent_teams.ipynb`；
- **市场（Market-based Bidding）**：
  - **典型实现**：Worker 通过 State 表达能力与报价，Supervisor 选择；
  - **实现位置**：在 Supervisor 节点内自定义 routing 逻辑（`add_branch` 返回节点名）；
  - **不直接支持**，需用户自定义 routing 函数；
- **Send API 动态扇出**：
  - **典型实现**：`Send("worker_node", {"input": item})` 在运行时动态创建 worker；
  - **实现位置**：`libs/langgraph/langgraph/graph/_branch.py` / `pregel/_algo.py`（`map-reduce` pattern）；
  - **典型场景**：Map 阶段生成多个 Worker，Reduce 阶段汇总结果；
- **Command API（Handoff 风格）**：
  - **典型实现**：`Command(goto="next_node", update={"key": value})`；
  - **实现位置**：`libs/langgraph/langgraph/types.py`（`Command` 类）；
  - **典型场景**：Agent 主动 handoff 到下一个角色；

### 7.2 记忆系统（Memory System）

LangGraph 记忆分三层：

- **短期（In-memory conversation buffer）**：
  - **实现位置**：`channels/LastValue`、`channels/Topic`；
  - **State 字段**：`messages: Annotated[list, add_messages]`；
  - **典型实现**：`from langgraph.graph import MessagesState`；
  - **触发**：在 StateGraph 中添加 `messages` reducer；
- **长期（向量库 + 文件系统）**：
  - **实现位置**：`libs/checkpoint/langgraph/store/`（`InMemoryStore` / `PostgresStore`）；
  - **Store API**：`store.put(namespace, key, value)` / `store.search(namespace, query)`；
  - **向量记忆**：`store.search(query="...", query_filter=...)` 支持语义检索；
  - **典型实现**：`docs/docs/concepts/storage.md`；
- **Checkpoint（线程级持久化）**：
  - **实现位置**：`libs/checkpoint/langgraph/checkpoint/`；
  - **线程（Thread）**：`thread_id` 标识一个会话；
  - **持久化**：`MemorySaver` / `SqliteSaver` / `PostgresSaver`；
  - **典型实现**：`checkpointer=MemorySaver()` / `PostgresSaver.from_conn_string(...)`；
  - **场景**：可中断、可恢复、断点续传；
- **跨线程记忆（Cross-thread Memory）**：
  - **实现位置**：`libs/checkpoint/langgraph/store/`；
  - **Store**：`store = InMemoryStore()` 跨 thread 共享；
  - **命名空间**：`("users", user_id, "memories")` 模式组织；
  - **场景**：用户级偏好、历史总结、跨会话知识；
- **记忆压缩与检索**：
  - **Summarization**：在 StateGraph 中加 `summarize` 节点压缩历史消息；
  - **RAG over Memory**：用 Store + 向量检索实现 long-term memory RAG；
  - **典型实现**：`examples/advanced/long_term_memory.ipynb`；
- **Schema（结构化记忆）**：
  - **实现位置**：`libs/checkpoint/langgraph/store/base/`;
  - **Pydantic 模型**：定义结构化记忆 schema；
  - **典型实现**：`from pydantic import BaseModel`；

### 7.3 工具调用协议（Tool Calling Protocol）

LangGraph 工具调用通过 `ToolNode` + `messages` channel 统一抽象：

- **Function Calling（OpenAI / Anthropic / Gemini）**：
  - **实现位置**：`libs/langgraph/langgraph/prebuilt/`（`ToolNode`、`tools_condition`）；
  - **支持模型**：OpenAI GPT-4 / Anthropic Claude / Google Gemini / Cohere / Mistral / Ollama / vLLM；
  - **绑定工具**：`llm.bind_tools(tools)` 或 `ChatOpenAI(model="...", tools=...)`；
  - **执行工具**：`ToolNode(tools).invoke({"messages": [...]})`；
- **ReAct（Reasoning + Acting）**：
  - **典型实现**：`create_react_agent(llm, tools)`（LangGraph 内置 helper）；
  - **实现位置**：`libs/langgraph/langgraph/prebuilt/chat_agent_executor.py`；
  - **循环**：Reason → Act → Observe → Reason，直到 done；
- **CodeAct（生成代码执行）**：
  - **典型实现**：自定义 StateGraph 节点生成代码，调用 `PythonREPL` 工具；
  - **沙箱**：`langchain_experimental.utilities.PythonREPL` 或外部 E2B / Docker；
  - **典型场景**：数据分析、可视化、文件处理；
- **Tool Use + Reflection（反思）**：
  - **典型实现**：在 StateGraph 中加 `reflect` 节点评估工具结果；
  - **实现位置**：自定义节点读取 `messages`，决定 retry / continue / fallback；
  - **典型场景**：自我修正、错误恢复；
- **Structured Outputs（结构化输出）**：
  - **实现位置**：`libs/langgraph/langgraph/graph/_state.py`（State schema）；
  - **Pydantic 模型**：`class State(TypedDict): result: MyModel`；
  - **典型实现**：`with_structured_output(MyModel)` 在 LLM 调用时；
- **Tool 错误处理**：
  - **实现位置**：`libs/langgraph/langgraph/prebuilt/tool_node.py`（`handle_tool_call_error`）；
  - **Fallback**：在 `ToolNode` 配置 `handle_tool_call_error` 回调；
  - **Retry**：用 `RetryPolicy` 配置重试策略；

### 7.4 人机协作（Human-in-the-Loop）

LangGraph HITL 通过 **Interrupt** + **Command** 实现完整人在回路：

- **Interrupt（中断）**：
  - **API**：`graph.interrupt(value)` 或 `interrupt_before` / `interrupt_after` 配置；
  - **实现位置**：`libs/langgraph/langgraph/types.py`（`Interrupt` 类）/ `pregel/_algo.py`（interrupt 处理）；
  - **触发时机**：`compile(interrupt_before=["node_a"], interrupt_after=["node_b"])`；
  - **运行时中断**：`node` 内调用 `interrupt({"question": "..."})` 暂停执行；
- **Resume（恢复）**：
  - **API**：`graph.invoke(Command(resume="answer"), config={"configurable": {"thread_id": "..."}})`；
  - **实现位置**：`libs/langgraph/langgraph/types.py`（`Command` 类）/ `pregel/_algo.py`（resume 处理）；
  - **状态恢复**：从 Checkpoint 恢复 thread state；
- **Approval Workflow（审批流）**：
  - **典型模式**：自动提决策 → interrupt 等待审批 → 审批后 resume；
  - **实现位置**：`libs/langgraph/langgraph/prebuilt/tool_node.py`（Tool 调用前 interrupt 审批）；
  - **典型场景**：删除文件、转账、发送邮件等高风险操作；
- **动态中断（Dynamic Interrupt）**：
  - **API**：在 `node` 内 `from langgraph.types import interrupt; interrupt(...)`；
  - **场景**：根据 LLM 输出决定是否需要人工介入（如不确定的判断）；
- **Edit State（修改状态）**：
  - **API**：`graph.invoke(Command(update={"messages": [HumanMessage("修正")]}))`；
  - **典型场景**：人工 review LLM 输出后修改；
- **Checkpoint Fork**：
  - **API**：`checkpointer.list(config)` 列出 thread checkpoints；
  - **Fork**：`graph.invoke(..., config={"configurable": {"thread_id": "new_thread"}}, checkpoint_id=...)`；
  - **场景**：A/B 测试不同路径；
- **Streaming & Observability（可观测性）**：
  - **Streaming**：`graph.stream()` / `graph.astream_events()`；
  - **模式**：values / updates / messages / events / custom；
  - **LangSmith 集成**：自动 trace 所有节点执行；
  - **LangGraph Studio**：可视化 IDE（断点、step-through、time-travel）；

## 八、索引机制（State / Channel）

LangGraph 没有传统数据库索引，但其 **State Channel** 是核心抽象：

- **LastValue**：
  - **实现位置**：`libs/langgraph/langgraph/channels/last_value.py`；
  - **语义**：保留最后一个值（每次 update 覆盖）；
  - **典型用途**：`current_step`、`agent_name`；
- **Topic**：
  - **实现位置**：`libs/langgraph/langgraph/channels/topic.py`；
  - **语义**：保留所有 value list（每次 update append）；
  - **典型用途**：`messages`（消息历史）；
- **BinaryOperatorAggregate**：
  - **实现位置**：`libs/langgraph/langgraph/channels/binop.py`；
  - **语义**：用二元操作符累积（如 add / multiply / union）；
  - **典型用途**：`accumulator`、`set_union`、`dict_merge`；
- **NamedBarrier**：
  - **实现位置**：`libs/langgraph/langgraph/channels/named_barrier.py`；
  - **语义**：等待多个 channel 都有值才触发（同步 barrier）；
  - **典型用途**：Map-Reduce 的 reduce 阶段等待所有 map 完成；
- **EphemeralValue**：
  - **实现位置**：`libs/langgraph/langgraph/channels/ephemeral_value.py`；
  - **语义**：单次 round 用完即清空；
  - **典型用途**：中间计算结果；
- **Channel 实现方式**：
  - 每个 channel 实现 `update()`/`get()`/`is_available()`/`consume()` 接口；
  - Pregel 算法通过 channel 触发节点（super-step 模型）；

## 九、查询/计算/接口层

### 9.1 图执行引擎（Graph Execution Engine）

- **Pregel（BSP 模型）**：
  - **实现位置**：`libs/langgraph/langgraph/pregel/_algo.py` / `_loop.py`；
  - **BSP 阶段**：super-step 内并行执行所有活跃节点 → barrier → 下一个 super-step；
  - **Channel 触发**：节点声明需要读取的 channel，channel 有值才触发；
- **Pregel Loop**：
  - **实现位置**：`libs/langgraph/langgraph/pregel/_loop.py`；
  - **生命周期**：prepare → tick → execute nodes → write channels → barrier → check done；
- **Concurrent Executor**：
  - **实现位置**：`libs/langgraph/langgraph/pregel/_executor.py`；
  - **同步执行**：`ConcurrentExecutor`（asyncio + ThreadPool）；
  - **异步执行**：`AsyncConcurrentExecutor`；
- **Streaming API**：
  - **实现位置**：`libs/langgraph/langgraph/pregel/_remote_run_stream.py` / `_io.py`；
  - **模式**：`mode="values"` / `"updates"` / `"messages"` / `"events"` / `"debug"` / `"custom"`；
  - **流式输出**：`async for chunk in graph.astream(state, config, mode="values")`；
- **Subgraph**：
  - **API**：`graph.add_node("sub", subgraph.compile())`；
  - **状态隔离**：每个 subgraph 独立 state schema；
  - **通信**：通过父图 state 通道；

### 9.2 状态管理（State / StateGraph / Checkpoint / Thread / Run）

- **StateGraph**：
  - **API**：`from langgraph.graph import StateGraph; builder = StateGraph(State)`；
  - **节点**：`builder.add_node("node_name", func)`；
  - **边**：`builder.add_edge("node_a", "node_b")`；
  - **条件边**：`builder.add_conditional_edges("node_a", routing_func, {"path1": "node_b"})`；
  - **入口**：`builder.add_edge(START, "node_a")`；
  - **出口**：`builder.add_edge("node_z", END)`；
- **Compile**：
  - **API**：`graph = builder.compile(checkpointer=..., interrupt_before=...)`；
  - **返回**：`Pregel` 实例；
- **Invoke**：
  - **同步**：`result = graph.invoke({"input": "..."})`；
  - **异步**：`result = await graph.ainvoke({"input": "..."})`；
  - **流式**：`async for chunk in graph.astream({"input": "..."}): print(chunk)`；
- **Config**：
  - **Thread ID**：`config = {"configurable": {"thread_id": "..."}}`；
  - **User ID**：`{"configurable": {"user_id": "..."}}`；
  - **Checkpoint ID**：`{"configurable": {"thread_id": "...", "checkpoint_id": "..."}}`（time-travel）；
- **Checkpoint**：
  - **抽象**：`libs/checkpoint/langgraph/checkpoint/base/`（`Checkpoint`、`CheckpointMetadata`）；
  - **序列化**：`serde/`（JSON / pickle / ormsgpack）；
  - **持久化实现**：`MemorySaver` / `SqliteSaver` / `PostgresSaver`；
- **Thread**：
  - **定义**：一次会话的完整执行轨迹；
  - **线程 ID**：唯一标识一个会话；
  - **Checkpoint 链**：thread 下多个 checkpoint（每个 super-step 一个）；
- **Run**：
  - **单次执行**：一次 `invoke`/`ainvoke`/`stream` 一次 run；
  - **Run ID**：标识一次 run（用于跟踪 / 重试）；

### 9.3 多模态工具（Multimodal Tools）

- **浏览器**：`playwright` / `selenium` 作为自定义 Tool；
- **代码执行**：E2B / Docker / Modal 作为 sandbox；
- **文件操作**：本地文件系统 / S3 / GCS；
- **API 调用**：通过 `requests` / `httpx` 自定义 Tool；
- **RAG 工具**：LangChain VectorStore + LangGraph ToolNode；
- **数据库**：`SQLDatabase`（LangChain 内建）+ ToolNode；
- **示例**：`examples/agent_executor/`（多模态工具示例）；

### 9.4 多 Agent 通信（Multi-Agent Communication）

- **共享 State**：所有 Agent 共享同一份 State（blackboard）；
- **Channel 消息总线**：通过 `messages` channel 传递消息；
- **A2A 协议**：LangGraph 不直接实现 A2A（Agent-to-Agent Protocol），但可通过 `RemoteGraph` 跨进程通信；
- **消息队列**：通过 Send API 异步分发；
- **HTTP API**：`RemoteGraph` 客户端跨网络调用 Remote Graph 服务；

## 十、查询/计算/接口层（API & SDK）

- **Python API**：
  - `langgraph.graph.StateGraph` / `MessageGraph`；
  - `langgraph.graph.add_node` / `add_edge` / `add_conditional_edges`；
  - `langgraph.pregel.Pregel`（compile 后）；
  - `langgraph.checkpoint.memory.MemorySaver`；
  - `langgraph.checkpoint.postgres.PostgresSaver`；
  - `langgraph.checkpoint.sqlite.SqliteSaver`；
  - `langgraph.store.memory.InMemoryStore`；
  - `langgraph.store.postgres.PostgresStore`；
  - `langgraph.types.Command` / `Interrupt` / `Send`；
  - `langgraph.func.entrypoint` / `task`；
- **Streaming API**：`astream` / `astream_events`；
- **LangGraph SDK（Python + JS）**：
  - **位置**：`libs/sdk/`；
  - **HTTP API**：与 LangGraph Platform 通信；
  - **RemoteGraph**：跨进程 / 跨网络调用 Graph；
- **LangGraph CLI**：
  - `langgraph dev`（启动本地 dev server）；
  - `langgraph up`（部署到生产）；
- **LangGraph Studio**：
  - 桌面 / Web 可视化 IDE；
  - 断点、step-through、time-travel；
  - **位置**：LangGraph Studio 仓库独立；
- **LangGraph Platform（商业）**：
  - **托管服务**：部署 LangGraph Graph；
  - **可观测性**：内置 LangSmith 集成；
  - **Scaling**：水平扩展、Auto-scaling；
- **LangSmith（可观测性）**：
  - 自动 trace 所有节点、Tool 调用、LLM 调用；
  - Dataset / Evaluator / Online Evaluation；

## 十一、生态与集成

- **客户端 SDK**：Python（首选）、JS（LangGraph SDK）；
- **LLM 集成**：通过 LangChain 集成所有 LLM（OpenAI / Anthropic / Google / Cohere / Mistral / Ollama / vLLM / Together / Fireworks）；
- **Vector Store**：通过 LangChain 集成所有 Vector DB（Chroma / Milvus / Weaviate / FAISS / Qdrant / LanceDB / pgvector / Pinecone）；
- **Tool 集成**：通过 LangChain Tools（Arxiv / Wikipedia / Tavily / SerpAPI / Python REPL / Playwright / Selenium / Requests）；
- **Checkpoint Backend**：Memory / SQLite / Postgres / Redis；
- **Store Backend**：Memory / Postgres；
- **Observability**：LangSmith（首选）/ OpenTelemetry / Langfuse；
- **部署**：LangGraph Platform（托管）/ LangGraph Studio（IDE）/ Docker / Kubernetes；
- **企业版**：LangGraph Platform + LangSmith Enterprise + LangChain Enterprise；

## 十二、源码学习路径

- **必读模块**（按学习顺序）：
  1. `libs/langgraph/langgraph/version.py`：版本；
  2. `libs/langgraph/langgraph/graph/state.py`：StateGraph 节点 / 边 / 编译；
  3. `libs/langgraph/langgraph/graph/_branch.py`：条件分支（Conditional）；
  5. `libs/langgraph/langgraph/graph/_node.py`：节点包装（同步 / 异步 / runnable）；
  6. `libs/langgraph/langgraph/graph/message.py`：消息处理（add_messages reducer）；
  7. `libs/langgraph/langgraph/pregel/main.py`：Pregel 实例化；
  8. `libs/langgraph/langgraph/pregel/_algo.py`：Pregel 同步 / 异步 / 流式算法；
  9. `libs/langgraph/langgraph/pregel/_loop.py`：Pregel Loop 主循环；
  10. `libs/langgraph/langgraph/pregel/_executor.py`：Concurrent Executor；
  11. `libs/langgraph/langgraph/pregel/_checkpoint.py`：Checkpoint 协调；
  12. `libs/langgraph/langgraph/pregel/_messages.py`：消息读写；
  13. `libs/langgraph/langgraph/pregel/_io.py`：Streaming IO；
  14. `libs/langgraph/langgraph/channels/last_value.py`：LastValue channel；
  15. `libs/langgraph/langgraph/channels/topic.py`：Topic channel；
  16. `libs/langgraph/langgraph/channels/binop.py`：BinaryOperator channel；
  17. `libs/checkpoint/langgraph/checkpoint/base/__init__.py`：Checkpoint 抽象；
  18. `libs/checkpoint/langgraph/checkpoint/memory/__init__.py`：MemorySaver；
  19. `libs/checkpoint/langgraph/store/base/`：Store 抽象；
  20. `libs/langgraph/langgraph/types.py`：Command / Interrupt / Send 类型；
  21. `libs/langgraph/langgraph/prebuilt/chat_agent_executor.py`：create_react_agent 工厂；
  22. `libs/langgraph/langgraph/prebuilt/tool_node.py`：ToolNode 工具执行；
  23. `libs/langgraph/langgraph/func/__init__.py`：@entrypoint / @task 函数式 API；
  24. `libs/langgraph/langgraph/managed/`：managed values；
- **LOC 与语言**：
  - `libs/langgraph/langgraph/` ~50k LoC；
  - `libs/checkpoint/langgraph/` ~10k LoC；
  - `libs/sdk/` ~5k LoC；
  - 主语言：**Python**（100%）；
  - 部分 TypeScript（SDK）；
- **构建系统**：
  - **Hatchling**（`pyproject.toml`，PEP 517 build backend）；
  - **uv**（依赖管理）；
  - **monorepo**（`libs/langgraph/`、`libs/checkpoint/`、`libs/sdk/` 三包）；
  - **CI/CD**：GitHub Actions；
  - **release-please**（发布管理）；

## 十三、学习路径

- **入门（1 周）**：
  - 阅读官方 Tutorial（https://langchain-ai.github.io/langgraph/）；
  - 跑通 `examples/chat_agent_executor/`（基础 ReAct Agent）；
  - 理解 StateGraph / Node / Edge / Conditional；
  - 跑通 `examples/human_in_the_loop/`（HITL 示例）；
- **进阶（2-3 周）**：
  - 深入学习 Pregel 算法（`_algo.py` / `_loop.py`）；
  - 实战 Checkpoint 持久化（Memory / SQLite / Postgres）；
  - 学习 Send API（Map-Reduce）；
  - 学习 Store 跨线程记忆；
  - 学习 Streaming API（values / updates / events）；
- **高级（1-2 月）**：
  - 阅读源码（Pregel + Checkpoint + Store + Channel）；
  - 自定义 Channel（实现 `update`/`get`/`is_available`/`consume`）；
  - 自定义 Checkpointer（Redis / DynamoDB）；
  - 自定义 Store Backend（向量库集成）；
  - LangGraph Platform 部署；
  - LangSmith 可观测性集成；
- **推荐资源**：
  - 官方文档：https://langchain-ai.github.io/langgraph/
  - GitHub：https://github.com/langchain-ai/langgraph
  - LangGraph Studio：https://studio.langchain.com/
  - 论文：Pregel: A System for Large-Scale Graph Processing（BSP 模型）；
  - 博客：https://blog.langchain.dev/

## 十四、风险与陷阱

- **学习曲线陡**：StateGraph / Pregel / Channel / Checkpoint 概念多，新人需 2-3 周入门；
- **Channel 行为复杂**：LastValue vs Topic vs BinaryOperator 语义差异大；
- **Checkpoint 性能开销**：每个 super-step 都序列化 / 反序列化状态，开销大；
- **Store 与 Checkpoint 边界**：短时（thread-level）vs 长期（cross-thread）易混淆；
- **Interrupt 与 Command 配合**：resume 时必须传 Command(resume=...) 而非 input；
- **Streaming 模式选择**：values / updates / messages / events 差异大，选错易丢数据；
- **Subgraph 状态隔离**：父子图通信靠显式 channel 映射；
- **Send API 动态扇出**：必须在 conditional edge 中返回 Send 列表；
- **LangGraph Platform 锁定**：使用 Platform 后切换到自部署成本高；
- **LangSmith 依赖**：生产部署建议接 LangSmith；
- **Python 性能瓶颈**：Python 调度开销大，超长任务（>1000 step）性能下降；
- **Checkpoint Backend 不一致**：Memory / SQLite / Postgres 行为差异大（事务支持、并发）；
- **Tool 错误处理**：ToolNode 默认吞掉异常，需自定义 handle_tool_call_error；
- **State 不可序列化**：State 中包含 LLM / DB 连接等不可序列化对象会报错；
- **持久化 Schema 迁移**：State 字段增删需考虑旧 Checkpoint 反序列化兼容；
- **Hitl 与 Async 配合**：async interrupt 在某些情况下需特殊处理；

## 十六、参考资料

- 官方仓库：https://github.com/langchain-ai/langgraph
- 官方文档：https://langchain-ai.github.io/langgraph/
- LangGraph Studio：https://studio.langchain.com/
- LangGraph Platform：https://langchain-ai.github.io/langgraph/concepts/langgraph_platform/
- LangSmith：https://docs.smith.langchain.com/
- 论文：Pregel: A System for Large-Scale Graph Processing（Google BSP）
- 论文：BSP vs MapReduce（Bulk Synchronous Parallel）
- 博客：https://blog.langchain.dev/tag/langgraph/
- Tutorial：https://langchain-ai.github.io/langgraph/tutorials/introduction/
- Discord：https://discord.gg/langchain
- LangChain Academy：https://academy.langchain.com/

## 十七、与本项目关联

LangGraph 作为「图编排 + StateGraph + Pregel + HITL + Streaming」行业代表 Agent 框架，与本项目（engineering/ 轨道 C/C++ 自研存储与向量索引）在「状态机持久化 + BSP 图执行 + Channel 触发模型 + Checkpoint 审计」四个维度具有直接参考价值：

- **StateGraph ↔ B+Tree Index Manager**：LangGraph 的 StateGraph + Channel 抽象与本项目 B+Tree 索引管理（`engineering/src/db/index/bplus_tree/`）的状态机模式有相似性——都是「声明式 schema + 命令式驱动 + 状态变更追踪」。本项目 wiki `docs/db-wiki/extension/postgres.md` 已详细分析 PG 索引 AM 抽象。
- **Pregel BSP 模型 ↔ 多模态存储抽象**：相比 Lazy Approx 的 Pregel 模型，本项目实现的是「多模态存储统一抽象」（`engineering/include/db/mm_storage.h`、`engineering/src/db/storage/mm/mm_storage.c`），把 KV / Vector / Timeseries / Document / Spatial / Graph / Yang 七种数据模型统一管理；LangGraph 是控制流式通道级抽象，本项目是数据流级抽象，可对照学习。
- **Checkpoint 持久化 ↔ PG 风格 WAL**：LangGraph 的 Checkpoint（`libs/checkpoint/langgraph/checkpoint/base/`）与本项目 PG 风格的 WAL（`engineering/src/db/storage/`）都是"**核**"持久化设计——本项目 WAL 在 `engineering/src/db/storage/log/` 实现，可对照 LangGraph Checkpoint 抽象学习"**原子提交 + 事务协调 + 反序列化**"。
- **HITL Interrupt ↔ 审批流**：LangGraph 的 HITL Interrupt（`libs/langgraph/langgraph/types.py` Interrupt 类）与本项目 PG 风格的 GUC 配置系统（`engineering/src/db/core/guc.c`）+ WAL 协调组合，可在「运行时配置变更 + 持久化协调」方向对照学习。
- **Channel Trigger ↔ Hot/ Cold 启发**：LangGraph 的 channel 触发模型（仅 channel 有值才触发节点）可启发本项目的冷热分层（`engineering/src/db/storage/cold.c` 未直接存在但可参考 `engineering/src/db/storage/aging/` 老化层）——都是"**按需启变**"思路。
- **Store 跨线程记忆 ↔ 向量索引**：LangGraph Store（`libs/checkpoint/langgraph/store/`）支持向量检索，可与本项目的 HNSW 索引（`docs/db-wiki/vector/` 已系统梳理）对照学习。
- **Streaming API ↔ 流式读取**：LangGraph 的 Streaming（`libs/langgraph/langgraph/pregel/_io.py`）与本项目 `engineering/src/db/storage/blob/` Blob 流式读取对照——都是"**背压 + 流式 + 观测**"思路。
- **学习建议**：阅读 LangGraph 源码重点关注「Pregel BSP 模型 + Channel 触发 + Store 抽象」，与本项目 `engineering/src/db/storage/mm/` + `engineering/src/db/distributed/` 做架构对照，加深对「多通道协同 + 持久化协调」的理解。