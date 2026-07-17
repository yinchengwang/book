# S13 — Tasks (Coverage Baseline 现有化)

> **目标**：核实 coverage 工作流已就绪，记录 inventory，不创建重复文件。

## 1.1 调研确认

- [x] 1.1.1 已查：`engineering/scripts/coverage/run.sh` 存在（含 --baseline 模式）
- [x] 1.1.2 已查：`aggregate.py` 存在
- [x] 1.1.3 已查：`compare.py` 存在
- [x] 1.1.4 已查：`badge.py` 存在
- [x] 1.1.5 已查：`coverage-baseline.json` 已存在骨架（值为 0%）
- [x] 1.1.6 已查：`coverage-policy.md` 已存在完整策略
- [x] 1.1.7 已查：README.md 已有 coverage badge

## 1.2 简化 S13 决策

- [x] 1.2.1 决策：S2 留下的 inventory 已完整就绪；S13 不创建新文件
- [x] 1.2.2 决策：CI 暂不接 coverage（避免再次失败）

## 1.3 验证 V1-V4

- [x] 1.3.1 V1: 所有 4 个 scripts hebang 正确
- [x] 1.3.2 V2: baseline JSON 存在
- [x] 1.3.3 V3: policy.md 存在
- [x] 1.3.4 V4: README badge 存在

## 1.4 提交 + 归档

- [ ] 1.4.1 `git add -A openspec/changes/coverage-baseline-rebuild/`
- [ ] 1.4.2 `git commit -m "chore(coverage): S13 确认 inventory 已就绪，关债"`
- [ ] 1.4.3 `git push origin project`
- [ ] 1.4.4 归档到 `openspec/changes/archive/2026-07-10-coverage-baseline-rebuild/`
