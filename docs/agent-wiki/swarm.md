# Swarm Wiki（OpenAI 教学版多 Agent 框架）

> 参考版本：`swarm` Python 包（`swarm/` 主包），参考 HEAD：b292a33a6 前的最终 release（swarm 0.1+，OpenAI 实验性框架，已停止主动维护）。本 wiki 重点介绍 Swarm 作为「**Handoff 协议 + Agent-as-Tool + 极简抽象 + 教学导向 + 上下文变量传递**」的多 Agent 教学框架定位、源码架构、四个 Agent 维度（角色协作 / 记忆 / 工具调用 / 人机协作）的深度展开，以及与本项目 `engineering/` 自研存储与向量索引的对照参考。

## 一、项目定位

Swarm 是由 **OpenAI**（Solutions 团队）于 **2024 年 10 月**开源的 **「教育 / 实验性多 Agent 框架」**，定位是 **「An experimental framework for building multi-agent systems」**——以 **Handoff（Agent 间主动交接）+ Agent-as-Tools（Agent 作为工具）+ 上下文变量** 三个核心抽象，把多 Agent 协作简化为最少的代码。作为 OpenAI Agents SDK 的前身，Swarm 主要面向学习者。

- **核心痛点**：
  - 缺少**极简教学框架**（LangGraph / AutoGen 都偏复杂）；
  - 缺少**Handoff 协议示范**（OpenAI 官方实现）；
  - 缺少**Agent as Tool**（Agent 作为其他 Agent 的工具）；
  - 缺少**上下文变量**（Agent 间共享上下文变量）；
  - 缺少**OpenAI 官方教学材料**；
- **典型用户**：
  - **学习者**：学习多 Agent 协作；
  - **教师 / 学生**：多 Agent 教学；
  - **原型验证**：极简多 Agent 原型；
  - **OpenAI Agents SDK 用户**：了解前身；
- **核心卖点**：
  - **Handoff 协议**：OpenAI 官方 Handoff 实现（Agent 主动交接）；
  - **Agent-as-Tool**：其他 Agent 作为 Tool 调用；
  - **上下文变量（context_variables）**：Agent 间共享上下文；
  - **极简 API**：核心代码 <1000 行；
  - **教学导向**：注释丰富 / 示例完整；
  - **OpenAI 官方**：OpenAI 出品；
  - **MIT 许可**：可商用、修改、私有学习；
  - **REPL 模式**：交互式 REPL 调试；
  - **Types 抽象**：Pydantic 类型化；

## 二、发展历史与社区

- **起源（2024-10）**：OpenAI Solutions 团队发布 Swarm v0.1（GitHub 公告 + Cookbook）；
- **里程碑**：
  - 2024-10：v0.1（Handoff + Agent-as-Tool + context_variables）；
  - 2025-03：**宣布停止主动维护**（已被 OpenAI Agents SDK 取代）；
  - 2025-03：OpenAI 官方推荐迁移到 OpenAI Agents SDK；
  - 2025-至今：维护模式（bug fix only，无新功能）；
- **维护状态**：**被动维护**（OpenAI 团队已迁移到 OpenAI Agents SDK）；
- **社区**：
  - GitHub 20k+ star（教学热度高）、`swarm` PyPI 月下载量 100 万+（教学持续使用）；
  - Discord / GitHub Discussions；
  - 教学示例：20+ 官方示例；
  - 前身：OpenAI Agents SDK（2025-03 发布）；
- **关键贡献者**：OpenAI Solutions 团队、贡献者 200+；

## 三、许可与商业限制

- **核心代码**：**MIT License**（`swarm/`、`examples/`、`tests/`），可商用、修改、私有部署；
- **生态关系**：
  - **上游依赖**：
    - Python 3.10+；
    - OpenAI Python SDK；
    - Pydantic v2（types.py）；
    - tiktoken；
    - termcolor（REPL 输出）；
  - **下游生态**：约 10000+ 学习者 / 教师 / 学生使用 Swarm 教学；

## 四、业界主要使用方与案例

- **国际**（公开）：
  - **多家大学**：多 Agent 教学；
  - **多家培训机构**：OpenAI 官方教学；
  - **多家初创公司**：原型验证；
- **国内**：
  - 多家培训机构：多 Agent 教学；
  - 多家开发者：学习者。
- **典型场景**：
  - **多 Agent 教学**：Handoff + Agent-as-Tool 概念演示；
  - **极简原型**：Handoff 协议演示；
  - **OpenAI Agents SDK 迁移教学**：前身学习；
  - **教育演示**：多 Agent 教学演示；
  - **学习材料**：Handoff + Agent-as-Tool 协议基础；

## 五、一句话对比定位

> Swarm 是 **「OpenAI 出品 + Handoff 协议 + Agent-as-Tool + 极简 + 教学 + 上下文变量 + 20k star + MIT」**；与 **LangGraph（图状态机）**、**CrewAI（角色化 + Process）**、**AutoGen（GroupChat 对话）**、**MetaGPT（软件公司角色）**、**OpenAI Agents（Handoff 工业版）**、**AgentScope（多 Agent 通用）** 构成多 Agent 生态。Swarm 的差异化是 **「OpenAI 教学版 + 极简 + Handoff + Agent-as-Tool + 已被 OpenAI Agents SDK 取代 + 教学理想」**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                  用户应用层（User Apps）                              │
│        Swarm REPL / Python API / Examples                           │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Public API（swarm/）                                │
│      Swarm（主入口）/ Agent / Result / ContextVariables             │
│      Handoff / Agent-as-Tool / REPL                                │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Core（swarm/core.py）                               │
│      Swarm 类（run_loop + active）                                   │
│      Agent 类（name + instructions + functions + tool_call）       │
│      Handoff 函数（Agent 间主动交接）                                │
│      context_variables（上下文变量）                                  │
│      tool_call（Tool 调用抽象）                                      │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Types（swarm/types.py）                              │
│      Agent 抽象                                                       │
│      Function 抽象                                                      │
│      Result 抽象                                                       │
│      Pydantic 模型定义                                                  │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Util（swarm/util.py）                                │
│      工具函数（消息处理 / 调试 / 序列化）                              │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  REPL（swarm/repl/）                                  │
│      交互式 REPL / 调试 / 单步执行                                    │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  外部依赖（External Dependencies）                   │
│      OpenAI Python SDK（chat completion + function calling）       │
│      Pydantic / tiktoken / termcolor                                  │
└─────────────────────────────────────────────────────────────────────┘
```

进程模型：Swarm 是**纯 Python 用户态库**（单进程）；通过 OpenAI Python SDK 调用 LLM；无独立进程。

## 七、Agent 类型专项（核心扩展章节）

### 7.1 角色协作模式（Role Collaboration）

Swarm 通过 **Agent + Handoff + Agent-as-Tool** 实现四种协作模式：

- **中心化（Orchestrator-Worker）**：
  - **典型实现**：Triage Agent（Orchestrator）通过 Agent-as-Tool 调用 Worker Agent；
  - **实现位置**：`swarm/core.py`（Agent-as-Tool）；
  - **API**：`worker_tool = worker_agent.as_tool(); triage = Agent(tools=[worker_tool])`；
- **对等（Peer-to-Peer / Handoff）**：
  - **典型实现**：Agent 通过 Handoff 主动交接；
  - **实现位置**：`swarm/core.py`（Handoff 函数）；
  - **API**：`agent = Agent(functions=[handoff_to(other_agent)])`；
  - **机制**：LLM 决策调用 handoff 函数；
- **分层（Hierarchical / Supervisor）**：
  - **典型实现**：通过嵌套 Handoff 实现 Supervisor → Worker；
  - **API**：`supervisor.functions = [handoff_to(worker1), handoff_to(worker2)]`；
- **市场（Market-based Bidding）**：
  - **典型实现**：通过 Tool + 多 Agent 复杂逻辑；
  - **不直接支持**，需用户自定义；
- **Context Variables 共享**：
  - **典型实现**：所有 Agent 共享 context_variables；
  - **实现位置**：`swarm/core.py`（context_variables）；
  - **API**：`swarm.run(..., context_variables={"user_id": "..."})`；
  - **机制**：每个 Tool 可访问 / 修改 context_variables；
- **Handoff Tool Filter**：
  - **典型实现**：Handoff 时过滤 / 转换输入；
  - **API**：在 Handoff 函数中实现过滤；
- **Agent-as-Tool 调用图**：
  - **典型实现**：Agent 作为其他 Agent 的工具；
  - **机制**：LLM 决策调用 Agent；

### 7.2 记忆系统（Memory System）

Swarm 记忆系统**极简**，主要靠 **context_variables + messages**：

- **短期（In-memory conversation buffer）**：
  - **实现位置**：`swarm/core.py`（messages）；
  - **存储**：当前 Run 的消息历史；
  - **API**：自动维护在 Swarm.run() 调用中；
- **长期（Context Variables）**：
  - **实现位置**：`swarm/core.py`（context_variables）；
  - **存储**：跨 Step 共享的 dict；
  - **API**：`context_variables = {"user_id": "..."}`；
  - **机制**：Tool 可读写 context_variables 实现记忆；
- **Vector Memory**：
  - **不直接支持**，需用户自定义 Tool + Vector Store；
- **File Memory**：
  - **不直接支持**，需用户自定义 Tool + 文件系统；
- **Memory 压缩**：
  - **不直接支持**，需用户自定义；
- **跨 Run 记忆**：
  - **不直接支持**，需用户持久化 context_variables；
- **REPL 调试**：
  - **典型实现**：REPL 中可查看 / 修改 context_variables；
  - **实现位置**：`swarm/repl/`；

### 7.3 工具调用协议（Tool Calling Protocol）

Swarm 通过 **Function Calling（OpenAI 专属）+ Agent-as-Tool** 实现工具调用：

- **Function Calling（OpenAI）**：
  - **实现位置**：`swarm/core.py`（tool_call）；
  - **支持模型**：OpenAI GPT（专属，其他 LLM 通过 OpenAI 兼容 API）；
  - **API**：`agent = Agent(functions=[my_function])`；
- **Agent-as-Tool**：
  - **典型实现**：其他 Agent 作为 Tool；
  - **实现位置**：`swarm/core.py`；
  - **API**：`tool = other_agent.as_tool(); agent = Agent(functions=[tool])`；
  - **机制**：Agent 返回的 string 当作 Tool 输出；
- **Handoff Tool**：
  - **典型实现**：Agent 间 Handoff；
  - **实现位置**：`swarm/core.py`；
  - **API**：`function = transfer_to(other_agent)`；
- **ReAct（Reasoning + Acting）**：
  - **典型实现**：Agent 默认使用 ReAct 循环；
  - **机制**：每轮 LLM 决策调用 Function；
- **CodeAct（生成代码执行）**：
  - **典型实现**：自定义 Function + Python REPL；
  - **沙箱**：本地 / Docker；
- **Tool Use + Reflection**：
  - **典型实现**：在 Function 中实现 Reflection；
- **Structured Outputs**：
  - **实现位置**：`swarm/types.py`（Pydantic 模型）；
  - **API**：Function 返回 Pydantic 模型；
- **Tool Error Handling**：
  - **实现位置**：`swarm/core.py`（tool_call 处理）；
  - **API**：Function 内部 try/except；

### 7.4 人机协作（Human-in-the-Loop）

Swarm HITL 通过 **REPL / Function / context_variables** 实现（极简）：

- **REPL HITL**：
  - **典型实现**：REPL 中可随时介入；
  - **实现位置**：`swarm/repl/`；
  - **API**：REPL 单步调试；
- **Custom HITL Function**：
  - **典型实现**：自定义 Function 调用 `input()` 等待人工；
  - **API**：`def get_human_input(): return input("请输入: "); agent.functions = [get_human_input]`；
- **Context Variables HITL**：
  - **典型实现**：通过 context_variables 持久化人工决策；
  - **API**：`context_variables["approval"] = input("Approve? [y/n]: ")`；
- **Approval Workflow**：
  - **典型实现**：Handoff 到 approval_agent；
  - **API**：自定义 approval_agent 等待人工；
- **Audit Trail**：
  - **不直接支持**，需用户自定义；
  - **API**：通过 OpenAI API Log 追踪；

## 八、索引机制（Index / Types）

Swarm 索引通过 **Types + Agent** 实现：

- **Types 抽象**：
  - **实现位置**：`swarm/types.py`（Pydantic 模型）；
  - **机制**：类型化 Agent / Function / Result；
  - **API**：`Agent(name="...", instructions="...", functions=[...])`；
- **Function Index**：
  - **实现位置**：`swarm/core.py`；
  - **API**：`agent.functions = [func1, func2, ...]`；
- **Context Variables Index**：
  - **实现位置**：`swarm/core.py`；
  - **API**：`context_variables = {"user_id": "..."}`；

## 九、查询/计算/接口层

### 9.1 Swarm 执行引擎（Swarm Execution Engine）

- **Swarm.run_loop()**：
  - **实现位置**：`swarm/core.py`（run_loop）；
  - **API**：`swarm.run(...)`；
  - **机制**：ReAct 循环 + Function Calling + Handoff；
- **Active Agent**：
  - **实现位置**：`swarm/core.py`（active）；
  - **API**：`swarm.active_agent`（当前活跃 Agent）；
- **Streaming**：
  - **典型实现**：通过 OpenAI Streaming API；
  - **API**：`swarm.run(..., stream=True)`；
- **REPL**：
  - **实现位置**：`swarm/repl/`；
  - **API**：`swarm.repl.run_demo()`；
- **Async**：
  - **API**：`swarm.run_async(...)`（异步版本）；

### 9.2 状态管理（State / Context）

- **Messages State**：
  - **实现位置**：`swarm/core.py`（messages）；
- **Context Variables State**：
  - **实现位置**：`swarm/core.py`（context_variables）；
- **Active Agent State**：
  - **实现位置**：`swarm/core.py`（active_agent）；
- **Persistence**：
  - **不直接支持**，需用户手动持久化；
- **REPL State**：
  - **实现位置**：`swarm/repl/`（REPL 状态）；

### 9.3 多模态工具（Multimodal Tools）

- **Code Execution**：
  - **典型实现**：自定义 Function + Python REPL；
- **Web Search**：
  - **典型实现**：自定义 Function + Tavily / Serper；
- **File Operations**：
  - **典型实现**：自定义 Function + 文件 API；
- **RAG Tool**：
  - **典型实现**：自定义 Function + Vector Store；
- **Database Tool**：
  - **典型实现**：自定义 Function + SQLAlchemy；
- **Multi-modal**：
  - **典型实现**：OpenAI GPT-4V；
- **Agent-as-Tool**：
  - **典型实现**：其他 Agent 作为 Tool 调用；
- **MCP Tool**：
  - **不直接支持**，需用户集成；

### 9.4 多 Agent 通信（Multi-Agent Communication）

- **Handoff**：
  - **实现位置**：`swarm/core.py`（Handoff 函数）；
- **Context Variables**：
  - **实现位置**：`swarm/core.py`（context_variables）；
- **Agent-as-Tool**：
  - **机制**：通过 Function Calling；
- **A2A Protocol**：
  - **不直接支持**，需用户自定义 HTTP / gRPC；

## 十、查询/计算/接口层（API & SDK）

- **Python API（核心）**：
  - `swarm.Swarm`（`swarm/core.py`）；
  - `swarm.Agent`（`swarm/core.py`）；
  - `swarm.Result`（`swarm/types.py`）；
  - `swarm.util`（`swarm/util.py`）；
  - `swarm.repl`（`swarm/repl/`）；
- **REPL**：
  - `swarm.repl.run_demo()`（交互式 REPL）；
- **CLI**：
  - `python examples/swarm.py`（运行示例）；
- **第三方 SDK**：
  - **OpenAI Python SDK**（核心）；
- **OpenAI Agents SDK 迁移**：
  - **API**：https://github.com/openai/openai-agents-python（已替代 Swarm）；

## 十一、生态与集成

- **LLM 集成**：OpenAI GPT（专属）/ 通过 OpenAI 兼容 API（Anthropic / Gemini / Ollama）；
- **Tool 集成**：Function Calling + Agent-as-Tool + Handoff Tool；
- **Context Variables**：跨 Step 共享状态；
- **REPL**：交互式调试；
- **Examples**：20+ 官方示例（customer_service / triage / airline_customer_service / personal_shopper / etc.）；
- **教学材料**：OpenAI Cookbook 教程；
- **OpenAI Agents SDK 迁移**：OpenAI 官方推荐迁移；
- **社区示例**：100+ 社区示例；
- **教学使用**：大学课程 / 培训机构 / OpenAI 官方教学；
- **生产部署**：不推荐（已被 OpenAI Agents SDK 取代）；
- **学术使用**：多 Agent 协作研究；

## 十二、源码学习路径

- **必读模块**（按学习顺序）：
  1. `swarm/__init__.py`：导出列表；
  2. `swarm/core.py`：Swarm + Agent 主类（约 1000 行核心）；
  3. `swarm/types.py`：Pydantic 类型定义；
  4. `swarm/util.py`：工具函数；
  5. `swarm/repl/`：交互式 REPL；
  6. `examples/`：20+ 示例（推荐 `customer_service` / `triage` / `airline_customer_service`）；
  7. `tests/`：单元测试；
- **LOC 与语言**：
  - `swarm/` ~2000 LoC（极简）；
  - `examples/` ~5000 LoC；
  - 主语言：**Python**（100%）；
- **构建系统**：
  - **setuptools**（`setup.py`）；
  - **pip**（依赖管理）；
  - **单包**（`swarm/` 一包）；
  - **CI/CD**：GitHub Actions；

## 十三、学习路径

- **入门（半天）**：
  - 阅读 OpenAI Cookbook 教程（https://cookbook.openai.com/examples/orchestrating_agents）；
  - 阅读 `examples/customer_service.py`（最简示例）；
  - 阅读 `swarm/core.py` 核心实现（约 1000 行）；
  - 跑通 REPL：`swarm.repl.run_demo()`；
  - 理解 Handoff + Agent-as-Tool + context_variables 三要素；
- **进阶（1-2 周）**：
  - 跑通所有 20+ 官方示例；
  - 实现自定义 Handoff 流程；
  - 实现自定义 Agent-as-Tool；
  - 实现复杂 context_variables 状态机；
  - 学习迁移到 OpenAI Agents SDK；
- **推荐资源**：
  - 官方仓库：https://github.com/openai/swarm
  - OpenAI Cookbook：https://cookbook.openai.com/examples/orchestrating_agents
  - OpenAI Agents SDK：https://github.com/openai/openai-agents-python（继任者）
  - Discord：https://discord.gg/openai

## 十四、风险与陷阱

- **已停止主动维护**：Swarm 已被 OpenAI Agents SDK 取代，仅做 bug fix；
- **OpenAI 绑定**：默认依赖 OpenAI API，其他 LLM 需通过兼容 API；
- **生产不推荐**：教学版，不适合生产部署；
- **极简抽象**：缺少高级特性（持久化 / Tracing / Guardrail）；
- **Context Variables 一致性**：并发 context_variables 修改可能冲突；
- **Handoff 死循环**：Agent 间 handoff 可能形成环路；
- **REPL 仅调试**：REPL 是调试工具，不能用于生产；
- **Performance 问题**：Python 调度开销大；
- **Token 消耗大**：Handoff 多步调用消耗大量 token；
- **Function 错误处理**：Function 内部需 try/except；
- **OpenAI Agents SDK 迁移成本**：迁移到新 SDK 需重写代码；
- **教学示例多但简**：示例多但都比较简单，复杂业务需自行扩展；
- **社区生态弱**：相比 LangGraph / CrewAI 社区较小（已停止维护）；

## 十六、参考资料

- 官方仓库：https://github.com/openai/swarm
- OpenAI Cookbook：https://cookbook.openai.com/examples/orchestrating_agents
- OpenAI Agents SDK（继任者）：https://github.com/openai/openai-agents-python
- 博客：https://openai.com/blog/swarm
- Discord：https://discord.gg/openai
- Forum：https://community.openai.com/

## 十七、与本项目关联

Swarm 作为「Handoff 协议 + Agent-as-Tool + 极简抽象 + 教学导向」行业代表教学版多 Agent 框架，与本项目（engineering/ 轨道 C/C++ 自研存储与向量索引）在「Handoff 协议 + Agent-as-Tool + Context Variables 共享」三个维度具有参考价值：

- **Handoff 协议 ↔ Handoff Routing**：Handoff 协议（`swarm/core.py`）与本项目 SQL 优化器的 Handoff Routing（如子查询下推）思路相似——都是"**控制权转移 + 上下文保留**"。
- **Agent-as-Tool ↔ Tool 抽象**：Agent-as-Tool（`swarm/core.py`）与本项目 Tool 抽象（`engineering/src/db/index/` + 自研算法库）的"**接口 + 实现分离 + 可调用**"机制相似。
- **Context Variables ↔ Context**：Config 与 Context 共享状态与本项目 GUC（`engineering/src/db/core/guc.c`）+ Context（`engineering/src/db/executor/`）的"**配置驱动 + 上下文传递**"机制相似。
- **Types 抽象 ↔ Schema 抽象**：Types（Pydantic 模型）与本项目 PG 风格 Catalog（`engineering/src/db/storage/catalog/`）的"**Schema 抽象 + 类型化**"机制相似。
- **REPL ↔ 自研 CLI**：REPL 与本项目自研 CLI（`engineering/src/db/cli/`）对照。
- **OpenAI 绑定 ↔ LiteLLM 集成**：OpenAI 绑定依赖与本项目 LiteLLM 集成（`engineering/src/llm/llm.h`）对照——都是"**统一接口 + 多模型**"。
- **教学导向 ↔ 工程导向**：Swarm 教学导向 vs 本项目工程导向对照；本项目可在"**极简 API + 教学示例**"方向获得参考价值。
- **学习建议**：阅读 Swarm 源码重点关注「Handoff 协议 + Agent-as-Tool + Context Variables 共享」，与本项目 `engineering/src/db/executor/` + `engineering/src/db/index/` 做架构对照，加深对「极简抽象 + 控制权转移 + 上下文共享」的理解。
- **重点对照**：Swarm 极简哲学（核心 1000 行）与本项目 C/C++ 自研工程化哲学对照，是「Python 极简 vs C/C++ 工程化」的典型案例。本项目可在"**Handoff 协议 + 极简核心**"方向获得启发，建议本项目 `engineering/src/db/executor/` 参考 Swarm "**Handoff + Agent-as-Tool**" 设计模式来强化 Executor 协调能力。