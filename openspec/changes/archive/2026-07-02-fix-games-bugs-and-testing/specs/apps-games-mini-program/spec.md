# apps-games-mini-program

游戏小程序统一入口规范（Delta Spec - 增强版本）

## MODIFIED Requirements

### Requirement: 主菜单显示
系统 SHALL 显示游戏列表菜单，支持四个游戏。

#### Scenario: 显示四个游戏
- **WHEN** 用户启动游戏小程序
- **THEN** 显示带装饰边框的游戏列表：
  ```
  ╔═══════════════════════════════╗
  ║       🎮 游戏中心 🎮           ║
  ╠═══════════════════════════════╣
  ║  🐍 贪吃蛇                     ║
  ║  🔢 数独                       ║
  ║  🎮 2048                      ║
  ║  💎 消消乐                     ║
  ╚═══════════════════════════════╝
  ```

### Requirement: 消消乐入口
系统 SHALL 支持从游戏列表进入消消乐游戏。

#### Scenario: 选择消消乐
- **WHEN** 用户点击消消乐游戏卡片
- **THEN** 导航到消消乐游戏页面

#### Scenario: 消消乐游戏结束
- **WHEN** 消消乐游戏结束
- **THEN** 显示结果弹窗，可选择重玩或返回主页

## ADDED Requirements

### Requirement: 数据持久化存储
系统 SHALL 使用微信 Storage API 保存游戏数据。

#### Scenario: 存储游戏数据
- **WHEN** 游戏数据需要保存
- **THEN** 使用 `wx.setStorageSync` 异步保存到本地存储

#### Scenario: 读取游戏数据
- **WHEN** 游戏需要恢复进度
- **THEN** 使用 `wx.getStorageSync` 读取本地存储数据

#### Scenario: 版本迁移
- **WHEN** 应用更新后首次启动
- **THEN** 检测存储版本，执行必要的数据迁移

### Requirement: 游戏数据格式
系统 SHALL 使用统一的数据格式存储各游戏进度。

#### Scenario: 贪吃蛇数据格式
- **WHEN** 存储贪吃蛇数据
- **THEN** 格式为 `{ bestScore: number }`

#### Scenario: 数独数据格式
- **WHEN** 存储数独数据
- **THEN** 格式为 `{ bestScore: number, chapters: { unlocked: boolean, stars: Record<number, number> }[] }`

#### Scenario: 消消乐数据格式
- **WHEN** 存储消消乐数据
- **THEN** 格式为 `{ chapters: { unlocked: boolean, stars: Record<number, number> }[] }`