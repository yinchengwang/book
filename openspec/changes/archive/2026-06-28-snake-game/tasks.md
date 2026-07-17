## 1. 环境与目录结构

- [x] 1.1 确认 `project/snake/` 目录存在，`CMakeLists.txt` 指向 `project/` 而非 `src/project/`
- [x] 1.2 CMake 构建验证：重新 configure 后 `snake_game` target 可被 make 发现

## 2. 游戏核心数据

- [x] 2.1 定义全局变量：地图数组 `map[HEIGHT][WIDTH]`、蛇身数组 `snake[WIDTH*HEIGHT]`、`snake_len`、`food`、`dir`、`score`、`speed`、`game_over`
- [x] 2.2 定义枚举 `Dir`（上下左右）、`Diff`（简单/困难/地域）

## 3. 初始化

- [x] 3.1 `init_game()`: 在区域内随机生成蛇头位置，初始化长度为 3，朝右，调用 `spawn_food()`
- [x] 3.2 `spawn_food()`: 随机位置生成食物，排除与蛇身重叠，循环最多 1000 次

## 4. 输入处理

- [x] 4.1 Windows 分支：`conio.h` + `_kbhit()` + `_getch()`，识别方向键前缀 `0/224`
- [x] 4.2 Linux 分支：`termios` 非canonical 模式 + `select()` 实现 `kbhit`，`getch()` 读取单字符
- [x] 4.3 方向输入映射：WASD 和方向键均映射到 `next_dir`，180° 掉头不响应（已在 update 阶段保证）

## 5. 游戏循环

- [x] 5.1 主循环：非 `game_over` 时，每帧检测输入 → `update()` → `render()` → `sleep_ms(speed)`
- [x] 5.2 `update()`: 蛇头前移一格 → 撞墙检测 → 撞自己检测 → 食物碰撞 → 蛇身移动 + 生长处理
- [x] 5.3 `update_speed()`: 根据 `score / FOOD_SCORE` 计算等级，调整 `speed`（有下限）

## 6. 渲染

- [x] 6.1 清屏（Windows: `system("cls")`，Linux: `\033[2J\033[H]`）
- [x] 6.2 围墙渲染：上下一行 `#`，左右两侧 `#`
- [x] 6.3 蛇身渲染：蛇头 `@`（绿色），身体 `o`（绿色），ANSI 转义序列上色
- [x] 6.4 食物渲染：`*`（红色）
- [x] 6.5 状态面板：得分、长度、速度（ms）、难度

## 7. 难度与速度梯度

- [x] 7.1 启动菜单：显示三个难度选项，读取用户输入选择 `base_speed`
- [x] 7.2 速度梯度公式：三个难度各自独立的递减曲线，确保速度不低于下限

## 8. 构建与验证

- [x] 8.1 CMake 构建：`cmake --build build --target snake_game`，确认无编译错误
- [x] 8.2 运行验证：启动 → 选难度 → 开始 → WASD 移动 → 吃食物 → 计分 → 撞墙或撞自己死亡
