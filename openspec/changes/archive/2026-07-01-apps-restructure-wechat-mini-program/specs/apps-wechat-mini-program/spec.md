# apps-wechat-mini-program

微信小程序适配规范，使用 WebAssembly 运行游戏逻辑。

## ADDED Requirements

### Requirement: WASM 模块加载
系统 SHALL 提供 `WasmLoader` 类，管理 WASM 模块的异步加载。

#### Scenario: 加载贪吃蛇 WASM
- **WHEN** 用户进入贪吃蛇页面
- **THEN** 异步加载 snake.wasm，加载完成后调用回调函数

#### Scenario: 加载失败处理
- **WHEN** WASM 文件加载失败
- **THEN** 显示错误提示"游戏加载失败，请检查网络"

#### Scenario: 重复加载优化
- **WHEN** 用户从数独页面返回再进入贪吃蛇页面
- **THEN** 如果 WASM 已加载则直接使用缓存

### Requirement: 游戏状态同步
系统 SHALL 提供 `GameBridge` 类，同步 WASM 游戏状态与微信 UI。

#### Scenario: 同步蛇身位置
- **WHEN** WASM 游戏状态更新后
- **THEN** `GameBridge.syncState()` 从 WASM 内存读取蛇身坐标，渲染到 WXML

#### Scenario: 同步棋盘状态（2048）
- **WHEN** WASM 游戏状态更新后
- **THEN** `GameBridge.syncState()` 从 WASM 内存读取 4x4 棋盘，渲染到 WXML

#### Scenario: 同步数独状态
- **WHEN** WASM 游戏状态更新后
- **THEN** `GameBridge.syncState()` 从 WASM 内存读取 9x9 棋盘和光标位置

### Requirement: 输入处理
系统 SHALL 将微信触摸/手势输入转换为 WASM 游戏指令。

#### Scenario: 贪吃蛇方向控制
- **WHEN** 用户在贪吃蛇页面滑动
- **THEN** 根据滑动方向调用 `WASM._set_direction(direction)`

#### Scenario: 2048 滑动控制
- **WHEN** 用户在 2048 页面滑动
- **THEN** 根据滑动方向调用 `WASM._move(direction)`

#### Scenario: 数独数字输入
- **WHEN** 用户点击数字键盘
- **THEN** 调用 `WASM._place_number(num)`

#### Scenario: 数独光标移动
- **WHEN** 用户滑动数独棋盘
- **THEN** 根据滑动方向更新光标位置

### Requirement: 游戏结束处理
系统 SHALL 在 WASM 返回游戏结束状态时显示结果界面。

#### Scenario: 贪吃蛇结束
- **WHEN** WASM 返回 `game_over = true`
- **THEN** 显示"游戏结束！得分: X"，提供重新开始和返回菜单选项

#### Scenario: 2048 胜利
- **WHEN** WASM 返回 `won = true`
- **THEN** 显示庆祝动画和"恭喜达到 2048！"

#### Scenario: 2048 失败
- **WHEN** WASM 返回 `game_over = true`
- **THEN** 显示"游戏结束！最终得分: X"

#### Scenario: 数独完成
- **WHEN** WASM 返回 `complete = true`
- **THEN** 显示"恭喜完成数独！"

### Requirement: 性能要求
系统 SHALL 满足以下性能指标：

| 指标 | 要求 | 说明 |
|------|------|------|
| WASM 加载时间 | < 2s | 首次加载 WASM 文件 |
| 游戏响应延迟 | < 50ms | 触摸输入到画面更新的延迟 |
| 内存占用 | < 20MB | WASM 模块内存 |

### Requirement: 包大小限制
系统 SHALL 遵守微信小程序包大小限制。

#### Scenario: 主包大小
- **WHEN** 提交小程序审核
- **THEN** 主包大小不超过 2MB

#### Scenario: 分包加载
- **WHEN** 使用分包机制
- **THEN** 每个游戏使用独立分包，按需加载