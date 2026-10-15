# Agent 框架克隆清单

本目录收录 13 个主流开源 Agent（自主代理）框架源码，作为学习与对照参考。

## 项目清单

| 项目 | GitHub | 本地路径 | tier | 备注 |
|------|--------|----------|------|------|
| langgraph | langchain-ai/langgraph | agent/langgraph/ | core | 图编排 Agent |
| crewai | crewAIInc/crewAI | agent/crewai/ | core | 多 Agent 协作 |
| autogen | microsoft/autogen | agent/autogen/ | core | Microsoft 出品 |
| metagpt | geekan/MetaGPT | agent/metagpt/ | core | 软件公司多 Agent |
| openai-agents | openai/openai-agents-python | agent/openai-agents/ | representative | OpenAI 官方 SDK |
| swarm | openai/swarm | agent/swarm/ | innovative | OpenAI 教学版 |
| agents | microsoft/agents | agent/agents/ | representative | Microsoft 新版 |
| chatdev | OpenBMB/ChatDev | agent/chatdev/ | representative | 教学型 |
| agentverse | OpenBMB/AgentVerse | agent/agentverse/ | representative | 研究型 |
| agixt | Josh-XT/AGiXT | agent/agixt/ | innovative | 自动化框架（原 AGiXT/AGiXT 已迁移） |
| open-interpreter | OpenInterpreter/open-interpreter | agent/open-interpreter/ | innovative | 代码解释器 |
| agentscope | modelscope/agentscope | agent/agentscope/ | representative | 阿里出品 |
| letta | letta-ai/letta | agent/letta/ | innovative | 长期记忆（原 MemGPT） |

## 层级说明

- **core**：学习必读、产业标准事实标杆。
- **representative**：具有代表性的官方/大厂产品。
- **innovative**：新锐思路，对前两类的补充。

## 更新方式

```bash
cd reference/ai/agent/<project>
git fetch --depth 1 origin
git reset --hard origin/main
```

## 来源

完整的元数据登记在仓库根的 [`reference/catalog.yml`](../catalog.yml) 中，以 `model: ai/agent` 字段标记。

## 注意事项

- 部分克隆（agixt、chatdev、crewai、llama-index）首次拉取时可能因网络抖动中断，请直接重试 `git clone --depth 1 <url> <path>`；本任务已在 `task-2-report.md` 中记录。
