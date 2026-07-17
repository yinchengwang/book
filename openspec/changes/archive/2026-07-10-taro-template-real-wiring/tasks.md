# S18 — Tasks（Taro-Template 子项目真实接入）

## 1.1 调研

- [x] 1.1.1 已查：`apps/web/taro-template/` 5 文件就绪
- [x] 1.1.2 已查：knowledge_hub 与 games-mini-program 都有完整 package.json

## 1.2 实施

- [ ] 1.2.1 新建 `apps/web/package.json` (npm workspaces)
- [ ] 1.2.2 修改 knowledge_hub/package.json：下放公共依赖 + devDep template
- [ ] 1.2.3 修改 knowledge_hub/babel.config.js
- [ ] 1.2.4 修改 knowledge_hub/config/index.ts
- [ ] 1.2.5 同上 for games-mini-program

## 1.3 验证 V1-V4

- [ ] 1.3.1 V1: `ls apps/web/` 含 package.json 顶层
- [ ] 1.3.2 V2: knowledge_hub `npm install` 装入 template
- [ ] 1.3.3 V3: knowledge_hub `npm run build:h5`
- [ ] 1.3.4 V4: games-mini-program `npm run build:h5`

## 1.4 提交 + 归档

- [ ] 1.4.1 `git add -A openspec/changes/taro-template-real-wiring/ engineering/apps/web/`
- [ ] 1.4.2 `git commit -m "feat(taro-template): 子项目真正接入 taro-template"`
- [ ] 1.4.3 `git push origin project`
- [ ] 1.4.4 归档到 `openspec/changes/archive/2026-07-10-taro-template-real-wiring/`
