# S15 — 设计文档（Games Mini-Program Inventory Confirm）

## 1. 当前架构

```
engineering/apps/
├── games/                   # C 版本（学习用）
│   ├── 2048/
│   ├── snake/
│   ├── sudoku/
│   ├── mini_program/       # Taro 项目（早期）
│   └── CMakeLists.txt
└── web/
    ├── games-mini-program/ # Taro 3.6 + React 18 + TypeScript（生产小程序）
    │   ├── src/pages/
    │   │   ├── index/
    │   │   ├── game2048/
    │   │   ├── snake/
    │   │   ├── sudoku/
    │   │   └── match3/    # Taro 独有
    │   └── package.json    # taro 3.6 + Vite 5
    └── knowledge_hub/       # 学习追踪平台（独立子项目）
```

## 2. 实施

### 2.1 新建 apps/games/README.md

```markdown
# Apps/Games

## C 版本（学习用）

`apps/games/{2048,snake,sudoku,mini_program}/`：纯 C 实现的命令行游戏。

- 目的：学习 C 语言 + 算法
- 编译：`cmake --build engineering/build --target {game2048,snake,sudoku}`
- 运行：相应 .exe

## Taro 双端版本（生产用）

`apps/web/games-mini-program/`：Taro 3.6 + React 18 + TypeScript，覆盖：

- 2048、Snake、Sudoku、Match3（新增）
- H5（Vite 5）+ WeChat WeApp 双端
- 构建：`cd engineering/apps/web/games-mini-program && npm run build:{h5,weapp}`

## 关系

- C 版本：单文件 ~100-300 行，便于学习算法与 C 编程
- Taro 版本：完整 UI + 游戏逻辑 + 双端部署
- 两个版本是平行的，非迁移关系（不互相更新）
```

## 3. 验证 V1-V4

| V | 命令 | 期望 |
|---|---|---|
| V1 | `ls engineering/apps/games/` | 含 4 个 C 游戏 |
| V2 | `ls engineering/apps/web/games-mini-program/src/pages/` | 含 5 个目录（含 match3） |
| V3 | 读两个 README | 关系清晰 |
| V4 | `git grep "knowledge_hub/"` 根 CLAUDE.md | 已修 |

## 4. 不做

- ❌ 重写任何游戏
- ❌ 改 Taro 项目结构
- ❌ 删除 C 版本（保留为学习材料）
