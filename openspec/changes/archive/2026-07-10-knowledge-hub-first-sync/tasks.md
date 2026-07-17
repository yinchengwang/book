# S27 — Tasks（Knowledge_Hub First Sync）

## 1.1 调研

- [x] 1.1.1 已查：S20+ 已创建 sync-learn-deep.sh
- [x] 1.1.2 已查：知识库端 `apps/web/knowledge_hub/src/data/learn_deep/` 已有大量真实数据

## 1.2 实施

- [ ] 1.2.1 创建示例 markdown 文件（学习层 6 个子目录各 1 个）
- [ ] 1.2.2 跑 `sync-learn-deep.sh --dry-run` 验证脚本
- [ ] 1.2.3 注释掉 --dry-run，跑真同步（rsync 到知识库）

## 1.3 验证 V1-V4

- [ ] 1.3.1 V1: 学习层 .gitkeep 被覆盖为示例 .md
- [ ] 1.3.2 V2: sync 脚本不报错
- [ ] 1.3.3 V3: 知识库 receive 了示例文件
- [ ] 1.3.4 V4: 6 个学习层文件均到位

## 1.4 提交 + 归档

- [ ] 1.4.1 `git add -A openspec/changes/knowledge-hub-first-sync/ learning/notes/learn_deep/`
- [ ] 1.4.2 `git commit -m "feat(sync): Learn Deep 首轮真同步"`
- [ ] 1.4.3 `git push origin project`
- [ ] 1.4.4 归档到 `openspec/changes/archive/2026-07-10-knowledge-hub-first-sync/`
