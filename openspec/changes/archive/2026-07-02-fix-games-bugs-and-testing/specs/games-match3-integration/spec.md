# games-match3-integration

消消乐游戏集成规范，将消消乐从 web/ 目录集成到小程序。

## Purpose

该规格定义消消乐游戏从小程序 web/ 版本集成到微信小程序的规范。

## ADDED Requirements

### Requirement: 消消乐入口

#### Scenario: 游戏列表显示消消乐
- **WHEN** 用户打开游戏小程序首页
- **THEN** 显示四个游戏入口：贪吃蛇、数独、2048、消消乐

#### Scenario: 进入消消乐游戏
- **WHEN** 用户点击消消乐入口
- **THEN** 导航到消消乐游戏页面

### Requirement: 消消乐核心逻辑

#### Scenario: 棋盘初始化
- **WHEN** 开始新游戏
- **THEN** 生成 9x9 棋盘，无连续 3 个相同宝石

#### Scenario: 宝石交换
- **WHEN** 用户点击两个相邻宝石
- **THEN** 如果产生匹配则执行交换，否则取消

#### Scenario: 宝石消除
- **WHEN** 宝石形成 3 个以上连续相同
- **THEN** 消除宝石，得分增加，新宝石从上方落下

#### Scenario: 步数限制
- **WHEN** 完成所有步数且未达成目标
- **THEN** 游戏失败，显示重玩选项

#### Scenario: 达成目标
- **WHEN** 在步数限制内达成目标
- **THEN** 游戏胜利，显示星星评价

### Requirement: 关卡系统

#### Scenario: 章节选择
- **WHEN** 进入消消乐游戏
- **THEN** 显示章节列表，根据星星数解锁章节

#### Scenario: 关卡选择
- **WHEN** 选择已解锁章节
- **THEN** 显示该章节所有关卡，已通过的显示星星数

### Requirement: 数据保存

#### Scenario: 保存关卡进度
- **WHEN** 完成关卡获得星星
- **THEN** 保存到本地存储

#### Scenario: 恢复游戏进度
- **WHEN** 重新进入消消乐
- **THEN** 恢复上次退出时的章节和关卡进度