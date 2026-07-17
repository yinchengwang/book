# S20+ — Tasks（Learn Deep rsync 实现）

## 1.1 调研

- [x] 1.1.1 已查：S19 stub 已存在
- [x] 1.1.2 决策：用 rsync 工具而非自写算法

## 1.2 实施

- [ ] 1.2.1 重写 `learning/scripts/sync-learn-deep.sh`：用 rsync 实现
- [ ] 1.2.2 添加 `--dry-run` 参数显示将执行的操作
- [ ] 1.2.3 添加 `--reverse` 选项：knowledge_hub → learning 方向

## 1.3 验证 V1-V3

- [ ] 1.3.1 V1: `bash sync-learn-deep.sh --dry-run` 不改文件
- [ ] 1.3.2 V2: 路径正确解析学习层 + 知识库
- [ ] 1.3.3 V3: rsync 输出符合预期

## 1.4 提交 + 归档

- [ ] 1.4.1 `git add -A openspec/changes/learn-deep-rsync-impl/ learning/scripts/`
- [ ] 1.4.2 `git commit -m "feat(sync): Learn Deep 数据双向 rsync 实现"`
- [ ] 1.4.3 `git push origin project`
- [ ] 1.4.4 归档到 `openspec/changes/archive/2026-07-10-learn-deep-rsync-impl/`
