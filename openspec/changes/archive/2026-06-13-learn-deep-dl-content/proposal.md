## Why

reading-radar 已有 300 个技术栈知识点（C/CPP/DS/DB/PY/Linux/VDB），但深度文章（learn-deep/）目前覆盖仅 93 篇且集中在 DB/Linux/VDB 三个栈。C、CPP、DS、PY 四个栈合计 ~181 个知识点完全没有深度学习维度的关联内容。即使已有的 93 篇文章，也没有从「该知识点在深度学习中的应用」这一角度展开论述。

这导致用户在阅读指针、B+树、内存管理等概念时，只能看到纯粹的技术解释，无法和当前最热门的深度学习/大模型领域建立连接，降低了知识的实用性和吸引力。

## What Changes

- 对全部 300 个知识点按「与深度学习的关联度」分为三级（强/中/弱），差异化篇幅生成 learn-deep 深度文章
- 对 93 篇已有文章，追加深度学习应用章节，保留原始内容不变
- 新增 ~207 篇 learn-deep markdown 文件到 `data/learn-deep/<item-id>.md`
- 重建 `data/app/learn-deep-bundle.js`，确保所有 300 个知识点在 learn.html 中可渲染
- 不修改 items-registry.js 或其他现有数据源
- **不破坏** 现有 learn.html 的渲染链路（仍兼容 LEARN_DEEP_CONTENT bundle + fetch 回退）

## Capabilities

### New Capabilities
- `dl-content-classification`: 将 300 个知识点按 DL 关联度自动分类（强/中/弱三级），产出分类清单作为后续生成的依据
- `dl-content-generation`: 按分类结果分批调用 LLM 生成/补充 learn-deep 文章，每篇符合对应级别的篇幅和深度要求，包含具体的技术连接和示例
- `dl-content-integration`: 将生成的 markdown 文件写入 `data/learn-deep/`、追加到现有文章、重建 `learn-deep-bundle.js` 并验证完整性

### Modified Capabilities
- 无（不修改现有规范，所有新增内容独立于现有系统）

## Impact

- 新增 207 个 `.md` 文件到 `data/learn-deep/`（按 item 所属栈存放到对应子目录，与现有 linux/db/vdb 一致）
- 新增 1 个 `.js` 文件到 `data/` 或修改 `data/app/learn-deep-bundle.js`（增量追加新条目）
- 对 93 篇已有文章的改动仅限末尾追加章节，不修改原始内容
- learn.html 无需改动（渲染链路已支持，唯一需要的是 bundle 包含新条目）
- 总 token 消耗预估：~200K（分批 Workflow 子 agent），远低于逐篇 agent 的 500K+
