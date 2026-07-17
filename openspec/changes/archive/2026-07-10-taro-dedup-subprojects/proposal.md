# S25 — Taro Subprojects Deps Deduplication

## What Changes

S18 创建了 npm workspace，但子项目 deps **未真正去重**。两个子项目仍各自维护完整 Taro 依赖：

- `knowledge_hub/package.json` 18 devDeps
- `games-mini-program/package.json` 14 devDeps

S25 把子项目 deps 中**已经下放到 `taro-template/`** 的项移除：

1. **`knowledge_hub/devDeps`**：删除 `@tarojs/cli`, `@tarojs/webpack-runner`, `babel-preset-taro`, `@types/node`, `@types/react`, `eslint`, `eslint-config-taro`, `eslint-plugin-react`, `typescript` 等（已在 taro-template）
2. **`games-mini-program/devDeps`**：同上
3. **保留**子项目特有 deps（@dnd-kit、framer-motion、@playwright/test 等）

## Why

**α 价值**：升级 Taro/React 时只更新一处

**前置依赖**：
- S18 npm workspaces 顶层 OK
- S16 taro-template 已有完整 peerDependencies

## Scope

**包含**：
- 子项目 package.json dep 移除
- V1-V3 验证：双项目 install + build:h5

**不包含**：
- ❌ 实际 npm install（依赖用户环境）
- ❌ 重写任何代码
- ❌ 改 Taro 版本
