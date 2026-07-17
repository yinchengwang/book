## Why

当前 `project/` 目录结构混乱，包含了游戏、工具和 Web 项目混杂在一起，且缺乏统一的公共代码抽象。同时，用户希望将三个 C 语言游戏（贪吃蛇、数独、2048）适配到微信小程序平台，实现移动端运行。

## What Changes

### 目录迁移
- 将 `project/` 整体迁移到 `apps/`，按类型分类：
  - `apps/games/` - 游戏项目（snake、sudoku、2048）
  - `apps/tools/` - 工具项目（calculator）
  - `apps/web/` - Web 项目（reading-radar、knowledge_hub）
- 将 `knowledge_hub/` 迁移到 `apps/web/knowledge_hub/`（学习追踪平台）
- 删除原 `project/` 目录和根目录的 `knowledge_hub/`
- 创建 `test/apps/` 用于独立项目的测试用例

### 公共代码抽取
- 创建 `apps/common/` 目录，抽取三个游戏共用的平台适配代码：
  - `terminal.h/terminal.c` - 终端操作封装（原始模式、UTF-8、键盘输入）
  - `menu.h/menu.c` - 统一菜单系统
- 重构三个游戏使用公共代码，消除代码重复

### 微信小程序适配
- 使用 WebAssembly 技术将 C 代码编译为 WASM 模块
- 为每个游戏创建 WASM 版本的核心逻辑（去除终端 UI）
- 开发微信小程序前端界面（WXML/WXSS/JS）
- 编写 JS 桥接层连接 WASM 模块和微信界面

### 构建系统更新
- 更新根 `CMakeLists.txt` 添加 `apps/` 子目录
- 为 `apps/` 创建独立的 CMake 构建配置
- 支持独立构建（每个游戏单独编译）和统一构建（游戏小程序）

## Capabilities

### New Capabilities
- `apps-common-terminal`: 跨平台终端适配层，封装 `console_raw_mode()`、`my_getch()`、`sleep_ms()` 等函数
- `apps-common-menu`: 统一游戏菜单系统，提供游戏选择、难度选择、结果展示等通用交互
- `apps-games-snake`: 贪吃蛇游戏 C 语言实现，支持终端渲染
- `apps-games-sudoku`: 数独游戏 C++ 实现，支持终端渲染
- `apps-games-2048`: 2048 游戏 C 语言实现，支持终端渲染
- `apps-games-mini-program`: 游戏小程序统一入口，支持三个游戏的集成运行
- `apps-wechat-mini-program`: 微信小程序版本，使用 WebAssembly 运行游戏逻辑
- `apps-web-knowledge-hub`: 学习追踪平台（从根目录迁移）

### Modified Capabilities
<!-- 现有能力的需求变更：无 -->

## Impact

### 受影响代码
- `project/` → `apps/` 迁移（文件移动）
- 根目录 `CMakeLists.txt` 新增 `add_subdirectory(apps)`
- `test/` 下新增 `test/apps/` 目录

### 新增依赖
- Emscripten SDK（用于编译 C 代码为 WebAssembly）

### 新增目录结构
```
apps/
├── CMakeLists.txt
├── common/
│   ├── terminal.h / terminal.c
│   ├── menu.h / menu.c
│   └── CMakeLists.txt
├── games/
│   ├── CMakeLists.txt
│   ├── snake/
│   ├── sudoku/
│   ├── 2048/
│   └── mini_program/
├── tools/
│   └── calculator/
└── web/
    ├── reading-radar/
    └── knowledge_hub/
```