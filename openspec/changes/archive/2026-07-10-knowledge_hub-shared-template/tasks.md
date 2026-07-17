# S16 — Tasks (Knowledge_Hub 共享模板抽取)

> **目标**：把 knowledge_hub 与 games-mini-program 的 Taro 公共配置抽到 `taro-template/`，建立 workspace 复用。

## 1.1 调研

- [x] 1.1.1 已查：knowledge_hub 与 games-mini-program 公共 Taro 3.6 + React 18 依赖
- [x] 1.1.2 已查：两者均 npm-based，Taro config 在 `config/index.ts`

## 1.2 创建 taro-template

- [ ] 1.2.1 创建 `engineering/apps/web/taro-template/package.json`（公共依赖）
- [ ] 1.2.2 创建 `engineering/apps/web/taro-template/babel.config.js`
- [ ] 1.2.3 创建 `engineering/apps/web/taro-template/tsconfig.json`
- [ ] 1.2.4 创建 `engineering/apps/web/taro-template/postcss.config.js` + `tailwind.config.ts`
- [ ] 1.2.5 创建 `engineering/apps/web/taro-template/config/index.base.ts`
- [ ] 1.2.6 创建 `engineering/apps/web/taro-template/README.md`

## 1.3 wire workspace

- [ ] 1.3.1 在 `engineering/apps/web/package.json`（新）引入 `workspaces`
- [ ] 1.3.2 把 knowledge_hub + games-mini-program + taro-template 列为 members

## 1.4 验证 V1-V4

- [ ] 1.4.1 V1: `ls engineering/apps/web/taro-template/` 8 个文件
- [ ] 1.4.2 V2: knowledge_hub npm install 通过
- [ ] 1.4.3 V3: knowledge_hub build:h5 通过
- [ ] 1.4.4 V4: games-mini-program build:h5 通过

## 1.5 提交 + 归档

- [ ] 1.5.1 `git add -A openspec/changes/knowledge_hub-shared-template/ engineering/apps/web/taro-template/`
- [ ] 1.5.2 `git commit -m "feat(taro-template): 抽取双 Taro 项目公共配置"`
- [ ] 1.5.3 `git push origin project`
- [ ] 1.5.4 归档到 `openspec/changes/archive/2026-07-10-knowledge_hub-shared-template/`
