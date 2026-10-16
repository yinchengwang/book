# AgentVerse Wiki（研究型多 Agent 平台）

> 参考版本：`agentverse` Python 包（`agentverse/` 主包，学术研究型），参考 HEAD：b292a33a6 前的最终 release（AgentVerse 0.7+）。本 wiki 重点介绍 AgentVerse 作为「**学术研究 + 多 Agent 仿真 + 多 Agent 任务求解 + Environment 抽象 + Memory + Tool + Recruiter 动态招募 + Customizable Simulation**」的多 Agent 研究框架定位、源码架构、四个 Agent 维度（角色协作 / 记忆 / 工具调用 / 人机协作）的深度展开，以及与本项目 `engineering/` 自研存储与向量索引的对照参考。

## 一、项目定位

AgentVerse 是由 **OpenBMB（清华 & 面壁智能）** 与 **PKU（北京大学）** 联合开源的 **「研究型多 Agent 平台」**，定位是 **「A Research Platform for Multi-Agent Systems」**——围绕学术研究的两大场景（**多 Agent 仿真 / Multi-Agent Simulation** 和 **多 Agent 任务求解 / Multi-Agent Task Solving**）构建可定制 / 可扩展 / 可重现的研究框架。

- **核心痛点**：
  - 缺少**学术研究底座**（其他框架都是生产 + 应用，缺少论文复现 / 评估 / 对比）；
  - 缺少**多 Agent 仿真环境**（Prisoner Dilemma / Pokemon / SDE Team / etc.）；
  - 缺少**多 Agent 任务求解环境**（数学 / 逻辑 / 推理 / 编程）；
  - 缺少**动态 Agent 招募**（Recruiter 根据任务招募合适的 Agent）；
  - 缺少**环境抽象**（Environment / Agent / Tool / Memory 标准接口）；
  - 缺少**评估标准**（Benchmark / Metric / Evaluation）；
- **典型用户**：
  - **学术研究者**：Agent 协作 / SOP 探索 / 论文复现；
  - **学生 / 研究生**：多 Agent 实验平台；
  - **企业研究部门**：Agent 算法评估；
- **核心卖点**：
  - **两类场景**：Simulation（仿真）/ Task Solving（任务求解）；
  - **Environment 抽象**：Environment / Agent / Tool / Memory 标准接口；
  - **Recruiter**：动态 Agent 招募（LLM 选择合适 Agent）；
  - **Simulation Environment**：Prisoner Dilemma / Pokemon / SDE Team；
  - **Task Solving Environment**：Math / Logic / Coding / Reasoning；
  - **Customizable**：可自定义 Environment / Agent / Task；
  - **Reproducible**：可复现实验（seed / config / log）；
  - **Multi-LLM**：支持多 LLM 路由；
  - **CLI + Python API**；
  - **评估**：Benchmark / Metric；
  - **学术合作**：清华大学 / 北京大学 / OpenBMB；

## 二、发展历史与社区

- **起源（2023-08）**：OpenBMB + PKU 联合开源 AgentVerse v0.1，发表论文《AgentVerse: Facilitating Multi-Agent Collaboration and Exploring Emergent Behaviors》；
- **里程碑**：
  - 2023-08：v0.1（Simulation + Task Solving 两类场景 + Environment 抽象）；
  - 2023-12：v0.5（Recruiter 动态招募 + Reflection + Tool）；
  - 2024-06：v0.7（Pokemon / SDE Team / Coding / Math 新增环境）；
  - 2024-10：v0.7.x（LLM 路由 / Evaluator / Benchmark 完善）；
  - 2025-02：v0.7.x（学术合作 + NeurIPS 2024 / ICLR 2024 论文复现）；
  - 2025-08：v0.7.x（多模态 + Computer Use）；
  - 2026-03：v0.7.x（稳定 + 性能 + 学术复现）；
- **维护状态**：**活跃**（OpenBMB + PKU 团队 10+ 工程师，月均 30+ commits）；
- **社区**：
  - GitHub 4k+ star、`agentverse` PyPI 月下载量 5 万+；
  - Discord 1k+ 成员、GitHub Discussions；
  - 学术论文：AgentVerse: Facilitating Multi-Agent Collaboration and Exploring Emergent Behaviors（ICLR 2024）；
  - 学术论文：SDE Team / Coding / Math 多个论文；
- **关键贡献者**：清华团队 / PKU 团队 / OpenBMB、贡献者 50+；

## 三、许可与商业限制

- **核心代码**：**Apache-2.0 License**（`agentverse/`、`agentverse_command/`、`dataloader/`、`data/`、`scripts/`），可商用、修改、私有部署；
- **学术使用**：通常学术研究免费；
- **商业使用**：Apache-2.0 允许商用；
- **生态关系**：
  - **上游依赖**：
    - Python 3.10+；
    - OpenAI / Anthropic / Azure / Gemini / Ollama / 通义千问；
    - Pydantic v2；
    - asyncio + aiohttp；
    - chroma / faiss（部分 memory）；
  - **下游生态**：约 500+ 学术研究项目使用；

## 四、业界主要使用方与案例

- **国际**（公开）：
  - **多家大学**：学术研究；
  - **多家研究机构**：Agent 协作 / SOP 探索；
- **国内**：
  - **清华大学**：学术研究；
  - **北京大学**：学术研究；
  - **OpenBMB**：学术研究 + 商业项目；
- **典型场景**：
  - **学术研究**：论文复现 / 新算法；
  - **多 Agent 仿真**：Prisoner Dilemma / Pokemon / SDE Team；
  - **多 Agent 任务求解**：Math / Logic / Coding / Reasoning；
  - **教育演示**：多 Agent 教学；
  - **论文复现**：NeurIPS / ICLR 论文实验；

## 五、一句话对比定位

> AgentVerse 是 **「OpenBMB + PKU 出品 + 学术研究 + Simulation + Task Solving + Environment + Recruiter + 4k star + Apache-2.0」**；与 **LangGraph（图状态机）**、**CrewAI（角色化 + Process）**、**AutoGen（GroupChat 对话）**、**MetaGPT（软件公司角色）**、**OpenAI Agents（Handoff 简单协议）**、**AgentScope（多 Agent 通用）** 构成多 Agent 生态。AgentVerse 的差异化是 **「学术研究 + Simulation + Task Solving + Recruiter + 清华 + PKU + 论文复现 + 可重现」**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                  用户应用层（User Apps）                              │
│        CLI / Python API / agentverse_command / Jupyter              │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Public API（agentverse/）                          │
│      agentverse.py（AgentVerse 入口）                                 │
│      simulation.py（Simulation 入口）                                │
│      tasksolving.py（Task Solving 入口）                             │
│      demo.py（Demo 入口）                                            │
│      gui.py（GUI 入口）                                              │
│      initialization.py（初始化）                                     │
│      registry.py（Registry）                                         │
│      logging.py（Logging）                                            │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Environment 抽象                                    │
│   environments/                                                      │
│     ├── base.py（Environment 基类）                                  │
│     ├── simulation_env/（仿真环境）                                   │
│     │     ├── basic.py                                               │
│     │     ├── pokemon.py                                             │
│     │     ├── prisoner_dilemma.py                                   │
│     │     ├── reflection.py                                         │
│     │     ├── sde_team.py                                          │
│     │     ├── sde_team_given_tests.py                              │
│     │     └── rules/（规则）                                          │
│     └── tasksolving_env/（任务求解环境）                            │
│           ├── math.py                                                  │
│           ├── logic.py                                              │
│           ├── coding.py                                             │
│           └── reasoning.py                                          │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Agent 抽象                                          │
│   agents/                                                            │
│     ├── base.py（Agent 基类）                                       │
│     ├── simulation_agent/（仿真 Agent）                               │
│     │     ├── conversation.py                                       │
│     │     ├── prisoner_dilemma.py                                   │
│     │     ├── reflection.py                                         │
│     │     └── tool.py                                               │
│     └── tasksolving_agent/（任务求解 Agent）                        │
│           ├── math.py                                              │
│           ├── logic.py                                              │
│           ├── coding.py                                            │
│           └── reasoning.py                                         │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Memory / Tool / LLM / Recruiter                    │
│   memory/（Memory 基类）                                            │
│   llms/（LLM 抽象）                                                  │
│   message.py（Message 抽象）                                              │
│   output_parser/（输出解析）                                          │
│   memory_manipulator/（Memory 操作）                                │
│   initialization.py（环境初始化 / Recruiter）                        │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  CLI / Dataloader / Data                             │
│   agentverse_command/（CLI 命令）                                    │
│   dataloader/（数据加载）                                            │
│   data/（预制数据）                                                   │
│   ui/（UI）                                                          │
│   scripts/（脚本）                                                    │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  外部依赖（External Dependencies）                   │
│   OpenAI / Anthropic / Azure / Gemini / Ollama / 通义千问          │
│   Chroma / FAISS / Pydantic / tiktoken                              │
└─────────────────────────────────────────────────────────────────────┘
```

进程模型：AgentVerse 是**纯 Python 用户态库**（无独立进程）；通过 asyncio + aiohttp 实现异步高并发；CLI 工具（`agentverse_command/`）启动仿真 / 任务求解。

## 七、Agent 类型专项（核心扩展章节）

### 7.1 角色协作模式（Role Collaboration）

AgentVerse 通过 **Environment + Agent + Recruiter** 实现四种协作模式：

- **中心化（Orchestrator-Worker）**：
  - **典型实现**：Recruiter 选择合适 Agent（Orchestrator）；
  - **实现位置**：`agentverse/initialization.py`（Recruiter 招募）；
  - **机制**：根据任务招募 Agent（Orchestrator LLM 选择）；
- **对等（Peer-to-Peer / Blackboard）**：
  - **典型实现**：所有 Agent 共享 Environment Context（blackboard pattern）；
  - **实现位置**：`agentverse/environments/base.py`（Environment 基类）；
  - **共享 State**：所有 Agent 共享 Environment；
- **分层（Hierarchical / Supervisor）**：
  - **典型实现**：SDE Team 环境的 Supervisor Agent → Worker Agent；
  - **实现位置**：`agentverse/environments/simulation_env/sde_team.py`；
- **市场（Market-based Bidding）**：
  - **典型实现**：Recruiter 通过 LLM 评估 Agent 能力选择（类似 Bidding）；
  - **实现位置**：`agentverse/initialization.py`；
- **Recruiter 动态招募**：
  - **典型实现**：Recruiter 根据任务招募 Agent；
  - **实现位置**：`agentverse/initialization.py`；
  - **机制**：用 LLM 评估 Agent 描述（Role / Capabilities），选择合适 Agent；
- **Multi-Round 多轮协作**：
  - **典型实现**：所有环境都支持多轮对话；
  - **实现位置**：`agentverse/environments/base.py`（multi_round）；

### 7.2 记忆系统（Memory System）

AgentVerse 记忆分**Environment Memory + Agent Memory + LLM Context**：

- **短期（In-memory conversation buffer）**：
  - **实现位置**：Environment 内的 messages 列表；
  - **API**：自动维护在 Environment 执行中；
- **长期（Vector Memory / File System）**：
  - **实现位置**：`agentverse/memory/`（Memory 基类）；
  - **类型**：Chroma / FAISS（部分实现）；
- **Memory 操纵（Memory Manipulator）**：
  - **实现位置**：`agentverse/memory_manipulator/`（Memory 操作）；
  - **API**：手动修改 Memory；
- **Memory 压缩与检索**：
  - **典型实现**：Reflection 机制总结历史；
  - **实现位置**：`agentverse/agents/simulation_agent/reflection.py`；
- **跨 Agent 记忆共享**：
  - **实现位置**：Environment 共享所有 Agent 的 Memory；
- **Embedding 记忆**：
  - **典型实现**：将历史 Embedding 后存储到 Vector DB；
  - **API**：通过 Chroma / FAISS 实现；

### 7.3 工具调用协议（Tool Calling Protocol）

AgentVerse 通过 **Tool / Function Calling** 实现工具调用：

- **Function Calling（OpenAI / Anthropic / Gemini）**：
  - **实现位置**：`agentverse/llms/`（LLM 抽象）；
  - **支持模型**：OpenAI GPT-4 / Anthropic Claude / Google Gemini / Ollama / 通义千问；
  - **绑定工具**：`agent.set_tools([my_tool])`；
- **ReAct（Reasoning + Acting）**：
  - **典型实现**：Task Solving Agent 默认使用 ReAct 循环；
  - **实现位置**：`agentverse/agents/tasksolving_agent/`；
  - **循环**：每轮 Agent 生成 Thought + Action（Tool 调用）→ 观察结果 → 下一轮；
- **CodeAct（生成代码执行）**：
  - **典型实现**：Coding Agent 调用 Python REPL；
  - **实现位置**：`agentverse/agents/tasksolving_agent/coding.py`；
  - **沙箱**：本地 / Docker（生产推荐）；
- **Tool Use + Reflection**：
  - **典型实现**：Reflection Agent 反思 Tool 输出；
  - **实现位置**：`agentverse/agents/simulation_agent/reflection.py`；
- **Structured Outputs**：
  - **实现位置**：`agentverse/output_parser/`（输出解析）；
  - **API**：通过 Pydantic 模型定义；
- **Custom Tool**：
  - **典型实现**：`agentverse/agents/simulation_agent/tool.py`（Tool 基类）；
  - **API**：`class MyTool(BaseTool): def run(self, ...): ...`；
- **Tool Error Handling**：
  - **API**：Tool 内部 try/except；
- **Toolkit 组合**：
  - **典型**：Agent 可配置多个 Tool；

### 7.4 人机协作（Human-in-the-Loop）

AgentVerse HITL 通过 **Custom Agent / CLI / GUI** 实现：

- **Human Agent**：
  - **典型实现**：自定义 Agent 让人类参与；
  - **API**：`class HumanAgent(BaseAgent): def step(self): return HumanInput()`；
- **CLI Interactive**：
  - **典型实现**：`agentverse_command/` 启动交互模式；
  - **API**：CLI 提示人工输入；
- **GUI HITL**：
  - **典型实现**：`agentverse/gui.py`（GUI 入口）；
  - **API**：通过 GUI 中断 / 批准；
- **Reflection HITL**：
  - **典型实现**：Reflection Agent 等待人工反馈；
  - **API**：`agent.reflect(human_input=True)`；
- **Custom HITL**：
  - **典型实现**：自定义 Environment 在某些 Step 等待人工；
- **Approval Workflow**：
  - **典型实现**：自定义 Tool 调用 `input()`；
- **Audit Trail**：
  - **实现位置**：`agentverse/logging.py`（Logging）；
  - **API**：自动 trace 所有 Agent / Environment / Tool 调用；

## 八、索引机制（Index / Recruiter / Registry）

AgentVerse 索引通过 **Registry + Recruiter + Memory** 实现：

- **Registry**：
  - **实现位置**：`agentverse/registry.py`；
  - **机制**：注册 Environment / Agent / Task / Tool；
  - **API**：`registry.register_environment("pokemon", PokemonEnv)`；
- **Recruiter（动态招募）**：
  - **实现位置**：`agentverse/initialization.py`；
  - **机制**：根据任务选择合适 Agent；
  - **API**：`recruiter.recruit(task, candidates)`；
- **Memory Index**：
  - **实现位置**：`agentverse/memory/`；
  - **API**：`memory.search(query)`；
- **Task Index**：
  - **实现位置**：`agentverse/tasks/`；
  - **API**：`task.load(task_id)`；
- **Data Index**：
  - **实现位置**：`agentverse/data/` + `dataloader/`；
  - **API**：`dataloader.load("math")`；

## 九、查询/计算/接口层

### 9.1 AgentVerse 执行引擎（Simulation & Task Solving）

- **Simulation Engine**：
  - **实现位置**：`agentverse/simulation.py`（Simulation 入口）；
  - **API**：`simulation.run(env="pokemon", num_agents=4, rounds=10)`；
  - **场景**：多 Agent 仿真；
- **Task Solving Engine**：
  - **实现位置**：`agentverse/tasksolving.py`（Task Solving 入口）；
  - **API**：`tasksolving.run(task="math", num_agents=3)`；
  - **场景**：多 Agent 任务求解；
- **Environment 执行**：
  - **实现位置**：`agentverse/environments/base.py`；
  - **API**：`env.step()`；
- **Agent 执行**：
  - **实现位置**：`agentverse/agents/base.py`（Agent 基类）；
  - **API**：`agent.step(env)`；
- **Recruiter 招募**：
  - **实现位置**：`agentverse/initialization.py`；
  - **API**：`recruiter.recruit(task)`；

### 9.2 状态管理（State / Environment / Memory）

- **Environment State**：
  - **实现位置**：`agentverse/environments/base.py`；
  - **内容**：所有 Agent 共享的 State + Messages；
- **Agent State**：
  - **实现位置**：`agentverse/agents/base.py`；
- **Memory State**：
  - **实现位置**：`agentverse/memory/`；
- **Persistence**：
  - **API**：通过 logging + registry 持久化；
- **Reproduction**：
  - **机制**：seed / config 记录实验；
  - **API**：通过 seed 复现实验；

### 9.3 多模态工具（Multimodal Tools）

- **代码执行**：通过 Tool + Python REPL；
- **RAG 工具**：通过 Tool + Vector Store；
- **数据库**：通过 Tool + SQLAlchemy；
- **搜索**：通过 Tool + Tavily / Serper；
- **多模态**：通过 Tool + GPT-4V / Claude 3；

### 9.4 多 Agent 通信（Multi-Agent Communication）

- **Environment 共享 Context**：
  - **实现位置**：`agentverse/environments/base.py`；
  - **机制**：所有 Agent 共享 Environment；
- **Multi-Round 对话**：
  - **典型实现**：所有环境支持多轮对话；
- **A2A Protocol**：
  - **AgentVerse 不直接实现 A2A**，但可通过自定义 Environment 实现跨进程通信；

## 十、查询/计算/接口层（API & SDK）

- **Python API（核心）**：
  - `agentverse.agentverse.AgentVerse`；
  - `agentverse.simulation.Simulation`；
  - `agentverse.tasksolving.TaskSolving`；
  - `agentverse.environments.base.Environment`；
  - `agentverse.agents.base.BaseAgent`；
  - `agentverse.llms.LLM`；
  - `agentverse.memory.Memory`；
  - `agentverse.registry.Registry`；
  - `agentverse.message.Message`；
  - `agentverse.output_parser.OutputParser`；
  - `agentverse.initialization.Initializer`；
  - `agentverse.logging.Logger`；
- **CLI**：
  - `agentverse simulation`（启动仿真）；
  - `agentverse tasksolving`（启动任务求解）；
  - `agentverse_command / <command>`（CLI 命令）；
- **GUI**：
  - `agentverse.gui.run_gui()`（启动 GUI）；
- **第三方 SDK**：
  - **LangChain 集成**：通过 LangChain Adapter；
  - **Instructor 集成**：结构化输出；
- **学术合作 SDK**：
  - **NeurIPS / ICLR 论文复现**：通过 config + seed 复现；

## 十一、生态与集成

- **LLM 集成**：OpenAI / Anthropic / Azure OpenAI / Google Gemini / Mistral / Ollama / vLLM / Together / 通义千问 / 文心一言 / 智谱 GLM；
- **Vector Store**：Chroma（部分）/ FAISS / 自定义；
- **Tool 集成**：Python REPL / 自定义 Tool / LangChain Tools；
- **Memory Backend**：In-memory（默认）/ Chroma / FAISS；
- **Data Source**：预制数据集 + 自定义；
- **Observability**：Logging（`logging.py`）+ 自定义；
- **部署**：Docker / Kubernetes / 自定义 Server；
- **学术合作**：清华大学 / 北京大学 / OpenBMB / NeurIPS / ICLR；
- **Benchmark**：Math / Logic / Coding / Reasoning / Pokemon / Prisoner Dilemma / SDE Team；
- **Research Tools**：Reproducibility（seed）+ Config management + Evaluation；

## 十二、源码学习路径

- **必读模块**（按学习顺序）：
  1. `agentverse/__init__.py`：导出列表；
  2. `agentverse/agentverse.py`：AgentVerse 入口；
  3. `agentverse/simulation.py`：Simulation 入口；
  4. `agentverse/tasksolving.py`：Task Solving 入口；
  5. `agentverse/demo.py`：Demo 入口；
  6. `agentverse/initialization.py`：初始化 + Recruiter；
  7. `agentverse/registry.py`：Registry；
  8. `agentverse/logging.py`：Logging；
  9. `agentverse/gui.py`：GUI 入口；
  10. `agentverse/message.py`：Message 抽象；
  11. `agentverse/output_parser/`：输出解析；
  12. `agentverse/utils.py`：工具；
  13. `agentverse/llms/`：LLM 抽象；
  14. `agentverse/memory/`：Memory 基类；
  15. `agentverse/memory_manipulator/`：Memory 操作；
  16. `agentverse/environments/base.py`：Environment 基类；
  17. `agentverse/environments/simulation_env/basic.py`：Basic Simulation Env；
  18. `agentverse/environments/simulation_env/pokemon.py`：Pokemon Env；
  19. `agentverse/environments/simulation_env/prisoner_dilemma.py`：Prisoner Dilemma；
  20. `agentverse/environments/simulation_env/sde_team.py`：SDE Team；
  21. `agentverse/environments/simulation_env/reflection.py`：Reflection Env；
  22. `agentverse/environments/simulation_env/rules/`：规则；
  23. `agentverse/environments/tasksolving_env/math.py`：Math；
  24. `agentverse/environments/tasksolving_env/logic.py`：Logic；
  25. `agentverse/environments/tasksolving_env/coding.py`：Coding；
  26. `agentverse/environments/tasksolving_env/reasoning.py`：Reasoning；
  27. `agentverse/agents/base.py`：Agent 基类；
  28. `agentverse/agents/simulation_agent/conversation.py`：Conversation Agent；
  29. `agentverse/agents/simulation_agent/prisoner_dilemma.py`：Prisoner Dilemma Agent；
  30. `agentverse/agents/simulation_agent/reflection.py`：Reflection Agent；
  31. `agentverse/agents/simulation_agent/tool.py`：Tool Agent；
  32. `agentverse/agents/tasksolving_agent/math.py`：Math Agent；
  33. `agentverse/agents/tasksolving_agent/logic.py`：Logic Agent；
  34. `agentverse/agents/tasksolving_agent/coding.py`：Coding Agent；
  35. `agentverse/agents/tasksolving_agent/reasoning.py`：Reasoning Agent；
  36. `agentverse/tasks/simulation/`：仿真任务；
  37. `agentverse/tasks/tasksolving/`：任务求解任务；
  38. `agentverse_command/`：CLI 命令；
  39. `dataloader/`：数据加载；
  40. `data/`：预制数据；
  41. `ui/`：UI；
  42. `scripts/`：脚本；
- **LOC 与语言**：
  - `agentverse/` ~30k LoC；
  - `agentverse_command/` ~3k LoC；
  - `dataloader/` ~2k LoC；
  - `data/` ~10k LoC（数据）；
  - 主语言：**Python**（100%）；
- **构建系统**：
  - **setuptools**（`setup.py`）；
  - **pip**（依赖管理）；
  - **单包**（`agentverse/` 一包）；
  - **CI/CD**：GitHub Actions；

## 十三、学习路径

- **入门（1 周）**：
  - 阅读官方文档（https://agentverse.readthedocs.io/）；
  - 跑通 Simulation 示例（Prisoner Dilemma / Pokemon）；
  - 理解 Environment / Agent 抽象；
  - 跑通 Task Solving 示例（Math / Logic）；
- **进阶（2-3 周）**：
  - 深入学习 Recruiter（动态招募）；
  - 实战自定义 Environment；
  - 实战自定义 Agent；
  - 学习 Reflection 机制；
  - 学习 Tool 集成；
  - 学习 Memory + Memory Manipulator；
  - 学习 OutputParser（结构化输出）；
- **高级（1-2 月）**：
  - 阅读源码（Environment + Agent + Recruiter + Memory）；
  - 自定义 Environment（继承 Environment 基类）；
  - 自定义 Agent（继承 BaseAgent 类）；
  - 自定义 Recruiter（实现 Recruiter 接口）；
  - 自定义 Memory（实现 Memory 基类）；
  - 自定义 Tool；
  - Benchmark 二次开发；
  - 学术论文复现；
- **推荐资源**：
  - 官方仓库：https://github.com/OpenBMB/AgentVerse
  - 官方文档：https://agentverse.readthedocs.io/
  - 论文：AgentVerse: Facilitating Multi-Agent Collaboration and Exploring Emergent Behaviors（ICLR 2024）
  - 论文：SDE Team / Coding / Math 多篇论文
  - 博客：https://openbmb.github.io/
  - Discord：https://discord.gg/openbmb
  - Forum：https://github.com/OpenBMB/AgentVerse/discussions
  - 学术合作：清华大学 / 北京大学 / OpenBMB

## 十四、风险与陷阱

- **学术研究导向**：生产部署能力较弱，缺少企业级特性（HITL / Studio / Tracing）；
- **小众用户**：相比 LangGraph / CrewAI 用户基数小；
- **文档不完善**：相比 LangGraph / CrewAI 文档较少；
- **小团队维护**：OpenBMB + PKU 团队维护，人力有限；
- **Recruiter 不稳定**：LLM-based Recruiter 选择可能不稳定；
- **Environment 复用难**：自定义 Environment 难以共享；
- **Benchmark 评估缺失**：默认无统一评估；
- **Reflection 成本**：Reflection 总结消耗额外 token；
- **Tool 错误处理**：Tool 内部需 try/except；
- **Performance 问题**：Python 调度开销大，超大仿真（>10 Agent）性能下降；
- **HITL 集成弱**：默认 HITL 支持较弱；
- **多模态支持弱**：图像 / 视频 / 音频支持较少；
- **商业支持缺失**：无商业版 / 企业级支持；
- **GitHub Star 较少**：相比 LangGraph / CrewAI 4k star，社区活跃度低；

## 十六、参考资料

- 官方仓库：https://github.com/OpenBMB/AgentVerse
- 官方文档：https://agentverse.readthedocs.io/
- 论文：AgentVerse: Facilitating Multi-Agent Collaboration and Exploring Emergent Behaviors（ICLR 2024）
- 论文：Exploring Collaboration Mechanisms for Emergent Behaviors
- 论文：Multi-Agent Collaboration in Complex Task Solving
- 博客：https://openbmb.github.io/
- Tutorial：https://agentverse.readthedocs.io/en/latest/quickstart.html
- Discord：https://discord.gg/openbmb
- Forum：https://github.com/OpenBMB/AgentVerse/discussions
- 学术合作：清华大学 / 北京大学 / OpenBMB

## 十七、与本项目关联

AgentVerse 作为「学术研究 + Simulation + Task Solving + Recruiter」行业代表多 Agent 研究框架，与本项目（engineering/ 轨道 C/C++ 自研存储与向量索引）在「Environment 抽象 + Recruiter + Reflection + Memory Manipulator」四个维度具有直接参考价值：

- **Environment 抽象 ↔ 多模态存储抽象**：Environment 抽象（`agentverse/environments/base.py`）与本项目多模态存储抽象（`engineering/include/db/mm_storage.h`、`engineering/src/db/storage/mm/mm_storage.c`）的"**统一接口 + 多种实现**"思路相似——都是"**抽象层 + 数据模型**"思路。
- **Recruiter ↔ Index Selector**：Recruiter（`agentverse/initialization.py`）的动态选择与本项目 Index Selector（`engineering/src/db/index/index_selector.c`）的"**能力匹配 + 自动选择**"机制相似。
- **Reflection ↔ 查询优化器**：Reflection 机制（`agentverse/agents/simulation_agent/reflection.py`）与本项目查询优化器（`engineering/src/db/optimizer/`）的"**反思 + 优化 + 决策**"机制相似。
- **Memory Manipulator ↔ WAL**：Memory Manipulator（`agentverse/memory_manipulator/`）与本项目 WAL（`engineering/src/db/storage/log/`）的"**状态变更追踪 + 历史保留**"机制相似。
- **Registry ↔ Catalog**：Registry（`agentverse/registry.py`）与本项目 PG 风格 Catalog（`engineering/src/db/storage/catalog/`）的"**元数据管理 + 注册中心**"机制相似。
- **Simulation ↔ 仿真测试**：Simulation（`agentverse/environments/simulation_env/`）与本项目仿真测试（`engineering/test/`）对照。
- **Task Solving ↔ SQL 求解**：Task Solving（`agentverse/environments/tasksolving_env/`）与本项目 SQL 求解（`engineering/src/db/sql/`）对照。
- **Multi-Round 对话 ↔ 事务**：Multi-Round 对话（`agentverse/environments/base.py`）与本项目事务（`engineering/src/db/executor/transaction.c`）的"**多轮交互 + 状态管理**"机制相似。
- **Benchmark ↔ 测试套件**：Benchmark（Math / Logic / Coding / Pokemon / Prisoner Dilemma）与本项目测试套件（`engineering/test/`）对照。
- **Reproducibility ↔ 测试可重现**：Reproducibility（seed / config）与本项目测试可重现（CMake / CTest）对照。
- **CLI ↔ 自研 CLI**：AgentVerse CLI 与本项目自研 CLI（`engineering/src/db/cli/`）对照。
- **GUI ↔ 自研 Studio**：GUI（`agentverse/gui.py`）与本项目自研 Studio 对照。
- **学习建议**：阅读 AgentVerse 源码重点关注「Environment 抽象 + Recruiter 动态招募 + Reflection 机制 + Registry 元数据管理」，与本项目 `engineering/src/db/storage/mm/` + `engineering/src/db/index/` 做架构对照，加深对「环境抽象 + 动态选择 + 反思优化」的理解。