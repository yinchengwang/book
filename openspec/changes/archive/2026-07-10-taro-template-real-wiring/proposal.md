# S18 — Taro-Template 子项目真实接入（CANCELED for S18')

## What Changes

S18 试图让 knowledge_hub + games-mini-program 真实接入 taro-template（npm workspaces + 引用公共依赖）。**实施中遇到实际工程复杂度超出 S18 范围**：

1. npm workspaces 在 Windows + GitHub Actions 路径不同
2. 模板的 `peerDependencies` 模式对 `dependencies` 的 drop 复杂
3. 两子项目当前都有 `node_modules` 重依赖，迁移需要 npm install 重做

**S18 决策**：简化范围——**仅做 npm workspaces 顶层**（用 `workspace:*` 但不删除子项目重复 deps），让 taro-template 通过 npm workspaces 链接。未来工作 S19+ 阶段逐项迁移公共依赖。

**变更内容**（简化版）：

1. 新建 `apps/web/package.json` 顶层，声明 npm workspaces
2. 在 knowledge_hub 和 games-mini-program 添加 `devDependencies: { "@book/taro-template": "*" }`
3. 不删除子项目重复 deps（保留双轨独立运行）

## Why

**α 价值**：
- npm workspaces 顶层声明让 `apps/web/` 变成单一 workspace
- 子项目可以 `import "@book/taro-template/babel.config"` —— 即使依赖未瘦

**前置依赖**：S16 已创建 taro-template

## Scope

**包含**：
- 新建 `apps/web/package.json`（workspaces）
- knowledge_hub/devDep + games-mini-program/devDep 引用模板
- V1-V4 验证

**不包含**：
- ❌ 实际删除重复依赖（保留双轨独立）
- ❌ 改 babel.config.js / config/index.ts 引入 import（避免多项目重构）

## Risk

| 风险 | 概率 | 缓解 |
|---|---|---|
| windows symlink | 中 | npm 不需要 symlink |
| 实际收益低 | 中 | S18 留作最低风险接入 |
