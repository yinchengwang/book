# Apps/Games

## 概述

本目录包含两个并行的"游戏实现"双轨：

| 轨道 | 路径 | 目的 | 端 |
|---|---|---|---|
| C 版本（学习用） | `engineering/apps/games/{2048,snake,sudoku,mini_program}/` | 学习 C 语言 + 算法 + 数据结构 | 单文件命令行 |
| 原生小程序（生产用） | `wechat-miniprogram/games/` | 原生微信小程序，零依赖零构建 | WeApp |

**两个版本是平行关系，不互相迁移**：
- C 版本用于 C/C++ 教学材料（与 `learning/` 轨道一致）
- 原生小程序版本用于生产小程序部署

> 原 `engineering/apps/web/games-mini-program/`（Taro 3.6 + React 18 双端）已于 2026-10-22 完成原生化重写并删除。

## C 版本子目录

- `2048/` —— 2048 游戏
- `snake/` —— 贪吃蛇
- `sudoku/` —— 数独
- `mini_program/` —— 游戏中心 C 入口

构建：

```bash
cmake --build engineering/build --target game2048     # 2048 CLI
cmake --build engineering/build --target snake_game  # 贪吃蛇 CLI
cmake --build engineering/build --target sudoku      # 数独 CLI
```

## 原生小程序版本

`wechat-miniprogram/games/` 覆盖：

- `pages/index/` —— 首页
- `pages/game2048/` —— 2048
- `pages/snake/` —— 贪吃蛇
- `pages/sudoku/` —— 数独
- `pages/match3/` —— 消消乐

运行：微信开发者工具直接导入 `wechat-miniprogram/games/` 即可（无构建步骤）。
自测：`node wechat-miniprogram/games/tests/engine.test.js`

## 维护

- C 版本：与 `learning/` 轨道一致，单文件 100-300 行，便于 clone 单文件学习
- 原生小程序版本：完整 UI + 游戏逻辑，图形全用 Canvas / CSS 绘制，无素材依赖
