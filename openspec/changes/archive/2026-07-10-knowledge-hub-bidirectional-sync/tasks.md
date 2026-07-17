# P3 — Tasks（Knowledge_Hub Bidirectional Sync）

## 1.1 调研

- [x] 1.1.1 已查：S20+ sync-learn-deep.sh 已含 --reverse 选项但行为与 forward 同
- [x] 1.1.2 已查：S27 已 first sync 6 README.md

## 1.2 实施

- [ ] 1.2.1 加 sync state 文件 `.sync-state.json`
- [ ] 1.2.2 sync-learn-deep.sh 实现：
  - `--reverse`：真实反向 sync
  - `--both`：先 reverse 后 forward
  - `init-state`：初始化 sync state（记录 mtime 当前）
  - `status`：列出待 sync 文件
- [ ] 1.2.3 conflict detection：列出冲突文件
- [ ] 1.2.4 验证 V1-V3

## 1.3 验证 V1-V3

- [ ] 1.3.1 V1: --reverse 跑通
- [ ] 1.3.2 V2: init-state 创建 .sync-state.json
- [ ] 1.3.3 V3: status 输出非空

## 1.4 提交 + 归档

- [ ] 1.4.1 `git add -A openspec/changes/knowledge-hub-bidirectional-sync/ learning/scripts/ learning/notes/learn_deep/.sync-state.json`
- [ ] 1.4.2 `git commit -m "feat(sync): bidirectional data flow"`
- [ ] 1.4.3 `git push origin project`
- [ ] 1.4.4 归档到 `openspec/changes/archive/2026-07-10-knowledge-hub-bidirectional-sync/`
