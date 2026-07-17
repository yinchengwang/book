# S16 — Knowledge_Hub 共享模板抽取

## What Changes

`engineering/apps/web/knowledge_hub/` 是 Taro + React H5/小程序双端学习追踪平台。**当前状态**：完整可运行，但是与 `engineering/apps/web/games-mini-program/` **共享 Taro 配置大量重叠**（webpack/vite/postcss/tailwind）。

S16 抽取 **Taro 共享模板** 到 `engineering/apps/web/taro-template/`，让知识库和小程序都基于此模板初始化。

**变更内容**：

1. **新建 `engineering/apps/web/taro-template/`**：
   - `package.json`（合并依赖：Taro 3.6 + React 18 + Vite 5 + TailwindCSS 通用）
   - `babel.config.js`
   - `config/index.ts`（Taro 构建配置共享）
   - `vite.config.ts`（Vite H5 配置共享）
   - `postcss.config.js`
   - `tailwind.config.ts`
   - `tsconfig.json`
   - `src/app.scss`（通用 reset）
   
2. **knowledge_hub 与 games-mini-program 添加 `extends: ./taro-template`** 引用

3. **验证 V1-V4**：双项目仍能 build（dev:h5 + build:h5 + build:weapp）

## Why

**α 价值（工程作品集）**：
- 两个 Taro 项目维护成本降低（一份 config 两份复用）
- 让"知识库"和"游戏小程序"互为参考实现

**β 价值（学习日志）**：
- 知识库和小程序作为 Taro 学习的典型例子，新贡献者基于模板快速上手

**前置依赖**：
- knowledge_hub 与 games-mini-program 都已成熟运行

## Scope

**包含**：
- 新建 taro-template 目录（含 7 个文件）
- knowledge_hub 与 games-mini-program 各改 package.json + config 引用模板
- V1-V4 验证：双项目仍能 build

**不包含**：
- ❌ 升级 Taro 版本（保留 3.6）
- ❌ 重写 game 业务代码
- ❌ 删除现有重复

## Risk

| 风险 | 概率 | 缓解 |
|---|---|---|
| Taro 模板抽取后双项目 build 失败 | 高 | V3 验证；保留现有 config 作为 backup |
| Tailwind 主题冲突 | 中 | 抽到模板需保留各自自定义 |
| 升级维护成本转嫁 | 低 | 模板仅 owner 维护 |
