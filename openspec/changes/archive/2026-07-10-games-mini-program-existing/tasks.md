# S15 — Tasks (Games Mini-Program Inventory Confirm)

> **目标**：写好 apps/games/README.md 说明 C 与 Taro 双轨关系，更正根 CLAUDE.md 路径。

## 1.1 调研

- [x] 1.1.1 已查：`engineering/apps/web/games-mini-program/` 完整 Taro 项目（5 个游戏）
- [x] 1.1.2 已查：`engineering/apps/games/` C 版本（4 个游戏）
- [x] 1.1.3 已查：根 CLAUDE.md 中说 `knowledge_hub/` 顶级——实际是 `apps/web/knowledge_hub/`

## 1.2 写 README

- [ ] 1.2.1 创建 `engineering/apps/games/README.md`
- [ ] 1.2.2 创建 `engineering/apps/web/games-mini-program/README.md`（如缺失）

## 1.3 修根 CLAUDE.md

- [ ] 1.3.1 把 `knowledge_hub/` 路径更新为 `apps/web/knowledge_hub/`

## 1.4 验证 V1-V4

- [ ] 1.4.1 V1: `ls engineering/apps/games/` 含 4 个 C 游戏
- [ ] 1.4.2 V2: `ls engineering/apps/web/games-mini-program/src/pages/` 含 5 个目录
- [ ] 1.4.3 V3: apps/games/README.md 关系清晰
- [ ] 1.4.4 V4: 根 CLAUDE.md 路径已修

## 1.5 提交 + 归档

- [ ] 1.5.1 `git add -A openspec/changes/games-mini-program-existing/ engineering/apps/games/README.md engineering/apps/web/games-mini-program/README.md CLAUDE.md`
- [ ] 1.5.2 `git commit -m "docs(games): README 说明 C 与 Taro 双轨 + 修根 CLAUDE.md 路径"`
- [ ] 1.5.3 `git push origin project`
- [ ] 1.5.4 归档到 `openspec/changes/archive/2026-07-10-games-mini-program-existing/`
