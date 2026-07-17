## Why

AlgorithmPractice 项目长期聚焦算法和数据结构学习，缺少一个完整的、可运行的小游戏项目来综合练习 C 语言核心技能（控制流、数据结构、交互输入输出）。贪吃蛇是经典的练手项目，适合作为 project/ 目录下的第一个独立示例。

## What Changes

- 新增 `project/snake/` 目录，包含贪吃蛇游戏的完整 C 实现
- 游戏运行于 100×50 的终端围墙内，支持 WASD / 方向键操控
- 提供三档难度（简单/困难/地域），速度随分数梯度提升
- 侧面面板实时显示得分、长度、当前速度

## Capabilities

### New Capabilities

- `snake-game`: 贪吃蛇游戏核心实现，包含地图渲染、蛇移动/生长、食物生成、碰撞检测（撞墙/撞自己）、难度选择、速度梯度

## Impact

- 新增 `project/snake/snake.c` — 独立可执行游戏程序，无外部依赖
- 新增 `project/snake/CMakeLists.txt` — CMake 构建配置
- 根 `CMakeLists.txt:95-97` 已支持 `project/` 目录自动发现，无需修改
