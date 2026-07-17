# S25 — Tasks（Taro Deps Dedup）

## 1.1 调研

- [x] 1.1.1 已查：taro-template 含基础 Taro + React 18 devDeps
- [x] 1.1.2 已查：两子项目 deps 重复

## 1.2 实施

- [ ] 1.2.1 移除 knowledge_hub/package.json 中重复 devDeps
- [ ] 1.2.2 移除 games-mini-program/package.json 中重复 devDeps

## 1.3 验证 V1-V3

- [ ] 1.3.1 V1: `npm ls` 显示模板被引用
- [ ] 1.3.2 V2: knowledge_hub devDeps 减至 ~10 个
- [ ] 1.3.3 V3: games-mini-program devDeps 减至 ~8 个

## 1.4 提交 + 归档

- [ ] 1.4.1 `git add -A openspec/changes/taro-dedup-subprojects/ engineering/apps/web/`
- [ ] 1.4.2 `git commit -m "chore(taro-template): 子项目去重"`
- [ ] 1.4.3 `git push origin project`
- [ ] 1.4.4 归档到 `openspec/changes/archive/2026-07-10-taro-dedup-subprojects/`
