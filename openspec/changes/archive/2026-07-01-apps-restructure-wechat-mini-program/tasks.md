## 1. 目录迁移

- [x] 1.1 创建 `apps/` 目录结构（games/、tools/、web/）
- [x] 1.2 移动 `project/snake/` → `apps/games/snake/`
- [x] 1.3 移动 `project/sudoku/` → `apps/games/sudoku/`
- [x] 1.4 移动 `project/2048/` → `apps/games/2048/`
- [x] 1.5 移动 `project/calculator/` → `apps/tools/calculator/`
- [x] 1.6 移动 `project/reading-radar/` → `apps/web/reading-radar/`
- [x] 1.7 移动 `knowledge_hub/` → `apps/web/knowledge_hub/`
- [x] 1.8 更新根 `CMakeLists.txt`，添加 `add_subdirectory(apps)`
- [x] 1.9 更新 `apps/` 下各子项目的 CMakeLists.txt（路径调整）
- [x] 1.10 创建 `test/apps/` 目录结构
- [x] 1.11 删除原 `project/` 目录
- [x] 1.12 运行构建测试验证迁移成功

## 2. 公共代码抽取

- [x] 2.1 创建 `apps/common/CMakeLists.txt`
- [x] 2.2 创建 `apps/common/terminal.h` 头文件
- [x] 2.3 创建 `apps/common/terminal.c` 实现（抽取三个游戏的平台适配代码）
- [x] 2.4 创建 `apps/common/menu.h` 头文件
- [x] 2.5 创建 `apps/common/menu.c` 实现（统一菜单系统）
- [x] 2.6 重构 `apps/games/snake/` 使用 `common/terminal.h`
- [x] 2.7 重构 `apps/games/sudoku/` 使用 `common/terminal.h`
- [x] 2.8 重构 `apps/games/2048/` 使用 `common/terminal.h`
- [x] 2.9 修复 `apps/tools/calculator/` 的编译问题
- [x] 2.10 编写 `test/apps/test_common_terminal.cpp` 测试终端适配层
- [x] 2.11 编写 `test/apps/test_common_menu.cpp` 测试菜单系统

## 3. 游戏小程序（PC 终端版）

- [x] 3.1 创建 `apps/games/mini_program/` 目录
- [x] 3.2 创建 `apps/games/mini_program/main.c` 主入口
- [x] 3.3 实现游戏选择菜单 `menu_main_select()`
- [x] 3.4 实现贪吃蛇启动函数 `launch_snake()`
- [x] 3.5 实现数独启动函数 `launch_sudoku()`
- [x] 3.6 实现 2048 启动函数 `launch_2048()`
- [x] 3.7 实现游戏结束返回菜单机制
- [x] 3.8 创建 `apps/games/mini_program/CMakeLists.txt`
- [x] 3.9 更新 `apps/games/CMakeLists.txt` 包含 mini_program
- [x] 3.10 测试独立构建和统一构建

## 4. 微信小程序项目搭建（纯 JS 方案）

- [x] 4.1 决定采用纯 JS 方案（跳过 WASM）
- [x] 4.2 创建微信小程序项目目录 `apps/web/games-mini-program/`
- [x] 4.3 初始化 Taro 项目配置
- [x] 4.4 创建 `app.config.ts` 入口配置
- [x] 4.5 创建 `pages/index/` 首页（游戏列表）
- [x] 4.6 创建 `pages/snake/` 贪吃蛇页面
- [x] 4.7 创建 `pages/sudoku/` 数独页面
- [x] 4.8 创建 `pages/game2048/` 2048 页面
- [x] 4.9 实现贪吃蛇棋盘组件
- [x] 4.10 实现数独棋盘组件
- [x] 4.11 实现 2048 棋盘组件
- [x] 4.12 实现消消乐游戏（参考开心消消乐）
- [x] 4.13 章节区域系统（普通/困难/超难关）
- [x] 4.14 章节解锁（星星解锁下一章节）
- [x] 4.15 星星评价系统（1-3星）
- [x] 4.16 步数验证（确保可通关）
- [x] 4.17 特殊元素效果（发条鸟/松鼠/蜗牛）

## 5. 游戏逻辑 JS 实现

- [x] 5.1 实现贪吃蛇 JS 核心逻辑 `utils/snake.ts`
- [x] 5.2 实现数独 JS 核心逻辑 `utils/sudoku.ts`
- [x] 5.3 实现 2048 JS 核心逻辑 `utils/2048.ts`
- [x] 5.4 实现贪吃蛇桥接层（页面组件）
- [x] 5.5 实现数独桥接层（页面组件）
- [x] 5.6 实现 2048 桥接层（页面组件）

## 6. H5 开发与测试

- [x] 6.1 创建 `web/` 目录（Vite 开发版）
- [x] 6.2 实现纯 React 版本首页
- [x] 6.3 Playwright E2E 测试通过 (5/5)

## 7. 集成测试与发布

- [ ] 7.1 安装依赖 `npm install`
- [ ] 7.2 H5 开发模式测试
- [ ] 7.3 微信开发者工具测试
- [ ] 7.4 测试页面切换和状态保持
- [ ] 7.5 优化包大小
- [ ] 7.6 提交微信小程序审核

## 备注

- **PC 终端版**（C 代码）：`apps/games/mini_program/game_center.exe`
- **微信小程序版**（JS）：`apps/web/games-mini-program/web/`（Vite 开发）
- 两个版本独立开发，互不影响