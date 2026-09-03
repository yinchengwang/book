# Web 游戏平台设计：C 核心 + WASM/服务器双形态

- 日期：2026-09-03
- 状态：已确认
- 范围：重写贪吃蛇与 2048，前端 Web 游玩，游戏逻辑由 C 实现

## 1. 背景与目标

现有 `engineering/apps/games/` 下的贪吃蛇（`snake/`）和 2048（`2048/`）是 C 控制台程序，
逻辑与平台 IO（conio/termios/ANSI 渲染）耦合，无法在 Web 端复用，也无单元测试。

目标：**前端 Web 游玩，游戏逻辑由 C 实现**。核心约束：

- 游戏逻辑抽为纯 C 库（零 IO），同一份代码同时支撑 WASM 与网络服务器两种形态
- 核心库纳入 gtest 单元测试体系（工程轨规范）
- 旧控制台版保留不动，互不影响

## 2. 架构总览

```
engineering/apps/games/
├── core/                      ← 纯 C 游戏核心库（零 IO，可单测）
│   ├── snake_core.c/h        贪吃蛇逻辑：tick(dir) → 棋盘状态
│   ├── g2048_core.c/h        2048 逻辑：move(dir) → 棋盘状态
│   └── CMakeLists.txt        静态库 games_core
├── server/                    ← 形态2：C 游戏服务器（libmicrohttpd）
│   ├── main.c                HTTP 入口
│   └── session.c             会话管理（游戏实例表）
├── wasm/                      ← 形态1：Emscripten 编译胶水层
│   └── wasm_binding.c        导出给 JS 的 API
└── web/                       ← 前端 H5（原生 JS + Canvas，无框架依赖）
    ├── index.html            游戏大厅（选游戏）
    ├── snake.html / 2048.html
    └── js/                   渲染 + 输入 + 通信层
```

数据流：

```
浏览器（JS/Canvas 渲染 + 键盘输入）
   ├── LocalEngine  ──直接调用──> game.wasm（C 核心，Emscripten 编译）
   └── RemoteEngine ──HTTP──────> C 游戏服务器（libmicrohttpd + C 核心）
```

## 3. 核心库设计（games_core）

### 3.1 统一原语

每个游戏核心只暴露三类接口：

| 原语 | 2048 | 贪吃蛇 |
|------|------|--------|
| `create()` | 初始化 4×4 棋盘，生成初始方块 | 初始化蛇身/食物/难度 |
| `step(input)` | `move(dir)` 执行一次滑动合并 | `tick(dir)` 推进一帧 |
| `snapshot()` | 返回棋盘整数数组 + 分数 + 状态 | 返回蛇身坐标 + 食物 + 分数 + 状态 |

### 3.2 关键约束

- **零 IO**：核心库不含 printf/malloc 以外的任何平台调用，随机数通过注入种子或可替换 rand 钩子实现，保证单测可重现
- **状态自包含**：所有状态挂在游戏结构体上（参照现有 `struct Game2048` 风格），不使用全局变量（修复现有 snake.c 全局状态问题）
- **紧凑状态表示**：棋盘用整数数组表示，WASM 与服务器直接消费同一份内存布局，无需序列化转换层

### 3.3 逻辑来源

从现有控制台版移植：

- 2048：滑动合并、计分、`can_move` 终局判定、胜利/继续逻辑（现有 `2048.c` 已较干净，主要做状态收口）
- 贪吃蛇：方向反转禁止、撞墙/自咬判定、吃食物生长、加速曲线（现有 `snake.c` 需消除全部全局变量）

### 3.4 单元测试

新增 `engineering/test/games/`：

- 2048：单行合并规则（含 `[2,2,2,2]→[4,4,0,0]` 等边界）、计分、无效移动检测、终局判定
- 贪吃蛇：移动/生长/撞墙/自咬/方向反转拒绝、食物不生成在蛇身上

## 4. 形态 1：WASM

- Emscripten 编译 `games_core` + `wasm_binding.c` → `game.wasm`
- 导出 API：`snake_create / snake_tick / snake_snapshot / g2048_create / g2048_move / g2048_snapshot` 及对应 `destroy`
- 构建脚本：`scripts/build-games-wasm.sh`（需本机安装 emsdk，文档中注明）

## 5. 形态 2：C 游戏服务器

- 基于 `third_part/libmicrohttpd`，REST 接口：

| 方法 | 路径 | 说明 |
|------|------|------|
| POST | `/game/new?type=snake\|2048` | 创建游戏实例，返回 session id |
| POST | `/game/{id}/input` | 提交方向输入 |
| GET  | `/game/{id}/state` | 获取当前棋盘快照（JSON） |

- 单进程内存会话表（哈希表：id → 游戏实例），不引入持久化（YAGNI）
- 为后续多人对战/排行榜预留扩展点，但本期不实现

## 6. 前端（H5）

- 原生 JS + Canvas，无框架依赖，不引入 Taro（链路最简）
- 页面：`index.html` 游戏大厅 + 每游戏一个页面
- **通信层抽象**：`LocalEngine`（调 WASM）与 `RemoteEngine`（HTTP 轮询 C 服务器）实现同一接口，页面上层无感切换；默认 WASM 本地模式
- 贪吃蛇由 JS `requestAnimationFrame`/定时器驱动帧循环，按 speed 推进 tick

## 7. 实施顺序

1. 抽 C 核心库 + gtest 单测（先 2048，后贪吃蛇）
2. WASM 编译 + H5 前端跑通 2048
3. H5 接入贪吃蛇（加帧循环驱动）
4. C 服务器 + RemoteEngine（可选增强）

## 8. 验证方式

| 层 | 验证 |
|----|------|
| 核心库 | `ctest --test-dir build/engineering -R games` 跑 gtest |
| WASM/H5 | 浏览器打开页面实际游玩（移动/合并/计分/结束） |
| 服务器 | curl 调 REST 接口 + 前端 RemoteEngine 模式联调 |

## 9. 不做的事（YAGNI）

- 多人对战、排行榜、账号体系
- 游戏状态持久化
- Taro/小程序集成（后续如需，WASM 产物可直接复用）
- 数独等其他游戏的 Web 化
