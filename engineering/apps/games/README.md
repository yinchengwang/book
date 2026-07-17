# Apps/Games

## 概述

本目录包含两个并行的"游戏实现"双轨：

| 轨道 | 路径 | 目的 | 端 |
|---|---|---|---|
| C 版本（学习用） | `engineering/apps/games/{2048,snake,sudoku,mini_program}/` | 学习 C 语言 + 算法 + 数据结构 | 单文件命令行 |
| Taro 双端（生产用） | `engineering/apps/web/games-mini-program/` | Taro 3.6 + React 18 + TypeScript 小程序 + H5 | H5 + WeApp 双端 |

**两个版本是平行关系，不互相迁移**：
- C 版本用于 C/C++ 教学材料（与 `learning/` 轨道一致）
- Taro 版本用于生产小程序部署

## C 版本子目录

- `2048/` —— 2048 游戏
- `snake/` —— 贪吃蛇
- `sudoku/` —— 数独
- `mini_program/` —— 早期 Taro 实验版（已迁移到 web/games-mini-program）

构建：

```bash
cmake --build engineering/build --target game2048     # 2048 CLI
cmake --build engineering/build --target snake_game  # 贪吃蛇 CLI
cmake --build engineering/build --target sudoku      # 数独 CLI
```

## Taro 双端版本

`engineering/apps/web/games-mini-program/` 覆盖：

- `pages/index/` —— 首页
- `pages/game2048/` —— 2048
- `pages/snake/` —— 贪吃蛇
- `pages/sudoku/` —— 数独
- `pages/match3/` —— 消消乐（Taro 独有，无 C 版本）

构建：

```bash
cd engineering/apps/web/games-mini-program
npm install
npm run dev:h5       # H5 开发
npm run dev:weapp     # 微信小程序开发
npm run build:h5      # H5 生产
npm run build:weapp    # 微信小程序生产
```

## 维护

- C 版本：与 `learning/` 轨道一致，单文件 100-300 行，便于 clone 单文件学习
- Taro 版本：完整 UI + 业务逻辑 + 双端部署
- 未来工作：抽取共享 Taro 配置到 `engineering/apps/web/taro-template/`
