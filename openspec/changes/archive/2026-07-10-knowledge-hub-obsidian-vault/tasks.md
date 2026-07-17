# S19 — Tasks（Knowledge_Hub → Obsidian 数据源说明）

> **目标**：文档化学习层数据与知识库小程序的数据关系，不实际迁移内容。

## 1.1 调研

- [x] 1.1.1 已查：`apps/web/knowledge_hub/src/data/learn_deep/` 数据已是 markdown
- [x] 1.1.2 已查：knowledge_hub/CLAUDE.md 已有"内容数据必须以 Markdown 文件存储"规则

## 1.2 实施

- [ ] 1.2.1 创建 `learning/notes/learn_deep/` 目录（占位）
- [ ] 1.2.2 创建 `learning/scripts/sync-learn-deep.sh`（stub）

## 1.3 文档

- [ ] 1.3.1 更新 `learning/notes/README.md` 增加 Learn Deep 来源说明
- [ ] 1.3.2 更新 `apps/web/knowledge_hub/CLAUDE.md` 增加"主存储来源"声明

## 1.4 验证 V1-V4

- [ ] 1.4.1 V1: `ls learning/notes/learn_deep/` 目录存在
- [ ] 1.4.2 V2: sync-learn-deep.sh 存在且可执行
- [ ] 1.4.3 V3: learning/notes/README.md 含 Learn Deep 段
- [ ] 1.4.4 V4: knowledge_hub/CLAUDE.md 含数据源声明

## 1.5 提交 + 归档

- [ ] 1.5.1 `git add -A openspec/changes/knowledge-hub-obsidian-vault/ learning/notes/ learning/scripts/ engineering/apps/web/knowledge_hub/CLAUDE.md`
- [ ] 1.5.2 `git commit -m "docs(learn-deep): 学习数据双源文档化 + 同步脚本 stub"`
- [ ] 1.5.3 `git push origin project`
- [ ] 1.5.4 归档到 `openspec/changes/archive/2026-07-10-knowledge-hub-obsidian-vault/`
