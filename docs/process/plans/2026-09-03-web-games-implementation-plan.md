# Web 游戏平台实施计划：C 核心 + WASM/服务器双形态

> **Agent 使用说明：** REQUIRED SUB-SKILL: `superpowers:subagent-driven-development`（推荐）或 `superpowers:executing-plans` 逐步执行。步骤使用复选框（`- [ ]`）语法追踪。

**目标：** 贪吃蛇与 2048 重写为 C 核心库 + Web 前端双形态

**架构：** 纯 C 游戏核心（零 IO、状态自包含）→ Emscripten 编译 WASM + C HTTP 服务器；前端原生 JS + Canvas，LocalEngine/RemoteEngine 双通信后端

**技术栈：** CMake 3.20+、C11、C++17、gtest（vendored）、Emscripten、libmicrohttpd（可选）

---

## 全局约束

- 所有对话/注释/Commit Message 使用简体中文
- 编译产物输出到 `build/engineering/`（不使用源码目录）
- 测试产物输出到 `test-results/engineering/games/`
- 旧控制台版（`snake/`、`2048/`）保留不动，互不影响
- 单元测试框架：gtest（googletest vendored in `third_part/`）

---

## 文件结构

```
engineering/
├── apps/games/
│   ├── core/                       ← 新建：C 游戏核心库
│   │   ├── g2048_core.c/h         2048 核心逻辑
│   │   ├── snake_core.c/h         贪吃蛇核心逻辑
│   │   └── CMakeLists.txt         games_core 静态库
│   ├── wasm/                       ← 新建：Emscripten 胶水层
│   │   ├── wasm_binding.c
│   │   └── CMakeLists.txt
│   └── server/                     ← 新建：C HTTP 服务器（可选，本期不实现）
│       ├── main.c
│       ├── session.c
│       └── CMakeLists.txt
│   ├── web/                        ← 新建：前端 H5
│   │   ├── index.html             游戏大厅
│   │   ├── 2048.html              2048 游戏页
│   │   ├── snake.html             贪吃蛇游戏页
│   │   ├── css/
│   │   └── js/
│   │       ├── engine.js          通信层抽象（LocalEngine + RemoteEngine）
│   │       ├── g2048.js           2048 渲染 + 输入
│   │       └── snake.js           贪吃蛇渲染 + 输入 + 帧循环
│   └── games.html                 ← 新建：总入口页面
└── test/games/                    ← 新建：单元测试
    ├── CMakeLists.txt
    ├── g2048_core_test.cpp
    └── snake_core_test.cpp
```

---

## 阶段 1：2048 C 核心库

### Task 1: 创建 games_core 目录骨架与 CMake 配置

**文件：**
- 创建：`engineering/apps/games/core/CMakeLists.txt`
- 创建：`engineering/apps/games/core/g2048_core.h`（接口声明）
- 创建：`engineering/apps/games/core/g2048_core.c`（存根）

**接口声明（g2048_core.h）：**

```c
#ifndef G2048_CORE_H
#define G2048_CORE_H

#include <stdbool.h>

#define G2048_SIZE 4
#define G2048_WIN  2048

/* 方向 */
typedef enum { G2048_UP, G2048_DOWN, G2048_LEFT, G2048_RIGHT } G2048Dir;

/* 游戏状态 */
typedef struct {
    int board[G2048_SIZE][G2048_SIZE];  /* 0 = 空 */
    int score;
    bool moved;      /* 本次 move 是否有效 */
    bool game_over;  /* 无可用移动 */
    bool won;        /* 已达成 2048 */
    bool keep_going; /* 达成后继续 */
} G2048Game;

/* —— 核心 API —— */
void  g2048_create(G2048Game *g, int seed);
void  g2048_move(G2048Game *g, G2048Dir dir);
bool  g2048_can_move(const G2048Game *g);
bool  g2048_has_won(const G2048Game *g);
int   g2048_tile_at(const G2048Game *g, int row, int col);  /* 供前端查询棋盘 */
void  g2048_snapshot(const G2048Game *g, int out_board[G2048_SIZE][G2048_SIZE],
                     int *out_score, bool *out_game_over, bool *out_won);
/* out_score/out_game_over/out_won 可为 NULL 表示不关心 */
#endif
```

- 存根实现：所有函数空实现或返回默认值，`g2048_create` 置空棋盘
- CMake：`add_project_library(games_core GAMES_CORE_SOURCES_VAR)`，产物进 `build/engineering/lib/libgames_core.a`

**步骤：**

- [ ] 创建目录 `engineering/apps/games/core/`
- [ ] 创建 `g2048_core.h` 声明上述接口
- [ ] 创建 `g2048_core.c` 存根实现（所有函数空返回）
- [ ] 创建 `core/CMakeLists.txt`：使用 `add_project_library(games_core GAMES_CORE_SOURCES_VAR)`
- [ ] 验证编译：`cmake --build build/engineering --target games_core`
- [ ] 提交：`core: 创建 games_core 存根库骨架`

---

### Task 2: 实现 2048 核心逻辑（从控制台版移植）

**文件：**
- 修改：`engineering/apps/games/core/g2048_core.c`（核心实现）

**逻辑来源：** 现有 `2048.c` 的 `slide_row`（单行压缩+合并）、四方向旋转/翻转变换、`can_move` 终局判定、`spawn_tile`（随机生成 2/4）

**新增接口实现：**

- `g2048_create`：用 `srand(seed)` 初始化，`spawn_tile` 生成初始方块（默认 2 个）
- `g2048_move`：四方向分支，内部复用 `slide_row` + 旋转/翻转
- `g2048_can_move`：有空格或相邻可合并
- `g2048_has_won`：棋盘含 >= 2048
- `g2048_snapshot`：复制棋盘/分数/状态到输出参数
- `g2048_tile_at`：返回指定格值（供前端逐格渲染）
- **消除全局变量**：所有状态通过 `G2048Game*` 参数传递（修复现有 2048.c 中 `prev_board`/`first_draw` 全局问题）
- **消除 IO**：无 `printf`，随机数通过 `seed` 参数注入（测试可重现）

**步骤：**

- [ ] 实现 `g2048_create`：棋盘置零、srand(seed)、spawn_tile(initial_tiles=2)
- [ ] 实现 `spawn_tile`（内部 static）：收集空格坐标、随机选位、90%生成2/10%生成4
- [ ] 实现 `slide_row`：单行压缩→合并→再压缩，返回合并得分
- [ ] 实现四方向移动（复用旋转/翻转 + slide_row）
- [ ] 实现 `g2048_can_move`、`g2048_has_won`
- [ ] 实现 `g2048_snapshot`、`g2048_tile_at`
- [ ] 验证编译：`cmake --build build/engineering --target games_core`
- [ ] 提交：`g2048_core: 实现核心逻辑，消除全局状态与 IO`

---

### Task 3: 2048 单元测试

**文件：**
- 创建：`engineering/test/games/CMakeLists.txt`
- 创建：`engineering/test/games/g2048_core_test.cpp`

**测试用例（参考 binary_search_gtest.cpp 格式）：**

```cpp
extern "C" {
#include <games/g2048_core.h>
}

namespace {

TEST(G2048CoreTest, CreateInitializesEmptyBoard) {
    G2048Game g;
    g2048_create(&g, 42);  // 固定种子保证可重现
    // 棋盘非空（spawn_tile 生成了方块）
    int non_zero = 0;
    for (int r = 0; r < G2048_SIZE; r++)
        for (int c = 0; c < G2048_SIZE; c++)
            if (g.board[r][c] != 0) non_zero++;
    EXPECT_GE(non_zero, 1);
}

TEST(G2048CoreTest, SlideRowMergesTwoIdentical) {
    G2048Game g;
    g2048_create(&g, 0);
    // 手设一行：[2,2,0,0] 向左应得 [4,0,0,0]
    g.board[0][0] = 2; g.board[0][1] = 2; g.board[0][2] = 0; g.board[0][3] = 0;
    g2048_move(&g, G2048_LEFT);
    EXPECT_EQ(g.board[0][0], 4);
    EXPECT_EQ(g.board[0][1], 0);
    EXPECT_TRUE(g.moved);
}

TEST(G2048CoreTest, SlideRowWithThreeSame) {
    G2048Game g;
    memset(&g, 0, sizeof(g));
    g.board[0][0] = 2; g.board[0][1] = 2; g.board[0][2] = 2; g.board[0][3] = 2;
    g2048_move(&g, G2048_LEFT);
    EXPECT_EQ(g.board[0][0], 4);  // 前两个合并
    EXPECT_EQ(g.board[0][1], 4);  // 后两个合并
    EXPECT_EQ(g.board[0][2], 0);
    EXPECT_EQ(g.board[0][3], 0);
}

TEST(G2048CoreTest, SlideRowNoChangeOnZeros) {
    G2048Game g;
    memset(&g, 0, sizeof(g));
    g.board[0][0] = 4; g.board[0][1] = 8; g.board[0][2] = 0; g.board[0][3] = 0;
    g2048_move(&g, G2048_LEFT);
    EXPECT_EQ(g.board[0][0], 4);
    EXPECT_EQ(g.board[0][1], 8);
    EXPECT_FALSE(g.moved);  // 无变化
}

TEST(G2048CoreTest, MoveUpRotatesAndSlides) {
    G2048Game g;
    memset(&g, 0, sizeof(g));
    g.board[0][0] = 2; g.board[1][0] = 2; g.board[2][0] = 0; g.board[3][0] = 0;
    g2048_move(&g, G2048_UP);
    EXPECT_EQ(g.board[0][0], 4);
    EXPECT_EQ(g.board[1][0], 0);
}

TEST(G2048CoreTest, CannotMoveWhenFullAndNoAdjacent) {
    G2048Game g;
    memset(&g, 0, sizeof(g));
    g.board[0][0] = 2;  g.board[0][1] = 4;  g.board[0][2] = 2;  g.board[0][3] = 4;
    g.board[1][0] = 4;  g.board[1][1] = 2;  g.board[1][2] = 4;  g.board[1][3] = 2;
    g.board[2][0] = 2;  g.board[2][1] = 4;  g.board[2][2] = 2;  g.board[2][3] = 4;
    g.board[3][0] = 4;  g.board[3][1] = 2;  g.board[3][2] = 4;  g.board[3][3] = 2;
    EXPECT_FALSE(g2048_can_move(&g));
}

TEST(G2048CoreTest, HasWonTrueWhen2048Present) {
    G2048Game g;
    memset(&g, 0, sizeof(g));
    g.board[2][2] = 2048;
    EXPECT_TRUE(g2048_has_won(&g));
}

TEST(G2048CoreTest, SnapshotCopiesAllFields) {
    G2048Game g;
    g2048_create(&g, 123);
    int board[G2048_SIZE][G2048_SIZE];
    int score;
    bool game_over, won;
    g2048_snapshot(&g, board, &score, &game_over, &won);
    for (int r = 0; r < G2048_SIZE; r++)
        for (int c = 0; c < G2048_SIZE; c++)
            EXPECT_EQ(board[r][c], g.board[r][c]);
    EXPECT_EQ(score, g.score);
    EXPECT_EQ(game_over, g.game_over);
    EXPECT_EQ(won, g.won);
}

}  // namespace
```

**CMakeLists.txt：**
```cmake
add_project_test(games_g2048_core_test GAMES_G2048_CORE_TEST_SOURCES_VAR
    games_core)
```

**步骤：**

- [ ] 创建 `test/games/CMakeLists.txt`：引用父级 test/CMakeLists.txt 模式
- [ ] 编写上述 8 个测试用例（参考 binary_search_gtest.cpp 的 `extern "C"` 格式）
- [ ] 运行单测：`ctest --test-dir build/engineering -R games_g2048_core_test --output-on-failure`
- [ ] 预期：全部 PASS
- [ ] 提交：`test(games): g2048_core 单元测试 8 个用例`

---

## 阶段 2：贪吃蛇 C 核心库

### Task 4: 贪吃蛇核心逻辑实现

**文件：**
- 创建：`engineering/apps/games/core/snake_core.h`
- 创建：`engineering/apps/games/core/snake_core.c`
- 修改：`engineering/apps/games/core/CMakeLists.txt`（加入新源文件）

**接口声明（snake_core.h）：**

```c
#ifndef SNAKE_CORE_H
#define SNAKE_CORE_H

#include <stdbool.h>

#define SNAKE_MAX_LEN 200
#define SNAKE_WIDTH   20
#define SNAKE_HEIGHT  20

typedef enum { SNAKE_UP, SNAKE_DOWN, SNAKE_LEFT, SNAKE_RIGHT } SnakeDir;

typedef struct {
    int x, y;
} SnakePoint;

typedef struct {
    SnakePoint body[SNAKE_MAX_LEN];  /* 0 = 头 */
    int        len;
    SnakePoint food;
    int        score;
    int        speed;        /* ms/帧 */
    bool       game_over;
    SnakeDir   dir;          /* 当前方向 */
    SnakeDir   next_dir;     /* 下一帧方向（输入缓冲） */
} SnakeGame;

/* —— 核心 API —— */
void snake_create(SnakeGame *g, int seed, int difficulty);  /* difficulty: 0=简单 1=困难 2=地狱 */
void snake_tick(SnakeGame *g);  /* 推进一帧：应用 next_dir、移动、吃食/死亡判定 */
void snake_input(SnakeGame *g, SnakeDir dir);  /* 设置下一帧方向（反转方向拒绝） */
bool snake_is_over(const SnakeGame *g);
int  snake_score(const SnakeGame *g);
int  snake_len(const SnakeGame *g);
int  snake_food_x(const SnakeGame *g);
int  snake_food_y(const SnakeGame *g);
int  snake_body_x(const SnakeGame *g, int i);  /* i=0 头 */
int  snake_body_y(const SnakeGame *g, int i);
#endif
```

**逻辑来源：** 现有 `snake.c` 的 `init_game`、`update`、`spawn_food`、`update_speed`，需消除全部全局变量

**关键实现要点：**
- `snake_tick`：先检查方向反转，再移动头、判断撞墙/自咬、吃食物生长、生成新食物
- `snake_input`：反转方向拒绝（`DIR_UP` vs `DIR_DOWN` 等）
- `snake_create`：随机初始化蛇身、`spawn_food` 生成食物、`srand(seed)` 注入
- 加速曲线：`speed` 随 `score` 递减（每吃 5 分加速），难度决定初始速度
- **消除全局变量**：所有状态挂在 `SnakeGame*` 上，无任何 `extern`

**步骤：**

- [ ] 创建 `snake_core.h` 声明上述接口
- [ ] 创建 `snake_core.c`：实现全部函数，移植 snake.c 逻辑，消除全局变量
- [ ] 验证编译：`cmake --build build/engineering --target games_core`
- [ ] 提交：`snake_core: 实现核心逻辑，消除全局状态`

---

### Task 5: 贪吃蛇单元测试

**文件：**
- 创建：`engineering/test/games/snake_core_test.cpp`

**测试用例：**

```cpp
extern "C" {
#include <games/snake_core.h>
}

namespace {

TEST(SnakeCoreTest, CreateInitializesSnake) {
    SnakeGame g;
    snake_create(&g, 42, 0);  // 固定种子
    EXPECT_GE(g.len, 3);
    EXPECT_TRUE(!snake_is_over(&g));
    EXPECT_EQ(snake_score(&g), 0);
}

TEST(SnakeCoreTest, MoveUpIncreasesY) {
    SnakeGame g;
    snake_create(&g, 0, 0);
    int head_y_before = g.body[0].y;
    snake_input(&g, SNAKE_UP);
    snake_tick(&g);
    EXPECT_EQ(g.body[0].y, head_y_before - 1);
}

TEST(SnakeCoreTest, ReverseDirectionRejected) {
    SnakeGame g;
    snake_create(&g, 0, 0);
    SnakeDir original = g.dir;
    snake_input(&g, SNAKE_LEFT);   // 假设初始向右
    // 反向输入不改变方向
    EXPECT_EQ(g.next_dir, original);  // 或断言为原方向
}

TEST(SnakeCoreTest, EatFoodGrows) {
    SnakeGame g;
    memset(&g, 0, sizeof(g));
    g.len = 3;
    g.body[0].x = 5; g.body[0].y = 5;
    g.body[1].x = 4; g.body[1].y = 5;
    g.body[2].x = 3; g.body[2].y = 5;
    g.food.x = 6; g.food.y = 5;  // 头前方
    g.dir = SNAKE_RIGHT;
    g.next_dir = SNAKE_RIGHT;
    int len_before = g.len;
    snake_tick(&g);
    EXPECT_EQ(g.len, len_before + 1);
    EXPECT_EQ(snake_score(&g), 5);
}

TEST(SnakeCoreTest, WallCollisionGameOver) {
    SnakeGame g;
    memset(&g, 0, sizeof(g));
    g.len = 3;
    g.body[0].x = 1; g.body[0].y = 5;
    g.dir = SNAKE_LEFT;
    g.next_dir = SNAKE_LEFT;
    snake_tick(&g);  // 撞墙
    EXPECT_TRUE(snake_is_over(&g));
}

TEST(SnakeCoreTest, SelfCollisionGameOver) {
    SnakeGame g;
    memset(&g, 0, sizeof(g));
    g.len = 4;
    g.body[0].x = 5; g.body[0].y = 5;
    g.body[1].x = 4; g.body[1].y = 5;
    g.body[2].x = 4; g.body[2].y = 4;  // 形成环形
    g.body[3].x = 5; g.body[3].y = 4;
    g.food.x = 99; g.food.y = 99;  // 不在食物处
    g.dir = SNAKE_UP;
    g.next_dir = SNAKE_UP;
    snake_tick(&g);  // 撞自己
    EXPECT_TRUE(snake_is_over(&g));
}

TEST(SnakeCoreTest, FoodNotOnSnake) {
    SnakeGame g;
    snake_create(&g, 99, 0);
    // 验证食物不在蛇身上
    for (int i = 0; i < g.len; i++) {
        EXPECT_FALSE(g.food.x == g.body[i].x && g.food.y == g.body[i].y);
    }
}

}  // namespace
```

**CMakeLists.txt 更新：**
```cmake
add_project_test(games_snake_core_test GAMES_SNAKE_CORE_TEST_SOURCES_VAR
    games_core)
```

**步骤：**

- [ ] 创建 `snake_core_test.cpp` 编写上述 7 个测试用例
- [ ] 更新 `test/games/CMakeLists.txt`：加入 `add_project_test(games_snake_core_test ...)`
- [ ] 运行单测：`ctest --test-dir build/engineering -R games_snake_core_test --output-on-failure`
- [ ] 预期：全部 PASS
- [ ] 提交：`test(games): snake_core 单元测试 7 个用例`

---

## 阶段 3：WASM 编译与 H5 前端

### Task 6: WASM 胶水层

**文件：**
- 创建：`engineering/apps/games/wasm/CMakeLists.txt`
- 创建：`engineering/apps/games/wasm/wasm_binding.c`

**构建目标：** Emscripten 将 `games_core` 编译为 `.wasm` + `.js`，由胶水层导出 C 函数给 JS 调用

**wasm_binding.c 导出：**

```c
#include <emscripten/emscripten.h>
#include "../../apps/games/core/g2048_core.h"
#include "../../apps/games/core/snake_core.h"

static G2048Game g2048_state;
static SnakeGame snake_state;

/* —— 2048 —— */
EMSCRIPTEN_KEEPALIVE
void* g2048_create(int seed) {
    g2048_create(&g2048_state, seed);
    return &g2048_state;
}

EMSCRIPTEN_KEEPALIVE
void g2048_move(int dir) {
    g2048_move(&g2048_state, (G2048Dir)dir);
}

EMSCRIPTEN_KEEPALIVE
int g2048_tile(int row, int col) {
    return g2048_tile_at(&g2048_state, row, col);
}

EMSCRIPTEN_KEEPALIVE
int g2048_score(void) { return g2048_state.score; }
EMSCRIPTEN_KEEPALIVE
bool g2048_game_over(void) { return g2048_state.game_over; }
EMSCRIPTEN_KEEPALIVE
bool g2048_won(void) { return g2048_state.won; }
EMSCRIPTEN_KEEPALIVE
bool g2048_can_move(void) { return g2048_can_move(&g2048_state); }

/* —— 贪吃蛇 —— */
EMSCRIPTEN_KEEPALIVE
void* snake_create(int seed, int diff) {
    snake_create(&snake_state, seed, diff);
    return &snake_state;
}

EMSCRIPTEN_KEEPALIVE
void snake_tick(void) {
    snake_tick(&snake_state);
}

EMSCRIPTEN_KEEPALIVE
void snake_input_dir(int dir) {
    snake_input(&snake_state, (SnakeDir)dir);
}

EMSCRIPTEN_KEEPALIVE
int snake_body_count(void) { return snake_len(&snake_state); }
EMSCRIPTEN_KEEPALIVE
int snake_body_x(int i) { return snake_body_x(&snake_state, i); }
EMSCRIPTEN_KEEPALIVE
int snake_body_y(int i) { return snake_body_y(&snake_state, i); }
EMSCRIPTEN_KEEPALIVE
int snake_food_x(void) { return snake_food_x(&snake_state); }
EMSCRIPTEN_KEEPALIVE
int snake_food_y(void) { return snake_food_y(&snake_state); }
EMSCRIPTEN_KEEPALIVE
int snake_score_val(void) { return snake_score(&snake_state); }
EMSCRIPTEN_KEEPALIVE
bool snake_over(void) { return snake_is_over(&snake_state); }
```

**CMakeLists.txt：**
```cmake
# 注意：需要本机安装 Emscripten SDK（emcc）
# 构建命令：emcc -o games.js games.c wasm_binding.c core/*.c -lm
# 产物：games.js + games.wasm
# 本 CMakeLists.txt 仅作占位说明，实际构建用 scripts/build-games-wasm.sh
```

**步骤：**

- [ ] 创建 `wasm/wasm_binding.c` 导出所有核心 API
- [ ] 创建 `scripts/build-games-wasm.sh`：emcc 构建脚本，产物输出到 `engineering/apps/games/web/`
- [ ] （可选）若本机无 emsdk，标注"需手动执行脚本"，提交代码骨架
- [ ] 提交：`wasm: Emscripten 胶水层与构建脚本`

---

### Task 7: H5 前端——2048

**文件：**
- 创建：`engineering/apps/games/web/2048.html`
- 创建：`engineering/apps/games/web/css/g2048.css`
- 创建：`engineering/apps/games/web/js/engine.js`（通信层抽象）
- 创建：`engineering/apps/games/web/js/g2048.js`

**engine.js（通信层抽象）：**

```javascript
// 通信后端抽象：默认使用 WASM，备用 C 服务器
class GameEngine {
    constructor() {
        this.backend = 'wasm';  // 'wasm' | 'server'
        this.serverUrl = null;
    }

    setWasm(Module) {
        this.wasm = Module;
        this.backend = 'wasm';
    }

    setServer(url) {
        this.serverUrl = url;
        this.backend = 'server';
    }

    // 2048 接口
    g2048_create(seed) {
        if (this.backend === 'wasm') {
            this._g2048 = this.wasm._g2048_create(seed);
            return this._g2048;
        }
        // RemoteEngine HTTP
    }

    g2048_move(dir) {
        if (this.backend === 'wasm') {
            this.wasm._g2048_move(dir);
        }
    }

    g2048_tile(r, c) { return this.wasm._g2048_tile(r, c); }
    g2048_score() { return this.wasm._g2048_score(); }
    g2048_game_over() { return this.wasm._g2048_game_over(); }
    g2048_won() { return this.wasm._g2048_won(); }
    g2048_can_move() { return this.wasm._g2048_can_move(); }
}
```

**g2048.js（渲染 + 输入）：**

```javascript
// Canvas 4×4 棋盘，16 个格子
// WASD / 方向键 → engine.g2048_move()
// 监听 g2048_game_over() / g2048_won()
// requestAnimationFrame 或按键触发渲染：读 16 个 tile 画色块+数字
```

**HTML 结构：**
```html
<!DOCTYPE html>
<html lang="zh">
<head>
    <meta charset="UTF-8">
    <title>2048</title>
    <link rel="stylesheet" href="css/g2048.css">
</head>
<body>
    <h1>2048</h1>
    <div id="score">分数: <span id="score-val">0</span></div>
    <canvas id="board" width="400" height="400"></canvas>
    <div id="msg"></div>
    <p>WASD 或方向键移动 | R 重新开始</p>
    <script src="js/engine.js"></script>
    <script src="js/g2048.js"></script>
    <!-- WASM 加载（由 emcc 生成） -->
    <script src="games.js"></script>
</body>
</html>
```

**颜色映射（g2048.css/JS）：**
- 0：灰 `#CDC1B4` / 2：白 / 4：浅橙 / 8：橙 / 16：红橙 / 32：红 / 64：深红 / 128：浅黄 / 256：黄 / 512：橙黄 / 1024：深黄 / 2048：金

**步骤：**

- [ ] 创建 `web/css/g2048.css`（格子颜色 + 布局）
- [ ] 创建 `web/js/engine.js`（GameEngine 抽象，LocalEngine 默认）
- [ ] 创建 `web/js/g2048.js`（Canvas 渲染 + 键盘输入 + 游戏循环）
- [ ] 创建 `web/2048.html`（页面骨架）
- [ ] 用浏览器打开 `2048.html`，WASM 加载后验证：方块生成、WASD 移动、合并、计分、游戏结束
- [ ] 提交：`web(g2048): H5 前端页面 + Canvas 渲染 + 键盘控制`

---

### Task 8: H5 前端——贪吃蛇

**文件：**
- 创建：`engineering/apps/games/web/snake.html`
- 创建：`engineering/apps/games/web/css/snake.css`
- 创建：`engineering/apps/games/web/js/snake.js`

**snake.js：**

```javascript
// Canvas 20×20 格子，每格像素 = 20px，总画布 400×400
// requestAnimationFrame / setInterval 驱动帧循环
// 每帧：engine.snake_tick() → 读蛇身坐标 → Canvas 绘制蛇身(绿)+食物(红)+网格
// WASD / 方向键 → engine.snake_input_dir()
// 显示：分数、速度、游戏结束 overlay
```

**HTML 结构：**
```html
<!DOCTYPE html>
<html lang="zh">
<head>
    <meta charset="UTF-8">
    <title>贪吃蛇</title>
    <link rel="stylesheet" href="css/snake.css">
</head>
<body>
    <h1>贪吃蛇</h1>
    <div id="info">分数: <span id="score-val">0</span> | 速度: <span id="speed-val">180</span>ms</div>
    <canvas id="board" width="400" height="400"></canvas>
    <div id="overlay" class="hidden">
        <div>游戏结束！得分: <span id="final-score">0</span></div>
        <button id="restart">再来一局</button>
    </div>
    <p>WASD 或方向键移动 | Q 退出</p>
    <script src="js/engine.js"></script>
    <script src="js/snake.js"></script>
    <script src="games.js"></script>
</body>
</html>
```

**步骤：**

- [ ] 创建 `web/css/snake.css`
- [ ] 创建 `web/js/snake.js`（帧循环 + Canvas 渲染 + 键盘输入）
- [ ] 创建 `web/snake.html`
- [ ] 浏览器验证：蛇移动、吃食物生长、撞墙游戏结束、重新开始
- [ ] 提交：`web(snake): H5 前端页面 + 帧循环 + Canvas 渲染`

---

### Task 9: 游戏大厅入口

**文件：**
- 创建：`engineering/apps/games/web/index.html`

**内容：** 标题 + 链接到 `2048.html` 和 `snake.html`

**步骤：**

- [ ] 创建 `web/index.html`
- [ ] 提交：`web: 游戏大厅入口页面`

---

## 阶段 4：C HTTP 服务器（可选增强）

> 本阶段本期不实现，留作后续扩展。服务器骨架在 Task 10 中创建。

### Task 10: C 服务器骨架

**文件：**
- 创建：`engineering/apps/games/server/CMakeLists.txt`（占位）
- 创建：`engineering/apps/games/server/main.c`（空入口 + TODO 注释）
- 创建：`engineering/apps/games/server/session.c`（TODO + 注释说明会话表设计）

**步骤：**

- [ ] 创建占位文件并注释说明 REST 接口设计（`/game/new`、`/game/{id}/input`、`/game/{id}/state`）
- [ ] 提交：`server: C 游戏服务器骨架（待实现）`

---

## 实施顺序与验证

| Task | 描述 | 验证命令 |
|------|------|---------|
| 1 | games_core 存根库 | `cmake --build build/engineering --target games_core` |
| 2 | 2048 核心逻辑 | `cmake --build build/engineering --target games_core` |
| 3 | 2048 单元测试 | `ctest --test-dir build/engineering -R games_g2048_core_test --output-on-failure` |
| 4 | 贪吃蛇核心逻辑 | `cmake --build build/engineering --target games_core` |
| 5 | 贪吃蛇单元测试 | `ctest --test-dir build/engineering -R games_snake_core_test --output-on-failure` |
| 6 | WASM 胶水层 | `scripts/build-games-wasm.sh`（需 emsdk）|
| 7 | H5 2048 前端 | 浏览器打开 `2048.html` 游玩验证 |
| 8 | H5 贪吃蛇前端 | 浏览器打开 `snake.html` 游玩验证 |
| 9 | 游戏大厅入口 | 浏览器打开 `index.html` |
| 10 | C 服务器骨架 | 留作后续 |

---

## 不做的事（YAGNI）

- 多人对战、排行榜、账号体系
- 游戏状态持久化
- Taro/小程序集成（WASM 可复用）
- 数独等更多游戏
- 阶段 4 服务器完整实现

---

## 自查清单

- [x] Spec 覆盖：每个设计章节都能指向对应 Task
- [ ] Placeholder scan：无 "TBD"、"TODO" 等占位符（除服务器骨架的明确 TODO）
- [ ] 类型一致性：g2048_core.h / snake_core.h 接口名称贯穿所有 Task
- [ ] 测试命名：`games_g2048_core_test` / `games_snake_core_test` 与 `add_project_test` 调用一致
- [ ] 提交纪律：每个 Task 独立提交，Commit Message 用中文
