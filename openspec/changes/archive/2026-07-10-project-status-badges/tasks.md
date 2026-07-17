# S23 — Tasks（Project Status Badges）

## 1.1 调研

- [x] 1.1.1 已查：README 已有 3 个 badges

## 1.2 实施

- [ ] 1.2.1 README 增加 5 个新 status badges（size/commits/changes/coverage-precise/learning-tests）
- [ ] 1.2.2 创建 `engineering/scripts/metrics/update-badges.sh`：script 框架

## 1.3 验证 V1-V3

- [ ] 1.3.1 V1: README 渲染 5 个新 badge
- [ ] 1.3.2 V2: shield URLs 格式合法
- [ ] 1.3.3 V3: 不破坏现有徽章

## 1.4 提交 + 归档

- [ ] 1.4.1 `git add -A openspec/changes/project-status-badges/ README.md engineering/scripts/metrics/`
- [ ] 1.4.2 `git commit -m "docs(badges): project status 5 badge"`
- [ ] 1.4.3 `git push origin project`
- [ ] 1.4.4 归档到 `openspec/changes/archive/2026-07-10-project-status-badges/`
