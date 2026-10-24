# 游戏中心 · games

四款经典小游戏的**原生微信小程序**实现：贪吃蛇 / 2048 / 数独 / 开心消消乐。

- **无框架**：纯原生小程序（`.wxml` / `.wxss` / `.js` / `.json`），不依赖 Taro / React / Vue
- **无构建**：直接用微信开发者工具打开本目录即可运行，无需 `npm install`
- **全离线**：所有数据存 `wx.storage`，无任何网络请求

---

## 快速开始

用**微信开发者工具**导入本目录即可（appid 见 `project.config.json`）。

```bash
# 跑引擎逻辑测试（Node 环境，无需小程序环境）
node tests/engine.test.js
```

---

## 项目结构

```
games/
├── app.js / app.json / app.wxss       # 入口（onLaunch 中执行存储迁移）
├── project.config.json                # 开发者工具配置（appid wxb4f3dc5da6d63cf2）
├── pages/
│   ├── index/                         # 首页：四个入口 + 成就墙 + 进度汇总
│   ├── snake/                         # 贪吃蛇（canvas + 触摸滑动）
│   ├── game2048/                      # 2048（canvas + 滑动/方向键）
│   ├── sudoku/                        # 数独（章节选择 → 9x9 棋盘 → 数字键盘）
│   └── match3/                        # 消消乐（章节选择 → 拖拽交换 → 连锁消除）
├── utils/
│   ├── game2048.js                    # 2048 引擎（纯逻辑）
│   ├── snake.js                       # 贪吃蛇引擎
│   ├── sudoku.js                      # 数独引擎（回溯求解 + 唯一解挖洞）
│   ├── match3.js                      # 消消乐引擎（特效 / 障碍物 / 章节）
│   ├── storage.js                     # wx.storage 持久化 + 版本迁移
│   ├── achievements.js                # 成就规则引擎（事件 → 规则 → 解锁）
│   ├── share.js                       # 分享文案
│   └── vibrate.js                     # 振动反馈（带节流）
└── tests/engine.test.js               # 51 项引擎回归测试
```

---

## 四个游戏

| 游戏 | 玩法 | 特色 |
|---|---|---|
| **贪吃蛇** | 滑动或方向键控制，吃食物 +10 分并加速 | 最高帧率 50ms/帧，逐帧加速 |
| **2048** | 滑动或方向键移动，合并同类数字 | 达成 2048 可选择「继续挑战」 |
| **数独** | 点格子 + 数字键盘填入 | 5 章 × 10 关，**回溯求解保证唯一解**，3 档难度 |
| **消消乐** | 点两个相邻宝石交换 | 6 章 50 关，直线/爆炸/彩虹特效，冰块/蜘蛛网/石头障碍物 |

### 消消乐系统

- **特效**：4 连生成直线 → 5 连生成彩虹 → 交叉生成炸弹
- **障碍物**：冰块（1 血）/ 蜘蛛网（2 血）/ 石头（3 血，不可被直线特效清除）
- **特殊元素**：🔑 钥匙 / 📦 宝箱 / 🐿️ 松鼠 / 🐦 发条鸟 / 🐌 蜗牛 / 🪙 金币
- **评级**：按剩余步数比例给 1~3 星；星数不足则章节不解锁

---

## 数据存储

所有数据以 `games_` 为前缀存于 `wx.storage`：

| Key | 内容 |
|---|---|
| `games_snake` | `{ bestScore }` |
| `games_game2048` | `{ bestScore }` |
| `games_sudoku` | `{ bestScore, chapters: [{ id, unlocked, stars }] }` |
| `games_match3` | `{ chapters: { c1: { unlocked, stars } } }` |
| `games_achievements` | `{ unlocked: [{ id, unlockedAt }], progress: {} }` |
| `games_version` | 数据版本号，用于迁移 |

首页提供「清空全部游戏数据」入口（二次确认）。

---

## 成就系统（8 项）

| ID | 名称 | 条件 |
|---|---|---|
| `snake_first_blood` | 初尝血腥 | 贪吃蛇首次得分 ≥ 10 |
| `snake_score_100` | 贪吃大师 | 贪吃蛇单局 ≥ 100 分 |
| `sudoku_first_clear` | 初次通关 | 完成任意一局数独 |
| `sudoku_no_hint` | 自有洞察 | 不用提示完成数独 |
| `game2048_reach_2048` | 2048 达成 | 合成出 2048 |
| `game2048_reach_4096` | 数值狂人 | 合成出 4096 |
| `match3_first_clear` | 初次通关 | 完成任意一关消消乐 |
| `match3_three_stars` | 完美三星 | 消消乐拿到三星 |

成就解锁通过 `subscribe()` 广播，各页面自行订阅并弹 Toast。

---

## 修复的三个原生 bug

重写过程中，`tests/engine.test.js` 抓出了原实现中三个会导致游戏不可玩的问题：

1. **贪吃蛇开局即死**（`utils/snake.js`）
   初始蛇身按「头在中心、往左排」构造，但取头用 `snake[length-1]`（实际取到最左节）。朝右走第一帧就撞上自己的第二节，**每次开局直接 game over**。
   修复：蛇身改为「尾 → 头」排列，蛇头在最右，向右前进时身后无阻。

2. **`wouldMatch` 首行崩溃**（`utils/match3.js`）
   生成棋盘首行时调用 `wouldMatch`，循环条件里的 `board[row][c] &&` 短路不成立——`board[row][c]` 尚未初始化即被解引用，**建棋盘直接抛 `TypeError`**。
   修复：改用逐格 `if (!cell || ...) break` 提前退出。

3. **`gem: 0` 被当成空格**（`utils/match3.js`）
   `isValidSwap` 用 `if (!board[r1][c1].gem) return false` 判空。但宝石类型 `0`（🟡 黄色）是**合法值**，导致**黄色宝石永远无法参与交换**。
   修复：改用 `=== null` 严格判空（与文件内其余 10 处保持一致）。

---

## 移植说明

原实现位于 `engine/apps/web/games-mini-program/`（Taro 3.6 + React 18 + TypeScript），已于 2026-10-22 完成原生化重写并删除。此处为按同等功能重写的原生实现，**并修复了上述三个原生 bug**。

对应关系：

| 原文件 | 新文件 |
|---|---|
| `src/utils/game2048.ts` | `utils/game2048.js` |
| `src/utils/snake.ts` | `utils/snake.js` |
| `src/utils/sudoku.ts` | `utils/sudoku.js` |
| `src/utils/match3.ts` | `utils/match3.js` |
| `src/utils/storage.ts` | `utils/storage.js` |
| `src/services/achievements.ts` | `utils/achievements.js` |
| `src/services/share.ts` | `utils/share.js` |
| `src/hooks/useVibration.ts` | `utils/vibrate.js` |
| `src/pages/*/index.tsx` | `pages/*/index.{js,wxml,wxss,json}` |

游戏核心算法最早来自 `engine/apps/games/` 的 C 实现，PC 端与小程序端共用同一套规则。