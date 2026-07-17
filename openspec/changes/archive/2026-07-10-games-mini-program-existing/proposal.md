# S15 — Games Mini-Program Inventory Confirm（双端迁移已完成）

## What Changes

调研发现 `apps/games/` 的 **Taro/React 双端迁移早已完成**——`engineering/apps/web/games-mini-program/` 是完整 Taro 3.6 + React 18 + TypeScript 项目，覆盖 5 个游戏：

| 游戏 | C 版本 | Taro 小程序版本 |
|---|---|---|
| 2048 | ✅ engineering/apps/games/2048/ | ✅ games-mini-program/src/pages/game2048/ |
| Snake | ✅ engineering/apps/games/snake/ | ✅ games-mini-program/src/pages/snake/ |
| Sudoku | ✅ engineering/apps/games/sudoku/ | ✅ games-mini-program/src/pages/sudoku/ |
| Match3 | ❌ 无 C 版本 | ✅ games-mini-program/src/pages/match3/（Taro 独有） |
| Index | — | ✅ games-mini-program/src/pages/index/ |

**双端支持**：
- H5：`npm run dev:h5` / `npm run build:h5` (Vite 5)
- WeApp：`npm run dev:weapp` / `npm run build:weapp`

**变更内容**：

1. **不改代码**：`engineering/apps/web/games-mini-program/` 已成熟完整
2. **新建 `apps/games/README.md`**：明确说明 C 版本（学习用）+ Taro 版本（生产小程序双端）的关系
3. **新建 `apps/games-mini-program/README.md`**：补全构建说明（如果缺失）
4. **更新根 CLAUDE.md**：澄清 `apps/web/games-mini-program` 已存在，旧的 "knowledge_hub 模式"参考可继续用

## Why

**α 价值**：
- Taro/React 双端迁移**已存在**——不应再次造轮子
- 把 "我们已有什么" 写进 README 让新 contributor 少走弯路

**β 价值**：
- 学习层（ds-c/orig/、algo-c/、code-solutions/）副产品——Taro 游戏作为 学习材料 的入口，连接学习者

**前置依赖**：
- S1 双轨架构：engineering 层有 apps/ + apps/web/ 分类

## Scope

**包含**：
- 1 个 apps/games/README.md（解释双轨）
- 1 个 apps/games-mini-program/README.md 校验或补全
- 更新根 CLAUDE.md 中 web 部分（纠正知识库路径）

**不包含**：
- ❌ 重写任何游戏
- ❌ 改 Taro 项目结构
- ❌ 引入新依赖（game-center 已有依赖充足）

## Risk

| 风险 | 概率 | 缓解 |
|---|---|---|
| games-mini-program/node_modules 巨大 | 低 | 不动；只更新 README |
| 双 README 维护成本 | 低 | 工程层主 README + 子 README 分离 |
