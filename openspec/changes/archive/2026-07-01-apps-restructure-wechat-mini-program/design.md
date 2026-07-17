## Context

当前 `project/` 目录结构存在以下问题：
1. 项目类型混杂（游戏、工具、Web）没有分类
2. 三个游戏（snake、sudoku、2048）存在重复的平台适配代码
3. 缺乏统一的入口和测试覆盖
4. 无法在移动端（微信小程序）运行

本设计旨在解决这些问题，同时支持微信小程序平台的适配。

## Goals / Non-Goals

**Goals:**
- 建立清晰的 `apps/` 目录结构，按类型分类独立应用
- 抽取公共代码，消除三个游戏间的代码重复
- 实现游戏小程序统一入口，支持一站式游戏选择
- 通过 WebAssembly 技术实现微信小程序端的 C 代码运行

**Non-Goals:**
- 不修改 `src/` 目录下的库代码结构
- 不在微信小程序端实现新游戏逻辑（仅迁移现有逻辑）
- 不改变游戏的核心算法和游戏性

## Decisions

### Decision 1: 目录结构按类型分类

**选择:** `apps/{games,tools,web}/` 三层分类

**备选方案:**
- `apps/{snake,sudoku,2048,...}` 平铺式：项目多了难以管理
- `project/games/`, `project/tools/` 保持 project 名称：语义不准确

**理由:** 类型分类更易扩展，新项目只需归类到对应目录

### Decision 2: 公共代码独立抽取

**选择:** 创建 `apps/common/` 目录存放公共代码

**备选方案:**
- 每个游戏复制粘贴：维护成本高
- 放在 `src/` 下：src 是算法库，apps 是应用，职责分离

**理由:** `common/` 与 `apps/` 同级，可被所有子项目引用，职责清晰

### Decision 3: WebAssembly 作为小程序适配方案

**选择:** 使用 Emscripten 将 C 代码编译为 WASM

**备选方案:**
- 云函数：需要网络，有延迟
- 纯 JS 重写：工作量大，失去 C 代码复用
- 原生插件：需要微信官方合作，普通开发者无法使用

**理由:** WASM 可在 WebView 中直接运行游戏逻辑，性能好，无需网络

### Decision 4: 游戏逻辑与渲染分离

**选择:** WASM 只负责游戏逻辑，WXML/CSS 负责渲染

**备选方案:**
- WASM 直接操作 Canvas：微信 Canvas API 无法直接调用
- 纯 JS 重写渲染：工作量大

**理由:** 利用微信的 WXML 组件系统，通过 JS 桥接层同步状态

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                      微信小程序端                                │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                    pages/                                 │  │
│  │   ├── index/          游戏列表首页                        │  │
│  │   ├── snake/          贪吃蛇游戏页                        │  │
│  │   ├── sudoku/         数独游戏页                          │  │
│  │   └── 2048/           2048游戏页                          │  │
│  └───────────────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                    components/                            │  │
│  │   ├── game-common/     通用游戏组件                       │  │
│  │   └── snake-board/     贪吃蛇棋盘组件                     │  │
│  │   └── sudoku-board/    数独棋盘组件                       │  │
│  │   └── 2048-board/      2048棋盘组件                       │  │
│  └───────────────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                    wasm/                                   │  │
│  │   ├── snake.js / snake.wasm                               │  │
│  │   ├── sudoku.js / sudoku.wasm                             │  │
│  │   └── 2048.js / 2048.wasm                                 │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

### 目录结构

```
apps/
├── CMakeLists.txt                      # 主构建配置
├── common/                             # 公共代码
│   ├── CMakeLists.txt
│   ├── terminal.h / terminal.c         # 终端适配层
│   ├── menu.h / menu.c                 # 菜单系统
│   └── game_common.h / game_common.c   # 游戏通用定义
├── games/
│   ├── CMakeLists.txt
│   ├── snake/                          # 贪吃蛇
│   │   ├── snake.h / snake.c
│   │   ├── snake_core.c                # 核心逻辑（WASM用）
│   │   ├── main.c                      # 独立运行入口
│   │   └── CMakeLists.txt
│   ├── sudoku/                         # 数独
│   │   ├── sudoku.h / sudoku.cpp
│   │   ├── sudoku_core.c               # 核心逻辑（WASM用）
│   │   ├── main.cpp
│   │   └── CMakeLists.txt
│   ├── 2048/                           # 2048
│   │   ├── 2048.h / 2048.c
│   │   ├── 2048_core.c                 # 核心逻辑（WASM用）
│   │   ├── main.c
│   │   └── CMakeLists.txt
│   └── mini_program/                   # 游戏小程序（PC终端版）
│       ├── main.c
│       └── CMakeLists.txt
├── tools/
│   └── calculator/
│       ├── calculator.h / calculator.c
│       ├── main.c
│       └── CMakeLists.txt
└── web/
    ├── reading-radar/                  # 阅读追踪（静态页面）
    └── knowledge_hub/                  # 学习追踪平台（Taro + React）

miniprogram/                            # 微信小程序（单独仓库）
├── app.js / app.json / app.wxss
├── pages/
├── components/
├── wasm/
└── utils/
    └── wasm_bridge.js                  # WASM 桥接层
```

### 公共代码接口设计

```c
// terminal.h - 终端适配层
typedef enum {
    TERM_OK = 0,
    TERM_ERR_INIT_FAILED,
    TERM_ERR_RESTORE_FAILED,
} TermResult;

TermResult terminal_init(void);         // 初始化（设置UTF-8 + 原始模式）
TermResult terminal_restore(void);      // 恢复终端
int        terminal_getch(void);        // 获取按键（非阻塞）
int        terminal_kbhit(void);        // 检测按键
void       terminal_sleep_ms(int ms);   // 延时
void       terminal_hide_cursor(void);  // 隐藏光标
void       terminal_show_cursor(void);  // 显示光标
void       terminal_clear(void);        // 清屏
```

```c
// menu.h - 菜单系统
typedef struct {
    const char *title;
    const char *options[8];
    int        option_count;
} MenuConfig;

typedef struct {
    const char *name;
    void (*launch)(void);
} GameEntry;

int  menu_main_select(const GameEntry *games, int count);
int  menu_difficulty_select(const char *title, const char *const *options, int count);
void menu_wait_exit(void);
void menu_show_banner(const char *title);
```

### WASM 编译配置

```bash
# 编译贪吃蛇核心为 WASM
emcc snake_core.c \
    -s WASM=1 \
    -s EXPORTED_FUNCTIONS='["_init_game","_update","_render","_get_state"]' \
    -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","getValue"]' \
    -s MODULARIZE=1 \
    -s EXPORT_NAME='SnakeModule' \
    -O2 \
    -o snake.js
```

### JS 桥接层设计

```javascript
// wasm_bridge.js
class GameBridge {
    constructor(module) {
        this.module = module;
        this.state = null;
    }

    // 初始化游戏
    init(difficulty) {
        this.module._init_game(difficulty);
        this.syncState();
    }

    // 获取游戏状态
    syncState() {
        // 从 WASM 内存读取游戏状态
        const ptr = this.module._get_state();
        // 解析状态并更新 this.state
    }

    // 处理输入
    handleInput(direction) {
        this.module._set_direction(direction);
        this.module._update();
        this.syncState();
        return this.state;
    }

    // 渲染（生成描述字符串供 WXML 使用）
    render() {
        return this.state; // 返回可序列化状态
    }
}
```

## Risks / Trade-offs

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| WASM 文件过大 | 加载慢 | 使用 `-O2` 优化，考虑分片加载 |
| WASM 不支持所有 C 特性 | 编译失败 | 限制使用指针运算，避免复杂内存管理 |
| 微信 WebView WASM 兼容 | 运行异常 | 测试主流机型，必要时回退到 JS 实现 |
| 三个游戏需要同时加载 | 内存占用 | 按需加载，退出游戏时释放 WASM 模块 |
| calculator 有编译问题 | 无法迁移 | 迁移后单独修复 |

## Migration Plan

### Phase 1: 目录迁移
1. 创建 `apps/` 目录结构
2. 移动 `project/*` → 对应 `apps/` 子目录
3. 更新根 `CMakeLists.txt`
4. 运行测试验证构建
5. 删除 `project/` 目录

### Phase 2: 公共代码抽取
1. 创建 `apps/common/terminal.*`
2. 创建 `apps/common/menu.*`
3. 重构三个游戏使用公共代码
4. 修复 calculator 编译问题

### Phase 3: 游戏小程序
1. 创建 `apps/games/mini_program/`
2. 实现统一入口和菜单系统
3. 测试独立构建和统一构建

### Phase 4: 微信小程序适配
1. 创建微信小程序项目结构
2. 抽取每个游戏的 core 文件
3. 使用 Emscripten 编译 WASM
4. 实现 WXML 界面
5. 编写 JS 桥接层
6. 集成测试

## Open Questions

1. **reading-radar** 是否需要迁移？它是静态 HTML，可能需要单独处理
2. **微信小程序 WASM 大小限制**？微信包有 2MB 限制，WASM 文件需要考虑分包
3. **是否需要游戏存档功能**？考虑使用微信的本地存储
4. **是否需要排行榜功能**？考虑使用微信云开发或云函数